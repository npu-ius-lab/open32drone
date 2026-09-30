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

**Open32Drone** is an open-source micro-drone platform based on the **ESP32-S3**, designed for robotics education, embedded development, and flight-control research.

Inspired by [Flix](https://github.com/okalachev/flix/tree/master), the project uses a compact codebase and adds an optical-flow sensor for indoor position and altitude hold.

Open32Drone supports MAVLink and ROS, giving developers an affordable platform to build and extend a micro drone for learning flight control, testing swarm algorithms and studying indoor navigation.

---

## Core Features

### ESP32-S3 Flight Control

- **Compact, modular hardware:** A Seeed Studio XIAO ESP32-S3 controller, four 8520 brushed motors, and a 1S battery form the aircraft.
- **Readable flight-control software:** Code for attitude estimation, stabilization, altitude hold, and position hold.
- **Practical development tools:** Sensor calibration, adjustable control parameters, persistent settings, and flight logs support testing and algorithm development.

### Indoor Flight with Optical Flow and ToF

An IMU and a TF-0850 optical-flow/ToF module support the following functions:

- **Attitude stabilization:** Gyroscope and accelerometer data are used to estimate and control aircraft attitude.
- **Altitude and position hold:** Downward-facing ranging and optical flow support low-altitude height and horizontal-position control.
- **Automatic takeoff and landing:** The Android app and ROS 2 interfaces can request takeoff and landing.

### Control Options

| Control option | What it offers |
| --- | --- |
| **Android app** | Connect over Wi-Fi, view flight status and battery level, take off and land with on-screen buttons, and fly using virtual sticks. |
| **SBUS transmitter** | Physical stick control, flight-mode selection, and emergency disarm. |
| **ROS 2 / MAVROS** | Read IMU, odometry, height, and battery data on a computer, and send velocity, position, takeoff and landing commands. |

### Open Hardware

- **3D-printed frame:** Frame models and MakerWorld printing resources are available.
- **Open PCB project:** The flight-controller baseboard provides power, motor drivers, and module connections; the design is available on JLC Open Hardware.
- **Modular electronics:** The controller, IMU, optical-flow/ToF module, motors, and battery can be assembled and inspected separately.
- **USB and OTA updates:** Install firmware over USB, then use the Android app for subsequent OTA updates while the aircraft is on the ground.

---

## Learning and Development

After assembly and first flight, use the source code and tutorials to continue learning:

- **Hardware and embedded systems:** Explore the wiring, sensor interfaces, motor outputs, and firmware main loop.
- **Feedback control:** Study attitude, altitude, and position control, and use logs to compare the effects of parameter changes.
- **ROS 2 programming:** Read telemetry and combine movement commands into repeatable experiments.
- **Simulation and reinforcement learning:** Run independent numerical-control and residual-PPO exercises, with an optional Isaac adapter for separately prepared scenes.

---

## Future Development

Building on its flight controller and ROS 2 interfaces, Open32Drone plans to expand into multi-drone coordination, environmental perception, and autonomous flight:

- **Multi-drone coordination and swarm formations:** Explore communication, shared state, and coordinated control for formation flight and collaborative tasks.
- **SLAM and indoor navigation:** Combine onboard sensors with ROS 2 computing to explore visual-inertial localization, mapping, and indoor navigation.
- **Autonomous obstacle avoidance and path planning:** Extend environmental perception to explore obstacle detection, local path planning, and autonomous navigation around obstacles.

These are planned development directions. Contributions and experiments in these areas are welcome.

---

## Documentation and Resources

Assembly, firmware installation, calibration, first flight, tuning, ROS 2, and source-build instructions are maintained in the tutorial site.

| Resource | Link |
| --- | --- |
| Source code and collaboration | [GitHub repository](https://github.com/npu-ius-lab/open32drone) |
| Build and development tutorials | [Open32Drone documentation](https://npu-ius-lab.github.io/open32drone/) |
| PCB design | [JLC Open Hardware](https://oshwhub.com/fanchewang/open32drone) |
| Frame printing | [MakerWorld](https://makerworld.com.cn/zh/models/2922108-open32drone-wu-ren-ji-8520kong-xin-bei-ji-jia-ros2#profileId-3425842) |

---

## Contributing

Contributions to maintain and improve Open32Drone are welcome:

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

Original Open32Drone code and documentation are licensed under the **[Apache License 2.0](https://www.apache.org/licenses/LICENSE-2.0)**, unless otherwise stated. Test methods and other components with separate license notices follow their respective terms.

See [LICENSE](./LICENSE) for the full terms and [LICENSE.txt](./LICENSE.txt) for the licensing scope. Third-party code, dependencies, hardware designs and media retain their applicable licenses; see the [third-party notices](./docs/project/third-party.md) for sources and exceptions.

[![License](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)
