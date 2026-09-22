# Offline exercises

[English](README.md) · [简体中文](README.zh-CN.md)

These exercises do not control an aircraft. `topic_lab.py` uses only
`/course/sample`; real flight uses the `software/ros2/` package.

| Script | Requirements | Purpose |
|---|---|---|
| `response_lab.py` | Matplotlib | One-dimensional PD response |
| `analyze_log.py` | Python standard library | CSV validation/statistics |
| `topic_lab.py` | ROS 2/rclpy/std_msgs | Example publisher/subscriber |
| `hover_lab.py` | NumPy/PyTorch/Matplotlib | CPU stationary-hover residual PPO |

From the repository root:

```bash
python3 -m venv .venv
source .venv/bin/activate
python3 -m pip install numpy torch matplotlib
python3 software/simulation/course/response_lab.py --output output/response
python3 software/simulation/course/hover_lab.py --output output/hover --iterations 400 --envs 128 --device cpu
python3 software/simulation/course/analyze_log.py --csv path/to/flight.csv --output output/analysis
```

The CSV requires `t`, `voltage`, `motor.rl`, `motor.rr`, `motor.fr`, `motor.fl`
in seconds, volts and 0–1 motor commands. Statistics do not automatically detect
hover or identify thrust. Keep raw personal flight captures out of commits.
PPO uses approximate dynamics and simulator state, not a deployable aircraft policy.
