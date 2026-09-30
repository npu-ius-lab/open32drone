# Residual PPO trajectory example

[English](README.md) · [简体中文](README.zh-CN.md)

PPO outputs three acceleration residuals. A geometric PD controller handles
attitude and motor allocation. Numerical training/evaluation does not connect to
an aircraft, perform visual recognition or learn autonomous path planning.

`model.json` contains teaching approximations for an 81 g / 60 mm vehicle.
Thrust, voltage scaling, drag and 40 ms motor response are not measured motor
specifications. Observations use simulator state without a complete sensor model.

| Files | Purpose |
|---|---|
| `dynamics.py`, `model.json` | Numerical dynamics, controller and network |
| `train.py`, `evaluate.py` | Training and held-out PD/PPO comparison |
| `physics_checks.py` | Gravity, torque signs, quaternions and basic checks |
| `showcase.py`, `preflight.py` | Given reference trajectory and numerical checks |
| `native_isaac.py` | Optional force-driven PhysX adapter; separate USD assets required |

Start on CPU with the [hover exercise](../course/README.md).
Trajectory training defaults to CUDA. With NumPy/PyTorch installed, run from the
repository root:

```bash
python3 software/simulation/rl_demo/physics_checks.py --output output/trajectory/physics.json
python3 software/simulation/rl_demo/train.py --output output/trajectory --iterations 1200 --envs 1024
python3 software/simulation/rl_demo/evaluate.py --run output/trajectory
python3 software/simulation/rl_demo/preflight.py --run output/trajectory
```

Use a new output directory per experiment. Compare failures as well as errors;
do not select only successful segments. Weights and raw run logs are not bundled.
Isaac requires `USD/open32droe/robot.usd` and its dependencies; see
[asset requirements](../../../docs/guide/07-rl.en.md). Without those assets, stop
at numerical evaluation. No Gazebo flight integration or real-aircraft RL
deployment is supplied.
