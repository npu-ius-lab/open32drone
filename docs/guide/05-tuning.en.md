# 04 · Flight tuning

Before changing parameters, observe the problem and check mechanics, sensors and power. Change one value at a time, repeat the same short maneuver and compare the result. Two or three repeated comparisons make it easier to identify what caused the difference.

## 4.1 Is it a parameter problem?

Check hardware first for these symptoms:

| Symptom | Check first |
|---|---|
| Flips to one side at takeoff | M0–M3 positions, rotation, CW/CCW propellers and IMU orientation |
| One side is always weak | Propeller damage, bent motor shafts, connectors, motor temperature and battery sag |
| Fine, high-frequency vibration | Deformed propellers, motor shafts, grommets, motor height and IMU mounting |
| Position hold fails only on certain floors | Texture, reflections, lighting and the optical-flow window |
| Height readings jump | ToF window, module tilt, near-range blind zone and wiring |
| Balance changes after a battery swap | Battery and accessory positions and actual takeoff mass |

Once the mechanics are stable, compare flights with the same battery, floor and height. Start with “take off to 0.65 m → center the sticks and hover for 5 seconds → land.”

## 4.2 Understand the four control layers {#tune-from-the-inside-out}

Check the loops from the inside out:

[![Tune from the inner loops to the outer loops](/media/figures/tuning-order.en.svg)](/media/figures/tuning-order.en.svg)

Stabilize the inner loops before adjusting the outer loops. P sets how strongly an error is corrected, I removes persistent error and D reduces overshoot caused by rapid change. Usually start with P, then I, and adjust D only when needed.

List all parameters:

```text
p
```

Read or write one parameter:

```text
p CTL_R_P
p CTL_R_P 4.02
```

Writes are saved to NVS. Record the old value first, then wait one second after writing and read it back.

## 4.3 Attitude oscillation and return to level

Standard attitude parameters are:

| Function | Roll | Pitch | Default |
|---|---|---|---:|
| Angle P | `CTL_R_P` | `CTL_P_P` | 4.47 |
| Angular-rate P | `CTL_R_RATE_P` | `CTL_P_RATE_P` | 0.05 |
| Angular-rate I | `CTL_R_RATE_I` | `CTL_P_RATE_I` | 0.20 |
| Angular-rate D | `CTL_R_RATE_D` | `CTL_P_RATE_D` | 0.001 |

### High-frequency oscillation

If the aircraft takes off but shakes rapidly and continuously, fix propeller and motor vibration first. With sound mechanics, reduce the affected axis's angular-rate P by 5–10%. For example, change Roll from `0.050` to `0.045`:

```text
p CTL_R_RATE_P 0.045
```

Repeat the same 5-second hover. If oscillation decreases and control remains firm, apply the same reduction to Pitch. Do not change P, I and D together.

### Slow oscillation or an overly sharp return to level

Large, low-frequency swings are more likely to involve the outer angle-P loop. Reduce `CTL_R_P` or `CTL_P_P` by about 10%, for example `4.47 → 4.02`. If the aircraft becomes sluggish and takes too long to level after releasing the sticks, increase it slightly toward the original value.

### Persistent lean to one side

A consistent lean usually calls for checking balance, motor thrust, frame distortion or accelerometer bias. Move the battery to center the balance, then repeat `ca`. Analyze the I term only if the same bias persists after mechanics and calibration have been checked.

## 4.4 Altitude problems

The main altitude parameters are:

| Parameter | Default | Purpose |
|---|---:|---|
| `ALT_P` | 0.747 | Main correction for height error |
| `ALT_I` | 0.10 | Remove persistent height error |
| `ALT_D` | 0.20 | Reduce overshoot using vertical velocity |
| `ALT_HOVER` | 0.49 | Nominal hover-thrust feed-forward |
| `ALT_VEL_MAX` | 0.45 | Maximum climb/descent speed |

For slow up-and-down oscillation around the target height, first check that ToF data is continuous, then reduce `ALT_P` by about 10%, for example:

```text
p ALT_P 0.67
```

If attitude is stable after takeoff but height gradually drifts low or high, inspect `ALT_I` and adjust it slightly. If the aircraft overshoots the target and then reverses, focus on ToF velocity and `ALT_D`.

`ALT_HOVER` is the collective thrust needed to maintain height near nominal voltage. The reference for an 81 g aircraft with 60 mm propellers is 0.49. If sensors and attitude are stable but hovering always needs a large altitude correction, estimate mean motor output from a flight log and adjust slightly. Do not raise `ALT_HOVER` to hide an aging battery or weak motor.

## 4.5 Horizontal drift and position hold

Position hold depends on optical flow. Default parameters are:

| Parameter | Default | Purpose |
|---|---:|---|
| `POS_HOLD_P` | 0.85 | Convert position error to target velocity |
| `POS_VEL_P_X/Y` | 0.35 | Horizontal velocity P |
| `POS_VEL_I_X/Y` | 0.04 | Horizontal velocity I |
| `POS_STICK_V` | 0.70 | Maximum stick-commanded horizontal speed |

Run `flow` over a clearly textured floor and confirm continuous updates. Circular drift during yaw calls for checking the standard 24 mm forward sensor offset and whether the module is level. For drift in a fixed direction, recheck flow bias, battery balance and IMU calibration.

If the aircraft slowly leaves the target and returns too slowly, increase `POS_HOLD_P` slightly. If it oscillates around the target, decrease it. Change 5–10% at a time and use the same hold height and flight duration.

## 4.6 Battery and thrust changes

The reference battery is about 4.2 V when full, and available motor thrust decreases during discharge. The board reads voltage through a 100 kΩ / 100 kΩ divider on `GPIO1/A0`. Assisted altitude and position hold use bounded feed-forward compensation.

Calibrate `PWR_VOLT_SCALE` with `pw` and a multimeter first. Repeat the same 5-second hover with a fresh and a lower-charge battery, comparing `voltage`, `hoverFF`, `voltComp` and all four motor outputs. If all four approach saturation as voltage drops, check battery internal resistance, propellers and motors before raising PID gains.

## 4.7 Compare two flights using logs

With the aircraft disarmed, run:

```text
log dump
```

Save the CSV, then use the repository's analysis script:

```bash
python3 simulation/course/analyze_log.py \
  --csv /path/to/flight.csv \
  --output output/my-flight-analysis
```

Compare at least these curves or fields:

- Target and actual Roll/Pitch.
- ToF height and target height.
- Optical-flow velocity and position error.
- Four motor outputs and any saturation.
- Battery voltage and compensation.
- Times immediately before and after the problem.

Record the original parameter, new value, maneuver and observation each time, using the same maneuver for comparison. Keep improvements and continue in small steps; restore the old value if the result worsens. Save the records so you can look up which parameters this aircraft has used and how they performed.

Once the aircraft can repeat position-hold takeoff, a 5–10-second hover, small translations and automatic landing, begin ROS control experiments.

## 4.8 Troubleshoot error messages

## Boot and pre-arm failures

### No serial output, or the LED only flashes once

1. Confirm that a complete merged image was written at offset `0x0`, not an
   app-only OTA image.
2. Use the correct ESP32-S3 USB port and 115200 baud serial monitor.
3. Perform a complete erase and USB flash once.
4. Inspect the boot log for partition, reset-loop, or brownout messages.

### GPIO21 keeps blinking after initialization

Run `pw` and compare the reported voltage with a DMM. A filtered value at or
below `3.10 V` for `1.5 s` starts the `2 Hz` warning; it clears only after the
value stays at or above `3.20 V` for `1.0 s`. The blink is an indicator only:
it does not cause landing or disarming. If the board has no divider, set
`PWR_VOLT_PIN=-1`; do not leave an unconnected ADC enabled.

### `motor PWM unavailable`

The four LEDC motor channels did not attach successfully. Do not bypass the
pre-arm gate. Confirm the firmware was built with the pinned Arduino-ESP32 core,
that no camera or other module uses the same LEDC resources, and that motor pins
are `4, 3, 6, 5` in rear-left, rear-right, front-right, front-left order.

### `gyro calibration incomplete`

Cold-boot on a rigid, level surface and do not touch the aircraft for at least
two seconds. If calibration does not complete, use `imu` to read the reason and
standard deviation. Remove vibration, airflow, a moving table, or a damaged
motor. `cg` restarts only gyro calibration; it does not replace `ca`.

### `invalid RC calibration/mapping`

Run `cr` with the receiver powered and follow all eight prompts. Each control
must map to a distinct persistent channel `0..7`. Android or ROS flight does not require the receiver to be powered on.

### Parameter storage error

If `sys` reports `Parameter storage: ERROR`, arming fails closed. Do not keep
flying with unsaved calibration. Reflash after a full erase and repeat `ca` and
`cr`; if the error returns, investigate flash/NVS hardware or partitioning.

### Loop rate is low or irregular

Do not remove safety checks or disable the flight log based on one `rate`
number. The expected result is near the fixed 300 Hz schedule. Disarm, run
`perf reset`, keep exactly one workload active for 10-20 seconds, then collect
`time` and `perf`. Repeat separately with no client, Android, ROS, and the QGC
parameter page. Compare deadline misses, maximum lateness, p95/p99/max, and the
named stage costs. `imu acquire` is the selected backend's `read()` cost; a
backend may perform family-specific internal transfers. The scheduled idle wait
is deliberately outside `perf`, so its sampled total is execution cost rather
than the 3.33 ms period. Large CLI, MAVLink, or housekeeping maxima point to a
different cause. Check the housekeeping-stage timings when investigating
logging overhead; the 25 Hz log is stored in a RAM ring buffer.

## TF-0850 and calibration

### Android says ToF is not ready while the aircraft is on the floor

The TF-0850 blind zone is below roughly `20 mm`. A fresh blind-zone packet is
enough for the firmware's ground sensor check, even though no numeric range can be displayed.
This Android message alone does not block takeoff. If the button
is disabled, diagnose MAVLink connection or physical SBUS ownership instead.

Use `flow` and check:

- `TOF UART healthy: 1`;
- packet age below `150 ms`;
- either a numeric distance or `blind-zone: 1`;
- packet counters increasing.

### Accelerometer calibration is rejected

Run `ca` disarmed, remove propellers, place the aircraft on all six requested
faces, and keep it motionless during collection. If any face is unstable or its
measurements fail the checks, the new calibration is rejected and the previous
values remain active.

### Two identical aircraft do not fly identically

The default controller parameters should remain common across the standard
airframe. Per-aircraft differences should first be handled mechanically and by
`ca`/`cr`, not by hidden trim:

- motor and propeller model/direction;
- bent shafts, loose arms, or unequal motor height;
- IMU rigidness and parallelism to the thrust plane;
- battery position and center of gravity;
- downward module orientation and the standard `24 mm` forward offset;
- calibration surface and vibration.

There is no automatic profile migration or hover-trim learning that silently
rewrites copied controller parameters.

## Android link and control

### `ENETUNREACH (Network is unreachable)`

The phone has no Wi-Fi route to the configured aircraft address, or Android
recreated that route. In direct-AP mode, connect to the aircraft SSID and keep
the address at `192.168.4.1`. In router STA mode, connect the phone to the same
router and enter the DHCP address printed by the firmware `wifi` command under
**Tools > Aircraft address**. Disable VPN, allow local-network access, and test:

```text
http://<aircraft-ip>:8080/api/ota/status
```

The client binds MAVLink, camera, and OTA to the Android Wi-Fi network that
contains the configured address. It closes stale sockets and retries when that
route returns; it never falls back to cellular data.

### Buttons are gray

Check the status banner:

- disconnected: no MAVLink heartbeat;
- physical SBUS priority: release the sticks and wait for the pilot lease to
  become idle, or power off the receiver if Android should be the only owner;
- app in background: return to the foreground; backgrounding stops manual
  control streaming intentionally.

A physical RC is not required for Android control.

### Takeoff works, then lands after several seconds

This usually indicates the phone stopped sending a fresh control stream, the
Wi-Fi route changed, or a stale pre-takeoff zero-throttle packet was used by an
older client. Use a matched APK/firmware set, keep the app foreground, and check
status text for `link loss`. Do not increase failsafe time to hide a broken
link.

## ROS 2 connection and commands

### Topics exist but contain no data

Nodes can create topics before the FCU connects. Check:

```bash
ping -c 3 <aircraft-ip>
ros2 run open32drone_driver control status
ros2 topic echo /open32drone/connected --once
```

Use `192.168.4.1` for the direct aircraft AP. In STA mode, launch with the same
DHCP address: `aircraft_ip:=<aircraft-ip>`.

Close Android and any other controller for this aircraft. Ensure the selected
`local_udp_port` is not bound by another MAVROS process. A stale
`/open32drone/UAS1/state` sample with `connected: false` is not a connection.

### `rqt` reports incompatible QoS

Subscribe to the reliable public bridge topics (`/open32drone/imu/data`,
`/open32drone/odom`, `/open32drone/pose`, `/open32drone/range/downward`) rather
than the MAVROS sensor topics. If warnings
remain, verify that the full `open32drone.launch.py` stack, including
`interface_bridge`, is running.

### `/open32drone/cmd_vel` has no effect

The aircraft must be connected, armed, have fresh pose, and reach Offboard
ACTIVE. Use the `control velocity` helper first and inspect:

```bash
ros2 topic echo /open32drone/offboard/status
ros2 topic echo /open32drone/flight/status
```

Direct `/open32drone/cmd_vel` must be streamed continuously. Physical SBUS movement has
priority. Do not change firmware gains to compensate for an inactive Offboard
state.

## OTA failures

OTA is accepted only while disarmed, landed, motor-inactive, outside automatic
flight and Offboard, and after the running image has passed boot validation.
Upload the app image only; never upload a merged USB image. Check:

```text
http://<aircraft-ip>:8080/api/ota/status
```

If an update fails, retain USB recovery access. A failed transfer should leave
the running slot intact; a new image that fails boot validation should roll
back.
