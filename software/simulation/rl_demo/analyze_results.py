"""Package quantitative evidence, plots and a Chinese acceptance report."""
import argparse
import csv
import hashlib
import json
import shutil
from pathlib import Path
import numpy as np
import torch
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from showcase import gate_results,GATE_TIMES,GATE_RADIUS_M,GATE_TUBE_RADIUS_M,DRONE_ENVELOPE_RADIUS_M,reference
from dynamics import ActorCritic

p=argparse.ArgumentParser();p.add_argument('--demo',type=Path,required=True)
p.add_argument('--training',type=Path,required=True);p.add_argument('--ancestor',type=Path,required=True)
p.add_argument('--package',type=Path,required=True);args=p.parse_args();demo=args.demo
read=lambda p:json.loads(p.read_text())
course=read(demo/'learned-course/native_result.json')
test=read(args.training/'evaluation-policy_0300-test.json')
x=np.load(demo/'learned-course/native_trace.npy')
surrogate=np.load(args.training/'preflight-policy_0300-trained_ppo_residual.npy')
assert x.shape==surrogate.shape and course['status']=='passed'
gates=gate_results(x)
minimum=np.inf
for tg in GATE_TIMES:
    pt,vel,_,_=reference(tg);normal=vel/np.linalg.norm(vel)
    d=x[:,1:4]-pt;axial=d@normal;radial=np.linalg.norm(d-axial[:,None]*normal,axis=1)
    clear=np.sqrt(axial**2+(radial-GATE_RADIUS_M)**2)-GATE_TUBE_RADIUS_M-DRONE_ENVELOPE_RADIUS_M
    minimum=min(minimum,float(clear.min()))
cross_backend=np.linalg.norm(x[:,1:4]-surrogate[:,1:4],axis=1)

# Verify the rendered visual and the full swept 60 mm propeller tip radius.
from pxr import Usd,UsdGeom
stage=Usd.Stage.Open(str(args.package/'USD/open32droe/robot.usd'));cache=UsdGeom.XformCache();parts=[]
for prim in stage.Traverse():
    if prim.IsA(UsdGeom.Mesh):
        vertices=np.array(UsdGeom.Mesh(prim).GetPointsAttr().Get())
        if len(vertices):parts.append(np.c_[vertices,np.ones(len(vertices))]@np.array(cache.GetLocalToWorldTransform(prim)))
xyz=np.concatenate(parts)[:,:3]+[0,0,0.006673]
visual_radius=float(np.linalg.norm(xyz,axis=1).max())
swept_radius=float(np.sqrt((np.hypot(0.043145,0.043171)+0.03)**2+(0.021193+0.006673+0.001)**2))
verification={'status':'passed' if all(g['passed'] for g in gates) and minimum>0 and DRONE_ENVELOPE_RADIUS_M>swept_radius else 'failed',
 'source':'post-processing the saved force-driven PhysX trajectory; no trajectory alteration',
 'envelope_radius_m':DRONE_ENVELOPE_RADIUS_M,'visual_vertex_count':len(xyz),'visual_radius_m':visual_radius,
 'conservative_swept_prop_radius_m':swept_radius,'minimum_clearance_to_any_ring_m':minimum,
 'gates':gates,'gates_passed':sum(g['passed'] for g in gates),
 'cross_backend_position_rms_m':float(np.sqrt(np.mean(cross_backend**2))),
 'cross_backend_position_max_m':float(cross_backend.max())}
(demo/'gate_clearance_verification.json').write_text(json.dumps(verification,indent=2)+'\n')
assert verification['status']=='passed',verification

(demo/'policy').mkdir(exist_ok=True);(demo/'training').mkdir(exist_ok=True);(demo/'source').mkdir(exist_ok=True)
shutil.copy2(args.training/'policy_0300.pt',demo/'policy/policy.pt')
net=ActorCritic();net.load_state_dict(torch.load(demo/'policy/policy.pt',map_location='cpu',weights_only=False)['model']);net.eval()
with torch.inference_mode():
    exported=torch.jit.trace(net,torch.zeros(1,35));exported.save(str(demo/'policy/actor.pt'))
    torch.manual_seed(6001);batch=torch.randn(32,35)*0.5
    difference=float((exported(batch)-net(batch)).abs().max())
    assert difference<1e-6 and float(exported(batch).abs().max())<=1
(demo/'policy/export_check.json').write_text(json.dumps({'status':'passed','batch_size':32,'max_absolute_difference':difference,'action_bounds':[-1,1]},indent=2)+'\n')
shutil.copy2(args.training/'config.json',demo/'training/fine_tuning_config.json')
shutil.copy2(args.training/'training.csv',demo/'training/fine_tuning.csv')
shutil.copy2(args.ancestor/'training.csv',demo/'training/initial_training.csv')
shutil.copy2(args.ancestor/'config.json',demo/'training/initial_training_config.json')
shutil.copy2(args.training/'evaluation-policy_0300-test.json',demo/'evaluation.json')
for source in Path(__file__).parent.iterdir():
    if source.suffix in ('.py','.json','.md'):shutil.copy2(source,demo/'source'/source.name)

# Standard plotting tools produce standalone evidence figures.
plt.style.use('dark_background')
colors={'pd':'#91a9c2','rl':'#43d9ec'}
fig,axs=plt.subplots(1,2,figsize=(12.8,4.4),layout='constrained')
for ax,file,limit,title in zip(axs,[args.ancestor/'training.csv',args.training/'training.csv'],[1000,1200],['Initial PPO training','Fine-tuning after environment / optimizer changes']):
    rows=list(csv.DictReader(file.open()));it=np.array([int(r['iteration']) for r in rows]);val=np.array([float(r['tracking_error_m']) for r in rows])
    keep=it<=limit;it=it[keep];val=val[keep]
    ax.plot(it,val,color=colors['rl'],alpha=.2,lw=.7)
    ax.plot(it[19:],np.convolve(val,np.ones(20)/20,mode='valid'),color=colors['rl'],lw=1.8)
    ax.set(xlabel='PPO iteration',ylabel='Training mean tracking error (m)',title=title);ax.grid(alpha=.12)
axs[1].axvline(300,color='#fdbc4f',ls='--');axs[1].text(318,axs[1].get_ylim()[1]*.88,'Selected checkpoint\niteration 300',color='#fdbc4f')
fig.suptitle('Training phases have different randomization; curves are not one unchanged experiment.',fontsize=11)
fig.savefig(demo/'training_curves.png',dpi=160);plt.close(fig)
fig,axs=plt.subplots(1,2,figsize=(12.8,4.8),layout='constrained')
axs[0].plot(x[:,14],x[:,15],color=colors['pd'],ls='--',label='Reference')
axs[0].plot(x[:,1],x[:,2],color=colors['rl'],label='Native PhysX flight')
axs[0].set(xlabel='x (m)',ylabel='y (m)',title='Complete 34-second course');axs[0].axis('equal');axs[0].legend()
axs[1].plot(x[:,0],x[:,17]*100,color=colors['rl']);axs[1].axvspan(29,31,color='#fdbc4f',alpha=.18,label='Additional disturbance')
axs[1].set(xlabel='Simulation time (s)',ylabel='Position error (cm)',title='Native tracking error');axs[1].legend()
for ax in axs:ax.grid(alpha=.12)
fig.savefig(demo/'native_trajectory.png',dpi=160);plt.close(fig)

names={'nominal':'平静工况','constant_wind':'持续扰动','motor_variation':'电机差异及质量误差','gusts':'突变阵风'}
table=[]
for k,label in names.items():
    row=test['cases'][k];a=row['baseline_pd'];b=row['trained_ppo_residual']
    table.append(f"| {label} | {a['rms_position_error_m']*100:.2f} cm | {b['rms_position_error_m']*100:.2f} cm | {'变差' if row['rms_improvement_percent']<0 else '降低 '+format(row['rms_improvement_percent'],'.1f')+'%'} | {b['survived_fraction']*192:.0f}/192 |")
native_rms=course['rms_error_m']*100;peak=course['max_error_m']*100
hover_base=read(demo/'baseline-hover-fixed/native_result.json')['rms_error_m']*100
hover_rl=read(demo/'learned-hover-fixed/native_result.json')['rms_error_m']*100
sha=hashlib.sha256((demo/'policy/policy.pt').read_bytes()).hexdigest()
report=f'''# Open32Drone 强化学习流程演示：验收结果

日期：2026-09-04。已完成粗模型、PPO 训练、独立评估、Isaac 原生受力飞行和录像。展示权重已冻结，随后才运行最终测试种子 2001、2002、2003。

## 直接预览

- [60 秒中文演示视频](Open32Drone_RL_Demo_60s.mp4)：流程说明、固定镜头对照、完整技巧飞行、全部测试结果。
- [34 秒原始 Isaac 飞行](learned-course/native_demo.mp4)：50 fps、原模拟速度，连续飞行无中途重设飞机位姿。
- [训练曲线](training_curves.png) / [原生轨迹和误差](native_trajectory.png)。
- [源码及复现步骤](source/README.zh-CN.md)、[独立评估 JSON](evaluation.json)、[净空与跨后端核验](gate_clearance_verification.json)。

## 原生物理结果

飞机采用用户的 81 g 模型外观，在 Isaac Sim 6.0.1 / PhysX 中每 5 ms 积分；50 Hz 控制循环根据网络输出和基础几何 PD 计算四电机推力及力矩。不是用目标轨迹逐帧设置飞机位置。

- 连续 34 秒：八字穿环 → 螺旋爬升 → 停驻及阵风恢复。
- 10/10 个环通过。环有真实静态碰撞体，并在记录轨迹上以 **100 mm 半径包络球**再次检查净空。最小全程包络净空约 **{minimum*1000:.1f} mm**；这是本条轨迹的几何检查，不代表真实飞行安全余量。
- 位置 RMS 误差 **{native_rms:.2f} cm**（排除前 2 秒）；全程峰值 **{peak:.2f} cm**。
- 最高速度 **{course['max_speed_m_s']:.2f} m/s**；最低高度 **{course['min_height_m']:.2f} m**。
- 相同策略、目标与扰动下，PyTorch 积分和 PhysX 轨迹之间的 RMS 差异 **{verification['cross_backend_position_rms_m']*1000:.3f} mm**，最大 **{verification['cross_backend_position_max_m']*1000:.3f} mm**。这验证了本次两套实现的一致性，不等于动力学已匹配真机。
- 固定镜头悬停对照，基础 PD / PPO 的 RMS 为 **{hover_base:.2f} / {hover_rl:.2f} cm**；两者使用相同初态、电压与扰动。

初次录像附带的净空分析使用 95 mm 球。最终增加桨叶扫掠余量，使用 100 mm 球在原轨迹上重算，10 环仍全部通过。未修改飞行轨迹或原始录像 JSON。当前静态外观顶点最大半径 {visual_radius*1000:.2f} mm；包含 60 mm 桨扫掠及桨厚余量的估计半径 {swept_radius*1000:.2f} mm。

## 最终独立测试

每类 3 个新种子 × 64 初态 × 12 秒，共 768 回合；基础 PD 和 PPO 各运行一遍，总计 1536 回合。两种控制全部完成。位置 RMS 排除前 2 秒，再按种子汇总。初始偏航范围约 ±34.4°，不是任意翻转初态。扩大误差工况不代表全参数范围均已覆盖。

| 工况 | 基础几何 PD | PPO 残差 + PD | RMS 变化 | PPO 完成 |
| --- | --- | --- | --- | --- |
{chr(10).join(table)}

平静工况下基础 PD 更准确；该策略的主要收益是补偿扰动与模型误差。对照控制器没有位置积分或在线模型辨识，不能把成绩解读成优于所有 PID、自适应或模型预测控制方法。测试条件的全部数值保存在 `evaluation.json`。

## 学习内容与证据边界

这是 **残差强化学习案例**。PPO 网络输出三轴加速度修正，基础控制器稳定姿态，电机分配优先保留总升力。输入包括仿真真值和电机模型内部状态。轨迹是任务给定的；没有用摄像头识别环，也没有学习自主规划、真实光流/ToF 定位或直接输出固件 PWM。

用户确认：有电压记录以后的日志可信，此前无电压日志可能不准。粗动力模型以含电压历史工作点和 81 g 稳态力平衡为依据；满命令推力、响应延迟、反扭矩、惯量与阻力仍有假设。7 分钟续航和 50,000 rpm 不能替代完整动力曲线。本案例不包含真实飞机试飞、固件修改或真机策略验收。

本次技巧为八字、穿环、螺旋和受扰恢复，未实现翻滚或倒飞。飞行起点已在空中，不把本片称为起飞到降落的任务。

## 权重与训练记录

- 初始阶段 v3 从随机权重开始，所选父检查点为第 1000 轮，65,536,000 步。
- 后续 v4 改进训练分布，加入突变扰动；actor/critic 分开裁剪梯度。完整训练 1200 轮，演示使用验证选出的第 300 轮检查点，增加 19,660,800 步。
- 所选策略的训练链累计 85,196,800 步。后面的训练不自动视为更好；检查点选择与最终未见种子测试分开进行。
- `policy/policy.pt` 的 SHA-256：`{sha}`。
- `policy/actor.pt` 是同一份策略的 TorchScript 推理导出，保留动作限幅；32 条批量输入的导出一致性检查通过。
- 初期失败与中断候选保留在项目 `artifacts/rl-demo/`。包括旧分配器在偏航大误差时损失升力、初期策略突变风退化、第一次真实撞环以及遮挡较多的摄影候选。没有把这些候选算作验收通过。

## 下一步需要补什么

优先补完整含电压飞行日志、对应固件版本和几档电量下的稳态片段；有条件再补推力和响应测量。随后加入 IMU、光流/ToF 的噪声、延迟、丢帧与估计器，再接飞控软件/硬件在环。当前流程已经可运行，后续资料用于提升模型可信度。

## 文件归属

Mac 当前交付目录为 Open32Drone 源码仓库的 `output/rl-demo/20260904-showcase-v1/`；源码在 `simulation/rl_demo/`。108 上对应工作集位于项目的 `source/rl_demo/` 和 `output/rl-demo/20260904-showcase-v1/`。历史候选和日志归入项目 `artifacts/rl-demo/`。未提交、推送、发布或操作真实飞行硬件。文件身份见 `BUILD_INFO.json` 与 `SHA256SUMS`。
'''
(demo/'RESULTS.zh-CN.md').write_text(report)
build={'date':'2026-09-04','status':'simulation_demo_validated','source_repository_head':'4608b72dad4f6c8689d78ed14c4f28435e5dce46',
 'source_state':'local uncommitted simulation source; not a firmware release','isaac_sim':'6.0.1','torch':'2.10.0+cu128','gpu':'NVIDIA RTX 4080 SUPER',
 'policy_sha256':sha,'selected_lineage':[{'run':'v3','checkpoint_iteration':1000,'steps':65536000},{'run':'v4','checkpoint_iteration':300,'steps':19660800}],
 'test_seeds':test['evaluation_seeds'],'native_result':course,'derived_verification':verification,
 'source_sha256':{f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in (demo/'source').iterdir() if f.is_file()}}
(demo/'BUILD_INFO.json').write_text(json.dumps(build,indent=2)+'\n')
print(json.dumps({'status':'passed','native_rms_cm':native_rms,'clearance_mm':minimum*1000,'cross_backend_rms_mm':verification['cross_backend_position_rms_m']*1000,'report':str(demo/'RESULTS.zh-CN.md')}))
