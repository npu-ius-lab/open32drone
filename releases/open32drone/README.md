# Open32Drone downloads

[English](README.md) · [简体中文](README.zh-CN.md)

Downloads use the Open32Drone project name. Select the file for USB flashing, OTA, Android or ROS 2 below.

| File | Purpose |
|---|---|
| [Open32Drone-20260928-190250-full.bin](Open32Drone-20260928-190250-full.bin) | Complete 8 MiB USB image for a new or erased board; flash at `0x0` |
| [Open32Drone-20260928-190250-app.bin](Open32Drone-20260928-190250-app.bin) | Ground-only A/B OTA application for the matching partition layout; not a full image |
| [Open32Drone-20260928-190250-android.apk](Open32Drone-20260928-190250-android.apk) | Android 0.1.2, `versionCode 3`; control, video and OTA |
| [Open32Drone-20260928-190250-ros2.tar.gz](Open32Drone-20260928-190250-ros2.tar.gz) | ROS 2 0.1.2 with keyboard/topic control, telemetry and test tools |

Build batch: 20260928-190250 (UTC+8). See [Firmware and first flight](../../docs/guide/04-firmware-flight.en.md) for installation links.

**Validation: propellers-off testing first.** This batch adds immediate hardware stop, near-ground blind-zone recovery, independent ROS emergency scheduling and Android reconnection fixes. Host regressions and builds pass; flashing and aircraft validation remain pending.

## Package identity

This 0.1.2 local test candidate fixes ToF-dropout thrust accumulation, ordinary versus forced disarm, Android exit/emergency state, ROS yaw direction, invalid IMU samples, Offboard handover, touchdown confirmation, flow-bias fallback, control-loop timeout, and UDP reply selection. Update firmware, APK, and ROS together. Application ID/signing key, physical RC gestures, IMU mounting rotation, and NVS parameter layout are unchanged.

Firmware targets the MPU6500/MPU9250 configuration with a 300 Hz control loop. These rebuilt artifacts have not been flashed or flight-tested.

Repository tests compare every archived ROS source file with `ros2/`, verify all four SHA-256 hashes, and match the embedded firmware identity with `firmware/`.

## Verify downloads

Run in this directory:

```bash
shasum -a 256 -c SHA256SUMS   # macOS
# sha256sum -c SHA256SUMS    # Linux
```

## Installation

- Follow [firmware and first flight](../../docs/guide/04-firmware-flight.en.md) for firmware and APK.
- Follow the [ROS 2 guide](../../docs/guide/06-ros.en.md) to install, build and launch the package. Rebuild and restart nodes after an update.
- A different APK signing key may require uninstalling the previous app; preserve necessary settings first.
- Use only one Android or ROS network flight controller per aircraft at a time.

Firmware dependencies are Arduino-ESP32 3.3.6, FlixPeriph 1.10.4 and MAVLink 2.0.25. Alternate IMU builds are not hardware flight validation. See [source and build](../../docs/reference/source-build.md) and [licenses and origins](../../docs/project/third-party.md).
