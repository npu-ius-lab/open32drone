# Open32Drone simulation

This directory holds simulation and learning source, separate from onboard firmware.
`rl_demo/` is the first residual-PPO workflow demo. Current results belong in
`output/rl-demo/`; run logs and historical candidates belong in the project's
`artifacts/rl-demo/`. It does not connect to or arm real hardware.

Start with [the workflow and reproduction guide](rl_demo/README.zh-CN.md).
The [course exercises](course/README.zh-CN.md) add offline response and log
analysis, an isolated ROS topic example, and a CPU-capable stationary-hover PPO
exercise. They are separate from real-aircraft control and the trajectory demo.
The verified local showcase is in `output/rl-demo/20260904-showcase-v1/`, with
the 60-second annotated video, frozen policy, native traces and final report.
