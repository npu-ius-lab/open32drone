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

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=ZjQ5YjI1YTI2MjQ1N2M0OWY2MTRjNWMyM2RlNGQyOTJfMTc0ZGZlN2M4MDFmM2YwZWJiZDcxNzhiMjJmOTkzZDRfSUQ6NzY0NTg5MzkzODIwNDc4OTkzOV8xNzgwMjIxMTU1OjE3ODAzMDc1NTVfVjM)

Model: Seeed Studio XIAO ESP32-S3 Sense

Reference price: 90 RMB

Module documentation: https://wiki.seeedstudio.com/cn/xiao_esp32s3_getting_started/

##### Frame and Propellers

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=MDgwYTRlNDJhMTE2MGVlMWRjNzE1ZDM1YWRlMzdmY2NfM2ZlOTA0NTZiOTc0NzUwZjUwODg3ZjA5YTNhMzIzMDlfSUQ6NzY0NTg5MzkzNjIwNDMwMzU2NF8xNzgwMjIxMTU1OjE3ODAzMDc1NTVfVjM)

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=ZDQ3OTMwMjg5ZTk3ZWNhYzU3YTJjZWUzNzQ1MWFiYWRfODQ5MTVkNDMyMWMyMDM5MzQ0YTQ1MjgyNzI1YWZlMjdfSUQ6NzY0NTg5MzkzNzY0Njk5NjY3Nl8xNzgwMjIxMTU1OjE3ODAzMDc1NTVfVjM)

Model: 12.3 cm wheelbase frame, 76 mm propellers for 1 mm motor shafts

Required: 1 frame, 4 propellers

Reference price: 19 RMB per set

##### Coreless Motors

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=NTMzZGQ3MDNiOWQwYzgzNWFjYzc0YzkwZGVkYmZlMjBfZDU2YzcyM2Q5ZWM4MmZmZTRkZjlkNWNlNzUyNTA5OTNfSUQ6NzY0NTg5MzkzNTU5MTk1MTUzOF8xNzgwMjIxMTU1OjE3ODAzMDc1NTVfVjM)

Model: 8520 coreless motor, 1 mm shaft diameter

Required: 4

Reference price: 24 RMB for 4 pieces

##### IMU Module, 10-Axis Sensor

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=YWY1MGU4MDNjZTE0NzU4MGI5NDU4NTRjYjBiNDdjNjJfNGNjNGVlOGI1YWFmMTQwMmQzYzIxMzY1NjNhZWJkYzFfSUQ6NzY0NTg5MzkzNzE2MDM5MTg3NF8xNzgwMjIxMTU1OjE3ODAzMDc1NTVfVjM)

Model: GY-91 nine-axis MPU9250 + BMP280 barometer

Required: 1

Reference price: 14 RMB

##### Boost Converter Module

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=NWJkNGUwODQxZmZkNmJkODZkZjgyMjMwOGUzMmJhYzVfNDhhN2ZiM2I5MzEwYTFmMjQ5MzRiZDllNGZlODczZGJfSUQ6NzY0NTg5MzkzNjQyNjYwMTY2NV8xNzgwMjIxMTU1OjE3ODAzMDc1NTVfVjM)

Model: 3.3 V to 5 V boost converter

Required: 1

Reference price: 4.5 RMB

##### ToF Optical-Flow Module

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=MzZiZDhmYjk5YmI5NmEwZjQ4ZDgyMWU5YzdiYzM3MGZfZTg0Y2JmNzJiY2IzNmUzMDUzMTJkYmZlMWNjMDU4YjNfSUQ6NzY0NTg5MzkzNjY3OTgwMDAxMl8xNzgwMjIxMTU1OjE3ODAzMDc1NTVfVjM)

Model: CORVON link protocol

Required: 1

Reference price: 68 RMB

##### Motor Driver Chip

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=YzI3NDQyMGIyZTU0ZmQ0MmJkYzYxNTdmNjIyZjlmZTlfZWY3YmM1OWQxOWNkYTQ5Y2EyNDMxM2E3YWI3MGQ1MzNfSUQ6NzY0NTg5MzkzNzQ0MTUwODU3Nl8xNzgwMjIxMTU1OjE3ODAzMDc1NTVfVjM)

Model: AO3400 MOSFET

Required: 4

Reference price: 0.4 RMB for 4 pieces

##### Remote Controller

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=ZjBmMjJmNzlmNGJlMDVkZTBiYTMyZTQxZTQ4ZTI2M2VfMjc2YzRiYmU0MjhiZmZkMmM2ZmIyMWYxODVkNmZkNzJfSUQ6NzY0NTg5MzkzNjg5MjAzODM0OF8xNzgwMjIxMTU1OjE3ODAzMDc1NTVfVjM)

Model: Flysky i6S single controller

Required: 1

Reference price: 249 RMB

##### Receiver

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=MDQ3ZjM4NDVmN2U1OWRmNDk4NmEzNmUxNjc4YjE3N2JfYzgyYmEwYTk3NTY5OGYxOTU0YWQ2MWUxYmFhN2VmZGVfSUQ6NzY0NTg5MzkzOTA3NzIyMTU5M18xNzgwMjIxMTU1OjE3ODAzMDc1NTVfVjM)

Model: Flysky A8S receiver, SBUS receiver

Required: 1

Reference price: 65 RMB

##### Other Materials

Motor sockets, battery, pin headers, female headers, and related small parts.

#### 2. Baseboard Fabrication

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=MTFjZmRkZTRiOWVhNzM4ZjU1MGZhODM5YWU1Mjg5MmFfMGUyNTM4Y2QzODllMTAyM2U2Zjk2NWY3ODYyMjRjMjBfSUQ6NzY0NTg5MzkzNjY3MzgwMzQ4NF8xNzgwMjIxMTU1OjE3ODAzMDc1NTVfVjM)

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=N2FkMmNkOWYyMGY2MTUwMTNhYzI0YTI0ODViN2Y1NDlfNjEzYTRhZDc4NTAxZjFhMTNmZjBlYmUyM2ZjZDNhZmZfSUQ6NzY0NTg5MzkzODU4NjQyMjQ4N18xNzgwMjIxMTU1OjE3ODAzMDc1NTVfVjM)

Model: Custom board

Required: 1

PCB link: https://oshwhub.com/fanchewang/open32drone

##### 2.1 Open the Design

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=MGI5ZDMxZjViNDMxZDU1Zjk1ZjE0MTMwNjM3MzUyZDVfM2I3NjliMmEwNThmMGEyZTkwNzg3YzZjZDAzMjE1ODNfSUQ6NzY0NTg5MzkzNzc1MTkxOTgwOV8xNzgwMjIxMTU1OjE3ODAzMDc1NTVfVjM)

##### 2.2 Order the PCB

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=NzZlY2RmMGUzZTE1YWUwNzM0ZTM3NGMwMjhjY2FlMGFfZTMyMDNlM2NkODNjM2M5MThiYThkODViNjI1YjI3OWZfSUQ6NzY0NTg5MzkzNTUzMzI5NjgzM18xNzgwMjIxMTU1OjE3ODAzMDc1NTVfVjM)

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=MTI0ZTgyMTYxMzBiZmU3YjM2Y2YxMDNkZWM4ZjEzZWFfMTdkNTU3ZTZlODVjZGM5NGIxMGYzNzE5Zjk3MGI3ZmFfSUQ6NzY0NTg5MzkzODI5NzA0ODI2OF8xNzgwMjIxMTU1OjE3ODAzMDc1NTVfVjM)

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=YjFjNTkyMGNmOWM3ZTlhNmFhZGUyODAwYzUwYmVjMDFfNmJjYTgxNzFhZmMxMDQyNTJjYjFlYTA1ZmYxZTVhMWNfSUQ6NzY0NTg5MzkzNjIyNjg2NDM0OF8xNzgwMjIxMTU1OjE3ODAzMDc1NTVfVjM)

Keep the other options at their default values.

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=NzZiMmJiNTY5YTdkMDUzODcyNjhjYjVkZjY2YWQzMTNfMDlkNWNjMGNmOTU3ODczNmNjMmY2NWViODU0ZDg5MTZfSUQ6NzY0NTg5MzkzODM0NzM3OTkwM18xNzgwMjIxMTU1OjE3ODAzMDc1NTVfVjM)

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=ZjhmYjdkM2VjYTVlZTRmNzJlZGIzYjAwZmY2ZTZmNGFfZjNmMGJkODI1NWYyNmYyYmQ3NGY3YmYyNTNmY2I3MDFfSUQ6NzY0NTg5MzkzNjY3OTg4MTkzMl8xNzgwMjIxMTU1OjE3ODAzMDc1NTVfVjM)

#### 3. Drone Assembly

##### 3.1 Materials Check

Resistors: 0805 package, 10K or 20K are both acceptable.

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=YzAzMDU2ZjI5OWI1ODE4MGY5Mzc1NTQ1ODI2MjEwZTlfNWI2OTEyZmQzNGZiZmQ4NjRkY2MzOTdhMzllZTkxZmVfSUQ6NzY0NTg5MzkzNjc4NzI0NjI5Nl8xNzgwMjIxMTU1OjE3ODAzMDc1NTVfVjM)

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

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=ZWE1MWFmOGM4Mzc4NDQ3YTEyZmQwNzE5ZjQwZTAzYzNfMmMwODEzMzdmZmRmN2I3MzEzODRhNjY3Mjg0ZWRlOTlfSUQ6NzY0NTg5MzkzODM1MTQ5MjMwOV8xNzgwMjIxMTU1OjE3ODAzMDc1NTVfVjM)

###### Step 2: Solder Module Connectors (Female Headers and Pin Headers)

Once the SMD parts are secure, solder the connectors used for plug-in modules.

- **ESP32-S3 interface**: solder two rows of 2.54 mm female headers. Keep the height level, otherwise the main controller board will tilt after insertion.

- **Sensor interfaces**: include the GY91 module interface, optical-flow ToF combo module interface, and 5 V regulator module interface.

- **Soldering point**: female headers have many pins. It is recommended to solder two diagonal pins first for positioning, confirm that the connector is vertical, and then solder the remaining pins.

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=NzljZTkzNTI3NTZlNGVlMzgzZjMyOThjNjBiNThkNDZfYjgwZTM4M2VhNjJmMTQxZWE0OGI0YmQ1YjYzZDcwODBfSUQ6NzY0NTg5MzkzODczMzIyMzEzMV8xNzgwMjIxMTU1OjE3ODAzMDc1NTVfVjM)

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

The XIAO ESP32-S3 board package requires version **2.0.8** or later.

- **Step 1.** Download and install a stable version of Arduino IDE for your operating system.

https://www.arduino.cc/en/software/

- **Step 2.** Launch Arduino IDE.

- **Step 3.** Add the ESP32 board package to Arduino IDE.

Go to **File > Preferences**, and paste the following URL into **"Additional Boards Manager URLs"**:

```text
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=OGY2MTFkMjEwM2QyOTdkNTcwNjljNTA0OTgyOTFlNzRfZmE3N2FjYWQwYTc3N2JjMzUwZTYwN2RmYjNlNjM1ZmVfSUQ6NzY0NTg5MzkzNzMxOTc5MTgxNl8xNzgwMjIxMTU1OjE3ODAzMDc1NTVfVjM)

Go to **Tools > Board > Boards Manager...**, enter **esp32** in the search box, select the latest **esp32** package, and install it.

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=YzJhMmY0MzcyZjJkYzljNzQ3OTkzYzA1NjQxOGQ1ZjBfYjk5YTM5NzBkNTgyMzQ2NmMxODA2ZDBkYWY1ODMxNTRfSUQ6NzY0NTg5MzkzODU4NjQwNjEwM18xNzgwMjIxMTU1OjE3ODAzMDc1NTVfVjM)

- **Step 4.** Select your board and port.

At the top of Arduino IDE, you can directly select the port. It will likely be COM3 or higher. **COM1** and **COM2** are usually reserved for hardware serial ports. In the board selector on the left, search for **xiao** and select **XIAO_ESP32S3**.

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=MmUxY2UyZjZlYjBjNDM2MzM5MWRiYTZkZjgzMWVjNTNfNzJjNmEyM2E0ZDllM2FiZGI4NTUxMjZjZGMwZjcwMjVfSUQ6NzY0NTg5MzkzNjM3MTk3NzQwNF8xNzgwMjIxMTU1OjE3ODAzMDc1NTVfVjM)

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

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=MzlkMjAxMzlkNmM1OWFiOWFmZTYzNWUzZDQyM2QxZTVfOGYxMDk3MmFjMGUzMWVhMjhjOTE3MzNiNDdlMDhkODNfSUQ6NzY0NTg5MzkzNTI3MzIwMDg0NV8xNzgwMjIxMTU1OjE3ODAzMDc1NTVfVjM)

- **Step 3.** Select **XIAO ESP32-S3** as the board model, choose the correct port, and upload the program.

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=YmVkOTJmNmEwMTIxNWU3NjY4MDFlZDY1MTgzMzUyY2VfMmNkNjc0ZGE3ZTljNjYxYTNjMDhjMWI3MTViNTdmMGRfSUQ6NzY0NTg5MzkzODY5NTQwODg2MF8xNzgwMjIxMTU1OjE3ODAzMDc1NTVfVjM)

After the program is uploaded successfully, you will see the following output, and the orange LED on the right side of XIAO ESP32-S3 will blink.

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=NDQ1MTdhMTZjMTQ5Y2RhYTY4ZDVmYmQwZmQ0MGVlZDRfYmJhNThmYjAwZWJjYTRkNDk1MWM0YmZlNWVkODQyZGNfSUQ6NzY0NTg5MzkzOTQyMTAzOTc5NV8xNzgwMjIxMTU1OjE3ODAzMDc1NTVfVjM)

##### 1.5 Dependency Library Installation

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=YzdiNzQ0MDU5YjZiNDg5NjQ1NTQ3ZGU0MTI5ZWQwMWJfZWVjMzAwODMwYmU3YmNhZjA4NjhmMGEzMDQzNDFmNWRfSUQ6NzY0NTg5MzkzNTMwMjU0NDU4MF8xNzgwMjIxMTU1OjE3ODAzMDc1NTVfVjM)

#### 2. Flight-Control Firmware Architecture

##### 2.1 File Structure Overview

The firmware in this experiment is developed from the Flix architecture and adds serial optical-flow ToF, altitude hold, position hold, MAVLink UDP, serial CLI, and NVS parameter persistence. The following table lists the current source files and their responsibilities:

| File | Responsibility | Details |
|---|---|---|
| `proj_op32drone.ino` | Main entry point | Defines `WIFI_ENABLED` and `OPTICAL_FLOW_ENABLED`, declares global pose and optical-flow variables, initializes parameters, LED, motors, Wi-Fi, IMU, RC, and optical flow in `setup()`, and runs estimation and control at different rates in `loop()` |
| `control.ino` | Flight control | Contains STAB, ALT_HOLD, POS_HOLD, ACRO, and AUTO modes; attitude loop, rate loop, altitude loop, optical-flow position and velocity loops; supports ground takeoff gating and optical-flow failsafe |
| `estimate.ino` | State estimation | KalmanAngle attitude estimation; ToF and inertial fused altitude; serial optical-flow horizontal velocity estimation; ground zero-velocity lock, optical-flow gating, and raw optical-flow integration debug values |
| `flow.ino` | Optical-flow ToF driver | Uses `Serial1`; GPIO8 connects to optical-flow TX and GPIO7 connects to optical-flow RX; parses 19-byte packets with 0xDF frame header; valid height is 0.3-6.0 m; 120 ms timeout marks data invalid |
| `imu.ino` | IMU driver | MPU9250 over I2C, SDA=GPIO2, SCL=GPIO43, 400 kHz; gyro static self-learning; six-face accelerometer calibration |
| `rc.ino` | RC input | SBUS over `Serial2`, RX=GPIO44, TX=GPIO1; default Roll/Pitch/Throttle/Yaw/Mode channels are 0/1/2/3/6 |
| `motors.ino` | Motor output | Four LEDC PWM outputs, 10 kHz, 10-bit; GPIO4/3/6/5 correspond to rear-left/rear-right/front-right/front-left |
| `wifi.ino` | Wi-Fi and UDP | Prefer saved STA network, then try the built-in 2.4 GHz network; fall back to AP `flix/flixwifi` on failure; UDP 14550 is used for MAVLink |
| `mavlink.ino` | MAVLink communication | Sends HEARTBEAT, ATTITUDE_QUATERNION, RC_CHANNELS_RAW, ACTUATOR_CONTROL_TARGET, and SCALED_IMU; supports MANUAL_CONTROL, parameter read/write, arming, mode switching, and log reading |
| `cli.ino` | Serial command line | 115200 bps, compatible with UART0 and ESP32-S3 USB Serial/JTAG; supports commands for parameters, attitude, pose, Wi-Fi, logs, motors, calibration, reboot, and more |
| `parameters.ino` | Parameter storage | Uses ESP32 NVS namespace `flix`; reads existing parameters at boot and writes changes at low frequency to avoid filling NVS during startup |
| `log.ino` | Flight log | RAM circular log recording attitude, targets, position, velocity, raw optical-flow integration, optical-flow velocity, and thrust |
| `safety.ino` | Safety protection | Sets thrust to zero and switches to AUTO after RC timeout |
| `led.ino` / `time.ino` | Helper modules | LED status indication, `dt`, and `loopRate` statistics |
| `pid.h` / `kalman_angle.h` / `lpf.h` / `quaternion.h` / `vector.h` / `util.h` | Math and utilities | PID, two-state Kalman filter, first-order low-pass filter, quaternion, vector, Rate/Delay, and utility functions |

##### 2.2 Hardware Pin Mapping

The following pins are directly tied to the baseboard hardware. Confirm the baseboard schematic before making changes:

| Peripheral | GPIO | Description |
|---|---|---|
| I2C SDA | 2 | MPU9250 data line, 400 kHz; BMP280 on GY-91 is not used by the current firmware |
| I2C SCL | 43 | MPU9250 clock line |
| Optical-flow RX | 8 | Serial1 RX, connected to optical-flow module TX. Protocol: frame header 0xDF, 19-byte packet, 115200 bps |
| Optical-flow TX | 7 | Serial1 TX, connected to optical-flow module RX |
| SBUS RX | 44 | Serial2 RX, 100 Kbps, 25-byte frame |
| SBUS TX | 1 | Serial2 TX |
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

The main loop is split into multi-rate functional scheduling. `step()` updates `dt` and `loopRate`, and the control system runs layered loops through `Rate` timers:

```cpp
void loop() {
    static Rate pilotRate(80);
    static Rate positionRate(40);
    static Rate attitudeRate(150);
    static Rate innerRate(400);

    readIMU();
    step();
    readRC();
    readOpticalFlow();
    estimate();
    if (pilotRate) controlPilotLoop();
    if (positionRate) controlPositionLoop();
    if (attitudeRate) controlAttitudeLoop();
    if (innerRate) controlRateTorqueLoop();
    sendMotors();
    handleInput();
    processMavlink();
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

- Height: 0.3 m to 6 m

- Integration time: 5000 us to 100000 us

- Health timeout: no valid data for 120 ms -> unhealthy

##### 3.2 Attitude Estimation (`kalman_angle.h` + `estimate.ino`)

**Kalman filter**

- **Type**: two-state filter, Angle + Gyro Bias

- **Input**: accelerometer angle as measurement, calculated with `atan2`, plus gyro angular velocity as prediction

- **Output**: filtered angle, unbiased angular rate, and real-time bias

- **Default parameters**: Q_angle=0.001, Q_bias=0.003, R_measure=0.03

- **Instances**: one KalmanAngle for Roll and one for Pitch; Yaw is pure integration

**Altitude estimation**

- When ToF is valid, `position.z` converges toward `opticalFlowHeight` measured by the optical-flow module.

- When ToF is temporarily unavailable, vertical acceleration in the world frame is integrated for a short time, with damping applied to velocity.

- When low-altitude motor noise is strong, the accelerometer influence on altitude and attitude is reduced to prevent ground-effect vibration from biasing the attitude before takeoff.

- When `position.z < 0`, it is forced to zero to avoid altitude drifting below the ground.

**Horizontal velocity and position**

- Raw optical-flow velocity is first converted according to height and then gyro-compensated to obtain `flowCompBodyVel`.

- `raw_flow pos_xy` is raw optical-flow velocity integration, used for debugging sensor direction and scale.

- The control variables `velocity.x/y` and `position.x/y` are updated only after `flowAirborne=true`; before takeoff, ground zero-velocity locking is enabled to prevent floor texture noise from drifting the position-hold target.

- Takeoff detection mainly depends on `armed`, `controlThrottle > FLOW_ARM_T`, and `position.z > FLOW_ARM_Z`; defaults are `FLOW_ARM_T=0.12` and `FLOW_ARM_Z=0.22`.

##### 3.3 Flight Control (`control.ino`)

**Five flight modes**

| Mode | Value | Behavior |
|---|---|---|
| STAB | 2 | Default mode. Direct throttle + self-leveling attitude stabilization. RC sticks map to angle commands. |
| ALT_HOLD | 4 | Altitude-hold mode. Around mid-throttle, the current height is held; pushing or pulling throttle maps to climb or descent velocity commands. Defaults: `ALT_HOVER=0.45`, `ALT_P=1.00`, `ALT_V_P=0.50` |
| POS_HOLD | 5 | Position-hold mode. When optical flow is healthy, the drone is airborne, height is sufficient, and attitude is not excessively tilted, position gating is enabled. Releasing the stick locks the current position; moving the stick takes over and refreshes the target |
| ACRO | 1 | Rate-control mode. Sticks map directly to rate targets |
| AUTO | 3 | MAVLink external-control mode. Accepts SET_ATTITUDE_TARGET or SET_ACTUATOR_CONTROL_TARGET |

**Mode switching with RC channel 6**

```cpp
ch6 < 33%  -> STAB
ch6 33~66% -> ALT_HOLD
ch6 > 66%  -> POS_HOLD
```

**Arming and disarming**

```cpp
Arm: throttle lowest + yaw full right
Disarm: throttle lowest + yaw full left
```

**Main altitude-hold and position-hold parameters**

| Parameter | Default | Description |
|---|---:|---|
| `ALT_HOVER` | 0.45 | Hover-throttle baseline; altitude-hold stability depends on this first |
| `ALT_P` / `ALT_I` / `ALT_V_P` | 1.00 / 0.010 / 0.50 | Altitude error to velocity target, integral compensation, and vertical velocity damping |
| `POS_P` / `POS_V_MAX` | 0.25 / 0.12 | Position error to horizontal velocity target and maximum horizontal velocity limit |
| `HOLD_P_X/Y` | 0.015 / 0.014 | Proportional term of the optical-flow velocity loop |
| `HOLD_I_X/Y` | 0.006 / 0.006 | Integral term of the velocity loop |
| `HOLD_D_X/Y` | 0.0010 / 0.0010 | Differential braking term of the velocity loop |
| `HOLD_A_MAX` | 0.04 rad | Maximum attitude output for position-hold control |
| `POS_MIN_Z` | 0.22 m | Minimum height for position-hold gating |

##### 3.4 MAVLink Debugging (`mavlink.ino`)

**Transmitted messages**

| Message | Frequency | Description |
|---|---|---|
| HEARTBEAT (#0) | 1 Hz | type=QUADROTOR, autopilot=GENERIC, base_mode=armed/disarmed |
| EXTENDED_SYS_STATE | 1 Hz | Reports landed / in-air state |
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
| MAV_CMD_DO_SET_MODE | Switches `RAW/ACRO/STAB/AUTO/ALT_HOLD/POS_HOLD` |
| SET_ATTITUDE_TARGET | Receives attitude, rate, and thrust targets in AUTO mode |
| SET_ACTUATOR_CONTROL_TARGET | Receives direct motor-control values in AUTO mode |
| SERIAL_CONTROL | Passes CLI commands through MAVLink |

##### 3.5 ROS 2 / MAVROS Integration

The current `proj_op32drone` firmware only handles MAVLink v2 UDP transmission and reception. It does not contain a built-in ROS 2 node. ROS 2 integration must be done on the host computer through MAVROS or a custom MAVLink program connected to the flight controller's UDP 14550. By default, the flight controller first tries to connect to the saved 2.4 GHz Wi-Fi. If connection fails, it falls back to AP `flix/flixwifi`; in AP mode, the controller address is `192.168.4.1`. If the controller is connected to a router in STA mode, use the `STA IP` printed by the serial `wifi` command.

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

Enabled MAVROS plugins: sys_status / command / param / manual_control / imu

`command_long` and `rc_io` are commented out. `command_long` is used for complex commands such as waypoints or camera control, and `rc_io` is used for RC override. Uncomment them if these features are needed.

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
| ALT_HOLD | 4.0 | `ros2 service call /osdrone/command mavros_msgs/srv/CommandLong "{command:176,param1:1.0,param2:4.0}"` |
| POS_HOLD | 5.0 | `ros2 service call /osdrone/command mavros_msgs/srv/CommandLong "{command:176,param1:1.0,param2:5.0}"` |

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

- Throttle `z` ranges from [0, 1000], corresponding to 0-100% thrust in the controller. Current `ALT_HOVER=0.45`, roughly corresponding to z=450.

- Pitch/Roll [-1000, 1000] maps to the maximum tilt angle `CTL_TILT_MAX`, about 25 deg by default; yaw maps to `CTL_Y_RATE_MAX`.

- After switching modes, wait for heartbeat confirmation. QGC or `ros2 topic echo /osdrone/state` can show the current mode.

- Make sure throttle=0 before disarming, otherwise the controller will not respond to the disarm command.

##### 3.7 CLI Debug Commands

USB serial runs at 115200 bps. The current `cli.ino` is compatible with both UART0 and ESP32-S3 USB Serial/JTAG, and provides the following commonly used commands:

| Command | Output | Typical Use |
|---|---|---|
| `help` | All commands | View CLI supported by the current firmware |
| `p` / `p <name>` / `p <name> <value>` | Parameter list, single parameter, write parameter | Tune PID, optical-flow scale, hover throttle; when motors are not running, `syncParameters()` saves changes to NVS |
| `ps` / `psq` | Euler angles / quaternion | Quickly confirm attitude direction |
| `pose` / `pose <hz>` / `poseoff` | Attitude, position, velocity, optical-flow gating, raw optical-flow integration | Overview of whether the pose is trustworthy |
| `flowraw` / `flowraw <hz>` | Optical-flow measured velocity, gyro-compensated velocity, compensated velocity, bias, innovation | Check optical-flow direction, scale, and gyro compensation |
| `flowctrl` / `flowctrl <hz>` | `stationary/air/zero/use/gate/reject`, control position/velocity, altitude target | Determine why POS_HOLD gating does not open or why XY does not update |
| `rate` / `rate <hz>` | Angular rate, rate target, torque output, PID parameters | Tune attitude and rate loops |
| `mot` / `mot <hz>` | Four motor outputs, left-right/front-back differential, thrust target, torque target | Check mixer direction, motor direction, and attitude correction direction |
| `diag` / `diag <hz>` | Acceleration norm, vibration, accelerometer trust, attitude observation angle, gyro status | Diagnose IMU vibration, takeoff ground effect, and estimator health |
| `monoff` / `flowoff` / `rateoff` / `motoff` / `diagoff` | Stop rolling monitor | Prevent serial flooding |
| `monpause` / `monresume` | Pause/resume current rolling monitor | Temporarily stop output while tuning |
| `imu` | IMU status, calibration values, raw accelerometer and gyro | Check sensor and six-face calibration |
| `ca` / `cr` | Six-face accelerometer calibration / SBUS RC calibration | Use during first assembly or after rebuilding |
| `wifi` / `wifiscan` | Wi-Fi/AP/STA/MAVLink status and nearby networks | Troubleshoot connection problems |
| `wifi <ssid> <password>` / `wifi reset` | Save network / clear saved network | Switch router or password |
| `arm` / `disarm` | Arm/disarm through serial | Use for propeller-removed debugging |
| `raw` / `stab` / `acro` / `auto` / `alt` / `pos` | Manually switch mode | Quickly switch modes during indoor debugging |
| `mfr` / `mfl` / `mrr` / `mrl` | Single-motor test | Must remove propellers; used to confirm motor index |
| `log` / `log dump` | RAM log header / CSV data | Post-flight analysis of attitude, velocity, position, and optical flow |
| `sys` / `reboot` | System information / reboot | Check heap, chip temperature, or soft reboot |

#### 4. Tuning Recommendations

Tune only one class of parameters at a time, and record logs before and after each change.

##### 4.1 How to Use Debug Output

| Stage | Recommended Command | Key Fields | Judgment Criteria |
|---|---|---|---|
| Static after power-on | `diag 5` | `accNorm`, `vibe`, `trust`, `bias` | When static, `accNorm` should be close to 9.8, `vibe` should not be large, and `trust` should not stay near 0 for long |
| Handheld tilt | `pose 5` | `att_deg r/p/y` | Roll direction when tilted right and Pitch direction when tilted forward should match the body definition |
| Handheld translation | `flowraw 10` | `body_vel`, `comp`, `pos_xy` | When moving forward/right, optical-flow direction should match actual motion |
| Armed low throttle | `mot 10` | `FL/FR/RL/RR`, `LRdiff`, `FBdiff` | Motor differential direction should be correct during attitude correction, with no abnormal saturation on any output |
| Low-altitude STAB | `rate 10` | `cur/tgt/tq` | Angular rate should follow target and torque output should not shake excessively |
| ALT_HOLD | `pose 10`, `flowctrl 10` | `pos.z`, `vel.z`, `targetZ`, `vzT` | Height should converge around the target and not continuously drift up or sink |
| POS_HOLD | `flowctrl 10`, `flowraw 10` | `air/use/gate/rej`, `position.x/y` | XY enters the control loop only after `air=1`, `use=1`, and `gate=1` |
| Post-flight analysis | `log dump` | `position`, `velocity`, `rawFlowPos`, `flowCompVel`, `thrustTarget` | Save output as CSV and inspect trends with a spreadsheet or plotting tool |

##### 4.2 Basic Attitude Loop

1. Keep the drone in `STAB` mode and first confirm that it does not spin or keep tipping in one direction.
2. If high-frequency oscillation appears, first reduce `CTL_R_RATE_D` / `CTL_P_RATE_D` or check motor, propeller, and frame vibration.
3. If attitude response is slow, slightly increase `CTL_R_RATE_P` / `CTL_P_RATE_P`.
4. If the attitude can self-level but has slow bias, then consider slightly increasing Rate I. Do not tune integral first.

##### 4.3 Altitude Hold

For altitude hold, tune `ALT_HOVER` first. If hover throttle is inaccurate, `ALT_P` and `ALT_V_P` will be difficult to tune well.

| Symptom | First Adjustment |
|---|---|
| Slowly sinks after switching to `ALT_HOLD` | Increase `ALT_HOVER` |
| Slowly climbs after switching to `ALT_HOLD` | Decrease `ALT_HOVER` |
| Height oscillates up and down | Reduce `ALT_P`, or slightly increase `ALT_V_P` |
| Height response is sluggish | Slightly increase `ALT_P`, after confirming ToF height is stable |

##### 4.4 Optical-Flow Direction and Position Hold

The current control XY is not raw pixels. It is relative position estimation after height scaling, gyro compensation, bias subtraction, and gating. When tuning optical flow, distinguish three groups of values:

| Field | Meaning |
|---|---|
| `flowraw meas` | Raw velocity converted from the optical-flow module |
| `flowraw comp` | Velocity after subtracting body-rotation effects |
| `flowraw pos_xy` | Raw optical-flow integration, only for checking direction and scale |
| `flowctrl pos/vel` | XY position/velocity used by the controller |
| `flowctrl air/use/gate` | Whether the drone is airborne, whether optical flow is used, and whether POS_HOLD gating is open |

Calibration order:

1. Remove propellers, hold the drone by hand, and run `flowraw 10`.
2. If `pos_xy` direction is reversed, first check the optical-flow module mounting direction, then adjust the sign of `FLOW_SCL_X/Y`.
3. If `comp` drifts obviously while pitching/rolling in place by hand, fine-tune `FLOW_K_PIT` / `FLOW_K_ROL`.
4. After STAB is stable, briefly switch to `POS_HOLD` at low altitude and run `flowctrl 10`.
5. Only after `air=1`, `use=1`, and `gate=1`, `position.x/y` becomes the XY used for position-hold control.
6. If position hold diverges laterally, switch back to `STAB` first. Do not keep increasing `HOLD_P_X/Y` while it is diverging.

##### 4.5 RAM Log

`log dump` outputs CSV-format data. Current log columns include:

| Category | Fields |
|---|---|
| Time | `t` |
| Angular rate | `rates.x/y/z`, `ratesTarget.x/y/z` |
| Attitude | `attitude.x/y/z`, `attitudeTarget.x/y/z` |
| Control position | `position.x/y/z` |
| Control velocity | `velocity.x/y/z` |
| Raw optical flow | `rawFlowPos.x/y`, `rawBodyVel.x/y` |
| Compensated optical flow | `flowCompVel.x/y` |
| Thrust | `thrustTarget` |

It is recommended to save the `log dump` output as `.csv` and use a spreadsheet, Python, or Matlab to plot `position.z`, `velocity.z`, `position.x/y`, `rawFlowPos.x/y`, and `flowCompVel.x/y`. This is usually more useful than only watching flight video, because it helps identify whether the issue is in the altitude loop, optical-flow direction, velocity loop, or gating logic.

#### 5. Troubleshooting Table

| Symptom | Possible Cause | Solution |
|---|---|---|
| Flips immediately after takeoff | Propeller direction is wrong | Check X layout: diagonal motors spin in the same direction, adjacent motors spin in opposite directions |
| STAB is stable but XY drifts | Not in POS_HOLD or optical-flow gating is not open | Use serial `flowctrl 10` to check `air/use/gate/rej`; confirm throttle, altitude, and optical-flow health |
| Position hold corrects in the wrong direction | Optical-flow X/Y direction, scale, or gyro-compensation direction is wrong | Handheld test with `flowraw 10`; adjust `FLOW_SCL_X/Y` and `FLOW_K_PIT/ROL` if needed |
| Large altitude fluctuation | `ALT_HOVER` is inaccurate, ToF data jitters, or floor reflection is poor | Tune `ALT_HOVER` first, then fine-tune `ALT_P` and `ALT_V_P` |
| Hover oscillates | Position/velocity loop gains are too high | Reduce `HOLD_P_X/Y` or `HOLD_A_MAX` |
| Motor output is obviously biased to one side | Mixer direction, motor order, or attitude-estimation direction is wrong | Remove propellers, run `mot 10`, gently tilt the body, and confirm correction direction |
| Attitude loop oscillates | Frame vibration, excessive D term, or reduced IMU trust | Run `rate 10` and `diag 10`, and check `tq`, `vibe`, and `trust` |
| Arming fails | RC signal is abnormal or throttle is not at zero | Check SBUS wiring and use serial `rc` to inspect channel values |
| Wi-Fi cannot connect | Old NVS credentials, 5 GHz/incompatible encryption, or weak signal | Run `wifi reset` and `wifiscan`; confirm 2.4 GHz SSID and WPA2/WPA2-WPA3 mixed mode |
| MAVROS cannot connect | Flight-controller IP or UDP port is wrong | Use 192.168.4.1 in AP mode; in STA mode use the `STA IP` printed by serial `wifi`; port is 14550 |

## 3. Additional Content

### Reference Links

- Original Flix project: [github.com/okalachev/flix](https://github.com/okalachev/flix)

- MAVLink protocol documentation: [mavlink.io](https://mavlink.io)

- QGroundControl: [qgroundcontrol.com](https://qgroundcontrol.com)

- ESP32-S3 technical manual: [espressif.com](https://www.espressif.com)

- PX4 development guide for PID tuning: [docs.px4.io](https://docs.px4.io)

- Crazyflie technical documentation, Lee controller reference: [bitcraze.io](https://www.bitcraze.io)
