# Open32Drone 课程实验

本目录保存与课程新增逐讲教材配套的计算机练习，不连接真实飞机；`topic_lab.py` 只在 `/course/sample` 通信。真实飞机控制继续使用已有 `ros2/` 包。

| 脚本 | 课次 | 依赖 | 内容 |
| --- | --- | --- | --- |
| `response_lab.py` | 12 | Matplotlib | 一维 PD 响应、恒定扰动、CSV 与图表 |
| `analyze_log.py` | 13—14 | Python 标准库 | 当前字段的含电压 CSV 检查及统计 |
| `topic_lab.py` | 15 | ROS 2 / rclpy / std_msgs | 独立发布与订阅练习 |
| `hover_lab.py` | 24—27 | PyTorch、NumPy | 六自由度固定悬停残差 PPO、导出和留出对照 |

在教师准备的环境中，从仓库根运行：

```bash
python3 simulation/course/response_lab.py --output output/course-labs/response
python3 simulation/course/analyze_log.py --csv path/to/log.csv --output output/course-labs/log
python3 simulation/course/hover_lab.py --output output/course-labs/hover \
  --iterations 400 --envs 128 --device cpu
```

Python 依赖可在项目既有虚拟环境或共享教学环境中安装 `numpy torch matplotlib`，然后记录实际版本。不要在每次实验中复制工具链。ROS 2 使用对应发行版的系统环境，并按平台手册安装 MAVROS；`rclpy` 不包含在上面的数值依赖中。

悬停脚本复用兄弟目录 `rl_demo/dynamics.py` 与 `model.json`，保留两者的快照。模型参数仍是 81 g / 60 mm 粗模型，观测来自仿真真值和内部状态。策略并未运行到实物、Isaac 或 Gazebo。输出保存原始训练曲线、初始/最终权重、导出文件、每工况/种子指标和哈希。

`analyze_log.py` 输入要求：只保留 CSV 表头与数据区域，原串口文件另存；字段为 `t`、`voltage`、`motor.rl`、`motor.rr`、`motor.fr`、`motor.fl`，时间 s、电压 V、电机命令 0—1。缺字段、非有限数值、非法电压或非递增时间会被拒绝。统计不自动识别悬停，不推定推力。

教学数值例子与真实日志应分开存放，所有合成数据明确标记。完整教材与结果状态见[课程入口](../../docs/course/index.zh-CN.md)。
