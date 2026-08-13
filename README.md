# Open32Drone

<p align="center">
    <img src="img/drone.PNG" alt="Full drone view" />
</p>

<p align="center">
  <strong>
    <a href="./README_zh_CN.md">简体中文</a> &nbsp;|&nbsp;
    <a href="./README.md">English</a>
  </strong>
</p>

**Open32Drone** is an open-source ESP32-S3 micro-drone platform designed for research, education, embedded flight-control development, and robotics algorithm validation.

This project is developed from the open-source [Flix](https://github.com/okalachev/flix/tree/master) project. It retains Flix's lightweight code architecture and adds an optical-flow sensor to enable indoor position-hold and altitude-hold flight. Open32Drone supports MAVLink and ROS integration, providing developers with a low-cost, highly extensible micro aerial vehicle platform for learning flight-control theory, validating swarm algorithms, and researching indoor navigation.

---

## Core Features

### Compute Core: ESP32-S3

The project uses an **ESP32-S3** series chip as the main controller:

- **Dual-core high clock speed**: 240 MHz processing capability for real-time attitude estimation and communication.

- **Expansion capability**: Supports the ESP-DL instruction set, providing computing resources for lightweight edge-side vision processing.

### Navigation and Perception: Optical Flow + ToF Sensor

A serial optical-flow/ToF module closes the low-altitude loop without GPS or an external positioning system:

- **Indoor position hold**: Estimates horizontal motion using height scaling, delayed angular-rate compensation, outlier rejection, and gated integration.

- **Indoor altitude hold**: Uses the ToF sensor to measure height above ground without being affected by floor color or ambient light.

### Communication Ecosystem: MAVLink & ROS

- **QGC support**: Native MAVLink v2 connects directly to **QGroundControl** for parameter access, telemetry, arming, and external-control testing.

- **ROS 2 / MAVROS integration**: The `ros2_open32drone` package exposes telemetry, IMU, manual control, and an MJPEG camera node over Wi-Fi UDP for Python/C++ host applications.

### Low Cost and Easy Reproduction

- **General modular design**: Core components are common off-the-shelf modules and are easy to source.

- **Open-source hardware**: The custom PCB project and modular assembly design are provided for direct fabrication and hardware modification.

- **Documentation support**: The tutorial covers carrier-board soldering, vehicle assembly, firmware setup, calibration, and staged flight testing.

---

## Development Plan

**Open32drone** aims to build a miniaturized air-ground collaborative robotics ecosystem. Future development plans include:

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

## Hardware Overview

To balance reproducibility and maintainability, the hardware uses a modular **baseboard + modules** architecture.

| Physical View | Core Component | Key Parameters | Key Features / Resources |
| :---: | :--- | :--- | :--- |
| <img src="img/pcb1.png" width="100"> <br/> <img src="img/pcb2.png" width="100"> | **Base PCB** | EasyEDA | Used only as a carrier board, with four onboard MOS motor drivers. Complete **[PCB project files](https://oshwhub.com/fanchewang/open32drone)** are provided. Solder four MOSFETs and several connectors to complete the baseboard. |
| <img src="img/esp32.png" width="100"> | **Main Controller Module** | SeeedStudio XIAO ESP32S3 Sense | Dual-core 240 MHz, PSRAM, and OV3660 camera; runs flight control, MAVLink, ROS 2 interfaces, and MJPEG streaming. |
| <img src="img/frame.png" width="100"> | **Frame Structure** | 75/85 mm | A commercial ducted frame is recommended. The repository also provides an **STL 3D-printable model**. |
| <img src="img/motor.png" width="100"> | **Propulsion Motor** | 8520 brushed coreless motor (8.5 mm x 20 mm) | **1.0 mm** shaft diameter, with more payload margin than 720 motors. |
| <img src="img/paddle.png" width="100"> | **Propeller** | 76 mm | High lift efficiency, essential for accurate optical-flow position-hold hovering. |

---

## Firmware Structure

The firmware uses a compact modular organization for micro aerial vehicles and selected lightweight embedded components from [**Flix**](https://github.com/okalachev/flix).

| File | Responsibility |
|---|---|
| `open32drone_v3.ino` | Main entry point, flight/network/camera initialization, and unified real-time loop |
| `control.ino` | Flight modes, attitude loop, rate loop, altitude loop, position-hold loop |
| `estimate.ino` | Attitude, altitude, optical-flow horizontal velocity, and position estimation |
| `flow.ino` | Serial optical-flow ToF parsing and health checks |
| `imu.ino` | MPU9250 reading, coordinate rotation, gyro bias learning, accelerometer calibration |
| `rc.ino` | SBUS remote-control input and channel normalization |
| `motors.ino` | Four-channel LEDC PWM motor output |
| `wifi.ino` | AP/STA Wi-Fi, UDP 14550, and QVGA MJPEG streaming |
| `mavlink.ino` | MAVLink TX/RX, parameters, modes, logs, and CLI passthrough |
| `cli.ino` | Serial command line and debug waveform output |
| `parameters.ino` | NVS parameter read/write |
| `log.ino` | RAM circular flight log |
| `safety.ino` | RC-loss descent, offboard timeout, and protection logic |

---

## Pin Definitions

| Peripheral | GPIO | Description |
|---|---:|---|
| I2C SDA | 2 | MPU9250 |
| I2C SCL | 43 | MPU9250, 400 kHz |
| Optical-flow RX | 8 | ESP32 `Serial1` RX, connected to optical-flow TX |
| Optical-flow TX | 7 | ESP32 `Serial1` TX, connected to optical-flow RX |
| SBUS RX | 44 | ESP32 `Serial2` RX |
| SBUS TX | 9 | ESP32 `Serial2` TX |
| LED | 21 | Onboard NEOPIXEL |
| MOTOR 0 | 4 | Rear left |
| MOTOR 1 | 3 | Rear right |
| MOTOR 2 | 6 | Front right |
| MOTOR 3 | 5 | Front left |

---

## Quick Start

1. Install Arduino IDE 2.x.
2. Install ESP32 Arduino Core 3.3.6.
3. Open `open32drone_v3/open32drone_v3.ino`.
4. Select `XIAO_ESP32S3`, enable OPI PSRAM, and use the 8 MB application partition.
5. Make sure Arduino IDE can find the `FlixPeriph`, `SBUS`, `MAVLink`, and `Adafruit BMP280` dependencies.
6. Compile and flash.
7. Open the serial monitor at 115200 bps and enter `help` to view commands.
8. Connect to Wi-Fi `open32drone` with password `12345678`. The video stream is at `http://192.168.4.1/stream`, and MAVLink uses UDP port `14550`.

For the complete full-stack tutorial, see:

[Open32Drone: From Zero to Stable Flight](./tutorial.md)

---

## FAQ

**Thrust-to-weight ratio:** 8520 motors with 76 mm propellers can provide about 40 g to 50 g thrust per motor at 3.7 V. The total takeoff weight should be kept within 60 g to 80 g.

**Motor shaft diameter:** Make sure the 8520 motor shaft diameter is **1.0 mm**, otherwise the 76 mm propellers cannot be installed.

**Module pin definitions:** MPU9250 modules sold online may have slightly different pin orders. Check the VCC/GND/SCL/SDA order before soldering.

**Soldering order:** Solder the SMD parts such as MOSFETs and resistors on the baseboard first, then solder the female headers. This prevents the headers from blocking the soldering area.

**Spare parts:** Coreless motors are consumables. It is recommended to buy one or two additional motors during the first purchase.

**Build environment:** Arduino IDE 2.x or PlatformIO is recommended. Before the first build, install ESP32-S3 board support. See the environment setup section in the tutorial.

**Optical-flow module:** The firmware uses a 115200 bps serial optical-flow/ToF module and parses 19-byte packets beginning with 0xDF. When sourcing or replacing the module, verify the communication protocol rather than relying only on the optical-flow chip model.

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
