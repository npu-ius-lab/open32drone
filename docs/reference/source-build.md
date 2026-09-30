# Source and build

Start with the main loop to see how the aircraft reads sensors, stays stable and responds to a phone, radio transmitter or ROS program. The second half explains how to build the firmware, Android app and ROS 2 package. See [Parameters and interfaces](firmware.md) for individual settings.

## Repository layout

| Directory | Contents |
|---|---|
| `hardware/` | Frame models, mechanical specifications and purchasing information |
| `firmware/` | ESP32-S3 flight-controller source |
| `android/` | Android controller source |
| `ros2/` | ROS 2 driver, control commands and RViz configuration |
| `simulation/` | Numerical dynamics, control and reinforcement-learning experiments |
| `docs/` | Bilingual tutorials and website configuration |
| `tests/` | Reusable source and documentation checks |
| `releases/open32drone/` | Software packages, notes and checksums stored in the repository; see [GitHub Releases](https://github.com/npu-ius-lab/open32drone/releases) for downloads |

## Code architecture

[![Firmware control map](/media/figures/firmware-map.en.svg)](/media/figures/firmware-map.en.svg)

Think of the flight controller as doing three jobs:

1. **Work out what the aircraft is doing.** Read the IMU, optical flow and ToF to estimate tilt, height and movement.
2. **Work out what it should do next.** Turn stick movements, takeoff/landing requests or ROS commands into a target attitude, height or speed.
3. **Adjust the four motors.** Compare the current state with the target, then increase or decrease each motor's output to bring the aircraft closer to it.

The sections below follow these three jobs through the code.

## Reading order

Read these functions in order before following individual features:

1. `setup()` and `loop()` in `firmware/firmware.ino`: startup and the order of each loop;
2. `control()` in `firmware/control.ino`: how the control steps fit together;
3. `interpretControls()` in `firmware/control_modes.ino`: how sticks and mode selection become targets;
4. `updateAutoFlightControl()` in `firmware/control_auto_flight.ino`: automatic takeoff and landing;
5. `updateAltitudeHoldControl()` in `firmware/control_altitude.ino`: keeping a target height;
6. `updatePositionControlSplit()` in `firmware/control_position.ino`: reducing horizontal drift;
7. `controlAttitude()`, `controlRates()`, and `controlTorque()` in
   `firmware/control_stabilization.ino`: turning targets into four motor outputs.

## Flight-control main loop {#loop-timing}

The flight controller targets **300 iterations per second**, or one iteration roughly every **3.3 milliseconds**. `loop()` in `firmware.ino` shows the order:

[![One 300 Hz control cycle](/media/figures/control-cycle.en.svg)](/media/figures/control-cycle.en.svg)

Each iteration tries to read the sensors and uses valid readings to estimate attitude, height and horizontal velocity. It then uses the targets from the radio, phone or ROS to calculate the four motor outputs and updates the motor signals.

After updating the motors, the program handles serial and network messages. It also reads battery voltage, updates the LED and records logs at their own rates. Appearing in the main loop does not mean every job runs at 300 Hz.

Changed parameters are saved when there is no motor output. The program checks once per second whether a save is needed, avoiding storage-write delays during flight. At the end of each iteration, it also updates a completion timestamp so a watchdog can detect a stalled loop.

## Firmware file responsibilities {#firmware-file-ownership}

| File | Purpose | Look here when |
|---|---|---|
| `firmware.ino` | Start each module and run the main loop in order | Startup or execution order is unclear |
| `time.ino` | Schedule iterations and measure execution time | The loop is slow or its duration varies |
| `imu_backend.h`, `imu.ino` | Select the IMU driver, read measurements, convert mounting direction, filter and calibrate | IMU readings or calibration are wrong |
| `flow.ino` | Read TF-0850 data and check optical flow and range readings | Range or flow data is missing |
| `estimate.ino` | attitude, height and horizontal-motion estimates | measurements are valid but estimated state is wrong |
| `control.ino` | Call takeoff/landing, altitude, position and attitude control in order | Following the complete control sequence |
| `control_modes.ino` | Handle mode selection, sticks and radio takeover | Mode selection or takeover is wrong |
| `control_offboard.ino` | Receive external targets and check the command stream and required sensors before enabling control | ROS commands cannot start controlling the aircraft |
| `control_auto_flight.ino` | Progress through takeoff, landing and the mode change after takeoff | Automatic takeoff or landing behaves incorrectly |
| `control_altitude.ino` | height target, vertical PID correction and tilt compensation | ALT_HOLD or vertical response is wrong |
| `control_position.ino` | Correct horizontal movement using position and velocity estimated from optical flow | Position Hold drifts or cannot start |
| `control_stabilization.ino` | attitude loop, angular-rate PID and motor mixer | STAB oscillates or motor corrections have the wrong sign |
| `motors.ino` | Configure motor pins and write PWM signals | A motor has no output or the numbering is wrong |
| `rc.ino` | Read SBUS, calibrate sticks and recognize takeover and emergency stop | Radio transmitter behavior is wrong |
| `mavlink.ino` | Exchange commands and aircraft state, handle parameters and limit data sent at once | Android/ROS commands, QGC parameter reads or reported state are wrong |
| `safety.ino` | Check readiness, handle lost control links, disarm and stop tipped-over aircraft | Takeoff is refused or a fault does not stop the aircraft correctly |
| `loop_watchdog.ino` | Detect a stalled loop, stop motors and restart | The program stalls or triggers a watchdog restart |
| `parameters.ino` | Register parameters, check allowed values and load/save settings | Parameters are rejected or wrong after restart |
| `ota.ino` | Receive updates on the ground, check startup and return to the old firmware if needed | An update or rollback fails |
| `log.ino`, `cli.ino` | flight logs and serial inspection | diagnosing a repeatable symptom |

## Control modes

### 1. STAB: attitude and rate control

Stabilize mode makes the aircraft tilt and turn as requested. Roll and pitch sticks choose the target tilt; the yaw stick chooses turning speed. Throttle directly controls total thrust rather than holding a height.

[![Attitude and rate cascade](/media/figures/attitude-control.en.svg)](/media/figures/attitude-control.en.svg)

For example, suppose the aircraft is level and you ask it to tilt forward:

1. `controlAttitude()` compares the target and current attitude to calculate how fast it should rotate toward the target.
2. `controlRates()` compares that target rotation speed with the gyroscope measurement and uses PID to calculate a correction.
3. `controlTorque()` distributes total thrust and the corrections across four motors. This is called mixing: increasing some motor outputs while decreasing others makes the aircraft tilt or turn.

In PID, P responds to the current error, I compensates for a persistent error, and D responds to changes in error to damp rapid responses. When a motor cannot increase or decrease any further, the program scales the corrections together and undoes the angular-rate integral added in that iteration, avoiding continued accumulation.

### 2. ALT_HOLD: add vertical control

Altitude Hold adds a height target while retaining attitude stabilization. Start with `control_altitude.ino`.

The program starts with a base thrust close to that needed for hovering, then adds or subtracts thrust according to the height error and climb/descent speed. Centered throttle holds the target height; moving the stick above or below center gradually moves that target. During automatic takeoff or landing, the takeoff/landing code moves the target instead.

Battery voltage affects the thrust needed to hover, so recent valid voltage readings adjust the base thrust within a limited range. When readings time out, that compensation stops. It adjusts only the hover base thrust, not every PID correction. The program also compensates for tilt, which reduces the upward part of the total thrust.

If valid ToF readings temporarily stop, the height estimate is marked invalid, height-error accumulation pauses, and the previous correction fades rather than increasing thrust based on old height readings. See Automatic takeoff and landing below for prolonged range loss.

### 3. POS_HOLD: add horizontal control

Position Hold adds horizontal drift correction to Altitude Hold. Start with `control_position.ino`:

[![Position-control cascade](/media/figures/position-control.en.svg)](/media/figures/position-control.en.svg)

The program uses movement estimated from optical flow to choose a tilt that opposes drift. A stick movement requests a direction and speed and moves the target position; attitude control still keeps the aircraft stable.

Before enabling Position Hold, the program checks flow and range readings, whether the aircraft is airborne, whether tilt is small enough, and whether the yaw stick is asking for a substantial turn. Near the floor, some horizontal corrections are reduced to limit the effect of noisy low-height flow readings.

Some useful limits to follow in the code:

- Speed requests from sticks, position error and ROS are combined before `POS_STICK_V` limits the final horizontal speed.
- When flow cannot currently support Position Hold, `updateBoundedPositionFallback()` still lets valid stick commands control tilt; without those commands, it targets level attitude. Tilt stays within the Position Hold limit of `12 deg`, rather than switching to the larger Stabilize tilt range.
- Measured stationary flow offset is retained. If no reliable offset was measured and the code temporarily assumes zero offset, horizontal correction is limited to `3 deg` and velocity-error integration pauses.

## Which controller is in charge? {#mode-and-actuator-ownership}

The usual choices are Stabilize (`STAB`), Altitude Hold (`ALT_HOLD`) and Position Hold (`POS_HOLD`). Automatic takeoff/landing and Offboard control use an internal `AUTO` state. Offboard means that a program outside the aircraft, such as ROS, continuously supplies control targets.

One source must not arbitrarily overwrite another source's commands. Control authority follows these rules:

- **Radio emergency stop is checked independently**, including during automatic takeoff/landing and Offboard control.
- **A lost control link starts its own descent procedure.** Receiving network stick messages again does not directly resume normal control after the failsafe has started.
- **Powering the receiver does not take over.** Ordinary radio takeover needs a deliberate action, not merely one received frame. Changing the radio mode switch during automatic takeoff/landing can cancel that automatic sequence and take over.
- **Offboard and ordinary stick control are separate.** Leave Offboard or select an ordinary flight mode before sending phone or ROS manual stick commands.

## Automatic takeoff and landing

[![Automatic takeoff and landing](/media/figures/auto-flight-states.en.svg)](/media/figures/auto-flight-states.en.svg)

The flight controller breaks takeoff and landing into a sequence of small steps:

During takeoff, it gradually raises the target height. Once that height is reached, the aircraft continues in the selected Altitude Hold or Position Hold mode. During normal takeoff and landing, valid ongoing stick commands can still adjust horizontal movement and turning. Position Hold also contributes horizontal corrections when selected.

Landing first ends Offboard control, then lowers the target height, reduces thrust near the floor, confirms touchdown and disarms.

When the control link is lost, the program chooses a response based on range data:

- **Range data is still valid:** descend using height control and keep checking for ground contact.
- **Range data is also lost:** gradually reduce thrust for at most `SF_DESCEND_TIME` (5 seconds by default), then stop the motors. Without reliable range measurements, this timeout is not recorded as confirmed touchdown.

The latter response also handles prolonged range loss during automatic takeoff/landing. See `control_auto_flight.ino` and `safety.ino` for the triggering conditions.

## Android and ROS path

Android and ROS 2 communicate with the flight controller using MAVLink, which defines messages for modes, takeoff/landing, control targets and aircraft state:

[![Android / ROS command path](/media/figures/command-path.en.svg)](/media/figures/command-path.en.svg)

The phone or ROS tells the aircraft what it should do. Attitude control and motor output still run on the aircraft. Before acting on a request, the firmware checks the sensors, aircraft state and whether commands have timed out.

Use one control client at a time for each aircraft. The firmware records the reply address and source port from a valid controller heartbeat and keeps that address fixed while armed. To switch clients on the ground, close the old client, wait 3 seconds, then connect the other one.

QGC is an optional ground parameter tool. While disarmed, the aircraft accepts standard parameter messages for inspecting and changing settings; QGC is not used for takeoff or flight control in this project.

## Parameters and calibration

NVS is the chip's storage for settings that survive power-off. Startup loads valid saved values; if a setting has no saved value, it uses the source default.

Parameter changes are checked for allowed values, and ordinary parameter writes are rejected while armed. `syncParameters()` saves changed settings when there is no motor output; see [Flight-control main loop](#loop-timing).

## Making and testing changes

For a first contribution:

1. reproduce one symptom and save its log;
2. find the file responsible for that feature in the table above;
3. change one behavior or one parameter family;
4. add an automated test that reproduces the problem;
5. run the tests and rebuild the firmware;
6. flash the firmware and check sensors, motor order and emergency stop with
   propellers removed before a short, low-altitude flight test.

When changing multiple modules, test them separately before testing them together.
Keep interfaces and behavior unchanged during a refactor so that before-and-after results are easy to compare.

## What you need to build the firmware {#pinned-firmware-toolchain}

The commands below use these versions so the build environment can be reproduced:

| Dependency | Version |
|---|---|
| Arduino-ESP32 | `3.3.6` |
| FlixPeriph (IMU and SBUS) | `1.10.4` |
| MAVLink Arduino library | `2.0.25` |
| Board | `esp32:esp32:XIAO_ESP32S3` |
| Options | `PSRAM=opi,PartitionScheme=default_8MB,FlashMode=dio` |

Install once:

```bash
arduino-cli core install esp32:esp32@3.3.6 \
  --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli lib install "FlixPeriph@1.10.4" "MAVLink@2.0.25"
```

Run from the repository root to build for MPU6500/MPU9250. The directory below is temporary: move any firmware you need to keep into the project's `output/`, then remove the temporary build directory.

```bash
arduino-cli compile \
  --clean \
  --fqbn 'esp32:esp32:XIAO_ESP32S3:PSRAM=opi,PartitionScheme=default_8MB,FlashMode=dio' \
  --output-dir /private/tmp/open32drone-build \
  firmware
```

Choose the IMU driver through a build option when using a different sensor. The later estimation and control code remains the same, but mounting direction, readings and calibration still need checking on that hardware:

```bash
arduino-cli compile --clean \
  --fqbn 'esp32:esp32:XIAO_ESP32S3:PSRAM=opi,PartitionScheme=default_8MB,FlashMode=dio' \
  --build-property 'compiler.cpp.extra_flags=-DOPEN32DRONE_IMU_BACKEND=OPEN32DRONE_IMU_ICM20948' \
  --output-dir /private/tmp/open32drone-icm20948 firmware

arduino-cli compile --clean \
  --fqbn 'esp32:esp32:XIAO_ESP32S3:PSRAM=opi,PartitionScheme=default_8MB,FlashMode=dio' \
  --build-property 'compiler.cpp.extra_flags=-DOPEN32DRONE_IMU_BACKEND=OPEN32DRONE_IMU_MPU6050' \
  --output-dir /private/tmp/open32drone-mpu6050 firmware
```

Do not compile multiple backend profiles concurrently against one Arduino
cache. A clean sequential build avoids reusing objects compiled with another
backend macro. After changing an IMU, check its mounting direction and readings,
then complete calibration and motor checks.

The two relevant artifacts are different:

- `firmware.ino.merged.bin`: complete USB image, written at `0x0`;
- `firmware.ino.bin`: application image for A/B OTA only.

Use the full image at `0x0` over USB for the first installation and the application image for phone OTA. They are not interchangeable.

## Command-line flashing (optional) {#usb-flash}

For a normal installation, use the [browser flasher](../guide/04-firmware-flight.en.md#flashing). Use the method below for offline or command-line operation. Download the full firmware from the [Release](https://github.com/npu-ius-lab/open32drone/releases/latest) and open a terminal in its download folder. Replace the example filename with the one you downloaded.

Install [Python 3.10 or later](https://www.python.org/downloads/). These instructions pin esptool 5.1.0 and pyserial 3.5 in a separate virtual environment; pyserial also provides port listing and a calibration terminal. Follow only the steps for your operating system.

### Windows (PowerShell)

Reopen PowerShell after installing Python. In the download folder's address bar, type `powershell` and press Enter:

```powershell
py -3 --version
py -3 -m venv .venv
.\.venv\Scripts\python.exe -m pip install "esptool==5.1.0" "pyserial==3.5"
.\.venv\Scripts\python.exe -m esptool version
.\.venv\Scripts\python.exe -m serial.tools.list_ports -v
```

Python must be at least 3.10. Note the serial port shown in Device Manager or by the last command, such as `COM5`. Follow “Enter download mode, flash and restart” below, then replace the example port with your actual port:

```powershell
.\.venv\Scripts\python.exe -m esptool --chip esp32s3 --port COM5 erase-flash
.\.venv\Scripts\python.exe -m esptool --chip esp32s3 --port COM5 --baud 460800 write-flash 0x0 .\Open32Drone-20260928-190250-full.bin
```

### macOS (Terminal)

After installing Python, type `cd ` in Terminal, including the trailing space, drag the download folder into the terminal and press Enter:

```bash
python3 --version
python3 -m venv .venv
.venv/bin/python -m pip install "esptool==5.1.0" "pyserial==3.5"
.venv/bin/python -m esptool version
.venv/bin/python -m serial.tools.list_ports -v
```

Note the USB serial port, such as `/dev/cu.usbmodem1101`. Enter download mode and replace the example with your actual port:

```bash
.venv/bin/python -m esptool --chip esp32s3 --port /dev/cu.usbmodem1101 erase-flash
.venv/bin/python -m esptool --chip esp32s3 --port /dev/cu.usbmodem1101 --baud 460800 write-flash 0x0 Open32Drone-20260928-190250-full.bin
```

### Ubuntu / Debian (Terminal)

Install Python virtual-environment support, then change to the download folder for the remaining commands:

```bash
sudo apt update
sudo apt install python3 python3-venv
python3 --version
python3 -m venv .venv
.venv/bin/python -m pip install "esptool==5.1.0" "pyserial==3.5"
.venv/bin/python -m esptool version
.venv/bin/python -m serial.tools.list_ports -v
```

Note the serial port, such as `/dev/ttyACM0`. Enter download mode and replace the example with your actual port:

```bash
.venv/bin/python -m esptool --chip esp32s3 --port /dev/ttyACM0 erase-flash
.venv/bin/python -m esptool --chip esp32s3 --port /dev/ttyACM0 --baud 460800 write-flash 0x0 Open32Drone-20260928-190250-full.bin
```

### Enter download mode, flash and restart

1. Connect the XIAO with a USB data cable. Hold **BOOT**, press **RESET**, then release **BOOT**.
2. Check the port again: download mode may use a different port from normal boot.
3. Close serial monitors and run the two flash commands for your operating system. **erase-flash removes calibration, parameters and Wi-Fi settings**. This procedure is for first installation or full recovery, rather than a routine update that preserves settings.
4. Wait for successful completion, then press RESET to boot normally. Write the full image at `0x0`; do not substitute the app image.

### Open the serial port

List ports again after flashing. From the same folder, open the normal-boot port at 115200 baud:

```powershell
# Windows: replace COM5 with the normal-boot port
.\.venv\Scripts\python.exe -m serial.tools.miniterm COM5 115200 --eol LF
```

```bash
# macOS; on Linux, replace the port with /dev/ttyACM0
.venv/bin/python -m serial.tools.miniterm /dev/cu.usbmodem1101 115200 --eol LF
```

Press `Ctrl+]` to exit. After opening the terminal, press RESET once, leave the aircraft level and still, and wait for:

```text
Initializing complete
Gyro calibration complete
```

Then return to [Preflight preparation](../guide/04-firmware-flight.en.md#preflight).

### Troubleshooting

| Symptom | Action |
|---|---|
| No serial port | Try a known data cable, connect directly to the computer and enter BOOT again; rule out a charge-only cable first |
| Unknown USB device on Windows | Check the model in Device Manager and follow the [official XIAO instructions](https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/); do not install CH340/CP210x drivers without checking |
| Port busy | Close serial terminals and IDE monitors, then flash again |
| Permission denied on Linux | Run `sudo usermod -aG dialout "$USER"`, log out and back in; do not install Python packages with sudo |
| Stuck on Connecting | Enter BOOT again, list ports and check the USB connection |
| Interrupted write | Change `--baud 460800` to `--baud 115200` and retry |
| Firmware file not found | Check that the terminal is in the download folder and the browser has not added a duplicate-download suffix to the filename |

Tool reference: [Espressif esptool installation instructions](https://docs.espressif.com/projects/esptool/en/latest/esp32/installation.html). Do not proceed to first flight if flashing fails.

## Android build

```bash
cd android
./gradlew --no-daemon testDebugUnitTest lintDebug assembleDebug
```

Debug APK:

```text
android/app/build/outputs/apk/debug/app-debug.apk
```

The Android application is version `0.1.2` (`versionCode 3`). Use it with firmware from the same source revision.

After changing the app, check aircraft-hotspot connectivity, rejection of other aircraft's packets, reconnection, one-button takeoff/landing, stick input during takeoff/landing, radio takeover and emergency stop. Video runs on a separate thread below control-communication priority; displaying video does not control the aircraft.

## ROS 2 build

```bash
mkdir -p ~/osdrone_ws/src
cp -a ros2 ~/osdrone_ws/src/open32drone_driver
cd ~/osdrone_ws
rosdep install --from-paths src --ignore-src -r -y
colcon build --symlink-install
source install/setup.bash
```

The ROS package manifest uses `0.1.2`; use it with firmware and Android clients
from the same source revision.

For a workspace that should follow repository edits directly, use a symlink
instead of the copy command above when creating a new workspace:

```bash
mkdir -p ~/osdrone_ws/src
ln -s /path/to/open32drone/ros2 ~/osdrone_ws/src/open32drone_driver
```

Edit existing nodes under `ros2/open32drone_driver/`. For a new executable node, register its entry point in `setup.py` and update `launch/` if it should start with other nodes. Then rebuild this package:

```bash
cd ~/osdrone_ws
colcon build --symlink-install --packages-select open32drone_driver
source install/setup.bash
```

After starting the ROS stack described in section 3 of the ROS guide, run
`ros2 run open32drone_driver bench_test --duration 5` without propellers from a
second terminal. Normal applications use the published `cmd_vel`, odometry,
command topic, and services; do not replicate firmware arming, takeoff, or
landing state machines in another node. See [ROS 2 control](../guide/06-ros.en.md) for the
interface and first-flight sequence. If MAVLink fields or the handling of arming, takeoff or landing change, also check the firmware, Android app and related tests.

## Running tests on your computer {#host-validation}

Run from the repository root:

```bash
python3 -m compileall -q ros2
python3 -m unittest discover -s tests -p 'test_*.py' -v
git diff --check
```

Android validation:

```bash
cd android
./gradlew --no-daemon testDebugUnitTest lintDebug assembleDebug
```

Automated tests cover:

- hardware pins, motor channel configuration, modes, preflight checks and link-loss conditions;
- gyroscope, accelerometer and radio calibration, plus settings storage;
- TF-0850 parsing, valid flow/range conditions, IMU mounting direction and parameters;
- supported MAVLink commands, telemetry, voltage input, and A/B OTA;
- loop scheduling and timeout statistics, IMU build options, and sending of parameters and aircraft state;
- Android command, stick, route, selected-aircraft isolation, camera priority,
  reconnect, and version contracts;
- ROS control calculations, command sources, topics, coordinate transforms and OTA upload checks;
- basic repository shape and bilingual document links.

After rebuilding, also check sensors, motor output and controls on the aircraft.

## After compiling

1. Remove propellers, flash the new firmware and check that it starts normally.
2. Check IMU, ToF, battery voltage and the connection to your controller.
3. Check motor numbering, rotation, disarming and emergency stop.
4. Complete [preflight preparation](../guide/04-firmware-flight.en.md#preflight), then perform a short, low-altitude takeoff and landing in a clear area.
5. If anything is wrong, save the log and diagnose it before continuing.

## How OTA updates work

The phone uploads application firmware over HTTP `8080` together with its SHA-256 checksum. The controller writes it into the other application partition, leaving the running firmware untouched. Keeping old and new firmware in separate slots is called A/B OTA.

Updates are allowed only on the ground, while disarmed and with no motor output. Automatic takeoff/landing, Offboard control or a previous update awaiting startup checks also prevent a new update.

After restart, the program checks settings storage, IMU readings, gyro calibration, the main loop, TF-0850 and Wi-Fi. The new firmware is confirmed only after these remain healthy for the required interval. If checks do not pass before the timeout, it returns to the old firmware. See `ota.ino`.

## Before submitting a change

- Run the relevant tests and check that firmware, Android and ROS 2 still build.
- When changing control messages or interfaces, check both firmware and client handling.
- Update both language versions when features or operating steps change.
- Describe the change, test method and results in your PR. See [Contributing](../project/contributing.md).
