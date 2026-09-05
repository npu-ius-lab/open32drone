"""Make a 60-second Chinese walkthrough from verified, real simulation footage."""
import argparse
import json
import math
import subprocess
import wave
from pathlib import Path
import numpy as np
from PIL import Image,ImageDraw,ImageFont
from showcase import path,reference,GATE_TIMES

p=argparse.ArgumentParser();p.add_argument('--demo',type=Path,required=True)
p.add_argument('--training',type=Path,required=True);args=p.parse_args()
W,H,FPS=1280,720,50
BG=(8,15,29);INK=(236,244,251);MUTED=(151,174,197);CYAN=(67,217,236);GOLD=(253,188,79)
fontpath='/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc'
boldpath='/usr/share/fonts/opentype/noto/NotoSansCJK-Bold.ttc'
def font(size,bold=False):return ImageFont.truetype(boldpath if bold else fontpath,size,index=2)
F={size:font(size) for size in (18,20,22,24,28,32,38,48,64)};B={size:font(size,True) for size in (24,28,32,38,48,64)}
demo=args.demo
course=json.loads((demo/'learned-course/native_result.json').read_text())
baseline=json.loads((demo/'baseline-hover-fixed/native_result.json').read_text())
hover=json.loads((demo/'learned-hover-fixed/native_result.json').read_text())
test=json.loads((args.training/'evaluation-policy_0300-test.json').read_text())
assert course['status']=='passed' and course['course_status']=='passed'
assert baseline['status']=='passed' and hover['status']=='passed'
trace=np.load(demo/'learned-course/native_trace.npy')
tracebase=np.load(demo/'baseline-hover-fixed/native_trace.npy')
tracehover=np.load(demo/'learned-hover-fixed/native_trace.npy')
preview=Image.open(demo/'learned-course/preview_1000.png').convert('RGB')
def background():
    im=Image.new('RGB',(W,H),BG);d=ImageDraw.Draw(im)
    for x in range(0,W,80):d.line((x,0,x,H),fill=(14,25,42))
    for y in range(0,H,80):d.line((0,y,W,y),fill=(14,25,42))
    d.rectangle((0,0,8,H),fill=CYAN)
    return im
def text(d,xy,s,size=24,color=INK,bold=False):d.text(xy,s,font=B[size] if bold else F[size],fill=color)
def header(d,kicker,title):
    text(d,(38,24),kicker,20,CYAN);text(d,(38,64),title,38,bold=True)
def footer(d,s):text(d,(38,674),s,20,MUTED)
class Reader:
    def __init__(self,file):
        self.proc=subprocess.Popen(['/usr/bin/ffmpeg','-hide_banner','-loglevel','error','-i',str(file),'-f','rawvideo','-pix_fmt','rgb24','pipe:1'],stdout=subprocess.PIPE)
    def frame(self):
        needed=W*H*3;chunks=[];remaining=needed
        while remaining:
            b=self.proc.stdout.read(remaining)
            if not b:raise RuntimeError('unexpected end of verified source video')
            chunks.append(b);remaining-=len(b)
        return Image.frombytes('RGB',(W,H),b''.join(chunks))
    def close(self):
        self.proc.stdout.close()
        if self.proc.wait()!=0:raise RuntimeError('video decoder failed')

silent=demo/'Open32Drone_RL_Demo_silent.mp4'
enc=subprocess.Popen(['/usr/bin/ffmpeg','-y','-hide_banner','-loglevel','error','-f','rawvideo','-pix_fmt','rgb24','-s','1280x720','-r','50','-i','pipe:0','-an','-c:v','libx264','-threads','4','-preset','fast','-crf','18','-pix_fmt','yuv420p','-movflags','+faststart',str(silent)],stdin=subprocess.PIPE)
frames=0
def emit(im):
    global frames
    enc.stdin.write(im.tobytes());frames+=1
    if frames in (100,320,650,1150,1900,2800):im.save(demo/f'edited_preview_{frames:04d}.png')

# 0-3 seconds: hero still from the actual native rollout.
for j in range(150):
    im=preview.copy();overlay=Image.new('RGB',(W,H),BG);im=Image.blend(im,overlay,0.68)
    d=ImageDraw.Draw(im);text(d,(52,100),'OPEN32DRONE',28,CYAN,bold=True)
    text(d,(48,180),'81 克，学会稳稳穿环',48,bold=True)
    text(d,(52,270),'从粗动力模型到强化学习飞行',32)
    text(d,(52,345),'10 个环  /  八字航线  /  螺旋爬升  /  阵风恢复',24,GOLD)
    text(d,(52,604),'Isaac Sim 原生物理演示 · PPO 残差控制 · 50 fps',24,MUTED)
    emit(im)

# 3-10 seconds: workflow, with real training evidence rather than invented video.
for j in range(350):
    im=background();d=ImageDraw.Draw(im);header(d,'01 / BUILD → LEARN → VERIFY','这份飞行能力是怎么来的？')
    labels=[('01','粗模型','81 g / 60 mm','含电压日志提供初值'),('02','并行学习','1,024 个环境','PPO 优化加速度修正'),('03','独立评估','4 类未见工况','同条件对照基础 PD'),('04','原生复验','Isaac / PhysX','施加电机力与力矩')]
    active=min(3,j//80)
    for k,(num,title,big,small) in enumerate(labels):
        x=38+k*307;d.rounded_rectangle((x,172,x+283,408),radius=14,fill=(17,31,49),outline=CYAN if k<=active else (40,59,78),width=2)
        text(d,(x+20,190),num,24,CYAN);text(d,(x+20,238),title,32,bold=True)
        text(d,(x+20,298),big,24,GOLD);text(d,(x+20,352),small,18,MUTED)
    text(d,(44,456),'策略输出修正量，基础控制器稳定姿态，四电机模型负责产生推力。',28)
    text(d,(44,516),'选中权重的训练链：约 8,520 万步；随后冻结权重，用新种子测试。',24,MUTED)
    text(d,(44,568),'物理参数仍有粗估项；当前观测来自仿真状态。',24,GOLD)
    footer(d,'保留源码、检查点、训练 CSV、测试 JSON 和原始受力飞行轨迹。')
    emit(im)

# 10-18 seconds: fixed-camera, identical-condition native hover comparison.
r0=Reader(demo/'baseline-hover-fixed/native_demo.mp4');r1=Reader(demo/'learned-hover-fixed/native_demo.mp4')
for j in range(400):
    a=r0.frame().crop((160,105,1120,635)).resize((608,336),Image.Resampling.LANCZOS)
    b=r1.frame().crop((160,105,1120,635)).resize((608,336),Image.Resampling.LANCZOS)
    im=background();im.paste(a,(24,198));im.paste(b,(648,198));d=ImageDraw.Draw(im)
    header(d,'02 / SAME CONDITIONS','同一扰动，看看学习补偿带来的变化')
    text(d,(34,147),'基础几何 PD',28,MUTED,bold=True);text(d,(658,147),'PPO 残差 + 几何 PD',28,CYAN,bold=True)
    text(d,(34,549),f"当前位置偏差  {tracebase[j,17]*100:4.1f} cm",24)
    text(d,(658,549),f"当前位置偏差  {tracehover[j,17]*100:4.1f} cm",24)
    text(d,(34,596),f"后 6 秒 RMS  {baseline['rms_error_m']*100:.1f} cm",24,GOLD)
    text(d,(658,596),f"后 6 秒 RMS  {hover['rms_error_m']*100:.1f} cm",24,CYAN)
    footer(d,'相同初始状态、3.1 V 与持续扰动；固定镜头；RMS 排除前 2 秒。')
    emit(im)
r0.close();r1.close()

# 18-52 seconds: the complete 34-second flight, at original simulation speed.
r=Reader(demo/'learned-course/native_demo.mp4')
refpoints=np.array([path(t) for t in np.linspace(0,34,300)])
def mapxy(p):return (int(1125+p[0]*60),int(215-p[1]*60))
for j in range(1700):
    im=r.frame();d=ImageDraw.Draw(im);t=j/FPS
    d.rectangle((0,0,W,105),fill=BG);d.rectangle((0,635,W,H),fill=BG)
    if t<12:title='八字穿环';sub='沿连续曲线左右交叉，穿过前半程环阵'
    elif t<25:title='螺旋爬升';sub='环绕上升，保持航向与高度变化'
    else:title='精准停驻 · 阵风恢复';sub='29–31 秒施加额外扰动，随后回到目标点'
    text(d,(30,12),'03 / '+title,32,CYAN,bold=True);text(d,(30,60),sub,22)
    passed=sum(g['passed'] and g['crossing_s']<=t for g in course['gates'])
    text(d,(30,647),f"穿环 {passed:02d} / 10    位置偏差 {trace[j,17]*100:4.1f} cm    {t:05.2f} s",24)
    text(d,(30,684),'PPO 残差 + 几何 PD  ·  PhysX 受力仿真  ·  原速连续片段',20,MUTED)
    d.rectangle((0,631,int(W*(j+1)/1700),635),fill=CYAN)
    d.rounded_rectangle((1020,122,1260,308),radius=12,fill=(11,24,41),outline=(39,66,88))
    text(d,(1033,130),'航迹俯视图',18,MUTED)
    d.line([mapxy(p) for p in refpoints],fill=(54,78,100),width=2)
    if j>1:d.line([mapxy(p) for p in trace[:j+1:4,1:4]],fill=CYAN,width=2)
    x,y=mapxy(trace[j,1:4]);d.ellipse((x-5,y-5,x+5,y+5),fill=GOLD)
    emit(im)
r.close()

# 52-60 seconds: all four final held-out conditions, including the weaker case.
names={'nominal':'平静工况','constant_wind':'持续扰动','motor_variation':'电机差异','gusts':'突变阵风'}
for j in range(400):
    im=background();d=ImageDraw.Draw(im);header(d,'04 / MEASURED RESULTS','流程跑通，成绩也能复核')
    text(d,(40,140),'独立测试：每类 192 回合，每回合 12 秒；所列两种控制均完成全部回合。',22,MUTED)
    progress=min(1,(j+1)/60)
    for k,(case,label) in enumerate(names.items()):
        row=test['cases'][case];a=row['baseline_pd']['rms_position_error_m']*100;b=row['trained_ppo_residual']['rms_position_error_m']*100
        y=198+k*88;text(d,(42,y),label,24)
        d.rounded_rectangle((244,y+3,244+int(a*10*progress),y+23),radius=5,fill=(98,119,142))
        d.rounded_rectangle((244,y+33,244+int(b*10*progress),y+53),radius=5,fill=CYAN)
        text(d,(824,y),f'{a:.1f} → {b:.1f} cm',24)
        change=row['rms_improvement_percent']
        text(d,(1060,y),'仍需优化' if change<0 else f'降低 {change:.0f}%',22,GOLD if change<0 else CYAN)
    text(d,(42,567),f"原生赛道：10 / 10 环通过  ·  RMS {course['rms_error_m']*100:.1f} cm  ·  34 秒连续飞行",28,CYAN)
    text(d,(42,619),'灰：基础 PD   青：PPO 残差 + PD    |    下一步：补实测、接传感器闭环',22,MUTED)
    footer(d,'这是粗模型下的强化学习流程案例；尚未作为真机飞行策略验收。')
    emit(im)
enc.stdin.close()
if enc.wait()!=0:raise RuntimeError('video encoder failed')
assert frames==3000,frames

# Original quiet electronic backing: generated tones, no external music assets.
rate=48000;seconds=60;t=np.arange(rate*seconds)/rate;sound=np.zeros_like(t)
chords=[[130.813,155.563,195.998],[103.826,130.813,155.563],[155.563,195.998,233.082],[116.541,146.832,174.614]]
for k in range(15):
    mask=(t>=k*4)&(t<(k+1)*4);local=t[mask]-k*4;env=np.sin(np.pi*local/4)**2
    for note in chords[k%4]:sound[mask]+=0.022*env*np.sin(2*np.pi*note*local)
for beat in np.arange(0,60,0.5):
    ix=(t>=beat)&(t<beat+0.22);u=t[ix]-beat
    sound[ix]+=0.04*np.exp(-22*u)*np.sin(2*np.pi*(70*u-40*u*u))
sound*=np.minimum(t/1.5,1)*np.minimum((seconds-t)/2,1)
wav=demo/'original_ambient.wav'
with wave.open(str(wav),'wb') as f:f.setnchannels(1);f.setsampwidth(2);f.setframerate(rate);f.writeframes((sound*32767).astype('<i2').tobytes())
final=demo/'Open32Drone_RL_Demo_60s.mp4'
subprocess.run(['/usr/bin/ffmpeg','-y','-hide_banner','-loglevel','error','-i',str(silent),'-i',str(wav),'-c:v','copy','-c:a','aac','-b:a','128k','-shortest','-movflags','+faststart',str(final)],check=True)
(demo/'edit_manifest.json').write_text(json.dumps({'frames':frames,'fps':FPS,'duration_s':60,'audio':'original synthesized quiet backing',
 'segments':[{'start_s':0,'duration_s':3,'content':'actual rollout still and title'},
             {'start_s':3,'duration_s':7,'content':'workflow and training provenance'},
             {'start_s':10,'duration_s':8,'content':'same-condition native fixed-camera comparison'},
             {'start_s':18,'duration_s':34,'content':'entire native course at 1x simulation speed'},
             {'start_s':52,'duration_s':8,'content':'all held-out results, including nominal regression'}]},indent=2)+'\n')
print(json.dumps({'status':'completed','frames':frames,'video':str(final)}))
