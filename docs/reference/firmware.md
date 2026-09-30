# Firmware reference

Use this page to look up supported hardware, pin assignments, flight modes,
parameters, and communication interfaces. For your first flight, start with
[Flashing and first flight](../guide/04-firmware-flight.en.md). For tuning and
flight problems, see [Tuning and troubleshooting](../guide/05-tuning.en.md).

## Hardware and features {#identity-and-scope}

- Controller and airframe: ESP32-S3, X-layout quadrotor, four brushed motors.
- Attitude sensing: an I²C IMU; MPU6500/MPU9250 by default.
- Altitude and position hold: a TF-0850 optical-flow/ToF module over UART.
- Control: the Android app, an SBUS transmitter, or ROS 2/MAVROS.
- Firmware updates: USB flashing or Wi-Fi OTA while disarmed.

## Interfaces and pin assignments {#hardware-contract}

These are the current firmware pin assignments. Motor positions are viewed
from above, with the nose pointing forward.

| Function | Peripheral | Pins/configuration |
|---|---|---|
| IMU | I²C (`Wire`) | SDA `GPIO2`, SCL `GPIO43`, 400 kHz |
| Status LED | GPIO | `GPIO21` |
| Rear-left motor | LEDC channel 1 | `GPIO4` |
| Rear-right motor | LEDC channel 2 | `GPIO3` |
| Front-right motor | LEDC channel 3 | `GPIO6` |
| Front-left motor | LEDC channel 4 | `GPIO5` |
| Battery voltage | ADC1 | `GPIO1/A0`, 100 kΩ / 100 kΩ divider from `VBAT_SW` |
| SBUS | `Serial2` | RX `GPIO44`, TX `GPIO9` |
| TF-0850 | `Serial1` | RX `GPIO8`, TX `GPIO7`, 115200 8N1 |
| USB console | `Serial` | 115200 baud |

Motor PWM is 10 kHz at 10-bit resolution. All four PWM channels must initialize
successfully before arming. Disarmed output is zero; armed idle is 10% before the
pilot or automatic controller requests more thrust.

### Selecting an IMU {#imu-build-profiles}

Select the IMU model in `firmware/imu_backend.h` before compiling.

| Build value | Driver | Status |
|---|---|---|
| `OPEN32DRONE_IMU_MPU9250` | MPU6500/MPU9250/MPU9255 family | default configuration |
| `OPEN32DRONE_IMU_ICM20948` | ICM20948 | included in automated build checks; hardware testing still required |
| `OPEN32DRONE_IMU_MPU6050` | MPU6050 | included in automated build checks; hardware testing still required |

After replacing the IMU, check its wiring and orientation, calibrate it, and
complete propeller-off checks before attempting flight.

## Mounting and coordinate frames

- Airframe body frame: FLU (`x` forward, `y` left, `z` up).
- The IMU rotation defaults to roll `π`, pitch `0`, yaw `π/2`.
- The IMU may be above or below the motor plane, but must be rigid and parallel
  to the four-motor thrust plane.
- The combined TF-0850 module faces straight down and is mounted `24 mm`
  forward of the yaw center. This offset is compiled into yaw-motion
  compensation.
- ROS publishes a per-aircraft ENU transform such as
  `open32drone/odom -> open32drone/base_link`; MAVLink NED conversion occurs
  only at the protocol boundary.

## Boot sequence

[![Startup sequence](/media/figures/boot-sequence.en.svg)](/media/figures/boot-sequence.en.svg)

Gyro calibration runs at every cold boot and is not loaded from NVS. It needs
at least 500 samples and two seconds of stationary data. The LED is on during
initialization and turns off after setup. After initialization, GPIO21 blinks
at `2 Hz` only after filtered battery voltage stays at or below `3.10 V` for
`1.5 s`; it clears after voltage stays at or above `3.20 V` for `1.0 s`.

## Flight modes and switching controllers {#modes-and-control-ownership}

| Custom mode | Name | Purpose |
|---:|---|---|
| `2` | Stabilize | attitude stabilization, direct pilot throttle |
| `3` | Automatic | automatic takeoff, landing, or active Offboard control |
| `4` | Altitude Hold | attitude plus ToF height/vertical-speed control |
| `5` | Position Hold | altitude plus optical-flow horizontal hold |

Mode `3` is not user-selectable. The default three-position SBUS switch maps
low/middle/high to `2/4/5`.

Deliberate SBUS stick input takes priority over Android or ROS. A receiver
sending unchanged neutral stick values does not take control. The firmware
first observes neutral sticks, then uses subsequent stick movement to detect
takeover. The transmitter's emergency stop remains
available regardless of the current controller.

## Checks before arming {#pre-arm-contract}

The firmware refuses to arm in any of these conditions:

1. OTA is active;
2. NVS parameter storage is unavailable;
3. accelerometer calibration is running;
4. any motor PWM channel failed to initialize;
5. IMU data is missing or older than 50 ms;
6. gyro calibration is incomplete;
7. attitude or rate state is invalid;
8. the required RC mapping/link or MAVLink link is invalid;
9. active input throttle is above 5%;
10. measured control-loop rate is below 200 Hz;
11. the control-loop watchdog is unavailable or has detected a fault.

When Android or ROS sends a takeoff command, the firmware performs these
checks. If a check fails, it rejects the command and reports the reason.

## Automatic takeoff and landing

Automatic takeoff accepts a relative height from `0.20` to `5.80 m`. The
default is `0.60 m`. It begins with a 20% thrust limit, increases the limit by
`0.45/s`, uses a maximum of `ALT_TKO_THR`, advances the height target at
`0.40 m/s`, and keeps that target no more than `0.12 m` ahead of the measured
height. Before takeoff, valid TF-0850 blind-zone packets can confirm that the
sensor is online. During descent, a
near-ground range observed within the past second allows fresh blind-zone packets
to participate in touchdown detection, together with low thrust, low vertical
speed and sustained rest. Once disarmed with motors stopped, fresh blind-zone
packets and sustained rest can also restore ground confirmation. Sensor loss,
stale IMU data, or invalid range appearing at height are not evidence of touchdown.

`MAV_CMD_NAV_TAKEOFF`, used by Android and ROS 2, always hands over to
`POS_HOLD`; the standby mode shown before the command does not change that
result. Physical-SBUS assisted takeoff remains different by design and returns
to the Altitude/Position mode selected by its switch.

Normal automatic takeoff and landing control vertical motion only; the pilot
can still adjust roll, pitch, and yaw. Network stick commands are ignored
after failsafe activation.

In Position Hold, horizontal sticks command velocity while optical-flow data
is valid. When flow is temporarily unavailable, the sticks adjust roll and
pitch directly, still limited to `12 deg`. Without valid stick input, the
aircraft gradually levels out.

Landing descends at about `0.45 m/s` above `0.30 m`, then `0.28 m/s` near the
floor. Around `0.14 m`, it starts reducing thrust for touchdown (flare);
thrust can only decrease from this point. Fresh range,
low vertical speed, low thrust and resting inertial data must confirm contact
continuously for at least `300 ms`.

## Estimation

The IMU uses ±4 g and ±2000 deg/s ranges, an approximately 50 Hz hardware
low-pass filter, and an approximately 1 kHz sample rate. The firmware estimates
attitude from acceleration and angular rate, without magnetometer heading
correction.

Each TF-0850 packet contains 19 bytes. The firmware checks freshness, valid range and tilt, and plausible velocity before estimating position from horizontal velocity. Sudden outliers are filtered, and each estimator correction is bounded.

While the aircraft is stationary on the ground, the firmware collects 30 valid samples to estimate optical-flow bias automatically; no manual calibration is needed. Re-arming during the same boot does not clear the measured bias. When a fallback bias is used, `flowBiasFallback` marks it in the log; horizontal tilt correction is limited to `3°` and velocity-error integration pauses.

## Control-loop timing

The main loop starts on a fixed `300 Hz` schedule, reads one sample through the
selected IMU backend, then completes input, estimation, control, and motor
output in order. If one iteration misses its deadline, the scheduler records
the lateness and starts a new phase from the current time; it never runs a
catch-up burst. The rate reported by `time` counts complete control iterations.

An independent high-priority timer monitors complete armed control iterations.
If none completes for more than `100 ms`, it stops all four LEDC PWM channels
before restarting. `sys` reports initialization status. Actual motor-stop timing
still needs propeller-off fault-injection verification on hardware.

| Work | Service rate |
|---|---:|
| IMU, RC, TF-0850, estimation, control, motor output | every tick, 300 Hz |
| MAVLink command and telemetry scheduling | 150 Hz |
| Serial CLI | 100 Hz |
| OTA boot-health validation | 50 Hz while pending |
| Battery ADC | 10 Hz |
| In-memory flight log | 25 Hz while armed |
| Deferred NVS parameter sync | 1 Hz, motors stopped only |

The fixed schedule gives the controller a consistent calculation interval.
Serial and MAVLink services run at their own rates so communication and
diagnostic output do not occupy the flight loop for long periods.

Use these serial commands while disarmed:

```text
time
perf reset
# leave the selected link/workload running for 10-20 seconds
perf
```

`time` reports the latest one-second loop rate, mean, p95, p99, maximum,
300 Hz deadline misses/maximum lateness, and counts above 5/10 ms. `perf`
samples one loop in sixteen and reports mean and maximum execution cost for IMU
acquisition, inputs, estimators, control/motors, serial CLI, MAVLink/OTA, and
housekeeping. Scheduled idle waiting occurs before the sampled work and is not
reported as CPU cost.
For performance diagnosis, compare these workloads separately: aircraft alone, Android connected,
ROS connected, and QGC parameter view open. Do not run those clients together.
Verbose diagnostics and full log/parameter dumps are rejected while armed;
short status commands remain available.

## Default parameter values {#compiled-parameter-defaults}

Stored valid NVS values override these defaults. `p` prints the effective
values on the current aircraft.

### Attitude and rates

| Parameter | Default | Parameter | Default |
|---|---:|---|---:|
| `CTL_R_RATE_P` | `0.05` | `CTL_P_RATE_P` | `0.05` |
| `CTL_R_RATE_I` | `0.20` | `CTL_P_RATE_I` | `0.20` |
| `CTL_R_RATE_D` | `0.001` | `CTL_P_RATE_D` | `0.001` |
| `CTL_R_RATE_WU` | `0.30` | `CTL_P_RATE_WU` | `0.30` |
| `CTL_R_RATE_D_A` | `0.20` | `CTL_P_RATE_D_A` | `0.20` |
| `CTL_Y_RATE_P` | `0.30` | `CTL_Y_RATE_I` | `0` |
| `CTL_Y_RATE_D` | `0` | `CTL_Y_RATE_D_A` | `1` |
| `CTL_R_P` | `4.47` | `CTL_P_P` | `4.47` |
| `CTL_R_I` / `CTL_R_D` | `0` / `0` | `CTL_P_I` / `CTL_P_D` | `0` / `0` |
| `CTL_Y_P` | `3` | `CTL_TILT_MAX` | `0.523599 rad` |
| `CTL_R_RATE_MAX` | `6.28319 rad/s` | `CTL_P_RATE_MAX` | `6.28319 rad/s` |
| `CTL_Y_RATE_MAX` | `5.23599 rad/s` |  |  |
| `CTL_FLT_MODE_0` | `2` | `CTL_FLT_MODE_1` | `4` |
| `CTL_FLT_MODE_2` | `5` |  |  |

### Altitude and automatic takeoff

| Parameter | Default | Meaning |
|---|---:|---|
| `ALT_P` / `ALT_I` / `ALT_D` | `0.747 / 0.1 / 0.2` | height PID |
| `ALT_I_LIM` | `0.30` | height-integral limit |
| `ALT_CORR_MAX` | `0.25` | maximum correction around hover thrust |
| `ALT_VEL_MAX` | `0.45 m/s` | pilot vertical-speed limit |
| `ALT_STICK_DB` | `0.10` | deadband around fixed 50% stick center |
| `ALT_HOVER` | `0.49` | base hover thrust, before voltage compensation |
| `ALT_TKO_H` | `0.60 m` | assisted RC takeoff height |
| `ALT_TKO_TRIG` | `0.625` | assisted RC takeoff trigger |
| `ALT_TKO_THR` | `0.90` | automatic takeoff thrust cap |

### IMU and estimator

| Parameter | Default |
|---|---:|
| `IMU_ROT_ROLL` | `3.14159 rad` |
| `IMU_ROT_PITCH` | `0 rad` |
| `IMU_ROT_YAW` | `1.5708 rad` |
| `IMU_ACC_BIAS_X` | `0` before `ca` |
| `IMU_ACC_BIAS_Y` | `0` before `ca` |
| `IMU_ACC_BIAS_Z` | `0` before `ca` |
| `IMU_ACC_SCALE_X` | `1` before `ca` |
| `IMU_ACC_SCALE_Y` | `1` before `ca` |
| `IMU_ACC_SCALE_Z` | `1` before `ca` |
| `IMU_GYRO_BIAS_A` | `0.001` |
| `IMU_ACC_LPF_A` | `0.02` |
| `EST_ACC_WEIGHT` | `0.003` |
| `EST_LVL_WEIGHT` | `0.0002` |
| `EST_RATES_LPF_A` | `0.20` |

### Position and optical flow

| Parameter | Default | Meaning |
|---|---:|---|
| `POS_HOLD_P` | `0.85` | position-to-velocity gain |
| `POS_STICK_V` | `0.70 m/s` | final horizontal command-vector limit for pilot, position feedback, and Offboard velocity |
| `POS_VEL_P_X` | `0.35` | X velocity proportional gain |
| `POS_VEL_P_Y` | `0.35` | Y velocity proportional gain |
| `POS_VEL_I_X` | `0.04` | X velocity integral gain |
| `POS_VEL_I_Y` | `0.04` | Y velocity integral gain |
| `POS_VEL_D_X` | `0` | X velocity derivative gain |
| `POS_VEL_D_Y` | `0` | Y velocity derivative gain |
| `POS_CMD_RATE` | `1.20 rad/s` | command-angle slew limit |
| `FLOW_VEL_ALPHA` | `0.20` | fraction of the horizontal velocity estimate corrected by optical flow |
| `FLOW_INNOV_LIM` | `0.80 m/s` | limit on a single velocity-estimate correction |
| `FLOW_GYRO_P` | `-0.78` | optical-flow compensation for pitch rotation |
| `FLOW_GYRO_R` | `-0.77` | optical-flow compensation for roll rotation |
| `FLOW_GYRO_DLY` | `40 ms` | time alignment delay between optical flow and gyro data |
| `FLOW_BIAS_A` | `0.02` | optical-flow bias update fraction while stationary |

Horizontal movement also has these limits:

- the final commanded XY speed vector is at most `POS_STICK_V`;
- Position Hold roll/pitch is at most `min(CTL_TILT_MAX, 12 deg)` and changes
  no faster than `POS_CMD_RATE`;
- an integrated pilot/Offboard velocity target stays within `1.0 m` of the
  current estimate, preventing an old moving target from accumulating without
  bound;
- `FLOW_INNOV_LIM` limits one estimator correction, while `2.5 m/s` rejects an
  implausible TF-0850 sample. Neither raises the commanded speed;
- the ROS position helper separately limits a newly requested goal to `0.80 m`
  and approaches it at `0.15 m/s`; direct ROS velocity uses `POS_STICK_V`.

### RC, network, power, and safety

| Parameter | Default |
|---|---:|
| `RC_ROLL` | `0` |
| `RC_PITCH` | `1` |
| `RC_THROTTLE` | `2` |
| `RC_YAW` | `3` |
| `RC_MODE` | `6` |
| `RC_ZERO_0`, `RC_ZERO_1`, `RC_ZERO_3` | `1023` |
| `RC_ZERO_2`, `RC_ZERO_6` | `240` |
| `RC_ZERO_4`, `RC_ZERO_5`, `RC_ZERO_7` | `0` (unused compiled defaults) |
| `RC_MAX_0`, `RC_MAX_1`, `RC_MAX_2`, `RC_MAX_3`, `RC_MAX_6` | `1807` |
| `RC_MAX_4`, `RC_MAX_5`, `RC_MAX_7` | `0` (unused compiled defaults) |
| `WIFI_MODE` | `1` (access point) |
| `WIFI_PORT_LOC`, `WIFI_PORT_REM` | `14550`, `14550` |
| `MAV_SYS_ID` | `1` |
| `MAV_RATE_SLOW`, `MAV_RATE_FAST` | `2 Hz`, `10 Hz` |
| `PWR_VOLT_PIN` | `1` (`GPIO1/A0`; `-1` explicitly disables it) |
| `PWR_VOLT_SCALE`, `PWR_VOLT_LPF_A` | `2`, `0.20` |
| `PWR_COMP_REF` | `3.28 V` (reference voltage, where the factor is 1) |
| `PWR_COMP_SLP` | `0.472 / V` (factor increase per volt of voltage drop) |
| `PWR_COMP_MAX` | `1.20` (factor range: `1/1.20` to `1.20`; `1.00` disables compensation) |
| `SF_RC_LOSS_TIME` | `1 s` |
| `SF_DESCEND_TIME` | `5 s` |

These RC channel defaults are used when no calibration values have been
saved. Run `cr` before first use or after changing SBUS transmitters.
The firmware then uses the valid saved calibration values.

### Battery voltage and thrust compensation

The ADC samples 10 times per second using calibrated millivolt readings, 11 dB attenuation and a low-pass filter. Converted battery readings outside `2.0..4.5 V` are not published. The firmware does not estimate remaining charge percentage. Low voltage triggers an LED warning, but does not automatically prevent arming or trigger landing.

Valid voltage data produces a factor of `clamp(1 + PWR_COMP_SLP × (PWR_COMP_REF - voltage), 1/PWR_COMP_MAX, PWR_COMP_MAX)`. Disabled or stale sensing, or `PWR_COMP_MAX=1.00`, produces `1.00`. The factor multiplies only `ALT_HOVER`, before the altitude PID.

`pw` prints the selected GPIO, raw ADC millivolts, scale, filtered battery
voltage, and the active compensation factor. For optional DMM calibration while
disarmed:

```text
new scale = current PWR_VOLT_SCALE × DMM battery voltage / reported voltage
```

Valid saved parameters take priority over compiled defaults. Rebuilding and
flashing the firmware preserves them as long as NVS is not erased.

## Saving and restoring parameters {#nvs-behavior}

Parameters and calibration data are saved in the board's NVS flash storage
and survive power loss. Serial `p NAME VALUE`, ground-only MAVLink `PARAM_SET`,
`ca`, `cr`, and `ap`/`sta` save their corresponding settings. Boot gyro bias,
optical-flow ground bias, controller integrators, and flight targets are used
only during the current run, not saved as tuning values.

- Namespace: `flix`.
- Missing parameters use compiled defaults.
- Valid stored values take precedence; invalid values are ignored and reported over serial.
- Parameter writes occur at most once per second and only while motors are
  stopped, to avoid flash latency during flight.
- `preset` removes registered flight parameters and reboots, but preserves
  Wi-Fi credentials.
- A full chip erase removes all flight calibration and Wi-Fi credentials.
- Firmware updates do not automatically convert old parameters; follow the
  update notes for parameter changes between versions.

## Serial console

| Command | Purpose |
|---|---|
| `help` | command list |
| `p`, `p NAME`, `p NAME VALUE` | list/read/set effective parameters |
| `preset` | clear registered flight parameters and reboot |
| `time` | loop rate, timing, overrun count |
| `perf`, `perf reset` | sampled loop-stage cost or reset its counters; disarmed only |
| `ps`, `psq` | Euler attitude or quaternion |
| `imu` | sensor, gyro calibration, and landed state |
| `arm`, `disarm` | arm/disarm through the serial console |
| `stab` | select Stabilize while disarmed |
| `auto` | show how to enter automatic mode |
| `rc` | raw/normalized SBUS, owner, mode, armed state |
| `wifi` | configured/runtime mode, address, RSSI, UDP peer and counters |
| `ap SSID PASS` | store AP credentials and select direct mode; reboot required |
| `sta SSID PASS` | store router credentials and select STA; reboot required |
| `ota` | A/B slot and validation status |
| `pw` | GPIO, raw ADC millivolts, scale, and filtered battery voltage |
| `alt` | altitude target, ToF, thrust, phase |
| `flow` | TF-0850 packets, timing, bias, velocity, gates |
| `mot` | current motor outputs |
| `log`, `log dump` | log schema or buffered data |
| `cr`, `ca`, `cg` | RC, accelerometer, or restarted gyro calibration |
| `mfr`, `mfl`, `mrr`, `mrl` | one-second 15% motor test; remove propellers |
| `sys` | build, chip, loop, packets, NVS, task status |
| `reset`, `reboot` | reset estimator/calibration state or reboot |

Change parameters, calibrate, test individual motors, and run `reset` or `reboot` while disarmed. The firmware checks whether each operation is allowed and reports a reason if it rejects it.

## MAVLink interface {#mavlink-surface}

The firmware publishes heartbeat, system/extended state, battery, attitude,
IMU, local position, distance sensor, RC channels, actuator target, status text,
available/current modes, parameters, and logs. It accepts:

- arm, ordinary ground disarm, and explicit emergency stop (`param2=21196`);
- mode selection for Stabilize, Altitude Hold, and Position Hold;
- takeoff and land;
- `MANUAL_CONTROL`;
- attitude targets;
- local-NED position/velocity targets after a continuous setpoint stream
  passes the Offboard startup checks;
- parameter read/write and packet-by-packet log download.

Ordinary disarm returns `DENIED` in flight or when ground state is uncertain;
use `LAND` to descend. Active Offboard is not implicitly taken over by ordinary
`MANUAL_CONTROL`; explicitly select a pilot mode first. ROS yaw rate follows
ENU: positive is counterclockwise. MAVROS converts to NED and firmware converts
to its internal frame.

Parameter editing uses the standard `PARAM_REQUEST_LIST`,
`PARAM_REQUEST_READ`, `PARAM_SET`, and `PARAM_VALUE` messages. This is
independent of the outbound `SERIAL_CONTROL_DEV_SHELL` diagnostic-text mirror.
The latter only sends diagnostic text and does not accept remote CLI
commands. Parameter editing does not depend on it.

Telemetry remains at the configured per-message rates, but pending messages
are serialized one scheduled packet per control iteration. Parameter lists are
streamed at 20 values per second. Log downloads send one bounded packet per
iteration and only while disarmed. Logs come from the in-memory buffer in chronological order. Export CSV with serial `log dump` for analysis on a computer.

### Optional QGC ground parameter editor

QGC can view and edit parameters through the standard MAVLink parameter
interface.

1. Remove propellers, leave the aircraft disarmed and motors stopped.
2. Stop Android and ROS, connect the phone/computer to the aircraft Wi-Fi, and
   let QGC listen on UDP `14550`.
3. Wait about six seconds for the roughly 103 raw parameters to arrive at the
   bounded 20-parameter/s rate.
4. Search and change one named parameter. The firmware validates the value and
   returns its authoritative current value; writes are rejected while armed.
5. Leave the aircraft powered and stopped for at least two seconds so deferred
   NVS synchronization can complete. Reboot and read the value again.

## Connection loss and fault protection {#failsafe-timing}

- RC frame stale timeout: `150 ms`, with three lost-frame confirmation samples.
- MAVLink manual-control freshness: `500 ms`.
- Offboard setpoint timeout: `300 ms`.
- MAVLink heartbeat/link health: `3 s` for pre-arm connection checks.
- Configurable input-loss latch: `SF_RC_LOSS_TIME`, default `1 s`.
- Fresh ToF enables altitude-controlled failsafe landing with contact confirmation.
  Without range, `SF_DESCEND_TIME` (default `5 s`) bounds thrust reduction; expiry
  is not reported as confirmed touchdown.
- A failed IMU read or non-finite sample is discarded and counted. More than
  `50 ms` without a valid sample, or invalid attitude/rate estimates, immediately
  disarms. `imu` reports error counts and maximum sample gap.
- Stale retained height has `heightValid=0`; vertical velocity decays by elapsed
  time. `groundConfirmed` records contact state separately.
- Any armed body tilt over `70 degrees` sustained for `250 ms` immediately
  disarms with reason `tip-over`; this simple guard does not use ToF or impact
  acceleration.

## Wi-Fi and OTA

Default access point:

```text
SSID: open32drone
Password: 12345678
Aircraft IP: 192.168.4.1
MAVLink UDP: 14550
OTA HTTP: 8080
```

Router STA mode is selected explicitly while disarmed:

```text
sta LAB_SSID LAB_PASSWORD
reboot
wifi
```

`wifi` prints the DHCP address used by Android, ROS, camera, and OTA. If STA
does not connect within eight seconds during boot, firmware starts the saved AP
as a recovery network without rewriting `WIFI_MODE=2`; the next reboot retries
STA. Use `ap SSID PASS` to select AP permanently. `preset` preserves both AP
and STA credentials, while a complete flash erase removes them.

Connect only one Android or ROS controller to an aircraft at a time. After
receiving a valid controller heartbeat, the firmware records its IP address
and UDP port for replies. This address does not change while armed. Once
disarmed, a new controller's heartbeat can select a different address only
after the previous controller has sent no valid data for more than `3 s`.

The experimental camera endpoint is `GET /stream` and accepts one viewer;
it is independent from OTA HTTP `8080`.

OTA endpoints:

```text
GET  /api/ota/status
POST /api/ota/update
```

The update request supplies the application image's SHA-256 checksum in
`X-Firmware-SHA256`. The board has two application partitions, A and B;
OTA writes to the partition that is not currently running. Updates are
rejected while armed, airborne, running motors, in automatic flight or
Offboard, or while the current firmware is still completing its startup checks.

After the update restarts, the firmware checks parameter storage, the IMU,
gyro calibration, the control loop, TF-0850, and Wi-Fi. The new firmware must
pass these checks within the validation window. If it fails, the bootloader
restores the previous version.
