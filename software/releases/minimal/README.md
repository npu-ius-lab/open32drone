# Open32Drone Minimal downloads

[English](README.md) · [简体中文](README.zh-CN.md)

This directory contains the installable set retained for `feat-minimal`. Use
only files from this directory; do not mix in firmware or clients recovered
from older commits.

| File | Purpose |
|---|---|
| `Open32Drone-minimal-merged.bin` | Complete 8 MiB image for an erased/new MCU; flash by USB at offset `0x0` |
| `Open32Drone-minimal-app.bin` | Application-only image for the ground-only A/B OTA endpoint; never upload the merged image through OTA |
| `Open32Drone-Controller-0.1.apk` | Android control/camera client (`versionName 0.1`, `versionCode 1`) |
| `Open32Drone-ROS2-minimal.tar.gz` | ROS 2 `0.1.0` source package with launch, control, telemetry, TF, and test tools |

## Current update

- Both firmware images were rebuilt from the current `feat-minimal` source.
  The standard image uses the MPU6500/MPU9250 FlixPeriph backend, a fixed
  `300 Hz` complete-control schedule, and bounded `150/100/50 Hz`
  MAVLink/CLI/OTA services. Its embedded source SHA-256 binds both downloads to
  the current firmware tree. The tracked build remains AP-first after a complete
  erase; router STA defaults are not embedded in these public artifacts.
- Add operator-selected router STA mode with an eight-second recovery-AP path.
  MAVLink, Android camera, Android OTA, and ROS all use the selected aircraft
  IPv4 address. Only one Android or ROS controller may own an aircraft at a time.
- Add the bounded QVGA, JPEG-quality-10, 10 FPS MJPEG camera candidate. It uses
  a separate LEDC timer/channel, one low-priority core-0 HTTP task, one stream
  client, and no work in the `300 Hz` flight loop. Camera failure remains
  non-fatal.
- Position Hold now applies `POS_STICK_V=0.70 m/s` once to the final XY command
  vector, separates the `2.5 m/s` flow-sample plausibility check from commanded
  speed, and keeps temporary flow-gate fallback inside the existing `12 deg`
  position envelope instead of falling through to the `30 deg` Stabilize
  envelope. Flight logs expose this path as `posFallback`.
- The compiled standard-airframe defaults now match the successful-flight
  values `CTL_R_P=4.47`, `CTL_P_P=4.47`, and `ALT_P=0.747`. Accelerometer,
  transmitter, and voltage-divider calibration remain per device and are not
  copied from the test aircraft.
- ICM20948 and MPU6050 remain source/CI compile profiles, not extra downloads.
  They require separate board and flight validation before distribution.
- The Android client uses a configurable aircraft IPv4 address for MAVLink,
  camera, and OTA. It rejects UDP telemetry/ACKs from other addresses on a
  shared router. The compact control panel and `180 x 135 dp` 4:3 camera window
  sit between the two joysticks; the preview uses `fitCenter` and background
  decode priority. Position takeoff always hands over to `POS_HOLD`.
- The APK was rebuilt from the current Android source. Its package remains
  `com.osrbot.open32drone.controller`, and its signer matches the previous APK,
  so it can be installed as an update.
- ROS 2 now isolates each aircraft by IPv4 address, MAVLink System ID, local UDP
  bind port, ROS namespace, and TF prefix. The reliable telemetry bridge,
  lifecycle services, `cmd_vel`, absolute position, raw RC, RViz, bench test,
  and supervised flight test remain in the archive. Its service/client callback
  groups and MAVROS sensor QoS are safe for asynchronous lifecycle commands;
  its process helper also refuses to signal a stale, reused PID.

## Verify before installation

```bash
shasum -a 256 -c SHA256SUMS
```

Current identity:

- firmware: Arduino-ESP32 `3.3.6`, FlixPeriph `1.10.4`, MAVLink `2.0.25`;
- board: `esp32:esp32:XIAO_ESP32S3` with `PSRAM=opi`,
  `PartitionScheme=default_8MB`, and `FlashMode=dio`;
- Android: `0.1` (`versionCode 1`, debug signed);
- ROS 2 package: `0.1.0`.

The standard firmware and both alternate IMU profiles passed clean sequential
compilation with the pinned ESP32-S3 toolchain; only the standard firmware is
packaged here. The standard artifacts passed source-identity, image-layout,
partition, and A/B rollback-symbol checks. The operator previously confirmed
the retained `4.47 / 4.47 / 0.747` control defaults in flight, but this newly
packaged network/camera artifact still requires its own board, link, camera,
and free-flight confirmation. Alternate IMU profiles remain compile-only.
The APK passed unit tests, lint, build, signature verification, and manifest
readback. It uses a debug signer, so allow this file source on the phone. The
ROS 2 archive exactly matches the current `ros2/` source tree; this packaging
check is not a live ROS deployment or flight result.

The exact firmware/APK pair still needs a device check in both direct-AP and
router-STA modes: verify telemetry comes only from the selected aircraft,
confirm the compact camera preview and retry path, hold Position takeoff,
observe AUTO during the climb, then confirm Position after handover and verify
both sticks before moving farther.

For a first aircraft, use only [Getting started](../../docs/GETTING_STARTED.md):
section 4 chooses direct AP or router STA, and section 8 covers Android. After
the ordinary first flight passes, continue to
[ROS 2 control and development](../../docs/ROS2.md).
