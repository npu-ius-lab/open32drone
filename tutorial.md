# Open32Drone: From Zero to Stable Flight

<p align="center">
    <img src="img\drone.PNG" alt="Full drone view" />
</p>

<p align="center">
  <strong>
    <a href="./tutorial_zh_CN.md">简体中文</a> &nbsp;|&nbsp;
    <a href="./tutorial.md">English</a>
  </strong>
</p>

## 1. Project Overview

**Open32Drone Minimal** is an open-source ESP32-S3 micro-UAV platform for research, education, embedded flight-control development, and low-altitude robotics experiments.

The platform integrates inertial sensing, optical flow/ToF, SBUS, four brushed-motor drivers, Wi-Fi/MAVLink, and ROS 2 interfaces in a lightweight embedded architecture. A single ESP32-S3 executes attitude stabilization, assisted takeoff and landing, low-altitude altitude/position hold, and external position or velocity control. The carrier board, sensor timing, relative-state estimator, progressive control stack, and companion applications form one matched system implementation.

## 2. Project Tutorial

### Phase 1: Hardware and Assembly

#### Materials

##### Main Controller Module

<p align="center">
    <img src="img\seed-s3.PNG" />
</p>

Model: Seeed Studio XIAO ESP32-S3 Sense

Reference price: 90 RMB

Module documentation: https://wiki.seeedstudio.com/cn/xiao_esp32s3_getting_started/

##### Frame and Propellers

<p align="center">
    <img src="img\paddle1.PNG" />
</p>

<p align="center">
    <img src="img\paddle2.PNG" />
</p>

Model: 12.3 cm wheelbase frame, 76 mm propellers for 1 mm motor shafts

Required: 1 frame, 4 propellers

Reference price: 19 RMB per set

##### Coreless Motors

<p align="center">
    <img src="img\electric.PNG" />
</p>

Model: 8520 coreless motor, 1 mm shaft diameter

Required: 4

Reference price: 24 RMB for 4 pieces

##### IMU Module

<p align="center">
    <img src="img\imu.PNG" />
</p>

Model: GY-91 module with MPU9250 and BMP280. The current flight configuration disables the BMP280; attitude uses the MPU9250 and altitude hold uses the optical-flow module's ToF sensor.

Required: 1

Reference price: 14 RMB

##### Boost Converter Module

<p align="center">
    <img src="img\up_voltage.PNG" />
</p>

Model: 3.3 V to 5 V boost converter

Required: 1

Reference price: 4.5 RMB

##### ToF Optical-Flow Module

<p align="center">
    <img src="img\tof.png" />
</p>

Model: CORVON link protocol

Required: 1

Reference price: 68 RMB

##### Motor Driver Chip

<p align="center">
    <img src="img\mos.png" />
</p>

Model: AO3400 MOSFET

Required: 4

Reference price: 0.4 RMB for 4 pieces

##### Remote Controller

<p align="center">
    <img src="img\controller.png" />
</p>

Model: Flysky i6S single controller

Required: 1

Reference price: 249 RMB

##### Receiver

<p align="center">
    <img src="img\receiver.png" />
</p>

Model: Flysky A8S receiver, SBUS receiver

Required: 1

Reference price: 65 RMB

##### Other Materials

Motor sockets, battery, pin headers, female headers, and related small parts.

#### 2. Baseboard Fabrication

<p align="center">
    <img src="img\pcb1.png" />
</p>

<p align="center">
    <img src="img\pcb2.png" />
</p>

Model: Custom board

Required: 1

PCB link: https://oshwhub.com/fanchewang/open32drone

##### 2.1 Open the Design

<p align="center">
    <img src="img\design1.png" />
</p>

##### 2.2 Order the PCB

<p align="center">
    <img src="img\design2.png" />
</p>

<p align="center">
    <img src="img\design3.png" />
</p>

<p align="center">
    <img src="img\design4.png" />
</p>

Keep the other options at their default values.

<p align="center">
    <img src="img\design5.png" />
</p>

<p align="center">
    <img src="img\design6.png" />
</p>

#### 3. Drone Assembly

##### 3.1 Materials Check

Resistors: 0805 package, 10K or 20K are both acceptable.

<p align="center">
    <img src="img\all_stuff.png" />
</p>

##### 3.2 Tool Preparation

Soldering iron and solder wire.

##### 3.3 Soldering Process

###### Core Soldering Rule: SMD Parts First, Through-Hole Parts Later

If the female headers are soldered first, their tall plastic bodies will block the soldering iron tip and make SMD soldering extremely difficult. **Always solder the SMD components on the baseboard first, such as MOSFETs and resistors, and solder the female headers and pin headers last.**

###### Step 1: Solder the Power Stage (MOSFETs and Resistors)

**Soldering tip**: solder the resistors first, then solder the MOSFETs.

1. Add a small amount of solder to one PCB pad first.

2. Hold the component with tweezers, align it, and heat the pad to fix it in place.

3. Finally solder the remaining pins and make sure the joints are smooth and rounded.

**Warning**: MOSFETs have polarity and orientation. Check the PCB silkscreen direction carefully. If a MOSFET is reversed, the motor may spin at full speed immediately after power-on, which can easily cause a crash.

<p align="center">
    <img src="img\hanjie1.png" />
</p>

###### Step 2: Solder Module Connectors (Female Headers and Pin Headers)

Once the SMD parts are secure, solder the connectors used for plug-in modules.

- **ESP32-S3 interface**: solder two rows of 2.54 mm female headers. Keep the height level, otherwise the main controller board will tilt after insertion.

- **Sensor interfaces**: include the GY91 module interface, optical-flow ToF combo module interface, and 5 V regulator module interface.

- **Soldering point**: female headers have many pins. It is recommended to solder two diagonal pins first for positioning, confirm that the connector is vertical, and then solder the remaining pins.

<p align="center">
    <img src="img\hanjie2.png" />
</p>

###### Step 3: Solder Interface Components (Battery and Motors)

Finally, complete the input and output connections.

- **Motor sockets (4 pieces)**: solder them at the four corners. Sockets make it easy to quickly replace coreless motors when they wear out.

- **Battery lead**: check the **battery polarity (VCC/GND)** carefully and make sure the wire can handle the instantaneous high current when the 8520 motors run at full speed.

###### Post-Soldering Checklist

Before plugging in the ESP32-S3, perform the following tests:

1. **Short-circuit test**: use the continuity mode of a multimeter to check whether the positive and negative power rails, such as 5 V and GND, are shorted.

2. **Continuity test**: check whether the MOSFET output side, namely the motor port, is connected to the corresponding control pin.

3. **Visual inspection**: check for solder bridges, especially around MOSFETs and the dense female-header pins.

### Phase 3: Software and Development

#### 1. Embedded Development Environment Setup

##### 1.1 Arduino IDE Installation

Arduino IDE is one of the most commonly used integrated development environments for embedded development. It supports Windows, macOS, and Linux. This experiment recommends **Arduino IDE 2.x**. Compared with version 1.x, version 2.x uses a modern editor core and supports auto-completion, intelligent hints, an improved library manager, and a real-time serial monitor, which significantly improves development efficiency.

The current firmware is built with `arduino-esp32 3.3.6`; use the same version to keep the tutorial and build environment aligned.

- **Step 1.** Download and install a stable version of Arduino IDE for your operating system.

https://www.arduino.cc/en/software/

- **Step 2.** Launch Arduino IDE.

- **Step 3.** Add the ESP32 board package to Arduino IDE.

Go to **File > Preferences**, and paste the following URL into **"Additional Boards Manager URLs"**:

```text
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```

<p align="center">
    <img src="img\software1.png" />
</p>

Go to **Tools > Board > Boards Manager...**, enter **esp32** in the search box, and install **esp32 3.3.6**, matching the project validation environment.

<p align="center">
    <img src="img\software2.png" />
</p>

- **Step 4.** Select your board and port.

At the top of Arduino IDE, you can directly select the port. It will likely be COM3 or higher. **COM1** and **COM2** are usually reserved for hardware serial ports. In the board selector on the left, search for **xiao** and select **XIAO_ESP32S3**.

<p align="center">
    <img src="img\software3.png" />
</p>

After completing the preparation above, you can start writing, compiling, and uploading programs for XIAO ESP32-S3.

##### 1.2 BootLoader Mode

Sometimes, an incorrect program can make the XIAO lose its port or stop working normally. Common problems include:

- XIAO is connected to the computer, but no port number can be found.

- XIAO is connected and a port number appears, but program upload fails.

When either problem occurs, try putting XIAO into BootLoader mode. This solves most device-recognition and upload-failure problems. The method is:

- **Step 1.** Press and hold the `BOOT` button on XIAO ESP32-S3.

- **Step 2.** Keep holding `BOOT`, connect the board to the computer through a data cable, and release `BOOT` after connection.

- **Step 3.** Upload **File > Examples > 01.Basics > Blink** to check whether XIAO ESP32-S3 works correctly.

##### 1.3 Reset

When the program behaves abnormally, press `Reset` once while powered on to restart the uploaded program.

Holding `BOOT` during power-on and then pressing `Reset` once can also enter BootLoader mode.

##### 1.4 Run Your First Blink Program

By now, you should have a basic understanding of XIAO ESP32-S3 features and hardware. Next, use the simplest Blink example to make your XIAO ESP32-S3 blink for the first time.

- **Step 1.** Launch Arduino IDE.

- **Step 2.** Go to **File > Examples > 01.Basics > Blink** and open the example.

<p align="center">
    <img src="img\software4.png" />
</p>

- **Step 3.** Select **XIAO ESP32-S3** as the board model, choose the correct port, and upload the program.

<p align="center">
    <img src="img\software5.png" />
</p>

After the program is uploaded successfully, you will see the following output, and the orange LED on the right side of XIAO ESP32-S3 will blink.

<p align="center">
    <img src="img\software6.png" />
</p>

##### 1.5 Dependency Library Installation

<p align="center">
    <img src="img\software7.png" />
</p>

The standard build uses Arduino-ESP32 `3.3.6`, `FlixPeriph 1.10.4`, `MAVLink 2.0.25`, and SBUS. Select `XIAO_ESP32S3`, enable OPI PSRAM, and use the `default_8MB` A/B application partition with DIO Flash. The default IMU backend supports MPU6500/MPU9250; ICM20948 and MPU6050 are separate compile configurations that require their own validation. Minimal has no barometer-control path.

#### 2. Flight-Control Firmware Architecture

This section gives the shortest development path. See [Firmware Architecture](docs/FIRMWARE_ARCHITECTURE.md) for the complete module map, ownership model, background-service boundary, and source reading order.

##### 2.1 Runtime Structure

The firmware does not assign one flight task to every subsystem. Flight-critical work remains in the fixed 300 Hz high-priority `loopTask`: sensor acquisition, state estimation, target selection, control, and motor output execute in a fixed dependency order. CLI, MAVLink, and OTA boot validation are rate-limited inside that loop; the optional camera and MJPEG HTTP service run in a low-priority core-0 background task and never own control.

```mermaid
flowchart LR
  INPUT["imu · rc · flow"] --> EST["estimate"]
  EST --> SAFE["safety · ownership"]
  SAFE --> CTRL["auto · altitude · position · attitude · rate"]
  CTRL --> MOTOR["mix · PWM"]
  MOTOR --> SERVICE["CLI · MAVLink · log · NVS"]
  CAMERA["OV3660"] --> STREAM["lower-priority HTTPD video task"]
```

| Layer | Files | Code boundary |
|---|---|---|
|Entry and scheduling|`firmware.ino`, `time.ino`|Build switches, startup order, 64-bit monotonic time, main loop, and task priorities|
|Inputs|`imu_backend.h`, `imu.ino`, `rc.ino`, `flow.ino`|Compile-time selected IMU, SBUS, and TF-0850 packets with separate ToF and XY-flow sequences, timestamps, and health states|
|Estimation|`estimate.ino`|Attitude, ToF height/vertical speed, and horizontal velocity/relative position with angular-rate time alignment|
|Control|`control.ino`, `control_modes.ino`, `control_offboard.ino`, `control_auto_flight.ino`, `control_altitude.ino`, `control_position.ino`, `control_stabilization.ino`|Shared state plus ownership, Offboard, automatic flight, altitude, position, attitude, and rate layers|
|Safety and power|`safety.ino`, `power.ino`|Arm checks, link-loss descent, sustained-tip motor stop, battery voltage, and hover-thrust feedforward|
|Actuation|`motors.ino`|Quad-X mapping and four LEDC PWM outputs|
|Communications|`mavlink.ino`, `wifi.ino`, `camera.ino`, `ota.ino`|MAVLink, AP/STA networking, optional video, and ground-only A/B OTA|
|Observability|`cli.ino`, `log.ino`|Local serial diagnostics, 25 Hz RAM logs, and performance sampling every 16 cycles|
|Configuration and math|`parameters.ino`, `*.h`|Parameter validation/NVS, PID, filtering, quaternion, and vector utilities|

`flow.ino` receives TF-0850 data, `estimate.ino` updates height state, and `control_altitude.ino` implements altitude control. The flow/ToF module is mounted about 24 mm forward of the yaw center, and the horizontal estimator compensates that geometry.

##### 2.2 Hardware Pin Mapping

The following pins are directly tied to the baseboard hardware. Confirm the baseboard schematic before making changes:

| Peripheral | GPIO | Description |
|---|---|---|
| I2C SDA | 2 | Compile-time selected IMU data, 400 kHz |
| I2C SCL | 43 | Compile-time selected IMU clock line |
| Optical-flow RX | 8 | Serial1 RX, connected to optical-flow module TX. Protocol: frame header 0xDF, 19-byte packet, 115200 bps |
| Optical-flow TX | 7 | Serial1 TX, connected to optical-flow module RX |
| SBUS RX | 44 | Serial2 RX, 100 Kbps, 25-byte frame |
| SBUS TX | 9 | Serial2 TX |
| LED | 21 | Onboard NEOPIXEL |
| Battery voltage | 1 / A0 | `VBAT_SW × 0.5` divider input |
| MOTOR 0 | 4 | LEDC PWM 10 kHz -> AO3400 -> rear left (CW) |
| MOTOR 1 | 3 | Rear right (CW) |
| MOTOR 2 | 6 | Front right (CCW) |
| MOTOR 3 | 5 | Front left (CCW) |

#### 3. Core Subsystems

##### 3.1 Optical-Flow Sensor (`flow.ino`)

**Packet format**

| Byte | Field | Description |
|---|---|---|
| 0 | Header | 0xDF |
| 1-3 | ID/Dev/Sta | 0x15, 0x00, 0x55 |
| 4 | Len | 0x0C, data length 12 bytes |
| 6-7 | ToF | uint16, in mm |
| 10-11 | FlowX | int16, pixel displacement |
| 12-13 | FlowY | int16, pixel displacement |
| 14-15 | IntTime | uint16, in us |
| 16 | Valid | 245 = data valid |
| 18 | Checksum | Sum of the first 18 bytes & 0xFF |

**Velocity conversion formula**

```text
v = flow * (1/10000) * height(m) / dt(s)
```

**Validity conditions**

- `dataValid == true` (byte 16 = 245)

- Height: 0.05 m to 6 m

- Integration time greater than zero

- Health timeout: no valid data for 150 ms -> unhealthy

##### 3.2 Attitude Estimation (`quaternion.h` + `estimate.ino`)

**Quaternion attitude estimator**

- Low-pass-filtered gyroscope rates are integrated into the attitude quaternion as rotation vectors.

- While landed, the accelerometer corrects the estimated gravity direction with `EST_ACC_WEIGHT=0.003`.

- In flight, gravity correction uses the weaker `EST_LVL_WEIGHT=0.0002` only when thrust, acceleration norm, and angular rate pass reliability checks.

- Roll, Pitch, and Yaw share one quaternion representation, avoiding axis coupling from direct Euler-angle integration.

**Altitude estimation**

- Height comes directly from the optical-flow module's ToF sensor, with `position.z` storing filtered relative height above ground.

- Height difference between valid samples produces `velocity.z`, followed by low-pass filtering.

- A height jump above 0.45 m is accepted at 10%, and the maximum height-change rate is limited to 2 m/s.

- Height and vertical speed reset when the motors are stopped and ToF is invalid.

**Horizontal velocity and position**

- Flow displacement is converted using ToF height and integration time, then compensated with angular rates from 40 ms earlier to obtain `flowCompBodyVel`.

- The compensated velocity passes through ground bias learning, a three-sample median, innovation limiting, and low-pass smoothing before world-frame conversion.

- The control variables `velocity.x/y` and `position.x/y` are updated only after `flowAirborne=true`; before takeoff, ground zero-velocity locking is enabled to prevent floor texture noise from drifting the position-hold target.

- Airborne detection requires arming, throttle above 0.12, fresh flow, and valid ToF continuously for 250 ms.

##### 3.3 Flight Control (`control.ino` + `control_*.ino`)

**Progressive control modes**

| Mode | Value | Behavior |
|---|---|---|
| STAB | 2 | Default attitude-stabilized mode. Throttle maps directly to collective thrust, and Roll/Pitch sticks map to attitude targets. |
| ALT_HOLD | 4 | ToF altitude hold. Mid-throttle holds the target height; deviation from mid-stick commands bounded vertical speed. Collective thrust combines hover feedforward and altitude PID. |
| POS_HOLD | 5 | Optical-flow position hold. The controller locks position after altitude and horizontal estimates qualify; Roll/Pitch sticks move the target through velocity commands. |

`AUTO` is an internal ownership state for automatic flight and validated Offboard control, not a fourth pilot mode. The three public modes reuse the same attitude, rate, and mixer inner loops and add vertical and horizontal feedback only when ToF and optical-flow state qualify.

**Mode switching with RC channel 6**

```cpp
ch6 < 25%      -> STAB
ch6 25% ~ 75% -> ALT_HOLD
ch6 > 75%     -> POS_HOLD
```

**Arming and disarming**

```cpp
Arm: throttle lowest + yaw full right
Disarm: throttle lowest + yaw full left
```

**Main altitude-hold and position-hold parameters**

| Parameter | Default | Description |
|---|---:|---|
| Altitude PID | Current registered firmware values | Device values may be overridden by NVS; read back `ALT_P`, `ALT_I`, and `ALT_D` with `p` |
| `ALT_HOVER` | 0.49 | Reboot baseline for hover thrust; adapted slowly in RAM after takeoff |
| `ALT_VEL_MAX` | 0.45 m/s | Maximum vertical-speed command outside the throttle mid-stick deadband |
| `POS_HOLD_P` | 0.80 | Position error to horizontal velocity target |
| `POS_STICK_V` | 0.70 m/s | Target-point speed at full Roll/Pitch stick |
| `POS_VEL_P_X/Y` | 0.30 / 0.30 | Horizontal velocity-loop proportional gain |
| `POS_VEL_I_X/Y` | 0.10 / 0.10 | Horizontal velocity-loop integral gain, limited to 0.08 rad per axis |
| `POS_VEL_D_X/Y` | 0 / 0 | Horizontal velocity-loop derivative gain |
| `POS_CMD_RATE` | 1.20 rad/s | Position-control attitude-command slew limit |
| `FLOW_GYRO_P/R` | -0.78 / -0.77 | Pitch/Roll rotational-flow compensation |
| `FLOW_GYRO_DLY` | 40 ms | Flow and angular-rate time alignment |

##### 3.4 MAVLink Debugging (`mavlink.ino`)

**Transmitted messages**

| Message | Frequency | Description |
|---|---|---|
| HEARTBEAT / CURRENT_MODE | 2 Hz | Reports GENERIC quadrotor identity, current custom mode, and arm state |
| EXTENDED_SYS_STATE / SYS_STATUS | 2 Hz | Reports landed state, sensor health, and system status |
| BATTERY_STATUS | 2 Hz | Reports measured voltage when a voltage-sense pin is configured; default hardware reports unknown |
| ATTITUDE_QUATERNION | 10 Hz | Quaternion attitude and angular velocity, converted according to MAVLink FRD coordinate conventions |
| RC_CHANNELS_RAW (#35) | ~10 Hz | Raw PWM values for 16 channels |
| ACTUATOR_CONTROL_TARGET | 10 Hz | Current normalized outputs for four motors |
| SCALED_IMU | 10 Hz | Accelerometer and gyroscope data |
| LOCAL_POSITION_NED / DISTANCE_SENSOR | 10 Hz | Reports onboard relative position/velocity and valid ToF range |

**Important received messages**

| Message / Command | Function |
|---|---|
| MANUAL_CONTROL | External manual control, mapped to throttle, pitch, roll, and yaw |
| PARAM_REQUEST_LIST / PARAM_REQUEST_READ / PARAM_SET | Parameter reading and setting |
| MAV_CMD_COMPONENT_ARM_DISARM | MAVLink arming/disarming; arming is rejected when throttle is higher than 0.05 |
| MAV_CMD_DO_SET_MODE / DO_SET_STANDARD_MODE | Selects supported stabilized, altitude, position, and AUTO modes |
| MAV_CMD_NAV_TAKEOFF / MAV_CMD_NAV_LAND | Executes sensor-gated automatic takeoff and landing |
| SET_ATTITUDE_TARGET | Receives attitude, rate, and thrust targets in AUTO after stream warmup |
| SET_POSITION_TARGET_LOCAL_NED | Receives bounded local position/velocity and altitude/vertical-speed targets in AUTO |
| SERIAL_CONTROL | Mirrors local diagnostic text to MAVLink only; no inbound remote shell is accepted |

Minimal does not accept direct MAVLink motor control. Use the standard parameter protocol only while disarmed; in flight, the supported entry points are gated lifecycle commands, a manual-control lease, and Offboard position or velocity targets.

##### 3.5 ROS 2 / MAVROS Integration

The repository connects MAVROS to flight-controller UDP 14550 and provides IMU/odometry/ToF/battery/RC interfaces, `/cmd_vel`, `/goal_pose`, lifecycle commands, TF, RViz2, and acceptance tools. The aircraft creates AP `open32drone` by default or can explicitly join a router in STA mode; only Android or ROS 2 may control one aircraft at a time. The current ROS package does not relay experimental HTTP MJPEG or publish `camera_info`. Installation, single- and multi-aircraft naming, public interfaces, and supervised flight are consolidated in [ROS 2 Companion Software and Automatic Flight](docs/AUTOMATIC_FLIGHT_AND_ROS2.md).

**Architecture**

```python
open32drone_driver
├── MAVROS                 <- UDP 14550 -> Open32Drone
├── interface_bridge       <- Reliable IMU/odom/ToF/battery/RC + diagnostics + TF
├── flight_manager         <- arm/takeoff/land/mode commands
├── offboard_control       <- 20 Hz position/velocity targets and watchdog
└── rc_bridge              <- explicitly enabled raw RC stream
```

**Key parameters**

| Parameter | Value | Description |
|---|---|---|
| `fcu_url` | `udp://0.0.0.0:14550@192.168.4.1:14550` | Default AP connection; replace the address with serial `wifi` output in STA mode |
| `tgt_system / tgt_component` | `1 / 1` | MAVLink target identifiers for Open32Drone |
| setpoint rate | 20 Hz | Offboard node continuously refreshes position or velocity targets |
| warmup | at least 0.35 s, 5 samples, 10 Hz | Firmware checks stream continuity before entering AUTO |
| watchdog | 0.30 s | Onboard Offboard failure handling starts after target updates stop |
| public QoS | Reliable | Bridge topics such as `/imu/data`, `/odom`, and `/range/downward` support ordinary ROS tools |

##### 3.6 ROS 2 Manual-Control Command Reference

The tutorial keeps only the shortest operating path; the interface table, multi-aircraft naming, TF, acceptance rules, and troubleshooting live in the dedicated ROS 2 guide. Automatic takeoff lets the firmware perform preflight, arming, climb, and position-hold handover before a finite-duration body-frame target is sent.

| Action | Parameter | Command |
|---|---|---|
| Take off | 0.65 m above the takeoff surface | `ros2 run open32drone_driver control takeoff --height 0.65` |
| Move forward | +X, 0.25 m/s for 1.5 s | `ros2 run open32drone_driver control velocity 0.25 0.00 0.00 --duration 1.5` |
| Move left | +Y, 0.25 m/s for 1.5 s | `ros2 run open32drone_driver control velocity 0.00 0.25 0.00 --duration 1.5` |
| Climb | +Z, 0.20 m/s for 1 s | `ros2 run open32drone_driver control velocity 0.00 0.00 0.20 --duration 1` |
| Land | Controlled descent and automatic disarm | `ros2 run open32drone_driver control land` |

`/cmd_vel` uses body coordinates (+X forward, +Y left, +Z up), while `/goal_pose` is an absolute target in `open32drone/odom`. Keep one control source active, run `ros2 run open32drone_driver bench_test --duration 5` with propellers removed before automatic flight, and use `control status` to verify a live connection.

##### 3.7 A/B Firmware Update and the Minimal Bundle

The current development configuration uses the ESP32-S3 8 MB `default_8MB` A/B layout: `ota_0` starts at `0x10000`, `ota_1` at `0x340000`, and each app slot is `0x330000` bytes. A legacy single-app layout requires one USB migration; subsequent wireless updates write only the inactive slot.

The matched files under `releases/minimal/` are:

| File | Purpose |
|---|---|
|`Open32Drone-minimal-merged.bin`|Complete 8 MiB USB image written at `0x0` for a new MCU or recovery after full erase|
|`Open32Drone-minimal-app.bin`|Application image for ground-only A/B OTA|
|`Open32Drone-Controller-0.1.apk`|Matched Android client, `versionName 0.1`, `versionCode 1`|
|`SHA256SUMS`|Integrity verification before flashing or upload|

Perform the first migration with propellers removed and avoid `erase-flash` when calibration data must be retained. After migration, run `sys`, `imu`, `flow`, and `ota` to verify both app slots, gyro calibration, ToF, loop timing, and the local device token. Wireless update requires a landed, disarmed vehicle with automatic flight, Offboard, and video stopped. Android and ROS 2 uploaders accept only the application `.bin`, never the merged image. They transmit the token, exact length, and SHA-256. The new slot is confirmed only after parameter storage, IMU, attitude, loop rate, ToF, and MAVLink remain healthy; otherwise the bootloader rolls back.

##### 3.8 CLI Debug Commands

USB serial runs at 115200 bps. The current `cli.ino` is compatible with both UART0 and ESP32-S3 USB Serial/JTAG, and provides the following commonly used commands:

| Command | Output | Typical Use |
|---|---|---|
| `help` | All commands | View CLI supported by the current firmware |
| `p` / `p <name>` / `p <name> <value>` | Parameter list, single parameter, write parameter | Tune PID, estimator, and flow parameters; changes are saved to NVS while motors are stopped |
| `ps` / `psq` | Euler angles / quaternion | Quickly confirm attitude direction |
| `imu` | IMU, calibration values, and landed state | Check the sensor and six-face calibration |
| `rc` | 16 channels, normalized input, mode, and armed state | Check SBUS mapping and the three-position switch |
| `flow` | ToF, flow velocity, gyro compensation, position, gates, and reject codes | Check the position-estimation chain |
| `alt` | Altitude engagement, target, ToF height, thrust, and reject code | Check the altitude-control chain |
| `mot` | Four motor outputs | Check motor mapping and mixer direction |
| `time` | Loop rate, average/maximum period, and overruns | Check real-time loop load |
| `ca` / `cr` | Six-face accelerometer calibration / SBUS RC calibration | Use during first assembly or after rebuilding |
| `wifi` | AP/STA, IP, client, and MAVLink status | Check the network connection |
| `ap <ssid> <password>` / `sta <ssid> <password>` | Save AP or router-STA settings | Reboot to apply; failed STA startup opens a recovery AP |
| `arm` / `disarm` | Arm/disarm through serial | Use for propeller-removed debugging |
| `stab` / `alt` / `pos` | Select one of the three flight modes | Propeller-off serial control-chain checks |
| `mfr` / `mfl` / `mrr` / `mrl` | Single-motor test | Must remove propellers; used to confirm motor index |
| `log` / `log dump` | RAM log header / CSV data | Post-flight analysis of attitude, velocity, position, and optical flow |
| `perf` | Per-stage loop timing | Evaluate the 300 Hz loop and background load while disarmed |
| `ota` | A/B update state and device token | Local authentication for the companion Android/ROS 2 uploader |
| `sys` / `reset` / `reboot` | System information / attitude reset / reboot | Inspect and maintain the firmware |

##### 3.9 Android Client and Matching Versions

Android 8.0 (API 26) or newer is required. One configurable aircraft IPv4 address is used for MAVLink, optional video, and OTA. The client provides altitude/position modes, automatic takeoff and landing, dual sticks, diagnostics, and A/B application-image upload.

The operating sequence is: join the `open32drone` Wi-Fi network, wait for MAVLink and ToF readiness, select an assisted mode, arm/take off, then land and confirm disarm. Video replaces the default background only after a fresh MJPEG frame arrives; a stream interruption does not freeze the last frame. Explicit physical SBUS activity takes ordinary control ownership and stops normal phone stick commands, while emergency disarm remains available.

The Minimal Android APK, firmware, and ROS 2 bundle form one matching interface set. Verify `SHA256SUMS` before installation or flashing and do not mix components from different commits. Android and ROS 2 must not control the same aircraft at once; QGC is limited to ground parameter maintenance. The current APK is debug-signed for development and controlled testing, not a product-signed release.

#### 4. Tuning Recommendations

Tune only one class of parameters at a time, and record logs before and after each change.

##### 4.1 How to Use Debug Output

| Stage | Recommended Command | Key Fields | Judgment Criteria |
|---|---|---|---|
| Static after power-on | `imu`, `ps` | Accelerometer, rates, and attitude | Output is stable and handheld tilt follows the body axes |
| RC check | `rc` | Channels, normalized input, mode, armed | Sticks and the three-position switch map correctly |
| Handheld translation/rotation | `flow` | RawVel, Filtered body, Gyro apparent, Position | Translation signs are correct; compensated velocity stays near zero during pure rotation |
| Motor mapping | `mfr/mfl/mrr/mrl` | Corresponding motor | All four commands match physical motor positions; remove propellers |
| Low-altitude STAB | `log dump` | rates, ratesTarget, attitude, motor | Rates and attitude follow targets without persistent motor saturation |
| ALT_HOLD | `alt`, `log dump` | Target, Alt(TOF), velocity.z, altReject | Height converges around the target captured on entry |
| POS_HOLD | `flow`, `log dump` | UsingFlow, PosGate, HoldGate, Locked, position.x/y | Position control starts after the gates and lock point engage |

##### 4.2 Basic Attitude Loop

1. Keep the drone in `STAB` mode and first confirm that it does not spin or keep tipping in one direction.
2. If high-frequency oscillation appears, first reduce `CTL_R_RATE_D` / `CTL_P_RATE_D` or check motor, propeller, and frame vibration.
3. If attitude response is slow, slightly increase `CTL_R_RATE_P` / `CTL_P_RATE_P`.
4. If the attitude can self-level but has slow bias, then consider slightly increasing Rate I. Do not tune integral first.

##### 4.3 Altitude Hold

Altitude hold combines hover-thrust feedforward with altitude PID. In assisted modes, the throttle stick is a vertical-speed command centered at 50%. First estimate hover thrust in `STAB` and store it as `ALT_HOVER`; after entering `ALT_HOLD`, center the stick to hold height and move it up/down to shift the altitude target. After ground arming, motors remain at idle until throttle exceeds the takeoff trigger for 0.20 s, then a bounded thrust ramp starts.

| Symptom | First Adjustment |
|---|---|
| Insufficient thrust after entering `ALT_HOLD` | Check `ALT_HOVER` and runtime `hoverEstimate`; do not substitute stick bias for feedforward calibration |
| Height oscillates | Check the ToF surface and reduce `ALT_P` / `ALT_D` if needed |
| Height response is sluggish | Confirm altitude engagement with `alt`, then adjust `ALT_P` or `ALT_VEL_MAX` slightly |
| Altitude hold does not engage | Inspect `Reject` and ToF height from `alt` |

##### 4.4 Optical-Flow Direction and Position Hold

The current control XY is not raw pixels. It is relative position estimation after height scaling, gyro compensation, bias subtraction, and gating. When tuning optical flow, distinguish three groups of values:

| Field | Meaning |
|---|---|
| `RawVel` in `flow` | Raw axis velocity from the flow module |
| `Filtered body` | Body velocity after gyro compensation and filtering |
| `Gyro apparent` | Apparent optical-flow velocity caused by body rotation |
| `Position X/Y` | Integrated horizontal relative position |
| `UsingFlow/PosGate/HoldGate/Locked` | Flow usage, estimator gate, and position-control state |

Calibration order:

1. Remove propellers, translate the drone forward/back and left/right, and run `flow` after each direction to confirm velocity signs.
2. Pitch and roll the drone in place, then run `flow`; `Filtered body` should remain near zero. Adjust `FLOW_GYRO_P/R` and `FLOW_GYRO_DLY` if needed.
3. Complete STAB and ALT_HOLD checks before trying low-altitude `POS_HOLD`.
4. Use `flow` for `UsingFlow`, `PosGate`, `HoldGate`, and `Locked`; use `log dump` for the continuous transition.
5. If position hold diverges, return to `STAB`, check directions and rotational compensation, then tune `POS_HOLD_P` and `POS_VEL_P_X/Y`.

##### 4.5 RAM Log

`log dump` outputs CSV-format data. Current log columns include:

| Category | Fields |
|---|---|
| Time | `t` |
| Angular rate | `rates.x/y/z`, `ratesTarget.x/y/z` |
| Attitude | `attitude.x/y/z`, `attitudeTarget.x/y/z` |
| Control position | `position.x/y/z` |
| Control velocity | `velocity.x/y/z` |
| Optical-flow chain | `flowRaw.x/y`, `flowComp.x/y`, `flowFilt.x/y`, `flowGyro.x/y` |
| Position control | `targetPosX/Y`, `targetVel.x/y`, `velError.x/y`, `posRollCmd`, `posPitchCmd` |
| Gates and diagnostics | `flowAgeMs`, `flowPosGate`, `posHoldGate`, `flowReject`, `posReject`, `altReject`, `actuatorOwner`, `autoPhase`, `posFallback` |
| Control output | `thrustTarget`, `motor.rl/rr/fr/fl`, `mixerScale`, `posSaturated`, `motorSaturated` |

The RAM log stores roughly the latest 12 seconds at 25 Hz and is exported with `log dump` after disarming; bulk export is blocked in flight. `perf` samples IMU, input, estimation, control, CLI, MAVLink, and housekeeping stage timing once every 16 control cycles. Safety retains link-loss descent and a single sustained-attitude threshold for minimum tip-over motor stop; it performs no collision classification and has no separate incident buffer.

#### 5. Preflight and Staged Acceptance

After a new build, a flight-critical code change, or a sensor replacement, progress in this order:

```mermaid
flowchart LR
  BUILD["build and static checks"] --> BENCH["propeller-off checks"]
  BENCH --> RESTRAINED["restrained low-power tests"]
  RESTRAINED --> FLIGHT["controlled low-altitude flight"]
  FLIGHT --> EVIDENCE["parameters · logs · version record"]
```

| Stage | Required work | Gate to proceed |
|---|---|---|
|Software|Build with fixed ESP32 Core/partition settings; run tests and credential checks|Build succeeds and version/parameter sources are known|
|Propeller-off|Run `sys`, `imu`, `rc`, `flow`; calibrate; test four motors; stop RC/Offboard input|Sensor directions, motor order, rejection logic, and stop paths are correct|
|Restrained power|Inspect spin-up, takeoff ramp, altitude engagement, position gates, landing, and the sustained-tip threshold|Output is continuous and bounded, takeover works, and no abnormal saturation occurs|
|Controlled flight|Validate STAB, ALT_HOLD, POS_HOLD, then automatic flight and external control|Attitude is stable, height/position corrections have the right sign, and failures degrade as intended|

Before flight, check frame orientation, motor/propeller installation, controller mounting, floor texture and illumination, ToF reflection, and area isolation. Hold the aircraft still at boot until `imu` reports completed gyro calibration. Use `rc` to check sticks and the three-position switch, and `flow` to inspect ToF and horizontal flow separately. The first powered flight should be a brief low-altitude STAB test; proceed to altitude and position hold only after manual recovery is demonstrated. Change one factor per run and record firmware, hardware, parameters, environment, and RAM logs.

#### 6. Troubleshooting Table

| Symptom | Possible Cause | Solution |
|---|---|---|
| Flips immediately after takeoff | Propeller direction is wrong | Check X layout: diagonal motors spin in the same direction, adjacent motors spin in opposite directions |
| STAB is stable but XY drifts | Not in POS_HOLD or optical-flow gating is not open | Use `flow` to inspect `UsingFlow/PosGate/HoldGate/Locked` and reject codes |
| Position hold corrects in the wrong direction | Optical-flow X/Y direction or angular-rate compensation is wrong | Translate and rotate the drone by hand, then inspect RawVel and Filtered body with `flow` |
| Large altitude fluctuation | ToF jitter, unsuitable reflecting surface, or excessive altitude gain | Run `alt` and inspect `flowHeight`, `position.z`, and `velocity.z` in the log |
| Hover oscillates | Position/velocity gains are too high | Reduce `POS_HOLD_P` or `POS_VEL_P_X/Y` |
| Motor output is obviously biased to one side | Mixer direction, motor order, or attitude-estimation direction is wrong | Remove propellers, run all four single-motor commands, and inspect corrections in the log |
| Attitude loop oscillates | Frame vibration or excessive rate-loop D gain | Inspect the frame and propellers, then analyze rates and motor output with `log dump` |
| Arming fails | RC signal is abnormal or throttle is not at zero | Check SBUS wiring and use serial `rc` to inspect channel values |
| Wi-Fi cannot connect | Incorrect SSID/password settings or unstable supply | Connect to default AP `open32drone`; use `ap <ssid> <password>` and reboot to change credentials |
| MAVROS cannot connect | Host is not on the flight-controller AP, or IP/UDP port is wrong | Confirm address 192.168.4.1 and port 14550, then inspect status with `wifi` |

## 3. Additional Content

### Reference Links

- Original Flix project: [github.com/okalachev/flix](https://github.com/okalachev/flix)

- MAVLink protocol documentation: [mavlink.io](https://mavlink.io)

- QGroundControl: [qgroundcontrol.com](https://qgroundcontrol.com)

- ESP32-S3 technical manual: [espressif.com](https://www.espressif.com)

- PX4 development guide for PID tuning: [docs.px4.io](https://docs.px4.io)

- Crazyflie technical documentation, Lee controller reference: [bitcraze.io](https://www.bitcraze.io)
