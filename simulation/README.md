# Simulation and learning

[English](README.md) · [简体中文](README.zh-CN.md)

These scripts do not connect to or arm an aircraft. Real flight control lives
in `ros2/`.

| Content | Available in this repository |
|---|---|
| PD response, CSV analysis and example ROS topic | [Exercises](course/README.md) |
| CPU stationary-hover residual PPO | `course/hover_lab.py`; NumPy, PyTorch, Matplotlib |
| Trajectory PPO, evaluation and physics checks | [RL workflow](rl_demo/README.md); CUDA for the default training command |
| Isaac force-driven adapter | `rl_demo/native_isaac.py`; separately prepared compatible USD assets |
| Complete URDF/USD, Gazebo flight integration, pretrained weights | Not bundled; tutorial videos are illustrative, not downloadable environments |

The model is a teaching approximation, not a measured motor curve. Its
observations use simulator state without a full noisy/delayed IMU/flow/ToF
pipeline. Policies are not ready for direct aircraft deployment.

See [the illustrated chapter](../docs/guide/07-rl.en.md) and
[asset requirements](../docs/guide/07-rl.en.md).
