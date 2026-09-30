# 离线教学练习

[English](README.md) · [简体中文](README.zh-CN.md)

这些练习不控制飞机；`topic_lab.py` 仅在 `/course/sample` 通信。
实机控制使用 `software/ros2/`。

| 脚本 | 依赖 | 用途 |
|---|---|---|
| `response_lab.py` | Matplotlib | 一维 PD 与扰动响应 |
| `analyze_log.py` | Python 标准库 | CSV 字段验证与统计 |
| `topic_lab.py` | ROS 2/rclpy/std_msgs | 示例话题发布订阅 |
| `hover_lab.py` | NumPy/PyTorch/Matplotlib | CPU 悬停残差 PPO |

从仓库根目录运行：

```bash
python3 -m venv .venv
source .venv/bin/activate
python3 -m pip install numpy torch matplotlib
python3 software/simulation/course/response_lab.py --output output/response
python3 software/simulation/course/hover_lab.py --output output/hover --iterations 400 --envs 128 --device cpu
python3 software/simulation/course/analyze_log.py --csv path/to/flight.csv --output output/analysis
```

分析输入只保留 CSV 表头和数据，包含 `t`、`voltage`、`motor.rl`、
`motor.rr`、`motor.fr`、`motor.fl`。时间为秒、电压为伏、电机命令 0—1。
统计不会自动识别悬停或推断推力。原始日志保留本地，不作为公开提交内容。

PPO 使用粗动力模型和仿真真值，不是可以直接飞行的策略。
详见[调参教程](../../../docs/guide/05-tuning.md)和[强化学习教程](../../../docs/guide/07-rl.md)。
