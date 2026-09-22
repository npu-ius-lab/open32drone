# Firmware reference

This page is the implementation reference for the current Open32Drone
firmware. New operators should complete [Getting started](../guide/04-firmware-flight.en.md)
first and use [Troubleshooting](../guide/05-tuning.en.md) only when an abnormal result
appears.

## Identity and scope

- project name: Open32Drone (existing firmware still reports the build identifier `minimal`; this is not a separate product or branch);
- target: ESP32-S3, Quad-X, four brushed motors;
- IMU: build-selected FlixPeriph I²C backend; default MPU6500/MPU9250;
- horizontal/height sensor: TF-0850 optical-flow/ToF over UART;
- control links: SBUS, dedicated Android, ROS 2/MAVROS;
- update: full USB flash plus ground-only A/B OTA.

The firmware does not use QGC as a flight-control client and does not contain
missions, barometer control, a complex collision classifier, direct MAVLink
motor control, hover trim, or automatic parameter-profile migration. The
current source has an experimental background HTTP MJPEG path; it is outside
the 300 Hz loop and is not yet software/hardware/flight release evidence. Standard
MAVLink parameters remain available for optional ground-only inspection and
editing from QGC.

## Hardware contract

| Function | Peripheral | Pins/configuration |
|---|---|---|
| Build-selected IMU | `Wire` | SDA `GPIO2`, SCL `GPIO43`, 400 kHz |
| Status LED | GPIO | `GPIO21` |
| Rear-left motor | LEDC channel 1 | `GPIO4` |
| Rear-right motor | LEDC channel 2 | `GPIO3` |
| Front-right motor | LEDC channel 3 | `GPIO6` |
| Front-left motor | LEDC channel 4 | `GPIO5` |
| Battery voltage | ADC1 | `GPIO1/A0`, 100 kΩ / 100 kΩ divider from `VBAT_SW` |
| SBUS | `Serial2` | RX `GPIO44`, TX `GPIO9` |
| TF-0850 | `Serial1` | RX `GPIO8`, TX `GPIO7`, 115200 8N1 |
| USB console | `Serial` | 115200 baud |

Motor PWM is 10 kHz at 10-bit resolution. All four LEDC attachments must
succeed before arming. Disarmed output is zero; armed idle is 10% before the
pilot or automatic controller requests more thrust.

### IMU build profiles

`software/firmware/imu_backend.h` selects one backend at compile time. Runtime
auto-detection across unrelated sensor families is deliberately avoided: it
would make startup, scaling, orientation, and failure handling harder to teach
and reproduce.

| Build value | Driver | Status |
|---|---|---|
| `OPEN32DRONE_IMU_MPU9250` | MPU6500/MPU9250/MPU9255 family | default standard-airframe profile |
| `OPEN32DRONE_IMU_ICM20948` | ICM20948 | CI compile profile; requires board-specific bench/flight validation |
| `OPEN32DRONE_IMU_MPU6050` | MPU6050 | CI compile profile; requires board-specific bench/flight validation |

All profiles expose the same accelerometer/gyroscope contract to estimation
and control. Selecting a different backend does not prove that its mounting
rotation, electrical interface, calibration, or flight tuning matches the
standard aircraft.

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

```text
power -> parameter storage -> LEDC motors -> optional camera allocation
      -> Wi-Fi/MAVLink/OTA -> optional camera stream
      -> selected IMU backend -> SBUS -> TF-0850 -> gyro calibration -> ready
```

Gyro calibration runs at every cold boot and is not loaded from NVS. It needs
at least 500 samples and two seconds of stationary data. The LED is on during
initialization and turns off after setup. After initialization, GPIO21 blinks
at `2 Hz` only after filtered battery voltage stays at or below `3.10 V` for
`1.5 s`; it clears after `3.20 V` for `1.0 s`. This is a visual warning, not an
arm, land, or flight-mode command. Use serial status and client telemetry, not
LED timing alone, to decide whether the aircraft is ready.

## Modes and control ownership

| Custom mode | Name | Purpose |
|---:|---|---|
| `2` | Stabilize | attitude stabilization, direct pilot throttle |
| `3` | Automatic | internal takeoff, landing, or validated Offboard ownership |
| `4` | Altitude Hold | attitude plus ToF height/vertical-speed control |
| `5` | Position Hold | altitude plus optical-flow horizontal hold |

Mode `3` is not user-selectable. The default three-position SBUS switch maps
low/middle/high to `2/4/5`.

Control priority is deliberate physical SBUS input, then the active Android or
ROS lease. A receiver that merely emits static frames does not own the
aircraft. After a neutral observation period, clear stick movement proves pilot
intent. The physical bottom-left emergency-disarm gesture remains independent
of ordinary ownership.

## Pre-arm contract

Arming fails closed if any of these checks fails:

1. OTA is active;
2. NVS parameter storage is unavailable;
3. accelerometer calibration is running;
4. any motor PWM channel failed to attach;
5. IMU data is missing or older than 50 ms;
6. gyro calibration is incomplete;
7. attitude or rate state is invalid;
8. the required RC mapping/link or MAVLink link is invalid;
9. active input throttle is above 5%;
10. measured control-loop rate is below 200 Hz.

Android/ROS automatic takeoff runs this same check atomically; clients must not
duplicate it with their own readiness state machine.

## Automatic takeoff and landing

Automatic takeoff accepts a relative height from `0.20` to `5.80 m`. The
default is `0.60 m`. It begins with a 20% thrust limit, increases the limit by
`0.45/s`, uses a maximum of `ALT_TKO_THR`, advances the height target at
`0.40 m/s`, and keeps that target no more than `0.12 m` ahead of the measured
height. A fresh TF-0850 blind-zone packet is valid ground evidence.

`MAV_CMD_NAV_TAKEOFF`, used by Android and ROS 2, always hands over to
`POS_HOLD`; the standby mode shown before the command does not change that
result. Physical-SBUS assisted takeoff remains different by design and returns
to the Altitude/Position mode selected by its switch.

The automatic controller owns vertical motion only. Live roll, pitch, and yaw
remain available during takeoff and landing. Position mode translates live
horizontal stick input into velocity while flow is valid. If the flow gate is
unavailable or still qualifying, roll/pitch slew toward live pilot input inside
the same `12 deg` position-control envelope; without a live pilot they slew
toward level. This prevents a gate transition from exposing the `30 deg`
Stabilize envelope.

Landing descends at about `0.45 m/s` above `0.30 m`, then `0.28 m/s` near the
floor, and commits to a one-way flare around `0.14 m` clearance. After contact
it disarms instead of commanding a rebound.

## Estimation

The selected backend is configured for ±4 g, ±2000 deg/s, an approximately
50 Hz hardware DLPF, and an approximately 1 kHz sensor rate. The flight
estimator consumes only acceleration and angular rate. A backend such as the
MPU9250 driver may initialize or transfer its magnetometer internally, but
Open32Drone never requests magnetic data and has no magnetic-heading fusion.
Yaw is gyro-integrated and corrected only by pilot or offboard yaw commands.
A software acceleration LPF (`IMU_ACC_LPF_A`) rejects motor/propeller vibration
before attitude and height estimation.

`EST_LVL_WEIGHT` is active, not obsolete. On the ground, gravity correction
uses `EST_ACC_WEIGHT`; in flight it uses the much smaller `EST_LVL_WEIGHT` only
when acceleration magnitude and rotation gates say gravity is reliable.

TF-0850 processing validates the 19-byte stream, freshness, integration time,
range, tilt, and plausible velocity. Horizontal estimation applies delayed
gyro compensation, the fixed 24 mm yaw-offset compensation, stationary ground
bias learning, spike/innovation limits, then integrates position. Ground bias
becomes ready after 30 valid stationary samples; it is runtime state, not a
manual calibration parameter. The `2.5 m/s` optical-flow threshold rejects an
implausible sensor sample; it is not a commanded flight-speed limit. Valid
measurements are not clamped to a gate boundary.

## Control-loop timing

The main loop starts on a fixed `300 Hz` schedule, reads one sample through the
selected IMU backend, then completes input, estimation, control, and motor
output in order. If one iteration misses its deadline, the scheduler records
the lateness and starts a new phase from the current time; it never runs a
catch-up burst. The reported rate is therefore complete control iterations,
not an empty-task counter.

| Work | Service rate |
|---|---:|
| IMU, RC, TF-0850, estimation, control, motor output | every tick, 300 Hz |
| Serial CLI | 100 Hz |
| MAVLink receive/transmit service | 150 Hz |
| OTA boot-health validation | 50 Hz while pending |
| Battery ADC | 10 Hz |
| In-memory flight log | 25 Hz while armed |
| Deferred NVS parameter sync | 1 Hz, motors stopped only |

The hot path caches the fixed IMU mounting quaternion and computes shared Euler
angles/body-up direction once per estimator update. Scheduled MAVLink telemetry
is spread across successive loops rather than emitted as one burst. These are
execution-cost changes only: this batch does not alter PID gains, estimator
weights, TF-0850 compensation, mode behavior, or motor mapping.

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
reported as CPU cost. `perf` is a diagnostic sampler, not a flight task.
Compare at least these workloads separately: aircraft alone, Android connected,
ROS connected, and QGC parameter view open. Do not run those clients together.
Verbose diagnostics and full log/parameter dumps are rejected while armed;
short status commands remain available.

## Compiled parameter defaults

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
| `ALT_HOVER` | `0.49` | hover feed-forward before bounded voltage compensation |
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
| `FLOW_VEL_ALPHA` | `0.20` | velocity innovation blend |
| `FLOW_INNOV_LIM` | `0.80 m/s` | innovation limit |
| `FLOW_GYRO_P` | `-0.78` | pitch rotation fit |
| `FLOW_GYRO_R` | `-0.77` | roll rotation fit |
| `FLOW_GYRO_DLY` | `40 ms` | delayed gyro alignment |
| `FLOW_BIAS_A` | `0.02` | stationary bias adaptation |

The remaining horizontal bounds have distinct jobs:

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
| `PWR_COMP_REF` | `3.28 V` (loaded-voltage crossover) |
| `PWR_COMP_SLP` | `0.472 / V` (normalized factor slope) |
| `PWR_COMP_MAX` | `1.20` (symmetric factor bound; `1.00` disables it) |
| `SF_RC_LOSS_TIME` | `1 s` |
| `SF_DESCEND_TIME` | `5 s` |

The RC endpoint values above are missing-key defaults for the standard
transmitter profile, not a substitute for verification. Run `cr` for each
transmitter when SBUS is used. Valid saved NVS endpoints override these values.

The ADC uses calibrated millivolts, explicit 11 dB attenuation, a 10 Hz sample
rate, and the configured low-pass filter. Samples outside the plausible 1S
range `2.0..4.5 V` are not published. Remaining capacity stays unknown and
voltage never gates flight. A fresh sample produces
`clamp(1 + PWR_COMP_SLP × (PWR_COMP_REF - voltage), 1/PWR_COMP_MAX,
PWR_COMP_MAX)`; stale/disabled sensing or `PWR_COMP_MAX=1.00` produces `1.00`.
The factor multiplies only `ALT_HOVER` before the height PID. A low-voltage
increase is capped at the proven `0.49` feed-forward during automatic takeoff,
then slews at no more than `0.05/s`; loss of a valid sample slews back to the
legacy feed-forward. It does not multiply PID attitude corrections or final
motor outputs, and it does not change direct STAB/Offboard throttle, the
automatic takeoff cap, or the one-way landing flare. The independent ESP32
brownout detector remains enabled.

`pw` prints the selected GPIO, raw ADC millivolts, scale, filtered battery
voltage, and the active compensation factor. For optional DMM calibration while
disarmed:

```text
new scale = current PWR_VOLT_SCALE × DMM battery voltage / reported voltage
```

A stored valid NVS parameter still overrides the compiled default. This rule is
intentional; the firmware never silently rewrites an aircraft's settings.

## NVS behavior

- Namespace: `flix`.
- Missing keys use compiled defaults; the firmware does not pre-fill every key.
- A valid stored value overrides the compiled default.
- Invalid stored values are ignored and reported.
- Parameter writes occur at most once per second and only while motors are
  stopped, to avoid flash latency during flight.
- `preset` removes registered flight parameters and reboots, but preserves
  Wi-Fi credentials.
- A full chip erase removes all flight calibration and Wi-Fi credentials.
- No code path silently migrates or rewrites an old tuning profile.

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
| `arm`, `disarm` | CLI lifecycle control |
| `stab` | select Stabilize while disarmed |
| `auto` | explain automatic ownership rule |
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

Parameter changes, calibrations, motor tests, reset, and reboot are rejected
while armed where applicable.

## MAVLink surface

The firmware publishes heartbeat, system/extended state, battery, attitude,
IMU, local position, distance sensor, RC channels, actuator target, status text,
available/current modes, parameters, and logs. It accepts:

- arm/disarm;
- mode selection for Stabilize, Altitude Hold, and Position Hold;
- takeoff and land;
- `MANUAL_CONTROL`;
- attitude targets;
- local-NED position/velocity targets after validated Offboard warmup;
- parameter read/write and bounded log download.

Direct MAVLink motor commands and mission execution are intentionally absent.

Parameter editing uses the standard `PARAM_REQUEST_LIST`,
`PARAM_REQUEST_READ`, `PARAM_SET`, and `PARAM_VALUE` messages. This is
independent of the outbound `SERIAL_CONTROL_DEV_SHELL` diagnostic-text mirror.
The latter does not accept remote CLI commands; it is not required for
parameter editing, and both paths are retained.

Telemetry remains at the configured per-message rates, but pending messages
are serialized one scheduled packet per control iteration. Parameter lists are
streamed at 20 values per second. Log downloads send one bounded packet per
iteration and only while disarmed; the bytes are the valid chronological rows
of the firmware's float log buffer. The serial `log dump` CSV remains the
primary human-readable flight-analysis format.

### Optional QGC ground parameter editor

QGC is not part of the flight-control path. The compatibility target is the
standard MAVLink parameter page, including QGC Android 5.0.3; live device
validation is still required before claiming that particular build as tested.

1. Remove propellers, leave the aircraft disarmed and motors stopped.
2. Stop Android and ROS, connect the phone/computer to the aircraft Wi-Fi, and
   let QGC listen on UDP `14550`.
3. Wait about six seconds for the roughly 103 raw parameters to arrive at the
   bounded 20-parameter/s rate.
4. Search and change one named parameter. The firmware validates the value and
   returns its authoritative current value; writes are rejected while armed.
5. Leave the aircraft powered and stopped for at least two seconds so deferred
   NVS synchronization can complete. Reboot and read the value again.

There is no QGC parameter metadata package yet, so units, descriptions,
drop-downs, and recommended ranges may be absent. Use this page for meanings
and limits. Do not use QGC to arm, change flight mode, take off, land, send a
mission, or send setpoints; use the matched Android app, ROS 2 package, or
physical SBUS path instead.

## Failsafe timing

- RC frame stale timeout: `150 ms`, with three lost-frame confirmation samples.
- MAVLink manual-control freshness: `500 ms`.
- Offboard setpoint timeout: `300 ms`.
- MAVLink heartbeat/link health: `3 s` for pre-arm connection checks.
- Configurable input-loss latch: `SF_RC_LOSS_TIME`, default `1 s`.
- Controlled thrust ramp-down: `SF_DESCEND_TIME`, default `5 s`.
- Any armed IMU/estimator failure immediately disarms.
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

Only one Android or ROS MAVLink controller may own one aircraft at a time. The
most recent valid UDP sender becomes the reply peer. The experimental camera endpoint is
`GET /stream` and accepts one viewer; it is independent from OTA HTTP `8080`.

Endpoints:

```text
GET  /api/ota/status
POST /api/ota/update
```

The update request requires the app image SHA-256 in
`X-Firmware-SHA256`. OTA writes only the inactive slot and is rejected while
armed, airborne, motor-active, automatic, Offboard, or pending boot validation.
The new image must keep storage, IMU, gyro, loop, TF-0850, and Wi-Fi healthy for
the validation window or the bootloader rolls back.
