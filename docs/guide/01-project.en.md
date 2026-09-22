# 01 · Project overview

## Start here

The README introduces the project; this documentation site explains how to build and use it. Follow the stages in order for a first build, or jump to the relevant stage for an existing aircraft.

| Your next task | Guide |
|---|---|
| Buy parts, print the frame, obtain the PCB and assemble | [Shopping list](03-hardware.en.md#purchasing) · [Assembly](03-hardware.en.md) |
| Download software/firmware/APK, flash, calibrate and make a first flight | [Firmware, calibration and first flight](04-firmware-flight.en.md) |
| Improve flight behavior or report a problem | [Tuning and diagnosis](05-tuning.en.md) · [Useful issue reports](../project/contributing.md) |
| Read telemetry and program motion with ROS 2 | [ROS 2 control](06-ros.en.md) |
| Compile or modify the software | [Source and build](../reference/source-build.md) · [Parameters and interfaces](../reference/firmware.md) |
| Explore simulation and reinforcement learning | [Separate numerical and simulation exercises](07-rl.en.md) |

Without a physical transmitter, choose the Android route in the first-flight chapter. Simulation and reinforcement learning are not first-flight prerequisites.

Open32Drone is a build-it-yourself indoor quadrotor for education and research.
Print the frame, solder the power/motor interconnect PCB, then fit the XIAO
ESP32-S3, IMU, TF-0850, motors and battery. Start with a repeatable manual flight;
ROS control and numerical learning exercises are later stages.

The original flight-control core is derived from Oleg Kalachev's Flix. This
project adds the board mapping, brushed-motor drive, optical-flow/ToF control,
voltage sensing, automatic actions, Android and ROS interfaces. It is not an
official Flix release.

## System map

```text
IMU + flow/ToF → estimates → rate/attitude/height/position control → motor mixer
                    ↑                 ↑                              │
                    └── next sample ──┴──── aircraft motion ──────────┘
                                      ↑
                          SBUS or Android or ROS
```

The reference airframe is about 103.3 × 103.3 mm with four 8520 motors and
matched 60 mm or 65 mm propellers. The illustrated 81 g build uses a 25 g 1S
18350 battery. Measure your actual assembly and preserve the mounting directions.

| Directory | Purpose |
|---|---|
| `software/hardware/` | Printable/editable frame and mechanical BOM |
| `software/firmware/` | Sensor acquisition, control, motors, network and maintenance |
| `software/android/` | Phone controller source |
| `software/ros2/` | ROS nodes, commands and RViz configuration |
| `software/simulation/` | Numerical dynamics, learning exercises and optional Isaac adapter |
| `software/releases/minimal/` | Matching firmware, APK and ROS source archive |
| `docs/guide/` | The single seven-chapter learning route |

URDF/USD assets and Gazebo integration require separate preparation; they are
not installed by the real-aircraft ROS package. The capability tables below
describe implemented interfaces, not a promise of performance in every environment.


## Flight and sensing

| Capability | State | Operator surface |
|---|---|---|
| Fixed 300 Hz flight loop | Active | `time`, `perf` |
| Stabilize / Altitude Hold / Position Hold | Active | SBUS switch, Android/ROS lifecycle |
| Relative-height takeoff and automatic landing | Active | Android/ROS command or assisted SBUS takeoff |
| MPU6500/MPU9250 default IMU | Active | build-selected backend, `imu` |
| ICM20948 and MPU6050 backends | Build option | separate compile and hardware validation required |
| TF-0850 optical flow and ToF | Active | `flow`, MAVLink telemetry |
| Battery voltage and bounded thrust feed-forward | Active | `pw`, MAVLink battery telemetry |
| Physical SBUS emergency disarm | Active | independent of Android/ROS ownership |
| Sustained-tip ground stop | Active, minimal | one attitude/time guard; not collision classification |

## Network and maintenance

| Capability | State | Boundary |
|---|---|---|
| Aircraft Wi-Fi AP | Active default | `ap <ssid> <pass>`, fixed aircraft IP `192.168.4.1` |
| Router Wi-Fi STA | Active | `sta <ssid> <pass>`; DHCP assigns the aircraft IP |
| STA boot recovery | Active | after an 8 s connection failure, firmware opens the saved AP without changing the configured STA mode |
| MAVLink command/telemetry | Active | UDP `14550`; one Android or ROS controller per aircraft at a time |
| Multi-aircraft ROS 2 graph | Active | unique namespace, System ID, host UDP port, IP, and TF prefix per aircraft |
| Standard MAVLink parameter protocol | Active | `PARAM_REQUEST_LIST`, `PARAM_REQUEST_READ`, `PARAM_SET`, `PARAM_VALUE` |
| MAVLink diagnostic-text mirror | Active, outbound only | `SERIAL_CONTROL_DEV_SHELL`; it does not accept remote CLI input |
| Local USB serial CLI | Active | parameters, calibration, diagnostics, motor test, network setup |
| In-memory flight log | Active | 25 Hz, approximately 12 seconds, serial/MAVLink download |
| Sampled loop profiler | Active | one sample per 16 loops; `perf` while disarmed |
| Ground-only A/B OTA | Active | HTTP `8080`, application image only after the first complete USB migration |
| Background MJPEG camera | Experimental source candidate | HTTP `/stream`, one viewer, outside the 300 Hz loop; not yet software/hardware/flight release evidence |

The firmware learns the MAVLink reply address from the most recent valid UDP
sender. Android and ROS must therefore not control the same aircraft at the
same time. Being on the same router does not change this ownership rule. The
camera server also permits one stream client; Android viewing and a separate
OpenCV reader cannot consume it simultaneously.

## Persistence

These actions can intentionally change NVS:

- serial `p <name> <value>` or ground-only MAVLink `PARAM_SET`;
- `ca` accelerometer calibration;
- `cr` SBUS calibration;
- `ap` or `sta` Wi-Fi configuration.

Boot gyro bias, optical-flow ground bias, controller integrators, flight
targets, and voltage compensation are runtime state and do not rewrite tuning
parameters. `preset` removes registered parameters but preserves AP/STA
credentials. A complete flash erase removes both parameters and credentials.

## Deliberately absent

The current Open32Drone stack has no GPS mission engine, barometer control,
magnetic-heading fusion, complex collision classifier, direct MAVLink motor
control, automatic trim, automatic tuning-profile migration, Bluetooth control,
web flight-control UI, or inbound remote shell. QGC is not a flight-control
dependency; its supported role is ground-only standard parameter inspection and
editing.
