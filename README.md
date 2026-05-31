# Open32Drone

<p align="center">
    <img src="img\drone.PNG" alt="Full drone view" />
</p>

<p align="center">
  <strong>
    <a href="./README_zh_CN.md">简体中文</a> &nbsp;|&nbsp;
    <a href="./README.md">English</a>
  </strong>
</p>

**Open32drone** is an open-source ESP32-S3-based micro drone platform designed for research, education, and algorithm validation.

This project is developed from the open-source [Flix](https://github.com/okalachev/flix/tree/master) project. It keeps Flix's lightweight code architecture and adds an optical-flow sensor for indoor altitude hold and position hold flight. Open32drone supports MAVLink and ROS integration, aiming to provide developers with a low-cost and highly extensible micro aerial vehicle test platform for learning drone control theory, validating swarm algorithms, and researching indoor navigation.

---

## Core Features

### Compute Core: ESP32-S3

The project uses an **ESP32-S3** series chip as the main controller:

- **Dual-core high clock speed**: 240 MHz processing capability for real-time attitude estimation and communication.

- **Expansion capability**: Supports the ESP-DL instruction set, providing computing resources for lightweight edge-side vision processing.

### Navigation and Perception: Optical Flow + ToF Sensor

An optical-flow ToF sensor module is integrated to enable stable hovering without GPS:

- **Indoor position hold**: Uses optical-flow data to monitor horizontal displacement and hold horizontal position.

- **Indoor altitude hold**: Uses the ToF sensor to measure height above ground, with reduced dependence on floor color or ambient light.

### Communication Ecosystem: MAVLink & ROS

- **QGC support**: Native MAVLink support allows direct connection to **QGroundControl** for visual parameter tuning, mission planning, and real-time telemetry monitoring.

- **MAVROS integration**: Supports MAVROS connections over Wi-Fi UDP. Users can treat the drone as a ROS node and write host-side Python/C++ programs for high-level control, SLAM mapping, or multi-drone collaboration experiments.

### Low Cost and Easy Reproduction

- **General modular design**: Core components are common off-the-shelf modules and are easy to source.

- **Open-source hardware**: Complete PCB project files are provided and can be ordered directly through the JLCPCB ecosystem.

- **Documentation support**: Detailed assembly guides, environment setup instructions, and precompiled firmware are provided to lower the entry barrier.

---

## Development Plan

**Open32drone** aims to build a miniaturized air-ground collaborative robotics ecosystem. Future development plans include:

### Edge Perception and Visual Intelligence

The plan is to integrate a lightweight video transmission module and extend visual perception capability:

- **On-device recognition**: QR-code navigation, color-block tracking, face following, and simple gesture control.

- **Vision-assisted navigation**: Combine video feature-point extraction with optical-flow position hold to improve robustness and potentially implement basic visual odometry (VO).

### Swarm Control and Collaborative Evolution

Using the wireless communication capability of the ESP32, the project will expand from single-drone control to multi-drone collaboration:

- **Distributed communication and collaboration**: Build a decentralized swarm network for position sharing and state synchronization between drones.

- **Low-cost swarm algorithm validation**: Lower the hardware barrier for swarm research and support 3-10 micro drones for laboratory-scale experiments such as collaborative search and formation flight.

### Fully Autonomous Indoor Navigation

Close the loop among perception, planning, and control at a very small scale:

- **Micro SLAM**: Explore miniaturized SLAM solutions based on multi-sensor fusion, such as ToF + optical flow + vision.

- **Dynamic obstacle avoidance**: Use multi-directional laser ranging sensors for omnidirectional obstacle avoidance and autonomous path planning in complex indoor environments.

---

## Roadmap

### Phase 1: Basic Stable Flight (2026-02 to 2026-04)

1. Optimize the attitude control algorithm based on ESP32-S3.

2. Integrate an optical-flow ToF all-in-one sensor for accurate indoor position hold and altitude hold.

3. Support the MAVLink protocol and the QGC ground station.

4. Support SBUS remote controllers.

5. Support MAVROS control of the drone.

### Phase 2: Vision and Interaction (2026 Q2-Q3)

1. **Video transmission integration**: Adapt ESP32-S3 video transmission or an external video module for low-latency video.

2. **Vision tasks**: Implement basic object recognition and target tracking based on ESP-WHO.

3. **Simulator**: Develop a simulator so code can be tested in simulation before real flights.

## Phase 3: Swarm and Ecosystem (2027+)

1. **Swarm**: Release swarm solutions and packages supporting collaborative flight with 3-10 drones.

---

## Hardware Overview

To balance reproducibility and maintainability, the hardware uses a modular **baseboard + modules** architecture.

| Physical View | Core Component | Key Parameters | Key Features / Resources |
| :---: | :--- | :--- | :--- |
| <img src="img/pcb1.png" width="100"> <br/> <img src="img/pcb2.png" width="100"> | **Base PCB** | EasyEDA | Used only as a carrier board, with four onboard MOS motor drivers. Complete **[PCB project files](https://oshwhub.com/fanchewang/open32drone)** are provided. Solder four MOSFETs and several connectors to complete the baseboard. |
| <img src="img/esp32.png" width="100"> | **Main Controller Module** | SeeedStudio XIAO ESP32S3 Sense | Dual-core 240 MHz, supports ESP-DL and wireless MAVROS connection. |
| <img src="img/frame.png" width="100"> | **Frame Structure** | 75/85 mm | A commercial ducted frame is recommended. The repository also provides an **STL 3D-printable model**. |
| <img src="img/motor.png" width="100"> | **Propulsion Motor** | 8520 brushed coreless motor (8.5 mm x 20 mm) | **1.0 mm** shaft diameter, with more payload margin than 720 motors. |
| <img src="img/paddle.png" width="100"> | **Propeller** | 76 mm | High lift efficiency, essential for accurate optical-flow position-hold hovering. |

---

## Firmware Structure

This project is developed from the open-source [**Flix**](https://github.com/okalachev/flix) flight-control framework.

| File | Responsibility |
|---|---|
| `proj_op32drone.ino` | Main entry point, module initialization, multi-rate scheduling |
| `control.ino` | Flight modes, attitude loop, rate loop, altitude loop, position-hold loop |
| `estimate.ino` | Attitude, altitude, optical-flow horizontal velocity, and position estimation |
| `flow.ino` | Serial optical-flow ToF parsing and health checks |
| `imu.ino` | MPU9250 reading, coordinate rotation, gyro bias learning, accelerometer calibration |
| `rc.ino` | SBUS remote-control input and channel normalization |
| `motors.ino` | Four-channel LEDC PWM motor output |
| `wifi.ino` | STA/AP Wi-Fi and UDP 14550 |
| `mavlink.ino` | MAVLink TX/RX, parameters, modes, logs, and CLI passthrough |
| `cli.ino` | Serial command line and debug waveform output |
| `parameters.ino` | NVS parameter read/write |
| `log.ino` | RAM circular flight log |

---

## Pin Definitions

| Peripheral | GPIO | Description |
|---|---:|---|
| I2C SDA | 2 | MPU9250 |
| I2C SCL | 43 | MPU9250, 400 kHz |
| Optical-flow RX | 8 | ESP32 `Serial1` RX, connected to optical-flow TX |
| Optical-flow TX | 7 | ESP32 `Serial1` TX, connected to optical-flow RX |
| SBUS RX | 44 | ESP32 `Serial2` RX |
| SBUS TX | 1 | ESP32 `Serial2` TX |
| LED | 21 | Onboard NEOPIXEL |
| MOTOR 0 | 4 | Rear left |
| MOTOR 1 | 3 | Rear right |
| MOTOR 2 | 6 | Front right |
| MOTOR 3 | 5 | Front left |

---

## Quick Start

1. Install Arduino IDE 2.x.
2. Install ESP32 Arduino Core 3.0.5 or a similar 3.x version.
3. Open `proj_op32drone/proj_op32drone.ino`.
4. Select the board type corresponding to ESP32-S3/XIAO ESP32-S3.
5. Make sure `libraries/FlixPeriph` and `libraries/MAVLink` under the project root can be recognized by Arduino IDE.
6. Compile and flash.
7. Open the serial monitor at 115200 bps and enter `help` to view commands.

For the complete full-stack tutorial, see:

[Open32drone: Full-Stack Embodied Intelligence Tutorial for Micro Drones](./tutorial.md)

---

## FAQ

**Thrust-to-weight ratio:** 8520 motors with 76 mm propellers can provide about 40 g to 50 g thrust per motor at 3.7 V. The total takeoff weight should be kept within 60 g to 80 g.

**Motor shaft diameter:** Make sure the 8520 motor shaft diameter is **1.0 mm**, otherwise the 76 mm propellers cannot be installed.

**Module pin definitions:** MPU9250 modules sold online may have slightly different pin orders. Check the VCC/GND/SCL/SDA order before soldering.

**Soldering order:** Solder the SMD parts such as MOSFETs and resistors on the baseboard first, then solder the female headers. This prevents the headers from blocking the soldering area.

**Spare parts:** Coreless motors are consumables. It is recommended to buy one or two additional motors during the first purchase.

**Build environment:** Arduino IDE 2.x or PlatformIO is recommended. Before the first build, install ESP32-S3 board support. See the environment setup section in the tutorial.

**Optical-flow module:** The current firmware supports serial optical-flow modules, such as PMW3901-series modules. Check the communication protocol and pin definitions carefully. Some online modules use SPI by default and require manual jumper changes to switch to serial mode.

**First-flight check:** Before takeoff, confirm in QGC that sensor data is being output normally, including IMU, optical flow, and ToF. In STAB mode, test motor direction and propeller installation direction at low throttle.

---

## Contributing

Open32drone is an open-source project, and community developers are welcome to help maintain and improve it:

* **Code contributions**: Fix bugs or submit new feature modules.
* **Documentation maintenance**: Help translate documentation or write more detailed tutorials.
* **Application demos**: Showcase research projects or creative works built with Open32drone.

---

## Authors and Acknowledgements

### Core Contributors

* **Unmanned System Research Institute, Northwestern Polytechnical University**
* **Xi'an Sandbox Technology Co., Ltd. OSRBOT**

<table>
  <tr>
    <td align="center">
      <img src="img\institute.png" width="400px" />
    </td>
    <td align="center">
      <img src="img\osrbot.png" width="400px" />
      <br />
    </td>
  </tr>
</table>

### Acknowledgements

Special thanks to the following excellent open-source project for providing inspiration and a foundation:

* [**Flix**](https://github.com/okalachev/flix) by Oleg Kalachev

---

## License

This project is licensed under the **[Apache License 2.0](https://www.apache.org/licenses/LICENSE-2.0)**.

[![License](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)
