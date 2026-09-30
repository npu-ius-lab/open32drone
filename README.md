# Open32Drone Minimal

<p align="center">
    <img src="img/drone.PNG" alt="Full drone view" />
</p>

<p align="center">
  <strong>
    <a href="./README_zh_CN.md">简体中文</a> &nbsp;|&nbsp;
    <a href="./README.md">English</a>
  </strong>
</p>

**Open32Drone** is an open-source micro-UAV platform based on the **ESP32-S3**, designed for research and education, embedded flight-control development, and robotics algorithm validation.

Open32Drone builds on the open-source [Flix](https://github.com/okalachev/flix/tree/master) project, retaining its lightweight code architecture while adding optical-flow sensing for indoor position and altitude hold. It supports MAVLink and ROS integration and provides a low-cost, extensible micro-aircraft platform for learning UAV control theory, validating swarm algorithms, and researching indoor navigation.

---

## Core Features

### Compute Core: ESP32-S3

The project uses an **ESP32-S3** series chip as the main controller:

- **Dual-core high clock speed**: 240 MHz processing capability for real-time attitude estimation and communication.

- **Expansion capability**: Supports the ESP-DL instruction set, providing computing resources for lightweight edge-side vision processing.

### Navigation and Perception: Optical Flow + ToF Sensor

A TF-0850 serial optical-flow/ToF module closes a low-altitude relative loop without GPS or an external positioning system:

- **Indoor position hold**: Estimates horizontal motion using height scaling, delayed angular-rate compensation, outlier rejection, and gated integration.

- **Indoor altitude hold**: Uses ToF relative height and filtered vertical speed to form an altitude-control loop.

### Communication Ecosystem: MAVLink & ROS & Video

- **QGC support**: Native MAVLink v2 support enables connection to **QGroundControl** for parameter access, state monitoring, arming, and external-control testing.

- **ROS 2 / MAVROS integration**: Provides IMU/odometry/ToF/battery interfaces, `/cmd_vel`, `/goal_pose`, raw RC, TF, RViz2, lifecycle commands, and propeller-off and supervised-flight tests. Multi-aircraft operation is isolated by aircraft IP, MAVLink System ID, local UDP port, ROS namespace, and TF prefix.

- **Bounded background service**: The optional MJPEG stream runs in a separate low-priority core-0 HTTP task, allows one viewer, and does not enter the 300 Hz flight loop. It remains an experimental feature.

### Low Cost and Easy Reproduction

- **General modular design**: Core components are common off-the-shelf modules and are easy to source.

- **Open-source hardware**: The custom PCB project and modular assembly design are provided for direct fabrication and hardware modification.

- **Documentation support**: The tutorial covers carrier-board soldering, vehicle assembly, firmware setup, calibration, and staged flight testing.

---

## Development Plan

**Open32Drone** aims to build a miniaturized air-ground collaborative robotics ecosystem. Future development plans include:

### Edge Perception and Visual Intelligence

The onboard OV3660 and current QVGA MJPEG stream provide the basis for continued visual-perception development:

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

## Documentation

| Documentation | English | 简体中文 |
| --- | --- | --- |
| Project overview | [README](README.md) | [项目说明](README_zh_CN.md) |
| Build, operation, and development | [Full Tutorial](tutorial.md) | [完整教程](tutorial_zh_CN.md) |

---

## FAQ

**Thrust-to-weight ratio:** 8520 motors with 76 mm propellers can provide about 40 g to 50 g thrust per motor at 3.7 V. The total takeoff weight should be kept within 60 g to 80 g.

**Motor shaft diameter:** Make sure the 8520 motor shaft diameter is **1.0 mm**, otherwise the 76 mm propellers cannot be installed.

**Module pin definitions:** MPU9250 modules sold online may have slightly different pin orders. Check the VCC/GND/SCL/SDA order before soldering.

**Soldering order:** Solder the SMD parts such as MOSFETs and resistors on the baseboard first, then solder the female headers. This prevents the headers from blocking the soldering area.

**Spare parts:** Coreless motors are consumables. It is recommended to buy one or two additional motors during the first purchase.

**Build environment:** Arduino IDE 2.x or PlatformIO is recommended. Before the first build, install ESP32-S3 board support. See the environment setup section in the tutorial.

**Optical-flow module:** The firmware uses a 115200 bps serial optical-flow/ToF module and parses 19-byte packets beginning with 0xDF. Verify the communication protocol when replacing it; floor texture, reflectivity, illumination, and height all affect optical-flow/ToF availability.

**First-flight check:** With the propellers removed, use the serial CLI commands `imu`, `flow`, and `rc` to verify inertial, optical-flow/ToF, and receiver input, then check motor order and direction at low output. Install the propellers only after these ground checks and conduct the first supervised STAB flight in a clear area.

---

## Contributing

Open32Drone is an open-source project, and community developers are welcome to help maintain and improve it:

* **Code contributions**: Fix bugs or submit new feature modules.
* **Documentation maintenance**: Help translate documentation or write more detailed tutorials.
* **Application demos**: Showcase research projects or creative works built with Open32Drone.

---

## Authors and Acknowledgements

### Core Contributors

* **Unmanned System Research Institute, Northwestern Polytechnical University**
* **Xi'an Sandbox Technology Co., Ltd. OSRBOT**

<table>
  <tr>
    <td align="center">
      <img src="img/institute.png" width="400px" />
    </td>
    <td align="center">
      <img src="img/osrbot.png" width="400px" />
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
