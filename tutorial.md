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

**Open32drone** is a high-performance, low-cost, research-and-education-grade open-source micro drone platform based on **ESP32-S3**.

This project is developed from the open-source [Flix](https://github.com/okalachev/flix/tree/master) project. It keeps the original lightweight code architecture and adds optical-flow and ToF sensors, enabling indoor position hold and altitude hold. Open32drone supports MAVLink and ROS integration, and aims to provide developers with a low-cost, highly extensible micro aerial vehicle platform for learning drone control theory, validating swarm algorithms, and researching indoor navigation.

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

##### IMU Module, 10-Axis Sensor

<p align="center">
    <img src="img\imu.PNG" />
</p>

Model: GY-91 nine-axis MPU9250 + BMP280 barometer

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
    <img src="img\tof.PNG" />
</p>

Model: CORVON link protocol

Required: 1

Reference price: 68 RMB

##### Motor Driver Chip

<p align="center">
    <img src="img\mos.PNG" />
</p>

Model: AO3400 MOSFET

Required: 4

Reference price: 0.4 RMB for 4 pieces

##### Remote Controller

<p align="center">
    <img src="img\controller.PNG" />
</p>

Model: Flysky i6S single controller

Required: 1

Reference price: 249 RMB

##### Receiver

<p align="center">
    <img src="img\receiver.PNG" />
</p>

Model: Flysky A8S receiver, SBUS receiver

Required: 1

Reference price: 65 RMB

##### Other Materials

Motor sockets, battery, pin headers, female headers, and related small parts.

#### 2. Baseboard Fabrication

<p align="center">
    <img src="img\pcb1.PNG" />
</p>

<p align="center">
    <img src="img\pcb2.PNG" />
</p>

Model: Custom board

Required: 1

PCB link: https://oshwhub.com/fanchewang/open32drone

##### 2.1 Open the Design

<p align="center">
    <img src="img\design1.PNG" />
</p>

##### 2.2 Order the PCB

<p align="center">
    <img src="img\design2.PNG" />
</p>

<p align="center">
    <img src="img\design3.PNG" />
</p>

<p align="center">
    <img src="img\design4.PNG" />
</p>

Keep the other options at their default values.

<p align="center">
    <img src="img\design5.PNG" />
</p>

<p align="center">
    <img src="img\design6.PNG" />
</p>

#### 3. Drone Assembly

##### 3.1 Materials Check

Resistors: 0805 package, 10K or 20K are both acceptable.

<p align="center">
    <img src="img\all_stuff.PNG" />
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
    <img src="img\hanjie1.PNG" />
</p>

###### Step 2: Solder Module Connectors (Female Headers and Pin Headers)

Once the SMD parts are secure, solder the connectors used for plug-in modules.

- **ESP32-S3 interface**: solder two rows of 2.54 mm female headers. Keep the height level, otherwise the main controller board will tilt after insertion.

- **Sensor interfaces**: include the GY91 module interface, optical-flow ToF combo module interface, and 5 V regulator module interface.

- **Soldering point**: female headers have many pins. It is recommended to solder two diagonal pins first for positioning, confirm that the connector is vertical, and then solder the remaining pins.

<p align="center">
    <img src="img\hanjie2.PNG" />
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
    <img src="img\software1.PNG" />
</p>

Go to **Tools > Board > Boards Manager...**, enter **esp32** in the search box, select the latest **esp32** package, and install it.

<p align="center">
    <img src="img\software2.PNG" />
</p>

- **Step 4.** Select your board and port.

At the top of Arduino IDE, you can directly select the port. It will likely be COM3 or higher. **COM1** and **COM2** are usually reserved for hardware serial ports. In the board selector on the left, search for **xiao** and select **XIAO_ESP32S3**.

<p align="center">
    <img src="img\software3.PNG" />
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
    <img src="img\software4.PNG" />
</p>

- **Step 3.** Select **XIAO ESP32-S3** as the board model, choose the correct port, and upload the program.

<p align="center">
    <img src="img\software5.PNG" />
</p>

After the program is uploaded successfully, you will see the following output, and the orange LED on the right side of XIAO ESP32-S3 will blink.

<p align="center">
    <img src="img\software6.PNG" />
</p>

##### 1.5 Dependency Library Installation

<p align="center">
    <img src="img\software7.PNG" />
</p>

The sketch depends on `FlixPeriph`, `SBUS`, `MAVLink`, and `Adafruit BMP280`. Select `XIAO_ESP32S3`, enable OPI PSRAM, and use the `max_app_8MB` partition for the camera, Wi-Fi, and flight-control stack.

#### 2. Flight-Control Firmware Architecture

##### 2.1 File Structure Overview

The firmware in this experiment is developed from the Flix architecture and adds serial optical-flow ToF, altitude hold, position hold, MAVLink UDP, serial CLI, and NVS parameter persistence. The following table lists the current source files and their responsibilities:

| File | Responsibility | Details |
|---|---|---|
| `open32drone_v3.ino` | Main entry point | Initializes parameters, motors, camera, Wi-Fi, IMU, BMP280, SBUS, and optical flow; runs acquisition, estimation, control, motor output, communication, logging, and parameter sync in order |
| `control.ino` | Flight control | Contains STAB, ALT_HOLD, POS_HOLD, ACRO, and AUTO; cascaded attitude/rate control, ToF altitude hold, optical-flow position/velocity control, and engagement/reset logic |
| `estimate.ino` | State estimation | Quaternion gyro integration with gated gravity correction; direct ToF height and vertical speed; optical-flow horizontal velocity and position with delayed angular-rate compensation |
| `flow.ino` | Optical-flow ToF driver | Uses `Serial1`; GPIO8 connects to optical-flow TX and GPIO7 to optical-flow RX; parses 19-byte packets beginning with 0xDF; valid height is 0.05-6.0 m; a 150 ms timeout marks data stale |
| `imu.ino` | IMU driver | MPU9250 over I2C, SDA=GPIO2, SCL=GPIO43, 400 kHz; gyro static self-learning; six-face accelerometer calibration |
| `rc.ino` | RC input | SBUS over `Serial2`, RX=GPIO44, TX=GPIO9; default Roll/Pitch/Throttle/Yaw/Mode channels are 0/1/2/3/6 |
| `motors.ino` | Motor output | Four LEDC PWM outputs, 10 kHz, 10-bit; GPIO4/3/6/5 correspond to rear-left/rear-right/front-right/front-left |
| `wifi.ino` | Wi-Fi, UDP, and video | Creates AP `open32drone` at `192.168.4.1`; UDP 14550 carries MAVLink and HTTP `/stream` serves QVGA MJPEG |
| `mavlink.ino` | MAVLink communication | Sends heartbeat, attitude, RC, actuator, IMU, and system state; supports MANUAL_CONTROL, parameters, arming, offboard control, log access, and CLI passthrough |
| `cli.ino` | Serial command line | Runs at 115200 bps and provides parameter, attitude, IMU, RC, flow, altitude, motor, network, log, calibration, and system commands |
| `parameters.ino` | Parameter storage | Uses ESP32 NVS namespace `flix`; reads existing parameters at boot and writes changes at low frequency to avoid filling NVS during startup |
| `log.ino` | Flight log | 50 Hz, 300-sample RAM circular log recording estimates, targets, flow compensation, control gates, saturation, and motor output |
| `safety.ino` | Safety protection | Enters a smooth descent after RC timeout and disarms on an AUTO offboard-target timeout |
| `led.ino` / `time.ino` | Helper modules | LED status indication, `dt`, and `loopRate` statistics |
| `pid.h` / `lpf.h` / `quaternion.h` / `vector.h` / `util.h` | Math and utilities | PID, first-order low-pass filter, quaternion, vector, Rate/Delay, and utility functions |

##### 2.2 Hardware Pin Mapping

The following pins are directly tied to the baseboard hardware. Confirm the baseboard schematic before making changes:

| Peripheral | GPIO | Description |
|---|---|---|
| I2C SDA | 2 | MPU9250 and BMP280 data, 400 kHz; BMP280 is diagnostic while flight height comes from ToF |
| I2C SCL | 43 | MPU9250 clock line |
| Optical-flow RX | 8 | Serial1 RX, connected to optical-flow module TX. Protocol: frame header 0xDF, 19-byte packet, 115200 bps |
| Optical-flow TX | 7 | Serial1 TX, connected to optical-flow module RX |
| SBUS RX | 44 | Serial2 RX, 100 Kbps, 25-byte frame |
| SBUS TX | 9 | Serial2 TX |
| LED | 21 | Onboard NEOPIXEL |
| MOTOR 0 | 4 | LEDC PWM 10 kHz -> A03400 -> rear left (CW) |
| MOTOR 1 | 3 | Rear right (CW) |
| MOTOR 2 | 6 | Front right (CCW) |
| MOTOR 3 | 5 | Front left (CCW) |

##### 2.3 Main Loop Execution Order

```cpp
#define WIFI_ENABLED 1         // Enable Wi-Fi MAVLink
#define OPTICAL_FLOW_ENABLED 1 // Enable optical-flow sensor
```

The main loop keeps a fixed data-dependency order: acquisition, timing, estimation, control, motor output, communication, and logging. MJPEG streaming runs as a lower-priority task while the flight `loopTask` keeps higher priority:

```cpp
void loop() {
    readIMU();
    step();
    readRC();
    readOpticalFlow();
    updateBaro();
    estimate();
    estimateHeight();
    estimateHorizontalVelocity();
    control();
    sendMotors();
    handleInput();
    processMavlink();
    readVoltage();
    logData();
    syncParameters();
}
```

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

##### 3.3 Flight Control (`control.ino`)

**Five flight modes**

| Mode | Value | Behavior |
|---|---|---|
| STAB | 2 | Default mode. Direct throttle + self-leveling attitude stabilization. RC sticks map to angle commands. |
| ALT_HOLD | 4 | ToF altitude hold. The entry height becomes the target; pilot throttle remains the base thrust and the altitude PID adds correction. |
| POS_HOLD | 5 | Optical-flow position hold. Once altitude hold, flow, ToF, attitude, and yaw conditions are satisfied, the controller locks the current point; Roll/Pitch sticks move the target. |
| ACRO | 1 | Rate-control mode. Sticks map directly to rate targets |
| AUTO | 3 | MAVLink external-control mode. Accepts SET_ATTITUDE_TARGET or SET_ACTUATOR_CONTROL_TARGET |

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
| Altitude PID | P=0.8, I=0.1, D=0.2 | Compile-time constants; correction limited to +/-0.2 and integral to +/-0.3 |
| `POS_HOLD_P` | 0.50 | Position error to horizontal velocity target |
| `POS_STICK_V` | 0.50 m/s | Target-point speed at full Roll/Pitch stick |
| `POS_VEL_P_X/Y` | 0.20 / 0.20 | Horizontal velocity-loop proportional gain |
| `POS_VEL_I_X/Y` | 0 / 0 | Horizontal velocity-loop integral gain |
| `POS_VEL_D_X/Y` | 0 / 0 | Horizontal velocity-loop derivative gain |
| `POS_CMD_RATE` | 1.20 rad/s | Position-control attitude-command slew limit |
| `FLOW_GYRO_P/R` | -0.78 / -0.77 | Pitch/Roll rotational-flow compensation |
| `FLOW_GYRO_DLY` | 40 ms | Flow and angular-rate time alignment |

##### 3.4 MAVLink Debugging (`mavlink.ino`)

**Transmitted messages**

| Message | Frequency | Description |
|---|---|---|
| HEARTBEAT (#0) | 2 Hz | type=QUADROTOR, autopilot=GENERIC, base_mode=armed/disarmed |
| EXTENDED_SYS_STATE | 2 Hz | Reports landed / in-air state |
| BATTERY_STATUS | 2 Hz | Reports battery fields when a voltage-sense pin is configured |
| ATTITUDE_QUATERNION | 10 Hz | Quaternion attitude and angular velocity, converted according to MAVLink FRD coordinate conventions |
| RC_CHANNELS_RAW (#35) | ~10 Hz | Raw PWM values for 16 channels |
| ACTUATOR_CONTROL_TARGET | 10 Hz | Current normalized outputs for four motors |
| SCALED_IMU | 10 Hz | Accelerometer and gyroscope data |

**Important received messages**

| Message / Command | Function |
|---|---|
| MANUAL_CONTROL | External manual control, mapped to throttle, pitch, roll, and yaw |
| PARAM_REQUEST_LIST / PARAM_REQUEST_READ / PARAM_SET | Parameter reading and setting |
| MAV_CMD_COMPONENT_ARM_DISARM | MAVLink arming/disarming; arming is rejected when throttle is higher than 0.05 |
| MAV_CMD_DO_SET_MODE | Switches `RAW/ACRO/STAB/AUTO`; select altitude and position hold with the RC three-position switch |
| SET_ATTITUDE_TARGET | Receives attitude, rate, and thrust targets in AUTO mode |
| SET_ACTUATOR_CONTROL_TARGET | Receives direct motor-control values in AUTO mode |
| SERIAL_CONTROL | Passes CLI commands through MAVLink |

##### 3.5 ROS 2 / MAVROS Integration

The repository's `ros2_open32drone` package runs on the host, connects MAVROS to flight-controller UDP 14550, and publishes the onboard MJPEG stream as ROS 2 image topics. The flight controller creates AP `open32drone` with password `12345678` at `192.168.4.1`.

**Architecture**

```python
ComponentContainer("open32drone")
├── mavros::Router  <- UDP -> ESP32 (AP: 192.168.4.1:14550 / STA: check serial wifi output)
│   ├── fcu_urls: udp://:14550@<flight-controller-ip>:14550
│   ├── gcs_urls: udp://0.0.0.0:14551@            (forward to ground station)
│   └── uas_urls: /open32drone_uas                (internal)
└── mavros::UAS     <- Router -> ROS topics
    namespace: /open32drone
```

**Key parameters**

| Parameter | Value | Description |
|---|---|---|
| `fcu_urls` | `udp://:14550@<flight-controller-ip>:14550` | Use 192.168.4.1 in AP mode; in STA mode, use the STA IP printed by serial command `wifi` |
| `gcs_urls` | `udp://0.0.0.0:14551@` | Also forwards to the ground station; 14551 avoids conflict with flight-controller port 14550 |
| `system_id` | 255 | GCS system ID, standard MAVLink convention |
| `component_id` | 240 | MAVROS component ID, standard value |
| `target_system_id` | 1 | Flight-controller SYSTEM_ID=1, consistent with `mavlink.ino` |
| `target_component_id` | 1 | Flight-controller component ID |
| `fcu_protocol` | v2.0 | MAVLink v2 |
| `connection_timeout` | 10.0 | Connection timeout, 10 seconds |
| `heartbeat_interval` | 1.0 | Heartbeat at 1 Hz |
| `timeout_heartbeat` | 5.0 | Consider offline after 5 seconds without heartbeat |
| `enable_autopilot_version_check` | false | Skip version check because the custom controller has no standard version number |

**Plugin whitelist**

Enabled MAVROS plugins: sys_status / command / param / manual_control / imu. `open32drone.launch.py` also starts the MJPEG camera node, which publishes `image_raw` and `image_raw/compressed`.

**IMU noise parameters**

```text
imu/frame_id: base_link
imu/linear_acceleration_stdev: 0.0003
imu/angular_velocity_stdev: 0.000349
imu/orientation_stdev: 1.0
```

**Topic remapping**

```text
/open32drone/UAS1/imu/data -> /imu/data
/open32drone/UAS1/imu/data_raw -> /imu/data_raw
/open32drone/UAS1/manual_control/send -> /manual_control
```

**QoS configuration for IMU**

```text
history: keep_last, depth: 10
reliability: best_effort
durability: volatile
```

##### 3.6 ROS 2 Manual-Control Command Reference

**Arm and disarm**

```text
# Arm
ros2 service call /osdrone/arming mavros_msgs/srv/CommandBool "{value: true}"

# Disarm
ros2 service call /osdrone/arming mavros_msgs/srv/CommandBool "{value: false}"
```

**Manual flight control**

Send `ManualControl` messages through the `/osdrone/send` topic. All four-axis parameters use the range **[-1000, 1000]**.

| Action | Parameter | Command |
|---|---|---|
| Hover | Throttle 200 | `ros2 topic pub /osdrone/send mavros_msgs/msg/ManualControl "{x:0,y:0,z:200,r:0,buttons:0}" --once` |
| Move forward | Pitch +300 | `ros2 topic pub /osdrone/send mavros_msgs/msg/ManualControl "{x:300,y:0,z:500,r:0,buttons:0}" --once` |
| Move backward | Pitch -300 | `ros2 topic pub /osdrone/send mavros_msgs/msg/ManualControl "{x:-300,y:0,z:500,r:0,buttons:0}" --once` |
| Move right | Roll +300 | `ros2 topic pub /osdrone/send mavros_msgs/msg/ManualControl "{x:0,y:300,z:500,r:0,buttons:0}" --once` |
| Move left | Roll -300 | `ros2 topic pub /osdrone/send mavros_msgs/msg/ManualControl "{x:0,y:-300,z:500,r:0,buttons:0}" --once` |
| Turn right | Yaw +200 | `ros2 topic pub /osdrone/send mavros_msgs/msg/ManualControl "{x:0,y:0,z:500,r:200,buttons:0}" --once` |
| Turn left | Yaw -200 | `ros2 topic pub /osdrone/send mavros_msgs/msg/ManualControl "{x:0,y:0,z:500,r:-200,buttons:0}" --once` |

**Note**: each `--once` command sends only one frame. Continuous flight requires repeated publishing or a node that publishes at a fixed rate.

**Flight-mode switching**

Use MAVLink `MAV_CMD_DO_SET_MODE` (command=176). `param1=1.0` means base_mode=CUSTOM, and `param2` is the submode number.

| Mode | param2 | ROS 2 Command |
|---|---|---|
| MANUAL | 0.0 | `ros2 service call /osdrone/command mavros_msgs/srv/CommandLong "{command:176,param1:1.0,param2:0.0}"` |
| ACRO | 1.0 | `ros2 service call /osdrone/command mavros_msgs/srv/CommandLong "{command:176,param1:1.0,param2:1.0}"` |
| STAB | 2.0 | `ros2 service call /osdrone/command mavros_msgs/srv/CommandLong "{command:176,param1:1.0,param2:2.0}"` |
| AUTO | 3.0 | `ros2 service call /osdrone/command mavros_msgs/srv/CommandLong "{command:176,param1:1.0,param2:3.0}"` |

**Parameter read/write**

```text
# Pull all parameters locally
ros2 service call /osdrone/pull mavros_msgs/srv/ParamPull "{force_pull: true}"

# Listen to parameter events to get the parameter list
ros2 topic echo /parameter_events

# Read specified parameters
ros2 service call /osdrone/get_parameters rcl_interfaces/srv/GetParameters "{names: ["CTL_R_RATE_P", "CTL_P_RATE_P", "CTL_Y_RATE_P"]}"
```

**Write parameters**

```text
ros2 service call /osdrone/set mavros_msgs/srv/ParamSetV2 "{force_set: true, param_id: "CTL_R_RATE_P", value: {type: 3, double_value: 0.15}}"
```

`type` corresponds to MAVLink `MAV_PARAM_TYPE`: 1=uint8, 2=int8, 3=uint16, 4=int16, 5=uint32, 6=int32, 9=float, using `double_value` in practice. Most parameters in the flight-controller code are float, so type=9 is commonly used.

**Usage notes**

- Each ManualControl `--once` command sends only one frame. Use a loop in code for continuous control.

- Throttle `z` ranges from [0, 1000], corresponding to 0-100% thrust in the controller.

- Pitch/Roll [-1000, 1000] maps to `CTL_TILT_MAX`, 30 degrees by default; yaw maps to `CTL_Y_RATE_MAX`.

- After switching modes, wait for heartbeat confirmation. QGC or `ros2 topic echo /osdrone/state` can show the current mode.

- Make sure throttle=0 before disarming, otherwise the controller will not respond to the disarm command.

##### 3.7 CLI Debug Commands

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
| `ap <ssid> <password>` | Save AP name and password | Change wireless settings; reboot to apply |
| `arm` / `disarm` | Arm/disarm through serial | Use for propeller-removed debugging |
| `raw` / `stab` / `acro` / `auto` | Select a debug mode | Debug the control chain through serial |
| `mfr` / `mfl` / `mrr` / `mrl` | Single-motor test | Must remove propellers; used to confirm motor index |
| `log` / `log dump` | RAM log header / CSV data | Post-flight analysis of attitude, velocity, position, and optical flow |
| `sys` / `reset` / `reboot` | System information / attitude reset / reboot | Inspect and maintain the firmware |

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

Altitude hold uses pilot base throttle plus PID correction. Find approximate hover throttle in `STAB`, keep a similar throttle when entering `ALT_HOLD`, and let the controller capture the entry height.

| Symptom | First Adjustment |
|---|---|
| Insufficient correction after entering `ALT_HOLD` | Keep base throttle near the hover point |
| Height oscillates | Check the ToF surface and reduce the compile-time altitude P/D gains if needed |
| Height response is sluggish | Confirm `AltHold=1` with `alt`, then increase the compile-time altitude P gain slightly |
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
| Gates and diagnostics | `flowAgeMs`, `flowPosGate`, `posHoldGate`, `flowReject`, `posReject`, `altReject` |
| Control output | `thrustTarget`, `motor.rl/rr/fr/fl`, `posSaturated`, `motorSaturated` |

The log stores the latest six seconds at 50 Hz. After disarming, run `log dump` and plot attitude, height, flow, position targets, control commands, and motor outputs with a spreadsheet, Python, or Matlab.

#### 5. Troubleshooting Table

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
