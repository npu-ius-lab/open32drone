# Open32drone

<p align="center">
    <img src="img\drone.PNG" alt="Full Assembly" />
</p>

<p align="center">
    <strong>Fully Open Source · Hardware-Software Integration</strong><br/>
    <strong>Free for Personal & Educational Use · Commercial Use Requires Authorization</strong>
</p>

<p align="center">
  <strong>
    <a href="./README_zh_CN.md">简体中文</a> &nbsp;|&nbsp;
    <a href="./README.md">English</a> 
  </strong>
</p>

**Open32drone** is an open-source micro-drone platform based on the **ESP32-S3**, designed for scientific research, education, and algorithm validation. 

This project is a secondary development based on the open-source project [Flix](https://github.com/okalachev/flix/tree/master). It retains Flix's lightweight code architecture while introducing Optical Flow and ToF sensors to achieve stable hovering (position and altitude hold) in indoor environments. Supporting the MAVLink protocol and ROS integration, Open32drone aims to provide developers with a low-cost, highly extensible MAV platform for learning control theory, validating swarm algorithms, and researching indoor navigation.

---

## Core Features

### 1. Processing Core: ESP32-S3
The project utilizes the Espressif **ESP32-S3** series chip as the main controller:
* **Dual-core High Frequency**: 240MHz processing speed to easily handle attitude estimation and data communication.
* **Extensibility**: Supports the ESP-DL instruction set, providing computational power for lightweight edge-side vision processing.

### 2. Navigation & Perception
Integrated **Optical Flow + ToF** two-in-one sensor module for stable hovering without GPS:
* **Indoor Position Hold**: Monitors horizontal displacement via optical flow data for precise station-keeping.
* **Centimeter-level Altitude Hold**: Uses the ToF sensor to accurately measure height above ground, unaffected by ground color or environment lighting.

### 3. Communication Ecosystem
* **QGC Support**: Native MAVLink protocol support allows direct connection to QGroundControl for visual parameter tuning, mission planning, and real-time telemetry.
* **MAVROS Integration**: Establishes a MAVROS connection via Wi-Fi (UDP). Users can treat the drone as a ROS node, using Python/C++ to write host programs for high-level control, SLAM mapping, or multi-agent coordination experiments.

### 4. Low Cost & Easy Replication
* **Modular Design**: Core components are standard off-the-shelf modules, making procurement easy.
* **Open Hardware**: Provides complete PCB project files, compatible with direct ordering from services like JLCPCB.
* **Documentation**: Includes detailed assembly guides, environment setup tutorials, and pre-compiled firmware.

---

## Hardware Architecture & Specifications

The hardware follows a **Baseboard + Module** design to ensure both maintainability and ease of assembly.

| Preview | Component | Specs | Key Features / Resources |
| :---: | :--- | :--- | :--- |
| <img src="img/pcb1.png" width="100"> | **Mainboard** | EasyEDA | Only acts as a carrier with 4-way MOS drivers. **[Full PCB project files](https://oshwhub.com/fanchewang/open32drone)** provided. |
| <img src="img/esp32_1.png" width="100"> | **Controller** | ESP32-S3 | Dual-core 240MHz, supports **ESP-DL** and wireless MAVROS. |
| <img src="img/frame.png" width="100"> | **Structure** | 75/85mm | Standard brushed frame; **STL 3D model files** available in repo. |
| <img src="img/motor.png" width="100"> | **Motor** | 8520 Coreless | **1.0mm shaft**, offers superior thrust margin compared to 720 motors. |
| <img src="img/paddle.png" width="100"> | **Propeller** | 76mm (2/3-blade) | High lift efficiency—critical for precise Optical Flow station-keeping. |



### Technical Highlights:

* **Mainboard (PCB)**: 
    * **Design**: A simplified "carrier" board. You only need to solder 4 MOS transistors and pin headers.
    * **Open Source**: Includes full EasyEDA project files, ready for ordering from **JLCPCB** or similar services.
    * **Interfaces**: Dedicated slots for MPU9250 (9-axis IMU), voltage regulators, and **Serial Optical Flow + ToF modules**.
* **Propulsion Matching**: 
    * **Shaft Warning**: Ensure the motor shaft is **1.0mm** to fit the 76mm propellers.
    * **Thrust-to-Weight**: Provides ~40g-50g thrust per motor at 3.7V. Aim for a total takeoff weight of **60g-80g** for optimal control.
* **Soldering Tips**: 
    * Solder surface-mount components (MOS, resistors) before the headers to prevent obstructions.

---

## DIY Case Studies

<p align="center">
    <img src="img/diy_case.png" alt="diy"  />
</p>

---

## Development Plan

Open32drone is dedicated to building a miniaturized air-ground collaborative robotic ecosystem. Future development focuses on three directions:

### 1. Edge Perception & Visual Intelligence
Plans to integrate lightweight image transmission modules to expand visual capabilities:

* **On-device Recognition**: QR code navigation, color tracking, face following, and simple gesture control.
* **Vision-Aided Navigation**: Feature point extraction to enhance optical flow robustness and implement basic Visual Odometry.

### 2. Swarm Control & Collaborative Evolution
Using ESP32's wireless capabilities to expand from single-unit control to multi-agent systems:

* **Distributed Communication**: Building decentralized networks for position sharing and state synchronization.
* **Low-cost Swarm Validation**: Enabling labs to deploy 3–10 micro-drones at low cost to verify collaborative search and formation flight algorithms.

### 3. Fully Autonomous Indoor Navigation
Closing the loop of perception, planning, and control at a micro scale:

* **Micro SLAM**: Exploring fusion-based (ToF + Optical Flow + Vision) miniaturized SLAM solutions.
* **Dynamic Obstacle Avoidance**: Utilizing multi-directional laser ranging sensors for omnidirectional avoidance and path planning.
---

## Roadmap

### Phase 1: Stable Flight (Achieved)
- [x] Attitude control optimization for ESP32-S3.
- [x] Integrated Optical Flow + ToF for precise indoor hovering.
- [x] MAVLink & QGroundControl compatibility.
- [x] Sbus receiver support.
- [x] MAVROS support for external control.

### Phase 2: Vision & Interaction (In Progress)
- [ ] **Image Transmission**: Adapt ESP32-S3 camera or external low-cost modules for low-latency video.
- [ ] **Visual Tasks**: Implement object recognition and tracking via ESP-WHO.
- [ ] **Simulator**: Develop a simulation environment for SITL (Software-In-The-Loop) testing.

### Phase 3: Swarm & Ecosystem (Future)
- [ ] **Swarm**: Release a decentralized swarm solution supporting 3–10 units.

---

## FAQ

* **Thrust-to-Weight Ratio**: 8520 motors with 76mm props provide ~40g–50g of thrust per motor at 3.7V. Recommended takeoff weight: **60g–80g**.
* **Shaft Diameter**: Ensure the motor shaft is **1.0mm**, otherwise the 76mm props will not fit.
* **Module Pinouts**: MPU9250 pinouts vary by manufacturer; verify `VCC/GND/SCL/SDA` before soldering.
* **Soldering Tip**: Solder surface-mount components (MOS, resistors) first before the pin headers to avoid blocking access.

---

## Contributing

Open32drone is an open project. We welcome community contributions:

* **Code**: Bug fixes or new feature modules.
* **Docs**: Translation or detailed tutorials.
* **Showcase**: Share your research or creative projects using Open32drone.

---

## Authors & Acknowledgments

### Core Contributors
* **Unmanned System Research Institute, Northwestern Polytechnical University**
* **Xi'an Sand-Table Information Technology Co., Ltd. (OSRBOT)**

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

### Acknowledgments
Special thanks to the following open-source project for inspiration and foundation:
* [**Flix**](https://github.com/okalachev/flix) by Oleg Kalachev

---

## License

This project is licensed under the **[Apache License 2.0](https://www.apache.org/licenses/LICENSE-2.0)** International License.

[![License](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)