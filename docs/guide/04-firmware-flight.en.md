# 04 · Firmware, calibration and first flight

## 1. Complete USB flash

Install Python 3 and `python3 -m pip install --user esptool`. On macOS,
`ls /dev/cu.usb*` lists candidate ports; Linux commonly uses `/dev/ttyACM0`,
and Windows uses the `COMx` number from Device Manager. Enter the bootloader
by holding BOOT, pressing RESET, then releasing BOOT. Use a USB data cable.

Use the complete 8 MiB `merged.bin` image for a new MCU, an erased MCU, or the
first A/B OTA migration. An application-only image cannot create the partition
table.

Verify the package first:

```bash
cd software/releases/minimal
shasum -a 256 -c SHA256SUMS
```

Put the ESP32-S3 into bootloader mode, replace the serial port below, then erase
and write the complete image:

```bash
python3 -m esptool --chip esp32s3 \
  --port /dev/cu.usbmodemXXXX erase-flash

python3 -m esptool --chip esp32s3 \
  --port /dev/cu.usbmodemXXXX --baud 921600 \
  write-flash 0x0 Open32Drone-minimal-merged.bin
```

If 921600 baud is unreliable, retry at 460800 or 115200. A complete erase
intentionally removes NVS parameters, accelerometer/RC calibration, and Wi-Fi
credentials. Do not copy another aircraft's accelerometer calibration into the
new aircraft.

After flashing, disconnect boot mode and perform a normal cold boot.

## 2. First boot

Place the aircraft level and completely still before applying power. Open the
serial terminal at 115200 baud. A healthy startup reaches all of these gates:

1. four motor PWM channels attach successfully at 10 kHz;
2. the configured Wi-Fi mode and MAVLink UDP socket start;
3. the compiled IMU backend initializes and reports the expected model (the
   standard image expects MPU6500/MPU9250);
4. TF-0850 UART packets arrive;
5. gyro calibration completes after at least 500 samples and 2 seconds;
6. the CLI prints `Initializing complete`.

Do not move the aircraft until `Gyro calibration complete` appears. If it does
not complete, run `imu` and use the reported failure reason before changing any
parameter.

Choose only one network. If unsure, use the first row:

| Situation | Network | Aircraft address | Setup required |
|---|---|---|---|
| First use, one Android, or one ROS host | Direct aircraft AP | `192.168.4.1` | No |
| Phone, ROS host, or several aircraft on one router | Router STA | Router DHCP address | Run `sta` once |

### 2.1 Direct aircraft AP (recommended first)

Default Wi-Fi credentials after a full erase are:

```text
SSID: open32drone
Password: 12345678
Aircraft IP: 192.168.4.1
MAVLink UDP: 14550
OTA HTTP: 8080
```

Change the AP credentials before using the aircraft outside a controlled lab:

```text
ap NEW_SSID NEW_PASSWORD
```

Reboot to apply the new credentials.

Connect the phone directly to `open32drone`; Android already uses the correct
default aircraft address. Connect the ROS computer to the same AP and use the
default launch command. Stay connected if the operating system warns that the
network has no Internet access.

### 2.2 Router STA (configure only when needed)

Keep direct AP mode for first flight and recovery. To place the aircraft,
Android phone, and ROS computer on one trusted lab router, configure STA over
the USB serial console while disarmed:

```text
sta LAB_SSID LAB_PASSWORD
reboot
```

The command stores both credentials and `WIFI_MODE=2`. After reboot, run
`wifi` over USB and record the DHCP `IP` shown for the aircraft. A DHCP
reservation in the router is recommended so that address does not change.

If STA cannot connect within eight seconds at boot, firmware starts the saved
aircraft AP as a recovery path. It does not overwrite the configured STA mode,
so the next reboot retries the router. Connect to the recovery AP, or use USB,
then return permanently to direct mode with:

```text
ap open32drone 12345678
reboot
```

In the Android app, open **Tools → Aircraft address** and enter the IP printed
by `wifi`; restore `192.168.4.1` when using the direct AP. For ROS 2, pass the
same address as `aircraft_ip:=<address>`. Android and ROS may share the router,
but they must not control the aircraft simultaneously.

## 3. Calibrate this aircraft

The four calibration mechanisms solve different problems and are not
interchangeable. Always remove propellers before calibration.

| Calibration | What it corrects | Stored | When to run |
|---|---|---|---|
| Boot gyro calibration | Zero-rate bias for this power cycle | No | Automatically on every boot |
| `ca` | Bias and scale of this physical accelerometer | Yes | New/erased MCU, new IMU, or remounted IMU |
| `cr` | Channels, directions, and endpoints of this SBUS link | Yes | New transmitter/receiver or full erase |
| `PWR_VOLT_SCALE` | Divider and ADC scale error on this board | Yes | New/erased MCU, repair, or divider change |

### 3.1 Gyro calibration

Place the aircraft on a rigid, motionless surface, power it without touching
the frame, wait for `Gyro calibration complete`, then use `imu` to confirm
`gyro calibrated: 1`. Firmware needs at least 500 fresh samples and two
seconds; it rejects arming until calibration completes.

Common failures are `MOVING` for frame/table motion, `MOTORS_ACTIVE` for
nonzero motor output, `ACC_NORM`/`ACC_NOISE`/`GYRO_NOISE` for orientation,
mounting, or vibration faults, and `SENSOR_BIAS` for output outside the healthy
range. `cg` restarts only the runtime gyro calibration; it does not replace
`ca`.

### 3.2 Six-face accelerometer calibration (`ca`)

Enter `ca`, then place the aircraft as prompted: level, nose up, nose down,
right side, left side, and upside down. Release the aircraft before each sample
window; do not hold it. After `Accelerometer calibration accepted`, place it
level, wait for gyro calibration again, then run:

```text
imu
p IMU_ACC_BIAS_X
p IMU_ACC_BIAS_Y
p IMU_ACC_BIAS_Z
p IMU_ACC_SCALE_X
p IMU_ACC_SCALE_Y
p IMU_ACC_SCALE_Z
```

At rest, `status` should be `OK`, acceleration magnitude should be near
`9.81 m/s²`, and scale values are normally near `1`. Motion, duplicate faces,
implausible gravity, or an out-of-range result rejects the complete candidate
and restores the previous calibration; no partial result is saved.

### 3.3 SBUS calibration (`cr`)

Run `cr` only when using physical SBUS. Power the transmitter, select the
correct model, and complete all eight prompts. Firmware verifies that all five
controls use distinct, usable channels. Then use `rc` to check:

| Input | Expected value |
|---|---|
| Centered roll, pitch, and yaw | Near `0` |
| Throttle minimum / maximum | Near `0` / `1` |
| Mode low / middle / high | About `0` / `0.5` / `1` |

The standard modes are low `STAB`, middle `ALT_HOLD`, and high `POS_HOLD`.
Android/ROS-only aircraft do not need an active transmitter or `cr` to fly.

### 3.4 Voltage scale

With the aircraft disarmed and motors stopped, measure the battery terminals
as `V_DMM`, run `pw` to obtain `V_FW`, then calculate:

```text
new scale = current PWR_VOLT_SCALE × V_DMM / V_FW
```

Write `p PWR_VOLT_SCALE <new value>`, wait at least one second, and run `pw`
again. Aim for less than `0.03 V` error; `0.05 V` is sufficient for the bounded
feed-forward. The fitted defaults `PWR_COMP_REF=3.28`, `PWR_COMP_SLP=0.472`,
and `PWR_COMP_MAX=1.20` are not per-aircraft voltage calibration values; do not
change them without evidence. On an old board without the divider, use
`PWR_VOLT_PIN=-1`; firmware then follows the uncompensated path exactly.

### 3.5 Flow, persistence, and physical faults

TF-0850 has no manual calibration command. Firmware uses the standard axes,
the fixed `24 mm` forward yaw compensation, and a runtime zero-flow bias
learned while disarmed and still. This cannot repair a tilted module, blocked
lens, or unsuitable floor texture/light.

`ca`, `cr`, and voltage scale are stored in NVS. Normal reboot and app OTA
preserve them; full erase and `preset` remove them. A firmware update does not
silently overwrite valid stored values. Save `sys`, `p`, `pw`, `imu`, `flow`,
and `rc` (when used) as the aircraft's bring-up record.

Calibration cannot fix a twisted frame, nonparallel motor axes, loose IMU or
TF-0850, weak motor, damaged propeller, shifted battery, supply sag, or an
intermittent connector. Identical frames may share compiled defaults, but each
physical IMU needs its own `ca`, and each SBUS pairing needs its own `cr`.

## 4. Propeller-off acceptance

Run these checks in order:

| Check | Command or action | Pass condition |
|---|---|---|
| Firmware identity | `sys` | Build is `minimal`; parameter storage is `OK`; loop stays near 300 Hz and deadline lateness remains small under the selected workload |
| IMU | `imu` | Status `OK`, backend/model match the flashed profile, sensor rate is plausible, loop is near 300 Hz, gyro calibrated, acceleration norm near gravity |
| ToF/flow | `flow` | UART packets advance; lifting the aircraft produces a plausible height; flow becomes healthy over a textured floor |
| Battery voltage | `pw` | GPIO is `1`; ADC millivolts are approximately half the DMM battery voltage; reported voltage agrees with the DMM |
| Motor position | `mfr`, `mfl`, `mrr`, `mrl` | Exactly the named motor runs for one second; no propellers installed |
| RC, if used | `rc` | Correct channels, signs, throttle minimum, three mode positions, no failsafe |
| Wi-Fi | `wifi` | UDP bound, correct IP, packet counters move when the selected client runs |
| Android/ROS | client status | Heartbeat connects with every other UDP 14550 client closed |
| Emergency stop | arm only in a restrained propeller-off setup, then disarm | All four outputs return to zero immediately |

Near the floor, the TF-0850 may report a packet inside its specified `20 mm`
blind zone and no numeric height. This is normal. Fresh blind-zone packets are
valid ground evidence for automatic takeoff; do not hold the aircraft in the
air to make the READY display change.

Any wrong motor position, control sign, sensor orientation, or intermittent
packet stream is a hard stop. Correct the hardware or mapping before installing
propellers.

The divider does not estimate a trustworthy remaining percentage and voltage
does not arm, inhibit takeoff, or create a low-battery decision. It does provide
a bounded collective feed-forward for Altitude Hold, Position Hold, and
automatic takeoff. Stabilize/direct-Offboard throttle and attitude corrections
remain unchanged. If a previous app-only installation stored
`PWR_VOLT_PIN=-1`, set it explicitly to `1` while disarmed, or use `preset`/a
complete erase when intentionally returning all registered parameters to
defaults.

NVS also preserves an explicitly stored compensation profile. After updating
this candidate, run `p`: the fitted profile is `PWR_COMP_REF=3.28`,
`PWR_COMP_SLP=0.472`, `PWR_COMP_MAX=1.20`. If an earlier test stored
`PWR_COMP_MAX=1.00`, compensation remains deliberately disabled until that
single value is changed; firmware never overwrites it silently.

## 5. Modes, ownership, and automatic actions

| Mode | Vertical control | Horizontal control | Use |
|---|---|---|---|
| `STAB` (`2`) | Direct throttle, `0..100%` request | Direct roll/pitch attitude and yaw rate | Experienced-pilot diagnosis |
| `ALT_HOLD` (`4`) | ToF hold; center stick holds height | Direct attitude and yaw rate | Manual altitude hold |
| `POS_HOLD` (`5`) | Same altitude control | Sticks command horizontal velocity when flow is valid | Normal indoor flight |
| `AUTO` (`3`) | Internal takeoff, landing, Offboard, or failsafe owner | Depends on the task | Not a standing pilot mode |

Altitude/Position throttle center is fixed at `50%` with a default `±10%`
deadband. Outside it, the stick requests vertical velocity up to `0.45 m/s`,
not direct motor duty. Only one ordinary controller may own the aircraft:
Android and ROS 2 must not control the same aircraft at the same time. Multiple
MAVROS instances on one ROS host use distinct local UDP ports. Deliberate
physical SBUS movement has priority, but static SBUS frames do not steal
Android/ROS ownership. The
physical RC emergency gesture is checked independently.

Automatic takeoff does not jump to 100%: its initial thrust cap is `20%`, it
grows by 45 percentage points/s, and remains bounded by
`ALT_TKO_THR=90%`. See [Firmware reference](../reference/firmware.md) for the
exact thrust, voltage-feed-forward, and landing equations. `STAB` is a separate
direct-throttle path and full stick can still request 100%; never hold high
throttle on the ground or with a blocked propeller.

Automatic takeoff and landing own only vertical motion; fresh pilot roll,
pitch, and yaw remain available. Landing descends at approximately `0.45 m/s`
above `0.30 m` and `0.28 m/s` below it, then enters a one-way flare, detects
touchdown, and disarms without reapplying thrust after a ToF bounce.

## 6. Choose one controller

### 6.1 Physical SBUS

After boot, let the receiver publish neutral controls briefly, then move a
stick or the mode switch to claim control.

- Arm: throttle bottom + yaw full right. The intentional `10%` armed idle is
  not takeoff.
- Stabilize takeoff: select low `STAB` and raise throttle manually.
- Assisted takeoff: select middle Altitude or high Position mode, arm, then
  hold throttle above `62.5%` for `0.20 s`. Firmware climbs to the default
  `0.60 m`.
- Assisted landing: after the aircraft is airborne, hold throttle below `5%`
  for `0.30 s`. Raise it above `60%` to cancel and capture current height.
- Moving the mode switch during an automatic action cancels it and returns to
  pilot mode.
- Emergency stop: first leave the emergency corner, then hold throttle bottom
  + yaw full left for at least `150 ms`.

### 6.2 Android

Install
[`software/releases/minimal/Open32Drone-Controller-0.1.apk`](../../software/releases/minimal/Open32Drone-Controller-0.1.apk),
close ROS and other MAVLink clients, and connect the phone to the
`open32drone` Wi-Fi. Choose to stay connected when Android reports no
Internet. The app does not require a transmitter; its connection indicator
must show live MAVLink, not only Wi-Fi association.

For normal operation, enter a relative height (`0.65` for first flight) and
hold Take off for `0.60 s`. Firmware atomically runs pre-arm, arms, climbs, and
hands over to Position Hold; no separate mode or arm action is required. The
left stick controls climb/descent and yaw, and the right stick controls
forward/back and left/right. Hold Land for controlled landing. Both sticks
remain available during takeoff and landing. In an emergency, hold Emergency
stop, or hold the left stick fully bottom-left for `0.20 s`; motors stop
immediately without a landing attempt.

The top-left label reports the FCU's current standby mode. It may show
Altitude after returning from Android Wi-Fi settings, but the Position-takeoff
button still performs one atomic command and always hands over to Position;
the app does not require a separate mode-selection step.

The height field, Emergency stop, Take off, and Land controls sit at the lower
center with a `16 dp` bottom inset. The visual center stays clear and both stick
areas keep their existing size.

Keep the app in the foreground while it owns the aircraft. For
`ENETUNREACH`, stay on aircraft Wi-Fi, close other UDP 14550 clients, and wait
for automatic route/socket recreation. Turning on the transmitter or touching
the screen is not a networking fix. Grey action buttons normally mean stale
MAVLink or deliberate SBUS ownership.

Android OTA accepts only `Open32Drone-minimal-app.bin`, and only while
disarmed, landed, outside automatic/Offboard control, with every motor stopped
and propellers removed. Never use `merged.bin` for OTA; initial A/B setup needs
the complete USB flash.

### 6.3 ROS 2

ROS is the advanced path after basic flight passes. Installation, lifecycle,
namespaced `cmd_vel`, position, raw RC, TF, RViz, multi-aircraft LAN setup, and
automatic tests are documented in
[ROS 2](06-ros.en.md). Close Android before starting ROS.

## 7. First guarded flight and safety

Before flight, use a fresh, undamaged protected 1S battery; inspect propeller
direction and attachment and each motor shaft; center the battery; confirm the
IMU/TF-0850 are rigid and clean; wait for gyro calibration; use a textured,
evenly lit floor; clear people from at least two metres around the test area;
wear eye protection; and memorize the emergency action.

Install the already verified clockwise/counter-clockwise propellers and keep
only one controller active. For the first flight, use Position Hold with a
`0.65 m` relative target. Observe takeoff verticality, height capture, XY
drift, oscillation, landing, and final motor stop separately. Abort, land, or
emergency-stop after unexpected lean, repeated oscillation, rapid drift, wrong
direction, or telemetry loss.

When arming or takeoff is rejected, read the exact reason instead of retrying.
Required gates include attached motor PWM, healthy parameter storage, fresh
IMU, completed gyro calibration, valid estimates, a healthy selected control
link, low manual throttle, and a fixed 300 Hz control loop that has not fallen
below the 200 Hz safety floor.

Tip-over protection is deliberately small: after arming, tilt over `70°` for
`250 ms` causes immediate disarm; it is not a collision classifier. On link
loss, firmware latches a configured descent and finally disarms. That is a
last-resort fallback, not precision Position landing, and must be tested first
with propellers removed.

After flight, wait for disarm and verify every propeller has stopped before
disconnecting power and touching the aircraft. Inspect motor/connector heat,
propeller damage, frame motion, and loose modules. Save logs before power-off
when investigating an anomaly.

“It left the floor” is not a first-flight pass. Change one variable per flight
and report the exact firmware filename and SHA-256, build ID/commit, effective
`p`, whether `ca`/`cr` were completed, controller, battery, floor/light, phase
where the symptom began, and the disarmed `log dump`. Add `time` and `perf` for
timing issues. State whether evidence is source/build, flash/boot,
propeller-off bench, or physical flight.
