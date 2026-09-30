# 01 · Project overview

## What is Open32Drone?

Open32Drone is an open-source micro quadrotor for education, research and DIY projects. Print the frame, solder the electronics and assemble an aircraft, then control it with a phone, transmitter or ROS 2.

The project includes frame models, PCB design resources, flight firmware, an Android app, a ROS 2 package and illustrated tutorials for understanding the system, modifying the code and running experiments.

![Open32Drone aircraft with a printed frame, carrier board and four brushed motors](/media/photos/drone-complete.jpg)

## What can you do?

- **Fly indoors:** use the IMU, optical flow and ToF for attitude stabilization, altitude hold, position hold and automatic takeoff and landing.
- **Control with a phone or transmitter:** use the Android app, or add an SBUS receiver and matching transmitter.
- **Program with ROS 2:** read sensors and flight state, send velocity and position targets, and combine takeoff and landing commands into experiments.
- **Explore simulation and reinforcement learning:** use separate numerical control and PPO exercises to learn about models, controllers and policy training.

## System overview

The aircraft combines a XIAO ESP32-S3, IMU, optical-flow/ToF module, four 8520 brushed motors and a 1S battery. A 3D-printed frame supports the components; the carrier board connects the modules and provides power and motor drive.

Three control options connect to the same aircraft:

![System overview: SBUS, Android or ROS 2 connects to the flight controller; sensors provide measurements and the carrier board drives four motors](/media/figures/system-overview.en.svg)

Android and ROS 2 connect over Wi-Fi; use one of them at a time. Phone control does not require a transmitter.

## Start building

For a first build, follow the stages below. If you have already assembled and flown the aircraft, continue with tuning, ROS 2 or simulation.

| Stage | What you will do |
|---|---|
| [02 · Start building](03-hardware.en.md) | Buy parts, print the frame, make the PCB, solder and assemble |
| [03 · First flight](04-firmware-flight.en.md) | Download the firmware and app, flash, calibrate and fly |
| [04 · Tuning and diagnosis](05-tuning.en.md) | Check and adjust code and parameters based on flight symptoms |
| [05 · ROS 2 control](06-ros.en.md) | Connect, read telemetry and program aircraft motion |
| [06 · Simulation and reinforcement learning](07-rl.en.md) | Run numerical simulations and explore control and policy training |

For ready-to-use software, see [GitHub Releases](https://github.com/npu-ius-lab/open32drone/releases). To modify the code, see [Source and build](../reference/source-build.md).

## Join the project

Share your build, report an issue, improve the tutorials or contribute code.

[Source repository](https://github.com/npu-ius-lab/open32drone) · [Issues](https://github.com/npu-ius-lab/open32drone/issues) · [Contributing](../project/contributing.md)

Original project code and documentation use [Apache License 2.0](../../LICENSE) by default. Test methods and other components with separate licenses follow their respective terms; third-party code, hardware resources and media retain their original licenses. See [LICENSE.txt](../../LICENSE.txt) for the scope and [Third-party notices](../project/third-party.md) for sources and exceptions.
