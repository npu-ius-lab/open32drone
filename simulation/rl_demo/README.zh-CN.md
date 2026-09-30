# 残差 PPO 轨迹示例

[English](README.md) · [简体中文](README.zh-CN.md)

PPO 输出三轴加速度残差，基础几何 PD 负责姿态及电机分配。
训练与评估使用数值模型，不连接真实飞机，不做视觉识别或自主规划。

`model.json` 定义 81 g/60 mm 的教学近似值；推力、电压关系、阻力和
40 ms 电机响应均不能作为厂家实测性能。观测为仿真真值，没有完整传感器噪声链路。

| 文件 | 用途 |
|---|---|
| `dynamics.py`、`model.json` | 数值模型、控制器和网络 |
| `train.py`、`evaluate.py` | 训练与留出工况 PD/PPO 对照 |
| `physics_checks.py` | 重力、力矩符号、四元数等基础检查 |
| `showcase.py`、`preflight.py` | 给定参考轨迹与数值检查 |
| `native_isaac.py` | 可选 PhysX 受力适配器，需要独立 USD 资产 |

CPU 入门先使用[悬停练习](../course/README.zh-CN.md)。
轨迹训练默认使用 CUDA；配置好 NumPy/PyTorch 后，从仓库根运行：

```bash
python3 simulation/rl_demo/physics_checks.py --output output/trajectory/physics.json
python3 simulation/rl_demo/train.py --output output/trajectory --iterations 1200 --envs 1024
python3 simulation/rl_demo/evaluate.py --run output/trajectory
python3 simulation/rl_demo/preflight.py --run output/trajectory
```

每次使用独立输出目录，比较完整的成功率和误差，不只比较成功片段。
最终权重、原始运行日志不在仓库内。重新训练生成自己的结果。

Isaac 需要另行准备 `USD/open32droe/robot.usd` 及引用资源，见[资产说明](../../docs/guide/07-rl.md)。
没有该资产时停在数值实验步骤，不使用指向某台开发机的路径。
示例没有 Gazebo 飞行接入或强化学习真机部署。
