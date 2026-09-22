# Open32Drone Complete Tutorial

[English](tutorial.md) · [简体中文](tutorial_zh_CN.md)

## Contents

- [01 · Project overview](#chapter-01)
- [02 · What you will build](#chapter-02)
- [03 · Building the aircraft](#chapter-03)
- [04 · Firmware, calibration and first flight](#chapter-04)
- [05 · Flight tuning](#chapter-05)
- [06 · ROS 2 control](#chapter-06)
- [07 · Reinforcement learning](#chapter-07)

---

<a id="chapter-01"></a>

## 01 · Project overview

<a id="chapter-01-section-1"></a>

### Start here

For a first build, follow the stages below in order. If you already have an aircraft, go straight to the part you need.

| What you want to do | Where to start |
|---|---|
| Buy parts, print the frame, order the PCB and assemble the aircraft | [Shopping list](#chapter-03-purchasing) · [Assembly](#chapter-03) |
| Download the firmware and APK, flash, calibrate and make a first flight | [Firmware, calibration and first flight](#chapter-04) |
| Improve flight behavior or report a problem | [Tuning and troubleshooting](#chapter-05) · [Reporting a problem](docs/project/contributing.md) |
| Connect ROS 2, read data and program movements | [ROS 2 control](#chapter-06) |
| Build or modify the software | [Source and build](docs/reference/source-build.md) · [Parameters and interfaces](docs/reference/firmware.md) |
| Try simulation and reinforcement learning | [Numerical exercises and simulation](#chapter-07) |

If you do not have a physical transmitter, use the Android option in the first-flight chapter.

<a id="chapter-01-section-2"></a>

### A small quadrotor you can build

Open32Drone is an open-source quadrotor project with modular electronics. The Open32Drone PCB handles power, four brushed-motor drivers and module connections. The XIAO ESP32-S3, IMU and optical-flow/ToF sensor are separate modules fitted to the aircraft.

The reference aircraft uses 8520 coreless motors and a 1S battery for indoor flight. An ESP32-S3 runs the flight controller at 300 Hz. The IMU measures attitude, while a downward-facing optical-flow/ToF module supports horizontal position hold and height control.

The original flight-control core comes from Oleg Kalachev's Flix. Open32Drone adds the board pin mapping, four brushed-motor outputs, optical-flow/ToF height and position control, battery-voltage compensation, automatic takeoff and landing, parameter storage, and Android and ROS 2 interfaces.

<a id="chapter-01-section-3"></a>

### Main components

The system can be divided into four groups.

| Group | Main components | Purpose |
| --- | --- | --- |
| Structure and propulsion | Printed frame, four 8520 motors, four 60/65 mm propellers, rubber motor grommets | Support the parts and produce lift and control torques |
| Flight control and sensing | Open32Drone PCB, XIAO ESP32-S3, IMU, optical-flow/ToF module | Drive the motors, estimate aircraft state and calculate control outputs |
| Power and communication | 1S battery, SBUS receiver, Wi-Fi | Supply power and receive transmitter or program commands |
| Ground software and simulation | Android APK, ROS 2, URDF/USD, Gazebo, Isaac Sim, PPO examples | Manual flight, robot programming, model checks and learning-based control |

<a id="chapter-01-section-4"></a>

### How data moves through the aircraft

The IMU measures angular velocity and acceleration, ToF measures height above the ground, and optical flow measures motion relative to the ground. The flight controller combines these readings into attitude, velocity and position estimates, then calculates four motor outputs from the pilot's or ROS program's targets.

```text
Sensor readings → State estimates → Attitude/height/position control → Motor mixer → Aircraft motion
       ↑                                                                                  │
       └────────────────────── New readings in the next cycle ────────────────────────────┘
```

<a id="chapter-01-section-5"></a>

### Reference aircraft

The dimensions, weight and parameters in this tutorial refer to the following build:

- Frame outline: approximately 103.3 × 103.3 mm.
- Four 8520 motors: 8 × 20 mm bodies with 1 mm shafts.
- Four 60 mm propellers: two CW and two CCW.
- 18350 1S 1300 mAh battery, measured at 25 g.
- Takeoff weight including the battery: approximately 81 g, with the horizontal center of gravity near the middle of the aircraft.
- XIAO ESP32-S3, MPU6500/MPU9250 IMU and optical-flow/ToF module.
- Three control options: physical SBUS, Android APK and ROS 2.

<a id="chapter-01-section-6"></a>

### Repository layout

| Directory | Contents |
| --- | --- |
| `software/hardware/` | Frame 3MF/STEP files, mechanical specifications and purchasing information |
| `software/firmware/` | ESP32-S3 flight-controller source |
| `software/android/` | Phone controller source |
| `software/ros2/` | ROS 2 driver, control commands and RViz configuration |
| `software/simulation/` | Teaching exercises, dynamics, Gazebo/Isaac and reinforcement-learning code |
| `software/releases/minimal/` | Matching complete firmware, OTA image, APK and ROS package |
| `docs/` | Installation, parameters, troubleshooting and project guides |

These directories contain the files and code needed for the build. The following sections describe the firmware's current features; Chapter 2 covers the build sequence.

<a id="chapter-01-section-7"></a>

### Flight features and sensors

The table below lists the main firmware features. Names such as `time` and `imu` are serial commands you can use to check the aircraft after flashing.

| Feature | Support | How to check or use it |
|---|---|---|
| Fixed 300 Hz flight-control loop | Enabled | `time`, `perf` |
| Stabilize / Altitude Hold / Position Hold | Enabled | SBUS mode switch, Android/ROS commands |
| Relative-height takeoff and automatic landing | Enabled | Android/ROS commands or assisted SBUS takeoff |
| Default MPU6500/MPU9250 IMU | Enabled | Backend selected at build time, `imu` |
| ICM20948 and MPU6050 backends | Build options | Require separate builds and checks on the corresponding hardware |
| TF-0850 optical flow and ToF | Enabled | `flow`, MAVLink telemetry |
| Battery-voltage sensing and thrust compensation | Enabled, with a compensation limit | `pw`, MAVLink battery telemetry |
| Physical SBUS emergency disarm | Enabled | Independent of Android/ROS control ownership |
| Motor stop after sustained tipping | Enabled | Uses tilt angle and duration; it cannot detect every type of collision |

<a id="chapter-01-section-8"></a>

### Network and maintenance

| Feature | Support | Notes |
|---|---|---|
| Aircraft Wi-Fi AP | Enabled by default | `ap <ssid> <pass>`; aircraft address fixed at `192.168.4.1` |
| Router Wi-Fi STA | Enabled | `sta <ssid> <pass>`; the router assigns an address through DHCP |
| STA startup recovery | Enabled | Opens the saved AP if it cannot connect to the router within 8 seconds; saved STA settings are retained |
| MAVLink commands and telemetry | Enabled | UDP `14550`; one Android or ROS controller per aircraft at a time |
| ROS 2 multi-aircraft control | Enabled | Each aircraft needs a unique namespace, System ID, local UDP port, IP address and TF prefix |
| Standard MAVLink parameter protocol | Enabled | `PARAM_REQUEST_LIST`, `PARAM_REQUEST_READ`, `PARAM_SET`, `PARAM_VALUE` |
| MAVLink diagnostic-text output | Enabled, outbound only | `SERIAL_CONTROL_DEV_SHELL`; remote CLI input is not accepted |
| Local USB serial CLI | Enabled | Parameters, calibration, diagnostics, motor tests and network setup |
| In-memory flight log | Enabled | 25 Hz, approximately 12 seconds; downloadable over serial/MAVLink |
| Sampled loop profiler | Enabled | Samples once every 16 loops; use `perf` while disarmed |
| Ground-only A/B OTA | Enabled | HTTP `8080`; app images can be used after the first complete USB installation |
| Background MJPEG video | Experimental; hardware and flight checks are still needed | HTTP `/stream`, one viewer at a time; image processing runs outside the 300 Hz flight-control loop |

Android and ROS both communicate with the aircraft through MAVLink. Firmware replies go to the most recent valid UDP sender, so close the other controller before taking control of the same aircraft. Video also supports only one viewer at a time: close the Android preview before opening the stream in OpenCV.

<a id="chapter-01-section-9"></a>

### Which settings survive a power cycle

NVS is the chip's nonvolatile parameter storage. The following operations write settings to it so they are retained after power is removed:

- Serial `p <name> <value>` or ground-only MAVLink `PARAM_SET`.
- `ca` accelerometer calibration.
- `cr` SBUS calibration.
- `ap` or `sta` network setup.

Startup gyro bias, optical-flow ground bias, controller integrals, flight targets and voltage compensation apply only to the current run. They do not overwrite saved parameters. `preset` resets registered parameters but keeps the AP/STA network names and passwords. A complete Flash erase removes both parameters and network settings.

<a id="chapter-01-section-10"></a>

---

<a id="chapter-02"></a>

## 02 · What you will build

For a first build, aim for a stable takeoff, hover and landing. Once you know the aircraft, try changing parameters, programming movements in ROS, and then simulation and reinforcement learning. You can work through these parts separately; there is no need to finish everything at once.

<a id="chapter-02-section-1"></a>

### What you can do with the project

<a id="chapter-02-section-2"></a>

#### Build a working aircraft

Print the frame and order bare PCBs using the matching production files. Follow the BOM and placement drawings to solder the components, connectors, power module and IMU. Fit the optical-flow/ToF module, XIAO, motors and battery, then label the nose, motor numbers and rotation directions. Photograph the wiring and mounting positions for later troubleshooting.

<a id="chapter-02-section-3"></a>

#### Install and understand the firmware

Chapter 4 explains which files to use for the first complete USB flash and later OTA updates. After flashing, calibrate the IMU, transmitter and battery voltage, then test all four motors with the propellers removed. Choose a first-flight method to suit your equipment:

- With an SBUS receiver and transmitter, use the physical sticks and three-position mode switch.
- Without a receiver, connect your phone to the aircraft hotspot and use the matching Android APK for automatic takeoff and landing.

<a id="chapter-02-section-4"></a>

#### Tune from what you observe

First identify the symptom: fast vibration, slow rocking, height oscillation or horizontal drift. Chapter 5 explains what to check for each one. Change one parameter at a time, repeat the same short flight and compare the result.

<a id="chapter-02-section-5"></a>

#### Control movement with ROS

After connecting ROS 2, check the IMU, range, battery and odometry data, then try one takeoff and landing. You can then send velocity and position targets, combine forward motion, sideways motion and turns into your own flight sequence, and record the data with rosbag.

<a id="chapter-02-section-6"></a>

#### Try simulation and reinforcement learning

Chapter 7 starts with links, joints, mass and collision shapes, then uses an approximate 81 g dynamics model for a CPU hover exercise and PPO training. The numerical exercises run directly. To watch the aircraft in Isaac, you need to prepare a compatible USD scene separately. Complete scenes and Gazebo flight integration are not bundled with the repository.

<a id="chapter-02-section-7"></a>

### Suggested order

```mermaid
flowchart TD
    A[3D-print the frame] --> B[Order the bare PCB]
    B --> C[Solder the PCB]
    C --> D[Assemble the aircraft]
    D --> E[Flash and calibrate over USB]
    E --> F{Choose a first-flight method}
    F -->|With SBUS| G[First position-hold flight with RC]
    F -->|Without a receiver| H[First position-hold flight with Android]
    G --> I[Tune based on flight behavior]
    H --> I
    I --> J[ROS takeoff, landing and velocity control]
    J --> K[ROS position control and waypoints]
    K --> L[URDF / USD and dynamics model]
    L --> M[PPO training and Isaac demos]
```

Start with the hardware chapters for your first build. If you already have a flying aircraft, you can begin with ROS. Before trying reinforcement learning, get familiar with coordinates, velocity and position targets, and practice a few simple ROS movements.

<a id="chapter-02-section-8"></a>

### Before you start

The hardware work requires basic soldering, multimeter and lithium-battery experience. For the software, you need to be able to change directories and run terminal commands. The ROS and reinforcement-learning chapters provide commands step by step; you do not need to know how to write complex nodes beforehand. Some Python, vector and PID knowledge will help explain what is happening.

For real flights, use an evenly lit indoor area with a textured floor and at least 2 m of clear space around the aircraft. Keep propellers off during soldering, flashing, calibration and motor tests. Fit them only after checking all four motor positions and rotation directions.

When you are ready, begin with frame printing and PCB fabrication in the next chapter.

---

<a id="chapter-03"></a>

## 03 · Building the aircraft

Print the frame and order the PCB first. Once the parts arrive, solder the board, then fit the controller, sensors, motors and battery. The sections below cover the files, parts and steps in that order. Leave the propellers off during assembly. Come back to fit them after flashing, calibration and motor-direction checks in the next chapter.

<a id="chapter-03-section-1"></a>

### 3.1 Frame and PCB

<a id="chapter-03-section-2"></a>

#### Print the frame

Bambu users can open the [MakerWorld frame page](https://makerworld.com.cn/zh/models/2922108-open32drone-wu-ren-ji-8520kong-xin-bei-ji-jia-ros2#profileId-3425842) and select “Open in Bambu Studio.” The profile specifies **0.2 mm layer height, 6 walls and 25% infill**. Select your printer and material before slicing. For other slicers, use the repository files below.

The recommended print project is `software/hardware/3d-model/open32drone-frame.3mf`. Import it at 100% scale; the main frame should measure approximately 103.3 × 103.3 mm. Check layer height, walls, supports and first-layer adhesion for your printer, nozzle and material. After printing, remove the supports and check that all four motor mounts are undistorted and the PCB holes line up without force.

Use `software/hardware/3d-model/open32drone-frame.stp` to edit the structure or inspect dimensions in another CAD program. After importing the STEP file, check the units against the approximately 103.3 mm frame outline rather than relying on the software's default units.

<a id="chapter-03-section-3"></a>

#### Order the PCB

Open the [JLC open-hardware PCB project](https://oshwhub.com/fanchewang/open32drone), open or clone the design in the editor, and check the hardware revision. If the project page does not show the electronic BOM, view and export it from the design.

Before ordering, prepare the Gerber and drill files, electronic BOM, front and back placement drawings, and interface and voltage information from the same revision. Set board thickness, copper thickness, surface finish and other manufacturing options according to the design requirements.

When the bare boards arrive, compare the outline, slots, through-holes, pads and silkscreen against the placement drawing before soldering. The photos below help identify parts and mounting directions; use the matching BOM and placement drawing for exact locations.

![Front and back of the controller PCB, showing silkscreen and connectors](img/pcb-bare-front-back.jpg)

Figure 3-1. Front and back of the Open32Drone PCB. The board handles power, motor drivers and module connections; the XIAO, IMU and optical-flow/ToF module are fitted separately.

<a id="chapter-03-section-4"></a>

### 3.2 Parts and tools

Frame files are listed under “Print the frame” above. Get the PCB design and onboard electronic BOM from the [JLC project](https://oshwhub.com/fanchewang/open32drone). Next, prepare the modules, mechanical parts and assembly tools.

<a id="chapter-03-purchasing"></a>

<a id="chapter-03-section-5"></a>

#### Shopping list

Quantities are **for one aircraft**; optional parts have a blank quantity. Use the specification column to choose a model when opening a listing, as sellers may offer packs of different sizes. See [assembly specifications](#chapter-03-section-7) for grommet dimensions and other mechanical details.

| No. | Part | Specification | Quantity per aircraft | Purchase link |
| :---: | --- | --- | :---: | --- |
| 1 | Flight-controller baseboard | Matching production files, electronic BOM and placement drawings | 1 | [JLC project](https://oshwhub.com/fanchewang/open32drone) |
| 2 | Printed frame | One complete frame set; approximately 103.3 × 103.3 mm, printed at 100% scale | 1 | [MakerWorld](https://makerworld.com.cn/zh/models/2922108-open32drone-wu-ren-ji-8520kong-xin-bei-ji-jia-ros2#profileId-3425842) |
| 3 | Controller | Seeed Studio XIAO ESP32-S3 Sense, including camera | 1 | [Listing](https://item.taobao.com/item.htm?id=796226570709) |
| 4 | IMU module | MPU9250; pin layout and mounting orientation must match the baseboard | 1 | [Listing](https://item.taobao.com/item.htm?id=867297908775) |
| 5 | Female header | 1 × 7 pins; pitch and height must match the board and controller | 2 | [Product option](https://item.taobao.com/item.htm?id=1040276180385&skuId=6058024109270) |
| 6 | Jumper cap | 2.54 mm pitch | 1 | [Listing](https://item.taobao.com/item.htm?id=1037786359471) |
| 7 | Boost module | Rated 5 V / 1 A output; input suitable for a 1S battery | 1 | [Product option](https://item.taobao.com/item.htm?id=1020492920926&skuId=6194359034311) |
| 8 | Optical-flow/ToF module | CORVON TF-0850; UART version, mounted facing down | 1 | [Listing](https://item.taobao.com/item.htm?id=825567548453) |
| 9 | Flow cable | 4-pin cable with reversed ends, 60 mm long; connectors and pinout must match the module | 1 | [Listing](https://item.taobao.com/item.htm?id=561435308484) |
| 10 | 8520 motor | 8 × 20 mm body, 1 mm shaft, MX1.25 connector, wire length ≥ 100 mm | 4 | Not yet provided |
| 11 | Propeller | 60 mm diameter; two CW and two CCW | 4 | [Listing](https://item.taobao.com/item.htm?id=651317554058) |
| 12 | Motor grommet | Ø8 × 2 mm; two black and two white recommended; [dimensions](#chapter-03-section-7) | 4 | [Listing](https://detail.tmall.com/item.htm?id=923643961535) |
| 13 | Mounting screw | 1 × 4 × 4 mm | 10 | [Listing](https://item.taobao.com/item.htm?id=658713209127&skuId=4755138613087) |
| 14 | Battery | 1S 18350; JST lead must match the baseboard connector and polarity | 1 | [Listing](https://item.taobao.com/item.htm?id=900687087724) |
| 15 | Battery retaining band | 25 mm diameter, 5 mm wide | 1 | [Listing](https://item.taobao.com/item.htm?id=583635067170) |
| 16 | SBUS receiver | Needed for a physical transmitter; must be compatible with it | | Optional |

Order onboard electronic components—resistors, capacitors, MOSFETs, diodes and connectors—from the electronic BOM for the matching [JLC design](https://oshwhub.com/fanchewang/open32drone).

<a id="chapter-03-section-6"></a>

#### Component photos

Click a photo to enlarge it. Models and quantities are listed in the shopping table above.

[![Controller](docs/public/media/purchasing/xiao-sense.webp)](docs/public/media/purchasing/xiao-sense.webp)

**Controller**

XIAO ESP32-S3 Sense · 1

[![IMU module](docs/public/media/purchasing/imu.webp)](docs/public/media/purchasing/imu.webp)

**IMU module**

MPU9250 · 1

[![Female headers](docs/public/media/purchasing/headers.webp)](docs/public/media/purchasing/headers.webp)

**Female headers**

1×7 pins · 2

[![Jumper cap](docs/public/media/purchasing/jumper.webp)](docs/public/media/purchasing/jumper.webp)

**Jumper cap**

2.54 mm · 1

[![Boost module](docs/public/media/purchasing/power-module.webp)](docs/public/media/purchasing/power-module.webp)

**Boost module**

5 V / 1 A · 1

[![Propellers](docs/public/media/purchasing/propellers.webp)](docs/public/media/purchasing/propellers.webp)

**Propellers**

60 mm · 4

[![Mounting screws](docs/public/media/purchasing/screws.webp)](docs/public/media/purchasing/screws.webp)

**Mounting screws**

1×4×4 mm · 10

[![Optical-flow/ToF module](docs/public/media/purchasing/flow-tof.webp)](docs/public/media/purchasing/flow-tof.webp)

**Optical-flow/ToF module**

CORVON optical flow and ranging · 1

[![Motor grommets](docs/public/media/purchasing/grommets.webp)](docs/public/media/purchasing/grommets.webp)

**Motor grommets**

Ø8×2 mm · 4, preferably two black and two white

[![Battery](docs/public/media/purchasing/battery.webp)](docs/public/media/purchasing/battery.webp)

**Battery**

18350 · 1, with a JST lead

[![Flow cable](docs/public/media/purchasing/flow-cable.webp)](docs/public/media/purchasing/flow-cable.webp)

**Flow cable**

4-pin, reversed ends, 60 mm · 1

[![Battery retaining band](docs/public/media/purchasing/battery-band.webp)](docs/public/media/purchasing/battery-band.webp)

**Battery retaining band**

25 mm diameter × 5 mm width · 1

<a id="chapter-03-section-7"></a>

#### Assembly specifications

- **Motor grommets:** Ø8×2 mm specification, 10 mm mounting hole, 2 mm groove height, 6 mm total thickness and 15 mm outer diameter; four required. Two black and two white are recommended, all with the same material and hardness.
- **Screws and propellers:** Ten 1×4×4 mm mounting screws and four 60 mm propellers, two CW and two CCW.
- **Flow cable:** Use the matching 4-pin, 60 mm cable. Match GND, power, TX and RX on the module and board; see [Mount the controller board](#chapter-03-section-18) for the pin assignments.
- **Battery connector:** Choose a JST plug and lead that match the baseboard. Check polarity with a multimeter before the first connection.

<a id="chapter-03-section-8"></a>

#### Tools

- **Soldering:** Temperature-controlled iron, solder, flux, fine tweezers and desoldering braid; add a temperature-controlled hot plate if using solder paste.
- **Inspection:** Multimeter and magnifier.
- **Assembly:** Suitable screwdriver, electronic scale and nonconductive work mat.

Set the soldering temperature according to the instructions for your solder or solder paste.

<a id="chapter-03-section-9"></a>

### 3.3 Soldering the board

<a id="chapter-03-section-10"></a>

#### 1. Sort the components

Separate resistors and capacitors, diodes, MOSFETs, connectors, headers and modules into small compartments. Work with one group at a time and mark each completed group on the placement drawing. For polarized parts, first locate pin 1, the cathode or the connector opening.

![PCB, connectors and modules laid out before assembly](img/parts-layout.jpg)

Figure 3-2. PCB, connectors, power module and IMU before soldering.

<a id="chapter-03-section-11"></a>

#### 2. Place the surface-mount components

Clean the pads, then apply solder paste evenly or pre-tin them. Fit low-profile, small parts first, followed by connectors and modules:

1. Resistors, capacitors and small-signal components.
2. MOSFETs, diodes and other components with a required orientation.
3. Motor connectors, power switch and other connectors.
4. Pin headers, female headers, power module and IMU.

Check each part from above to see that it is centered, then from the side to confirm both ends sit on the pads. Adjust misplaced parts before heating. If solder bridges form, remove them with flux and desoldering braid instead of repeatedly pushing neighboring parts with the iron.

![Placing the surface-mount components](img/smd-placement.jpg)

Figure 3-3. Surface-mount components in position. Use the nose arrow on the board as the direction reference throughout assembly.

<a id="chapter-03-section-12"></a>

#### 3. Complete the soldering

When using a hot plate, place the PCB flat on the surface and follow the solder's preheat, reflow and cooling instructions. Watch whether parts align as the solder melts. Let the board cool naturally before moving it. With an iron, secure one pin first, recheck orientation and position, then solder the remaining joints.

![Connectors and surface-mount components after soldering](img/connectors-soldered.jpg)

Figure 3-4. Board with connectors installed. Connector openings should face the direction in which their cables leave the board.

<a id="chapter-03-section-13"></a>

#### 4. Inspect the joints

Use a magnifier to inspect the power input, all four motor drivers, headers and connectors in turn. Solder should fully wet each pad and pin, with no bridges, dry joints, lifted pins or loose solder balls.

![Front of the soldered controller board](img/pcb-soldered.jpg)

Figure 3-5. Front of the board after soldering. Pay particular attention to the four motor outputs and the central component area.

With power disconnected, use a multimeter to check for a short between battery positive and negative, and check the connections on both sides of the power switch. For first power-up, use a current-limited supply or a protected 1S battery. Disconnect immediately if you notice unusual heat, smell or a rapid rise in current.

<a id="chapter-03-section-14"></a>

#### 5. Fit the headers and modules

Fit the power module on the back first, matching its input, output and GND to the board silkscreen. Then solder the XIAO female headers. Keep the two rows parallel so the XIAO slides in without force.

![Power module on the back of the board](img/power-board.jpg)

Figure 3-6. Position of the rear power module relative to the main board.

![Female headers and board connections](img/headers.jpg)

Figure 3-7. After soldering the headers, check their height and alignment from the side.

The IMU is a separate module fitted to the controller board. Follow the axis markings and mount it rigidly; thick, soft foam must not allow it to wobble. The standard firmware uses the IMU mounting rotation `roll=π`, `pitch=0`, `yaw=π/2`. The matching PCB and the orientation shown here correspond to that setting.

![IMU module silkscreen and pins](img/imu-module.jpg)

![IMU fitted to the controller board](img/imu-installed.jpg)

Figure 3-8. IMU module and completed controller board.

The board should now have its motor drivers, power components, XIAO headers and IMU installed. The optical-flow/ToF module connects by cable and will be mounted with the frame in the next step.

<a id="chapter-03-section-15"></a>

### 3.4 Assembling the aircraft

<a id="chapter-03-section-16"></a>

#### Orientation and motor numbers

With the nose pointing forward, look down from above:

```text
                            Nose / +X
                                ↑
              M3 front left              M2 front right

          +Y (left) ←       Body center       → -Y (right)

              M0 rear left               M1 rear right
                                ↓
                            Tail / -X
```

Firmware and simulation use the same motor numbering:

| Position | Motor | GPIO | Propeller-off test command | Simulation link |
| --- | --- | ---: | --- | --- |
| Rear left | M0 | 4 | `mrl` | `rotor_0_link` |
| Rear right | M1 | 3 | `mrr` | `rotor_1_link` |
| Front right | M2 | 6 | `mfr` | `rotor_2_link` |
| Front left | M3 | 5 | `mfl` | `rotor_3_link` |

<a id="chapter-03-section-17"></a>

#### Optical flow and ToF

Turn the frame upside down and place the optical-flow/ToF module in its front mounting position. The lens and ranging window must face the ground, clear of screws, tape and wiring. Keep the module parallel to the plane of the four motor thrust axes. Its standard position is about 24 mm forward of the aircraft's yaw center; the firmware compensates for this offset.

![Mounting position of the optical-flow/ToF module](img/flow-tof-install.jpg)

Figure 3-9. Optical-flow/ToF module mounted near the front of the frame, with its cable routed toward the center.

<a id="chapter-03-section-18"></a>

#### Mount the controller board

Turn the frame upright. Arrange the optical-flow/ToF cable, then place the board with its nose arrow pointing toward the frame's nose. Start all four mounting screws before gently tightening them in a diagonal pattern. Keep the board flat and make sure no wires are trapped underneath.

![Controller board fixed to the frame](img/mainboard-install.jpg)

Figure 3-10. Relative positions of the controller board, IMU and optical-flow/ToF module.

The optical-flow/ToF module uses UART: module TX connects to flight-controller RX (GPIO8), and module RX to flight-controller TX (GPIO7), at 115200 baud. The IMU uses I²C, with SDA on GPIO2 and SCL on GPIO43. Follow the PCB silkscreen when fitting the matching cables, and hold the plug itself when connecting or disconnecting it.

<a id="chapter-03-section-19"></a>

#### XIAO and receiver

Check for bent pins, then insert the XIAO ESP32-S3 vertically into the two header rows. Leave the USB-C port accessible from outside the frame. If using SBUS, secure the receiver in its mounting area and connect RX/TX and power. A receiver is optional when using only a phone or ROS.

![XIAO installed on the controller board](img/xiao-install.jpg)

Figure 3-11. XIAO inserted into the controller-board headers.

<a id="chapter-03-section-20"></a>

#### Grommets and motors

Press the four Ø8 mm motor grommets into the frame slots and check that each rim is fully seated. Insert the 8520 motors from the correct side, keeping all four at the same height with parallel shafts. Hold the motor case; do not press on the 1 mm shaft or pull the wires.

![Motor grommets fitted into the frame](img/motor-grommets.jpg)

![Side view of an 8520 motor and grommet](img/motor-install.jpg)

Figure 3-12. Grommets and motors. The grommets hold the motors and isolate some vibration.

Route each motor cable along its arm to the corresponding M0–M3 connector. Leave a little slack and keep all wiring outside the propeller discs. Leave the propellers off at this stage.

![All four motor cables connected to the board](img/motor-wiring.jpg)

Figure 3-13. Motor wiring after connection.

<a id="chapter-03-section-21"></a>

#### Secure the battery

The reference build uses an 18350 1300 mAh battery weighing 25 g. Secure it near the center of the aircraft so the center of gravity is close to the geometric center both front-to-back and side-to-side. Keep the power lead clear of the propellers and optical-flow/ToF windows. Weigh the aircraft with its battery, propellers and actual accessories fitted; the reference weight is about 81 g.

![Cylindrical battery mounted at the center](img/battery-install.jpg)

Figure 3-14. Cylindrical battery in the central mounting area. Keep it in the same position after each battery change.

If you add a camera or bracket, or switch to a pouch battery, reposition the battery to restore the horizontal balance. Follow the camera module's requirements for lens direction and ribbon-cable bend radius.

<a id="chapter-03-section-22"></a>

### 3.5 Checking motors and fitting propellers

Before applying power, check the voltage-sensing circuit: `VBAT_SW → 100 kΩ → GPIO1/A0 → 100 kΩ → GND`. The two resistors halve the battery voltage, so a 3.70 V battery should produce about 1.85 V at the ADC pin. Never connect the battery or 5 V directly to an ESP32-S3 GPIO.

On an older board without the voltage divider, set `PWR_VOLT_PIN` to `-1`. Use a multimeter to check for power shorts, correct supply voltages and ground connections before installing the controller module.

After flashing the firmware, run these serial commands in order:

```text
mrl
mrr
mfr
mfl
```

Each command spins one motor at low output for 1 second. Use a small strip of paper or phone slow-motion video to identify its direction as viewed from above. M0 and M2 should turn the same way; M1 and M3 should turn the opposite way. Put removable labels such as `M0 CW` and `M1 CCW` beside the grommets.

CW/CCW markings on a propeller identify its intended rotation direction. Fit CW propellers to motors measured as CW, and CCW propellers to those measured as CCW. Use the same diameter for all four. Seat the hubs fully without letting them rub against the motor cases.

![Reference propeller mounting positions](img/prop-install.jpg)

Figure 3-15. Propellers and motor positions. Use the labels from your propeller-off rotation tests to choose the final orientation.

Before fitting propellers, check once more that the board faces the right way, the IMU and optical-flow/ToF module are secure, all motor shafts are parallel, the battery is centered and all cables are clear of the propeller discs. Complete flashing and calibration without propellers in the next chapter, then return here to fit them.

![Completed Open32Drone reference aircraft](img/drone-complete.jpg)

Figure 3-16. Completed reference aircraft. The camera is optional; ordinary position hold uses the IMU and downward-facing optical-flow/ToF module.

---

<a id="chapter-04"></a>

## 04 · Firmware, calibration and first flight

After assembly, keep all four propellers off. This chapter installs the complete firmware on the XIAO ESP32-S3, calibrates the sensors and voltage reading, then guides you through a first position-hold takeoff using either an SBUS transmitter or an Android phone.

<a id="chapter-04-section-1"></a>

### 4.1 Release files

The three files you will use most often in `software/releases/minimal/` are:

| File | Use |
| --- | --- |
| `Open32Drone-minimal-merged.bin` | First USB flash; includes the bootloader, partition table and application |
| `Open32Drone-minimal-app.bin` | A/B OTA updates after the complete partition layout is installed |
| `Open32Drone-Controller-0.1.apk` | Android phone controller |

For a new XIAO, a fully erased XIAO, or the first installation of this partition layout, write the merged image at address `0x0`. The app image contains only the application and is used for OTA updates on an aircraft that already has the complete partition layout.

First verify the downloaded files:

```bash
cd /path/to/open32drone/releases/minimal
shasum -a 256 -c SHA256SUMS       # macOS
# sha256sum -c SHA256SUMS         # Linux
```

<a id="chapter-04-section-2"></a>

### 4.2 Complete USB flash

Install Python 3 and esptool:

```bash
python3 -m pip install --user esptool
```

Connect the XIAO with a USB data cable. On macOS, find the serial port with:

```bash
ls /dev/cu.usb*
```

On Windows, use the `COMx` port shown in Device Manager; on Linux it is usually `/dev/ttyACM0`. If no port appears, enter the XIAO bootloader: hold `BOOT`, press `RESET`, then release `BOOT`.

Replace the example port with your own:

```bash
python3 -m esptool --chip esp32s3 \
  --port /dev/cu.usbmodemXXXX erase-flash

python3 -m esptool --chip esp32s3 \
  --port /dev/cu.usbmodemXXXX --baud 921600 \
  write-flash 0x0 Open32Drone-minimal-merged.bin
```

If transfer errors occur, lower the baud rate to `460800`, then to `115200` if needed. After writing, press RESET and open the serial port at 115200 baud:

```bash
screen /dev/cu.usbmodemXXXX 115200
```

At startup, place the aircraft level on a hard table and leave it untouched. The serial output will show initialization of the motor channels, Wi-Fi, IMU, optical-flow/ToF module and gyro, ending with:

```text
Gyro calibration complete
Initializing complete
```

<a id="chapter-04-section-3"></a>

### 4.3 Check the sensors

For the first connection, use the aircraft hotspot: name `open32drone`, default password `12345678`, aircraft address `192.168.4.1`. If the computer also needs internet access, or you want to use a laboratory router, configure STA as described in Section 4.6.

Enter the following serial commands, pressing Enter after each line:

```text
sys
imu
flow
pw
```

`sys` shows the firmware version and 300 Hz loop status. `imu` shows the sensor model, sampling and gyro calibration results. `flow` shows optical-flow/ToF data and height. `pw` shows the ADC reading and calculated battery voltage.

On the floor, ToF may be inside its approximately 20 mm close-range blind zone. Lift the aircraft steadily to 20–60 cm; the reported distance should change with height. Move it slowly over a textured floor and check that the optical-flow data changes too.

<a id="chapter-04-section-4"></a>

### 4.4 Calibrate the aircraft

<a id="chapter-04-section-5"></a>

#### Six-face accelerometer calibration

Run `ca` after the first assembly, an IMU replacement or a complete erase. Follow the serial prompts through these orientations:

1. Level.
2. Nose up.
3. Nose down.
4. Right side down.
5. Left side down.
6. Upside down.

Release the aircraft after placing it in each position and let it sample while stationary on a rigid surface. When `Accelerometer calibration accepted` appears, return it to level, wait for gyro calibration to finish again and run `imu`. At rest, the acceleration magnitude should be close to `9.81 m/s²`.

<a id="chapter-04-section-6"></a>

#### Battery-voltage calibration

Measure the battery-terminal voltage with a multimeter as `V_DMM`, then run `pw` and record the firmware reading as `V_FW`. Read the current scale with `p PWR_VOLT_SCALE`, then calculate:

```text
New scale = Old scale × V_DMM ÷ V_FW
```

Write the new value, wait one second and check again with `pw`:

```text
p PWR_VOLT_SCALE YOUR_NEW_VALUE
```

For example, with an old scale of 2.000, a multimeter reading of 4.10 V and a firmware reading of 4.00 V, the new scale is `2.000 × 4.10 ÷ 4.00 = 2.050`.

<a id="chapter-04-section-7"></a>

#### SBUS calibration for transmitter control

If a receiver is fitted, turn on the transmitter and run `cr`. Complete the eight stick and switch actions shown in the serial prompts, then use `rc` to check the results:

| Action | Expected reading |
| --- | --- |
| Roll, pitch and yaw centered | Close to 0 |
| Throttle at minimum / maximum | Close to 0 / 1 |
| Three-position mode switch | Close to 0 / 0.5 / 1 |

Aircraft controlled only through Android or ROS do not need `cr`.

<a id="chapter-04-section-8"></a>

#### Calibration and saved parameters

Keep the aircraft still for at least 2 seconds after startup. The gyro needs at least 500 new samples to complete calibration. Run `cg` to start again; it repeats only the current gyro calibration.

`ca` saves results only after all six faces pass their checks. If any step fails, the previous calibration is kept and you need to repeat the procedure. Calibrate each IMU separately rather than copying values from another aircraft.

`ca`, `cr` and the voltage scale are saved in NVS and survive normal restarts and application OTA updates. A complete erase removes parameters and network settings. `preset` resets registered parameters but keeps the AP/STA network names and passwords.

Optical flow has no separate calibration command. While the aircraft is stationary and disarmed, firmware estimates the ground bias for that run. The module still needs to be level, its lens clean and the floor visibly textured. Firmware applies rotation compensation for a 24 mm forward offset, so the mounting position should match.

Set `PWR_VOLT_PIN=-1` on older boards without a voltage divider. If voltage sensing is fitted, calibrate `PWR_VOLT_SCALE` as described above. `PWR_COMP_REF=3.28`, `PWR_COMP_SLP=0.472` and `PWR_COMP_MAX=1.20` are thrust-compensation parameters and do not need changing with each voltage calibration. The GPIO21 low-voltage blink is only a warning; it does not trigger automatic landing.

<a id="chapter-04-section-9"></a>

### 4.5 Test all four motors with propellers removed

Keep the aircraft disarmed and run:

```text
mrl   # rear left M0
mrr   # rear right M1
mfr   # front right M2
mfl   # front left M3
```

Each command allows one motor to turn for about 1 second. Label its position and CW/CCW direction as viewed from above, then fit the matching propeller using the method in the previous chapter.

<a id="chapter-04-section-10"></a>

### 4.6 Connect to a Wi-Fi router

Router STA mode is recommended for everyday development. With the aircraft, Android phone and ROS 2 computer on the same LAN, the computer can stay online without repeatedly switching between the aircraft hotspot and laboratory network. Initial setup still uses USB serial. Keep the propellers off and the aircraft disarmed while configuring it.

Prepare a 2.4 GHz Wi-Fi network that the aircraft can join. The SSID must be 1–32 characters and the password 8–63 characters. Open the serial port at 115200 baud and check the current state:

```text
wifi
```

The default complete image shows AP mode and address `192.168.4.1`. Replace the example network name and password below with your router's settings:

```text
sta LAB_SSID LAB_PASSWORD
reboot
```

`sta` saves the router credentials and selects STA for the next boot. It does not switch the running network immediately; run `reboot` to apply it. After restart, use USB serial again to run:

```text
wifi
```

A successful connection should show these key fields:

```text
Configured mode: STA (2)
Mode: Client (STA)
Connected: 1
SSID: LAB_SSID
IP: 192.168.31.42
MAVLink UDP: bound 1 local 14550
```

The router assigns `IP` through DHCP. Use the address actually printed by your aircraft. Connect the phone or ROS 2 computer to the same router, then check that this address responds:

```bash
ping -c 3 192.168.31.42
```

In the router settings, reserve a DHCP address for the aircraft's Wi-Fi MAC address so it receives the same IP at each startup. Disable guest-network or client-isolation settings that prevent LAN devices from reaching each other. Store real SSIDs and passwords only on the aircraft, not in project source, tutorials or flight logs.

In Android, open **Tools → Aircraft address** and enter the `IP` shown by `wifi`. Use the same address for ROS 2:

```bash
ros2 launch open32drone_driver open32drone.launch.py \
  aircraft_ip:=192.168.31.42
```

Android and ROS 2 can both be on this LAN, but use only one MAVLink controller during a flight.

If the aircraft cannot reach the router within 8 seconds of startup, it opens its saved hotspot for recovery. The serial `wifi` output will show `Mode: Access Point (AP) - STA fallback`. Correct the router name or password and run `sta ...` and `reboot` again. To switch back to direct AP mode permanently, run:

```text
ap open32drone 12345678
reboot
```

<a id="chapter-04-section-11"></a>

### 4.7 Choose a first-flight controller

Both SBUS and Android can handle the first flight. Choose according to the equipment you have:

| Equipment | Option | Preparation |
| --- | --- | --- |
| SBUS receiver and paired transmitter | Option A: transmitter | Run `cr` and learn the emergency-stop stick gesture |
| No receiver, or a quick phone-based start | Option B: Android APK | Android phone on the same network as the aircraft |

Use one controller for the first flight. When using the phone, close ROS and other MAVLink clients. When using the transmitter, stop control from the phone app first.

<a id="chapter-04-section-12"></a>

#### Option A: SBUS transmitter

The three-position switch selects these modes:

| Switch position | Mode | Behavior |
| --- | --- | --- |
| Low | STAB: Stabilize | Throttle directly controls thrust; suitable for experienced pilots |
| Middle | ALT_HOLD: Altitude Hold | Centered throttle holds height |
| High | POS_HOLD: Position Hold | Optical flow holds horizontal position; recommended for the first flight |

Select the high Position Hold setting. Arm with minimum throttle and full-right yaw; the motors will idle at about 10%. Hold throttle above 62.5% for about 0.2 seconds to start assisted takeoff to the default 0.60 m height. Then center the throttle and make small horizontal corrections.

To land, hold throttle below 5% for about 0.3 seconds. The aircraft descends automatically and stops its motors after touchdown. To cancel descent, raise throttle above 60%.

The emergency-stop gesture is minimum throttle and full-left yaw for at least 150 ms. This stops the motors immediately, so an airborne aircraft will fall. Use it when a collision with a person, entanglement or loss of attitude control is imminent.

<a id="chapter-04-section-13"></a>

#### Option B: Android APK

Copy `Open32Drone-Controller-0.1.apk` to the phone and install it. Android may ask you to temporarily allow the file manager to “install unknown apps.”

Use the router STA connection configured in the previous section if available. Connect the phone to the same router, enter the DHCP address from the serial `wifi` command under **Tools → Aircraft address**, then wait for live flight-controller status at the top of the app.

For initial setup, or if the router is unavailable, use the aircraft hotspot. After a complete erase, the default network is:

```text
Wi-Fi: open32drone
Password: 12345678
Aircraft address: 192.168.4.1
MAVLink UDP: 14550
```

Connect the phone to this hotspot and choose to stay connected if Android reports “no internet.” Set **Tools → Aircraft address** back to `192.168.4.1`. Enter a relative height of `0.65` and hold “Take off” for about 0.60 seconds. Firmware will arm, climb and enter position hold.

The left stick controls height and yaw; the right stick controls forward/backward and sideways movement. Start with a small-area hover lasting 5–10 seconds, then hold “Land.” If the aircraft moves quickly toward a person, wall or furniture, use “Land” first. If it can no longer land safely, hold “Emergency disarm.”

Android control does not require a physical transmitter. If buttons turn gray, first check for MAVLink heartbeats at the top of the app. Then confirm that phone and aircraft are still on the same network and that the address matches the `wifi` output. In direct AP mode, also check that the phone has not switched to cellular data or another Wi-Fi network.

<a id="chapter-04-section-14"></a>

### 4.8 First flight

In Altitude Hold and Position Hold, throttle center is 50%. The default 40–60% band holds height; outside that band, the stick commands vertical speed rather than motor output directly. Roll, pitch and yaw inputs can still correct direction during automatic takeoff and landing. Moving the mode switch cancels the automatic action and returns to the selected mode. Valid physical SBUS input takes priority over network control.

On the ground, ToF may report only a blind zone rather than a numeric height. As long as those packets continue to update promptly, firmware can use them to check ground takeoff conditions. Do not hold the aircraft in the air to arm it.

Choose an evenly lit, textured floor with at least 2 m of clear space around the aircraft. Put the battery in the central position established during assembly and check that the downward-facing lens is clean. Power on, wait for gyro calibration, then:

1. Take off to a low height of 0.60–0.65 m.
2. Release or center the sticks and observe for 5 seconds.
3. Make small forward, backward, left and right movements.
4. Return to the starting area.
5. Land automatically and confirm the motors stop after touchdown.

An immediate flip to one side usually points to motor position, rotation, propellers or IMU orientation. Stop the motors and return to propeller-off checks. If the aircraft lifts off steadily but shows small oscillations, drift or height changes, use the next chapter to tune by symptom.

---

<a id="chapter-05"></a>

## 05 · Flight tuning

Watch how the aircraft shakes or which way it drifts before deciding what to check. Start changing control parameters only after the propellers, motors, sensors and power supply are working properly. Change one value at a time, repeat the same short flight and record the result so you can tell whether it helped.

<a id="chapter-05-section-1"></a>

### 5.1 Check whether the problem is mechanical

For these symptoms, check the hardware first:

| Symptom | Check first |
| --- | --- |
| Flips to one side at takeoff | M0–M3 positions, motor rotation, CW/CCW propellers, IMU orientation |
| One side is consistently weak | Propeller damage, bent motor shaft, connectors, motor temperature and battery voltage sag |
| Fine, rapid vibration | Propeller deformation, motor shafts, grommets, motor heights and IMU mounting |
| Position hold fails only over certain floors | Floor texture, reflections, lighting and optical-flow window |
| Height reading jumps | ToF window, module tilt, close-range blind zone and wiring |
| Balance changes after a battery swap | Battery and accessory positions, actual takeoff weight |

Once the mechanical condition is consistent, compare flights using the same battery, floor and height. A useful test is “take off to 0.65 m → center the sticks and hover for 5 seconds → land.”

<a id="chapter-05-section-2"></a>

### 5.2 The four control layers

Open32Drone's controllers work from the inner loops outward:

```text
Angular-rate loop → Attitude loop → Height/velocity loop → Horizontal position loop
```

The inner loop must be stable before you tune the outer loops. P sets how strongly errors are corrected, I removes persistent offsets, and D helps limit overshoot from rapid changes. Usually check P first, then I, and adjust D only when needed.

List all parameters:

```text
p
```

Read or write one parameter:

```text
p CTL_R_P
p CTL_R_P 4.02
```

Writes are saved to NVS. Record the old value first, then wait one second after a change and read it back to confirm.

<a id="chapter-05-section-3"></a>

### 5.3 Attitude oscillation and return to level

The standard attitude parameters are:

| Function | Roll | Pitch | Default |
| --- | --- | --- | ---: |
| Angle P | `CTL_R_P` | `CTL_P_P` | 4.47 |
| Rate P | `CTL_R_RATE_P` | `CTL_P_RATE_P` | 0.05 |
| Rate I | `CTL_R_RATE_I` | `CTL_P_RATE_I` | 0.20 |
| Rate D | `CTL_R_RATE_D` | `CTL_P_RATE_D` | 0.001 |

<a id="chapter-05-section-4"></a>

#### Rapid oscillation

If the aircraft takes off but shakes rapidly and continuously, fix propeller and motor vibration first. Once the mechanics are sound, reduce rate P for the affected axis by 5–10%. For example, reduce Roll from `0.050` to `0.045`:

```text
p CTL_R_RATE_P 0.045
```

Repeat the same 5-second hover. If shaking is reduced and control remains responsive, make a similar adjustment on Pitch. Do not change P, I and D together.

<a id="chapter-05-section-5"></a>

#### Slow rocking or an overly sharp return to level

Slow, large oscillations are more likely to involve the outer angle P loop. Reduce `CTL_R_P` or `CTL_P_P` by about 10%, for example `4.47 → 4.02`. If the aircraft becomes sluggish or takes too long to return to level after releasing the sticks, increase it slightly toward the original value.

<a id="chapter-05-section-6"></a>

#### Consistent lean to one side

A fixed-direction lean is usually caused by center of gravity, a weak motor, frame deformation or accelerometer bias. Reposition the battery to restore balance, then run `ca` again. Consider the I term only after the mechanics and calibration are consistent and the offset remains repeatable.

<a id="chapter-05-section-7"></a>

### 5.4 Height problems

The main height-control parameters are:

| Parameter | Default | Purpose |
| --- | ---: | --- |
| `ALT_P` | 0.747 | Main correction for height error |
| `ALT_I` | 0.10 | Removes persistent height offset |
| `ALT_D` | 0.20 | Uses vertical speed to reduce overshoot |
| `ALT_HOVER` | 0.49 | Nominal hover-thrust feed-forward |
| `ALT_VEL_MAX` | 0.45 | Maximum climb/descent speed |

If the aircraft slowly oscillates above and below the target height, first check that ToF data is continuous, then reduce `ALT_P` by about 10%, for example:

```text
p ALT_P 0.67
```

If takeoff is stable but height slowly settles too low or too high, investigate `ALT_I` with small adjustments. If it overshoots the target and then reverses, focus on ToF velocity and `ALT_D`.

`ALT_HOVER` is the collective thrust needed to hold height near the reference voltage. The reference value for an 81 g aircraft with 60 mm propellers is 0.49. If sensors and attitude are stable but the controller needs a large sustained height correction, estimate the mean hover motor command from the log and adjust in small steps. Do not raise `ALT_HOVER` to hide an aging battery or weak motor.

<a id="chapter-05-section-8"></a>

### 5.5 Horizontal drift and position hold

Position hold relies on optical flow. The defaults are:

| Parameter | Default | Purpose |
| --- | ---: | --- |
| `POS_HOLD_P` | 0.85 | Converts position error to target velocity |
| `POS_VEL_P_X/Y` | 0.35 | Horizontal velocity P |
| `POS_VEL_I_X/Y` | 0.04 | Horizontal velocity I |
| `POS_STICK_V` | 0.70 | Maximum horizontal stick-commanded speed |

Run `flow` over a clearly textured floor and confirm that data keeps updating. If yawing in place produces circular drift, check that the module is level and at the standard 24 mm forward offset. For persistent drift in one direction, check optical-flow bias, battery balance and IMU calibration.

If the aircraft slowly leaves its target without making a strong return, increase `POS_HOLD_P` slightly. If it rocks from side to side around the target, reduce it slightly. Change by 5–10% at a time, keeping the same hover height and flight duration.

<a id="chapter-05-section-9"></a>

### 5.6 Battery and available thrust

The reference battery is about 4.2 V when fully charged. Available motor thrust falls as the battery discharges. The board measures voltage through a 100 kΩ / 100 kΩ divider on `GPIO1/A0` and adds some thrust compensation in Altitude Hold and Position Hold. Compensation is limited and cannot indefinitely make up for battery decline.

First calibrate `PWR_VOLT_SCALE` using `pw` and a multimeter. Repeat the same 5-second hover with a fresh battery and a low-charge battery, comparing `voltage`, `hoverFF`, `voltComp` and all four motor outputs. If all four approach saturation as voltage falls, check battery internal resistance, propellers and motors before increasing PID gains.

<a id="chapter-05-section-10"></a>

### 5.7 Compare flights using logs

After disarming, run:

```text
log dump
```

Save the CSV, then use the repository's analysis script for a quick check:

```bash
python3 software/simulation/course/analyze_log.py \
  --csv /path/to/flight.csv \
  --output output/my-flight-analysis
```

Compare at least these traces or fields:

- Target and actual Roll/Pitch.
- ToF height and target height.
- Optical-flow velocity and position error.
- Four motor outputs and any saturation.
- Battery voltage and compensation.
- Times just before and after the problem occurs.

A useful tuning record needs four things: the original value, the new value, the repeated flight maneuver and the observed result. Keep helpful changes and continue in small steps; restore the old value if behavior worsens. This gives you a parameter record for that particular aircraft.

Once the aircraft can repeatedly take off into position hold, hover for 5–10 seconds, move a short distance and land automatically, you can move on to ROS control.

<a id="chapter-05-section-11"></a>

### 5.8 Troubleshoot by error message

<a id="chapter-05-section-12"></a>

### Startup and pre-arm failures

<a id="chapter-05-section-13"></a>

#### No serial output, or the LED flashes only once

1. Confirm that the complete merged image was written at `0x0`, rather than the app image used only for OTA.
2. Use the correct ESP32-S3 USB port and 115200 baud.
3. Erase the chip completely and flash again over USB.
4. Check the boot log for partition errors, repeated resets or undervoltage messages.

<a id="chapter-05-section-14"></a>

#### GPIO21 keeps blinking after initialization

Run `pw`, then check the battery with a multimeter. GPIO21 blinks at `2 Hz` when filtered voltage stays at or below `3.10 V` for `1.5 s`. Blinking stops only after voltage stays at or above `3.20 V` for `1.0 s`.

The blink is a low-voltage warning; it does not land or disarm the aircraft. On an older board without a divider, set `PWR_VOLT_PIN=-1` to disable sampling from a floating ADC pin.

<a id="chapter-05-section-15"></a>

#### `motor PWM unavailable`

This means not all four LEDC motor outputs initialized successfully, so firmware will not allow arming. Check that the build uses the project's specified Arduino-ESP32 core version, then check for camera or other modules using the same LEDC resources. Motor pins in rear-left, rear-right, front-right, front-left order should be `4, 3, 6, 5`.

<a id="chapter-05-section-16"></a>

#### `gyro calibration incomplete`

Place the aircraft on a hard, level table, power it up again and leave it untouched for at least two seconds. If calibration still fails, run `imu` to check the reason and standard deviation. Look for vibration, airflow, a moving table or damaged motors. `cg` restarts gyro calibration; use `ca` for six-face accelerometer calibration.

<a id="chapter-05-section-17"></a>

#### `invalid RC calibration/mapping`

Power the receiver, run `cr` and follow all eight actions. Each control must map to a different channel in `0..7` for its calibration to be saved. The receiver does not need to be on when using only Android or ROS.

<a id="chapter-05-section-18"></a>

#### Parameter storage error

If `sys` shows `Parameter storage: ERROR`, parameter storage has failed and firmware will refuse to arm. Erase and reflash, then repeat `ca`/`cr`. If the error persists, check Flash/NVS hardware and the partition layout. Resolve storage problems before flying.

<a id="chapter-05-section-19"></a>

#### Low or unstable loop rate

The flight-control loop should normally run close to 300 Hz. If `rate` is low, record how long each stage takes and find the cause before changing code.

Disarm, run `perf reset`, keep one operating condition active for 10-20 seconds, then save the `time` and `perf` output. Test the aircraft alone, with Android, with ROS and with the QGC parameter page separately. Compare missed deadlines, maximum lateness, p95/p99/maximum latency and individual stage times.

`imu acquire` measures the current IMU backend's `read()` call; sensor drivers may also perform transfers internally. `perf` excludes waiting for the next loop, so the sum of measured stage times is not the full 3.33 ms period.

If the maximum CLI, MAVLink or background-stage time grows significantly, inspect that code. The 25 Hz flight log uses a RAM ring buffer. Check the actual background-maintenance time before blaming it for slow loops; there is no need to begin by disabling logs or removing safety checks.

<a id="chapter-05-section-20"></a>

### TF-0850 and calibration

<a id="chapter-05-section-21"></a>

#### Android reports ToF not ready while the aircraft is on the floor

TF-0850 cannot give an accurate distance below about `20 mm`, so “ToF not ready” may appear on the ground. If blind-zone packets continue to arrive, firmware can still check ground takeoff conditions; this message alone does not add another takeoff restriction. If buttons are unavailable too, check the MAVLink connection and whether physical SBUS has control.

Run `flow` and check for:

- `TOF UART healthy: 1`.
- Data age below `150 ms`.
- A numeric distance or `blind-zone: 1`.
- A packet count that keeps increasing.

<a id="chapter-05-section-22"></a>

#### Accelerometer calibration is rejected

Remove the propellers, keep the aircraft disarmed and run `ca`. Place it on each of the six faces as prompted, release it and let it sample while stationary. If any face is invalid, or noise, gravity magnitude, scale or residual checks fail, none of the new results are saved. The previous calibration remains in use.

<a id="chapter-05-section-23"></a>

#### Aircraft built from the same kit behave differently

With the standard frame, begin with the same default control parameters. If two aircraft behave differently, compare assembly and calibration first:

- Motor/propeller models and directions.
- Bent shafts, loose arms or different motor heights.
- Rigid IMU mounting parallel to the thrust plane.
- Battery position and center of gravity.
- Downward sensor orientation and the standard `24 mm` forward offset.
- Calibration surface and vibration.

Current firmware does not change configured control parameters through automatic configuration migration or hover Trim learning. Make the mechanical setup consistent, calibrate each aircraft with `ca`/`cr`, then decide whether tuning is needed.

<a id="chapter-05-section-24"></a>

### Android connection and control

<a id="chapter-05-section-25"></a>

#### `ENETUNREACH (Network is unreachable)`

This usually means the phone's current Wi-Fi network cannot reach the configured aircraft address. It can also occur while Android switches or reconnects networks.

For direct AP, confirm that the phone is connected to the aircraft hotspot and the address is `192.168.4.1`. For router STA, connect the phone to the same router and enter the DHCP address printed by serial `wifi` under **Tools > Aircraft address**. Disable VPN, grant the app local-network permission, then try opening this address in a browser:

```text
http://<aircraft-ip>:8080/api/ota/status
```

The app sends MAVLink, video and OTA data over a Wi-Fi network that can reach the aircraft address. If the connection drops, it closes the old connection and reconnects when that Wi-Fi route returns. It does not switch to cellular data.

<a id="chapter-05-section-26"></a>

#### All buttons are gray

Check the status at the top:

- Disconnected: no MAVLink heartbeat.
- Physical SBUS has priority: release the sticks and wait for control to become available. If using only Android, you can also turn off the receiver.
- App in background: return it to the foreground. Manual control transmission stops when the app goes into the background.

You do not need to turn on a physical transmitter before using Android control.

<a id="chapter-05-section-27"></a>

#### Automatic landing a few seconds after takeoff

Check whether the app went into the background, Wi-Fi switched networks, or the status text shows `link loss`. An older client may also send an outdated zero-throttle frame after takeoff, so use matching APK and firmware versions.

Keep the app in the foreground, restore a stable connection and try again. Extending the link-loss timeout does not fix interrupted control data.

<a id="chapter-05-section-28"></a>

### ROS 2 connections and commands

<a id="chapter-05-section-29"></a>

#### Topic names appear but no data arrives

Nodes can create topics before the flight controller is connected. Check:

```bash
ping -c 3 <aircraft-ip>
ros2 run open32drone_driver control status
ros2 topic echo /open32drone/connected --once
```

For direct AP, use `192.168.4.1`. In STA mode, pass the aircraft's DHCP address when starting ROS: `aircraft_ip:=<aircraft-ip>`.

Close Android and other clients controlling this aircraft, then check that another MAVROS process is not using `local_udp_port`. If all you see is an old `/open32drone/UAS1/state` message with `connected: false`, the connection has not been established.

<a id="chapter-05-section-30"></a>

#### `rqt` reports incompatible QoS

In `rqt`, select the bridged topics: `/open32drone/imu/data`, `/open32drone/odom`, `/open32drone/pose` or `/open32drone/range/downward`. These use Reliable QoS and avoid the compatibility issue of subscribing directly to MAVROS sensor-data topics. If warnings persist, confirm that you used the complete `open32drone.launch.py` and that `interface_bridge` is running.

<a id="chapter-05-section-31"></a>

#### `/open32drone/cmd_vel` has no effect

Velocity commands require the aircraft to be connected and armed, with current position and attitude data, and in Offboard ACTIVE. Test with `control velocity` first, then check:

```bash
ros2 topic echo /open32drone/offboard/status
ros2 topic echo /open32drone/flight/status
```

When publishing to `/open32drone/cmd_vel` yourself, send messages continuously. Physical SBUS input takes priority, so check that too. If Offboard is not active, resolve the connection and mode issue first; changing firmware gains will not make the command work.

<a id="chapter-05-section-32"></a>

### OTA failures

Confirm that the aircraft has landed and is disarmed, motors have stopped, and automatic flight and Offboard are inactive. The current image must also pass startup validation before accepting OTA. Upload the app image; the merged image used for USB flashing cannot be used for OTA. Check the current state at:

```text
http://<aircraft-ip>:8080/api/ota/status
```

After an OTA transfer failure, the aircraft should continue using the current partition. If startup validation of the new image fails, it should roll back automatically. USB reflashing may still be needed if recovery fails, so keep the controller's USB port accessible.

---

<a id="chapter-06"></a>

## 06 · ROS 2 control

Once the aircraft flies steadily with a transmitter or phone, try ROS 2 control. Open32Drone communicates with the flight controller through MAVROS, publishes IMU, range, battery and odometry data as ROS topics, and provides takeoff, landing, velocity and position commands.

This chapter does not require QGC. Close Android before starting so ROS controls the aircraft on its own. The ROS package version is `0.1.0`; use it with firmware and Android from the same source revision. If several MAVROS processes run on one computer, each needs a different local UDP port. See the multi-aircraft section for details.

The following commands connect to a real aircraft. For simulation, see the [URDF / USD model notes](#chapter-07) in Chapter 7; the current ROS package does not have a simulation backend.

For your first ROS flight, follow this sequence:

1. Complete a successful first flight with a transmitter or Android, then close Android control.
2. Connect the ROS computer to the aircraft hotspot, or to the same router as an aircraft in STA mode.
3. Install as described in Section 2, then launch in one terminal using Section 3.
4. In another terminal, confirm that `/open32drone/connected` is `true`.
5. Keep Sections 4–5 as reference for now. Go to Section 6 and perform just one takeoff, hover and landing.

Leave multi-aircraft configuration until a single aircraft completes takeoff and landing successfully. During testing, use ROS as that aircraft's only MAVLink controller.

<a id="chapter-06-section-1"></a>

### 1. Requirements

- ROS 2, `colcon` and MAVROS installed.
- Computer connected directly to the aircraft AP, or to the same trusted router as an aircraft already configured for STA.
- ROS computer able to reach the chosen aircraft IPv4 address.
- Android and other MAVLink clients closed.
- Propellers removed during installation and bench checks.

Check the network before starting ROS:

```bash
ping -c 3 192.168.4.1  # In router mode, use the aircraft's STA address
```

<a id="chapter-06-section-2"></a>

### 2. Installation

Use the repository's `software/ros2/` directory or the matching ROS 2 source archive from the same build set:

```bash
mkdir -p ~/osdrone_ws/src
cp -a /path/to/open32drone/software/ros2 ~/osdrone_ws/src/open32drone_driver
cd ~/osdrone_ws
rosdep install --from-paths src --ignore-src -r -y
colcon build --symlink-install
source install/setup.bash
```

Load the workspace every time you open a new terminal:

```bash
source ~/osdrone_ws/install/setup.bash
```

<a id="chapter-06-section-3"></a>

#### Rebuild after changing source code

Your own nodes can subscribe to `/open32drone/odom` for state, publish velocity to `/open32drone/cmd_vel`, or use the existing command topics and services. The driver already handles MAVLink takeoff and landing, so you do not need to implement them again.

If you installed by copying as shown above, edit the source in `~/osdrone_ws/src/open32drone_driver/`, then run:

```bash
cd ~/osdrone_ws
colcon build --symlink-install --packages-select open32drone_driver
source install/setup.bash
```

Add new Python nodes to the source package's `open32drone_driver/` directory and register entry points in `setup.py`. Update `launch/` when adding launch parameters. After launching as described in Section 3, remove the propellers and run `ros2 run open32drone_driver bench_test --duration 5` in a second terminal. Once checks pass, perform one takeoff and landing using Section 6.

Ordinary ROS applications usually need changes only to the nodes. If you change the shared MAVLink protocol, also update firmware, Android and the relevant protocol tests. See the [development guide](docs/reference/source-build.md) for detailed build instructions.

<a id="chapter-06-section-4"></a>

### 3. Launch and check the connection

Start with the launch file supplied in the repository. Before takeoff, check both `connected=true` and continuous IMU, odometry and range updates.

When editing the launch file, do not add a global `name="mavros"` to `mavros_node`. It would rename internal plugins too, causing topic paths and configuration to stop matching. The supplied file maps ToF output to `UAS1/distance_sensor/tof`, which the bridge republishes as `range/downward`.

Before sending takeoff, the program makes a read-only `status` request to check that the flight controller can reply. If it times out, investigate the connection rather than repeatedly sending takeoff.

When recording a ROS bag, use live topics to monitor the flight. Stop recording normally before reading the SQLite database; do not query a file that is still being written.

For a direct connection to the aircraft hotspot, run:

```bash
ros2 launch open32drone_driver open32drone.launch.py
```

The default MAVROS address is:

```text
udp://0.0.0.0:14550@192.168.4.1:14550
```

For router STA, pass the DHCP address shown by the firmware's `wifi` command to the launch file:

```bash
ros2 launch open32drone_driver open32drone.launch.py \
  aircraft_ip:=192.168.31.42
```

You can also pass `fcu_url:=...` to set a custom MAVROS connection address. For everyday use, reserve the aircraft's DHCP address in the router so you do not have to look it up after every power cycle. The Android phone can be on the same router, but close its controller while using ROS.

Open another terminal and check these data streams:

```bash
source ~/osdrone_ws/install/setup.bash
ros2 run open32drone_driver control status
ros2 topic echo /open32drone/connected --once
ros2 topic hz /open32drone/imu/data
ros2 topic echo /open32drone/range/downward --once
```

Normally, `/open32drone/connected` is `true`, IMU data keeps updating, and downward ranging receives current TF-0850 packets. If there are topic names but no data, the connection is not complete. Follow the troubleshooting steps at the end of this chapter.

<a id="chapter-06-section-5"></a>

### 4. Available interfaces

<a id="chapter-06-section-6"></a>

#### Telemetry

| Topic | Type | Meaning |
|---|---|---|
| `/open32drone/connected` | `std_msgs/Bool` | Current heartbeat connection state |
| `/open32drone/state` | `mavros_msgs/State` | Connection, arming and mode |
| `/open32drone/imu/data` | `sensor_msgs/Imu` | Attitude and filtered IMU data |
| `/open32drone/imu/data_raw` | `sensor_msgs/Imu` | MAVROS raw IMU interface |
| `/open32drone/odom` | `nav_msgs/Odometry` | Local position and velocity |
| `/open32drone/pose` | `geometry_msgs/PoseStamped` | Local pose |
| `/open32drone/range/downward` | `sensor_msgs/Range` | Downward TF-0850 range |
| `/open32drone/battery` | `sensor_msgs/BatteryState` | Measured voltage; current and remaining percentage stay unknown; firmware handles assisted-flight thrust compensation |
| `/open32drone/rc/in` | `mavros_msgs/RCIn` | Physical SBUS channels |
| `/open32drone/rc/channels` | `std_msgs/UInt16MultiArray` | RC channels as a simple array |
| `/open32drone/diagnostics` | `diagnostic_msgs/DiagnosticArray` | Connection diagnostics |
| `/tf` | TF | `open32drone/odom -> open32drone/base_link` |

The bridge republishes key sensor topics with Reliable QoS. Select these topics in RViz and `rqt` without having to handle the MAVROS sensor-data QoS differences yourself.

<a id="chapter-06-section-7"></a>

#### Control

| Interface | Meaning |
|---|---|
| `/open32drone/command` | One-shot text commands such as takeoff and landing |
| `/open32drone/command/result` | JSON result matched to the original command |
| `/open32drone/cmd_vel` | Body-frame velocity: `x` forward, `y` left, `z` up |
| `/open32drone/goal_pose` | Absolute position target in `open32drone/odom` |
| `/open32drone/rc/override` | Raw SBUS-style channel test input |

Convenience services are also available:

```text
/open32drone/arm  /open32drone/disarm  /open32drone/takeoff
/open32drone/land  /open32drone/emergency_stop
```

The `/open32drone/takeoff` service uses `flight_manager.takeoff_height`. To specify a height explicitly, use the CLI or text topic.

<a id="chapter-06-section-8"></a>

### 5. Advanced: multiple aircraft on one LAN

To connect several aircraft at once, have them join a router in STA mode. In direct hotspot mode, every aircraft defaults to `192.168.4.1`, so that address cannot distinguish them on one LAN.

In addition to IP addresses, assign separate system IDs, ROS names and ports. This example distinguishes the topics, services, MAVROS interfaces and TF frames of two aircraft:

| Setting | Aircraft 1 | Aircraft 2 | Purpose |
|---|---:|---:|---|
| Aircraft STA address | `192.168.31.101` | `192.168.31.102` | Reach the intended physical aircraft |
| Firmware `MAV_SYS_ID` | `1` | `2` | Distinguish MAVLink systems |
| `robot_name` / TF prefix | `drone01` | `drone02` | Separate ROS names and coordinate frames |
| ROS host local UDP port | `14551` | `14552` | Avoid socket conflicts when two MAVROS instances run on one host |

Set the system ID once through each aircraft's local serial CLI, then restart and check it with `p MAV_SYS_ID`:

```text
p MAV_SYS_ID 1
```

Use a different system ID for the second aircraft. Reserve each aircraft's DHCP address in the router too, so address changes after a restart do not send the controller to the wrong aircraft.

For a central ROS program to discover both aircraft, use the same `ROS_DOMAIN_ID` for both sets of processes. This example runs both MAVROS instances on one host, so their local UDP ports must differ:

```bash
export ROS_DOMAIN_ID=32

# Terminal 1
ros2 launch open32drone_driver open32drone.launch.py \
  robot_name:=drone01 frame_prefix:=drone01 \
  aircraft_ip:=192.168.31.101 mav_sys_id:=1 local_udp_port:=14551

# Terminal 2
ros2 launch open32drone_driver open32drone.launch.py \
  robot_name:=drone02 frame_prefix:=drone02 \
  aircraft_ip:=192.168.31.102 mav_sys_id:=2 local_udp_port:=14552
```

Export the same Domain ID in every terminal and central control process that needs to discover the group.

Each ROS computer in that domain will then see two sets of topic names. This is expected:

```text
/drone01/state       /drone02/state
/drone01/cmd_vel     /drone02/cmd_vel
/drone01/odom        /drone02/odom
```

Two sets of topics in `ros2 topic list` mean DDS has discovered both groups of nodes, which the control program can access separately. Namespaces determine which aircraft receives a command: a message sent to `/drone01/cmd_vel` does not go to `/drone02/cmd_vel`. Specify the aircraft name when using command-line tools too:

```bash
ros2 run open32drone_driver control --robot-name drone01 status
ros2 run open32drone_driver control --robot-name drone02 takeoff --height 0.65
ros2 run open32drone_driver bench_test --robot-name drone01 --duration 5
```

Use different `ROS_DOMAIN_ID` values when two experiments should not discover each other. For one central program controlling several aircraft, normally use the same Domain ID and distinguish aircraft by namespace. Communication between separate domains needs an additional DDS/domain bridge.

If every aircraft has its own companion computer, each computer can use local UDP port `14550`, since sockets on different hosts do not conflict. The aircraft IP, firmware `MAV_SYS_ID`, `robot_name` and TF prefix must still match the correct aircraft. Place any later ROS image node in that aircraft's namespace too, for example `/drone01/camera/image_raw`.

Background processes are also managed per aircraft:

```bash
ros2 run open32drone_driver system start \
  --robot-name drone01 --aircraft-ip 192.168.31.101 \
  --mav-sys-id 1 --local-udp-port 14551
ros2 run open32drone_driver system status --robot-name drone01
ros2 run open32drone_driver system stop --robot-name drone01
```

Before reporting or stopping a process, the tool checks that the recorded PID still belongs to the aircraft namespace. If another process has reused that PID after a computer restart, the old record is ignored so the unrelated process is not stopped.

<a id="chapter-06-section-9"></a>

### 6. Normal flight sequence

ROS automatic takeoff enters Position Hold by default. Send `takeoff` directly: firmware performs pre-arm checks, arming, climb and position hold in order. There is no need to send `arm` separately or change the mode shown while waiting on the ground.

<a id="chapter-06-section-10"></a>

#### Start with one takeoff and landing

Place the aircraft in a clear, safe area with someone supervising:

```bash
ros2 run open32drone_driver control status
ros2 run open32drone_driver control takeoff --height 0.65
ros2 topic echo /open32drone/odom
ros2 run open32drone_driver control land
```

Wait for a successful takeoff result before sending movement commands. After landing, confirm `armed: false` and the landed state.

<a id="chapter-06-section-11"></a>

#### Velocity control

A successful ROS command means the flight controller has actually replied. Velocity control also waits for confirmation that the controller has entered AUTO mode.

Use `control velocity` to specify forward/backward, sideways and vertical velocity, and a duration. The tool enters Offboard, streams the requested velocity for that duration, then asks the aircraft to hold its current position:

```bash
# Forward, backward, left and right; each at 0.25 m/s for 1.5 s.
ros2 run open32drone_driver control velocity  0.25  0.00 0.00 --duration 1.5
ros2 run open32drone_driver control velocity -0.25  0.00 0.00 --duration 1.5
ros2 run open32drone_driver control velocity  0.00  0.25 0.00 --duration 1.5
ros2 run open32drone_driver control velocity  0.00 -0.25 0.00 --duration 1.5

# Ascend, descend and rotate in place.
ros2 run open32drone_driver control velocity 0 0  0.20 --duration 1.0
ros2 run open32drone_driver control velocity 0 0 -0.20 --duration 1.0
ros2 run open32drone_driver control velocity 0 0 0 --yaw-rate 0.50 --duration 2.0
```

The ROS node limits total horizontal speed to `0.70 m/s`, matching the firmware's default `POS_STICK_V`; firmware applies its own limit again on receipt. Vertical speed is limited to `0.35 m/s` and yaw rate to `1.0 rad/s`. If no new command arrives for more than `0.50 s`, the node captures the current position and switches to position hold.

When publishing directly to `/open32drone/cmd_vel`, send continuously, usually at 20 Hz:

```bash
ros2 topic pub -r 20 /open32drone/cmd_vel geometry_msgs/msg/Twist \
  '{linear: {x: 0.20, y: 0.0, z: 0.0}, angular: {z: 0.0}}'
```

Press `Ctrl-C` to stop publishing. Velocity messages must keep arriving; a single message will soon trigger a command timeout.

<a id="chapter-06-section-12"></a>

#### Position control

```bash
ros2 run open32drone_driver control position 0.30 0.00 0.65
```

Before sending a target, inspect `/open32drone/odom` to confirm the current position. Coordinates are absolute positions in the `open32drone/odom` frame. For example, `x=0.30` means reaching that coordinate, not moving another 0.30 m forward from the current position.

The new target is limited to a horizontal distance of `0.80 m` from the current position, with an approach speed no greater than `0.15 m/s`.

<a id="chapter-06-section-13"></a>

### 7. Text commands and services

The text topic is useful for teaching scripts:

```bash
ros2 topic pub --once /open32drone/command std_msgs/msg/String \
  '{data: "takeoff 0.65"}'
ros2 topic echo /open32drone/command/result
ros2 topic pub --once /open32drone/command std_msgs/msg/String '{data: "land"}'
```

Supported text commands:

```text
status
arm
disarm
emergency_stop
takeoff [height_m]
land
mode stabilize|altitude|position
offboard start|stop
rc start|stop
```

Examples of the convenience services:

```bash
ros2 service call /open32drone/takeoff std_srvs/srv/Trigger '{}'
ros2 service call /open32drone/land std_srvs/srv/Trigger '{}'
ros2 service call /open32drone/emergency_stop std_srvs/srv/Trigger '{}'
```

Send commands such as takeoff and landing once, then wait for the matching result before continuing. If no reply arrives, check packet loss or the flight controller's rejection reason rather than sending repeatedly.

<a id="chapter-06-section-14"></a>

### 8. Raw RC channel tests

Use this interface to inspect raw RC channels and protocol conversion. For ordinary autonomous flight programs, use the velocity or position commands above:

```bash
ros2 run open32drone_driver control rc \
  --roll 1023 --pitch 1023 --throttle 1100 --yaw 1023 --duration 1.0
```

Channel values use the SBUS range `[240, 1807]`. The bridge converts them into MAVLink `MANUAL_CONTROL`, so data must keep updating during the test. At the end of the command, the tool stops transmission and returns to position hold.

Physical SBUS takes priority. ROS RC will not start while the transmitter is sending valid control input. A supervising operator can keep the transmitter's emergency stop available, while normal takeoff and landing follow the ROS sequence.

View physical transmitter channels:

```bash
ros2 topic echo /open32drone/rc/in
ros2 topic echo /open32drone/rc/channels
```

<a id="chapter-06-section-15"></a>

### 9. RViz and TF

```bash
ros2 launch open32drone_driver open32drone.launch.py use_rviz:=true
```

The default RViz configuration shows odometry, pose, TF and downward range. Its fixed frame is `open32drone/odom`, and the bridge publishes `open32drone/odom -> open32drone/base_link`. For multiple aircraft, use each one's `frame_prefix`, for example `drone01/odom -> drone01/base_link`.

The current ROS package does not convert the experimental HTTP MJPEG stream into a ROS image topic or provide `camera_info`. To read images, open `http://<aircraft-ip>/stream` with OpenCV. Firmware allows only one video viewer at a time; close Android control while ROS controls the aircraft.

<a id="chapter-06-section-16"></a>

### 10. Check hover and position control with scripts

Keep the area below the aircraft clear during tests. Feet or moving objects change both ToF distance and optical-flow readings. If you deliberately test sensor obstruction, save a separate record and analyze it separately from normal hover data.

Propeller-off bench test:

```bash
ros2 run open32drone_driver bench_test --duration 5
```

Add `--require-rc` only if a physical transmitter is installed and calibrated. Add `--require-battery` only if voltage-sensing hardware is fitted.

Supervised flight test:

```bash
ros2 run open32drone_driver flight_test --height 0.65 --hover 5
```

The default `--pattern hover` performs takeoff, hover and landing without horizontal movement. The script waits for the aircraft to settle: horizontal and height errors must be within 0.10 m, horizontal and vertical speeds no greater than 0.08 m/s, and these conditions must hold for 1 second. Only then does the time specified by `--hover` begin.

Position-target test, with supervision and room to move:

```bash
ros2 run open32drone_driver flight_test --pattern cross --height 0.65 --distance 0.4 --hover 5 --output flight-cross.json
```

The sequence is: stable takeoff → hover → 0.4 m forward → return to origin → 0.4 m left → return to origin → hover → land. The script uses the heading at the end of the initial hover as its reference and fixes targets in the odometry frame. Targets do not move with aircraft drift.

At each point, the aircraft must settle within the allowed error for 1 second, then remain under observation for another second before continuing. If it has not reached the target within 20 seconds, the test reports failure and requests landing.

`--height-tolerance` sets the allowed XY/Z error for this test, defaulting to 0.10 m; it does not change firmware parameters. `--distance` accepts 0.1–0.7 m and does not change normal control distance limits. `--output` saves stage times and raw position samples. If the file already exists, the script reports an error before takeoff. Choose another filename to keep the previous record.

To check continuous velocity control, use a timed velocity command. This is a separate test from reaching position targets:

```bash
ros2 run open32drone_driver control velocity 0.15 0.00 0.00 --duration 10
```

Take off before running this command. The tool sends velocity at about 20 Hz and sends zero velocity after 10 seconds. Actual travel may not be exactly 1.5 m; after zero velocity is sent, keep watching to confirm that the aircraft stops.

The command reports failure if Offboard status becomes stale, Offboard is no longer active or the aircraft disarms. After an AUTO command ACK, ROS waits for actual AUTO mode feedback before showing ACTIVE. The test script does not automatically retry takeoff, and a landing request after failure is not sent repeatedly.

These results are judged using onboard odometry. To measure actual position accuracy, compare against an external positioning system.

<a id="chapter-06-section-17"></a>

### 11. If the aircraft does not respond

If you see `fresh local position is required`, check the ROS position topic first. Messages may be missing or more than 0.5 seconds old; the message alone does not mean the ToF sensor is broken. If Offboard warmup is rejected, the program retries within its activation deadline. If it still fails, land first, then save the startup log and the aircraft namespace's `offboard/status`, `UAS1/local_position/pose` and `UAS1/setpoint_raw/local` data. Find where messages stop arriving and fix the connection before changing PID or timeout parameters.

1. Confirm that `ping <aircraft-ip>` succeeds and check the launch parameter `aircraft_ip`.
2. Close Android before ROS takes control and confirm that no other process is using the selected `local_udp_port`.
3. Run `control status` to check the actual connection and confirm that data is updating.
4. Read `/open32drone/command/result` and MAVROS `statustext` for pre-arm rejection reasons.
5. Stop physical SBUS input when ROS needs control.
6. Read [troubleshooting](#chapter-05) before changing firmware parameters.

---

<a id="chapter-07"></a>

## 07 · Reinforcement learning

The previous chapter used ROS to send velocity and position targets and odometry to observe motion. Here, the aircraft runs in simulation, where a program repeats the same task and learns to reduce offsets caused by wind, propulsion differences and model errors.

The example uses residual reinforcement learning. A geometric PD controller still stabilizes attitude and distributes thrust among the four motors; the PPO network supplies only a three-axis acceleration correction. This keeps the existing controller in place and lets you compare the 81 g model's behavior with and without the learned policy under different disturbances.

<a id="chapter-07-section-1"></a>

### 7.1 Represent the aircraft as a robot model

The repository includes a numerical model and PPO exercises; start with the CPU example in Section 7.4. Complete aircraft URDF/USD scenes, a Gazebo flight backend and pretrained weights need separate preparation. The reference model and videos below explain the modeling approach. Prepare a compatible scene before running the Isaac section.

The videos and curves in this chapter come from teaching simulations. They illustrate training and comparison methods, not the real-flight performance of the downloadable firmware.

The reference URDF/USD model consists of a rigid body and four rotor joints:

```text
base_link
├── rotor_0_link  — continuous — rear left M0
├── rotor_1_link  — continuous — rear right M1
├── rotor_2_link  — continuous — front right M2
├── rotor_3_link  — continuous — front left M3
├── battery_link — fixed
├── camera_link  — fixed
├── imu_link     — fixed sensor frame
└── flow_tof_link — fixed
    ├── flow_link — fixed optical frame
    └── tof_link  — fixed range frame
```

`base_link` includes the printed frame, controller PCB, XIAO, grommets, motor cases, power components and mounting structure. These do not move relative to the body. IMU and optical-flow/ToF mass is included in the body too, while each sensor keeps a fixed coordinate frame for ROS and simulated sensors.

The battery remains a separate `battery_link` so its mass and position are easy to change. The camera is also a fixed link. Each propeller connects to the body through a `continuous` joint that allows unrestricted rotation.

The reference model's mass distribution is:

| Part | Mass |
| --- | ---: |
| Rigid body `base_link` | 54.2547 g |
| Four propellers | Approximately 1.4542 g |
| 18350 battery | 25.0000 g |
| Camera | Approximately 0.2911 g |
| Total | 81.0000 g |

The following video combines four Isaac Sim checks: appearance and the 81 g configuration, a close-up of the controller PCB, unpowered free fall, and movement of the four rotor joints.

[![Play video: model-checks](img/model-checks-poster.png)](img/videos/model-checks.mp4)

<a id="chapter-07-section-2"></a>

### 7.2 Start a model without a complete motor curve

Dimensions and a maximum speed of 50,000 rpm alone do not give the thrust of an 8520 motor with a 60 mm propeller. Converting maximum speed to angular velocity gives:

```text
50,000 × 2π ÷ 60 = 5,235.99 rad/s
```

This can serve as a joint-speed limit, but it still does not determine propeller thrust. To get an approximate model running, start with the forces during hover:

```text
Average hover thrust per motor
= Total mass × Gravitational acceleration ÷ 4
= 0.081 kg × 9.80665 m/s² ÷ 4
≈ 0.1986 N
≈ 20.25 gf
```

Next, inspect a flight log that includes voltage and average the four motor commands during stable hover. The reference log gives about 47.4%. A rough extrapolation then gives full-command thrust of about 0.419 N per motor; use 40 ms as an initial motor response time.

These are starting estimates. During training, vary thrust gain, mass, inertia, voltage and response time so the policy practices with different parameters and depends less on any one estimate.

Use this approximate model to start training and evaluation, then try the Isaac display after preparing a scene. Later, use a single-motor thrust stand to measure several PWM settings at 4.2, 3.9, 3.7 and 3.5 V, gradually replacing estimates with measured data.

<a id="chapter-07-section-3"></a>

### 7.3 The training task

The hover exercise uses 35 observations: position/velocity errors, attitude matrix, angular velocity, reference velocity and acceleration, previous action, integrated error, estimated motor forces and voltage. The network outputs three actions: residual acceleration in x, y and z, bounded to `±4 m/s²`.

Each simulation starts with slightly different attitude, propulsion parameters and wind disturbance. At each step, the policy is rewarded for:

- Staying close to the target position and velocity.
- Maintaining attitude and flight height.
- Making smooth actions without frequent large corrections.
- Avoiding flips, ground impacts and leaving the allowed area.

PPO runs multiple environments at once, collects observations, actions and results, and uses them to update the policy. After training, use previously unseen random seeds and stronger disturbances to compare the baseline PD controller with PPO residual control.

<a id="chapter-07-section-4"></a>

### 7.4 Run your first PPO exercise on a normal computer

Create a Python environment from the repository root:

```bash
python3 -m venv .venv
source .venv/bin/activate
python3 -m pip install --upgrade pip
python3 -m pip install numpy torch matplotlib
```

Run the CPU hover exercise:

```bash
python3 software/simulation/course/hover_lab.py \
  --output output/my-first-hover \
  --iterations 400 --envs 128 --device cpu
```

The output directory will contain:

| File | Contents |
| --- | --- |
| `training.csv` | Reward, position error and failure count per training iteration |
| `policy_initial.pt` / `policy_final.pt` | Initial and final policies |
| `actor.pt` | TorchScript policy that can be loaded independently |
| `evaluation.json` | PD and PPO results under conditions not used for training |
| `config.json` | All training settings |

Results from the reference run are shown below as mean RMS position error over 96 complete episodes:

| Horizontal disturbance | Baseline PD | PPO residual |
| ---: | ---: | ---: |
| 0.0 m/s² | 1.58 cm | 3.64 cm |
| 0.8 m/s² | 16.51 cm | 6.03 cm |
| 1.5 m/s² | 30.48 cm | 10.56 cm |

Simple PD is more accurate in calm conditions. As wind disturbance increases, PPO's learned compensation reduces position error. The residual policy mainly handles persistent disturbances and model errors, while the baseline controller remains responsible for stable flight.

![Position error of baseline PD and PPO under three disturbance levels](img/hover-evaluation.png)

Figure 7-1. CPU hover results under conditions not used for training.

![PPO training curves](img/training-curves.png)

Figure 7-2. Reward and error during training.

<a id="chapter-07-section-5"></a>

### 7.5 From hover to trajectory tracking

After the hover exercise, replace the fixed target with a continuous trajectory. The demonstration below follows a figure eight through 10 rings, climbs in a spiral, then hovers in gusts. The program supplies the trajectory; PPO learns to follow it and reduce disturbance-induced error.

First compare fixed-camera hover under the same disturbance. The first clip uses baseline PD, the second PPO residual control:

[![Play video: hover-pd](img/hover-pd-poster.png)](img/videos/hover-pd.mp4)

Baseline PD: a persistent disturbance produces a larger steady-state offset.

[![Play video: hover-ppo](img/hover-ppo-poster.png)](img/videos/hover-ppo.mp4)

PPO residual + PD: the policy compensates for the disturbance and returns close to the target.

The full 60-second demonstration covers the model, training, hover comparison, figure-eight ring traversal, spiral and gust recovery:

[![Play video: rl-demo-60s](img/rl-demo-poster.png)](img/videos/rl-demo-60s.mp4)

This Isaac Sim run lasted 34 seconds and passed through 10/10 rings, with approximately 12.11 cm RMS position error and a maximum speed of 1.10 m/s. Comparisons under other conditions are:

| Condition | Baseline PD | PPO residual + PD |
| --- | ---: | ---: |
| Calm | 2.72 cm | 8.19 cm |
| Persistent disturbance | 51.33 cm | 18.43 cm |
| Motor differences and mass error | 53.81 cm | 14.28 cm |
| Sudden gust | 34.46 cm | 26.92 cm |

<a id="chapter-07-section-6"></a>

### 7.6 Numerical training and optional Isaac Sim

The numerical training below does not require an external scene. To run Isaac at the end, prepare a compatible USD model. Its asset directory must contain `USD/open32droe/robot.usd` and all referenced meshes, materials and other files. Replace the example asset path with your actual path.

The full training script uses CUDA. Start with the physics checks on the training workstation:

```bash
cd /path/to/open32drone/software/simulation/rl_demo
python3 physics_checks.py \
  --output ../../output/rl-demo/my-run/physics-checks.json
```

Then train, evaluate and run the course preflight checks:

```bash
python3 train.py \
  --output ../../output/rl-demo/my-run \
  --iterations 1200 --envs 1024

python3 evaluate.py --run ../../output/rl-demo/my-run
python3 preflight.py --run ../../output/rl-demo/my-run
```

Launch Isaac Sim's separate Python environment with its own `python.sh`:

```bash
/path/to/isaac-sim/python.sh \
  /path/to/open32drone/software/simulation/rl_demo/native_isaac.py \
  --package /path/to/open32drone/output/simulation-model/OPEN32DRON_fixed_81g \
  --run /path/to/open32drone/output/rl-demo/my-run \
  --output /path/to/open32drone/output/rl-demo/my-run/native \
  --seconds 34 --record --visible
```

`native_isaac.py` applies the four motors' combined force and torque to the rigid body every 5 ms. Aircraft position and attitude come from PhysX integration; the trajectory, rings and cameras are for display and do not move the aircraft frame by frame.

<a id="chapter-07-section-7"></a>

### 7.7 Moving toward a real-aircraft policy

Several tasks remain before using the policy on a real aircraft:

1. Use a single-motor thrust stand to replace the initial maximum-thrust, response-time and reaction-torque estimates.
2. Add IMU, optical-flow and ToF noise, delay and dropped samples to the training environment.
3. Convert ROS recordings into the policy's 35-observation input. Start with replay inference, then constrained bench tests and low-height trials.

The example reads simulation state directly, and the task supplies ring positions. On a real aircraft, first establish where state and targets will come from, for example camera-based localization or target detection.

For initial integration, let the policy output limited acceleration or velocity corrections through ROS and the existing flight controller. Complete replay and bench checks before gradually trying low-height flight. Leave lower-level motor control until sufficient data and testing are available.

After this chapter, compare your training curves with the examples and use the differences to improve the model. Complete scenes, sensor simulation and real-aircraft policy transfer still need separate implementation. Numerical exercise results do not directly establish real-flight performance.

<a id="chapter-07-section-8"></a>

### 7.8 Model coordinates and check sequence

Use meters, kilograms and seconds throughout the model. The body frame follows ROS FLU: X forward, Y left, Z up. CAD software may use different axes, so confirm the transformation before connecting to firmware interfaces.

After importing a model, check scale, center of gravity and inertia separately. The frame file describes only part of the structure. Total mass also includes electronics, wiring, motors and the battery, so use measurements of the assembled aircraft.

Keep collision shapes simple and check for overlaps. During setup, check size and mass first, then gravity, contact behavior, rotor axes and force directions. Only then connect the controller for closed-loop simulation. Parameters are stored in `software/simulation/rl_demo/model.json`; these are model settings, not automatically calibrated values.

The adapter currently looks for `USD/open32droe/robot.usd`; preserve the spelling `open32droe`. When replacing the scene, also check mesh and material references and the Isaac version. Sensor simulation, firmware-in-the-loop, Gazebo integration and transfer from simulation to real flight each need their own implementation and testing.

## Further references

- [Source and build](docs/reference/source-build.md)
- [Parameters and interfaces](docs/reference/firmware.md)
