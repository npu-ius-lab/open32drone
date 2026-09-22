# 仿真与教学练习

[English](README.md) · [简体中文](README.zh-CN.md)

这里的脚本不连接或解锁真实飞机，实机控制使用 `software/ros2/`。

| 内容 | 此仓库可提供 |
|---|---|
| 一维 PD、CSV 日志分析、ROS 示例话题 | [课程练习](course/README.zh-CN.md) |
| CPU 悬停残差 PPO | `course/hover_lab.py`，NumPy/PyTorch/Matplotlib |
| 轨迹 PPO、评估、物理检查 | [RL 源码](rl_demo/README.zh-CN.md)；训练默认需要 CUDA |
| Isaac 原生适配器 | `rl_demo/native_isaac.py`；需要另外准备兼容 USD 场景 |
| 完整 URDF/USD、Gazebo 飞行集成、预训练权重 | 未提供；教程视频不是可下载的模型或策略 |

模型参数是教学用粗估计，不等于电机实测曲线。输出是仿真状态，
没有完整模拟真实 IMU/光流/ToF 的噪声和时延。策略不能直接接入真机。

运行方法见[图文进阶教程](../../docs/guide/07-rl.md)，
模型准备要求见[仿真参考](../../docs/guide/07-rl.md)。
