# Open32Drone downloads

[English](README.md) · [简体中文](README.zh-CN.md)

The `minimal` directory and filenames are retained for existing download and
update compatibility. The project is called Open32Drone, not a separate Minimal edition.

## Publication preview — not a new flight-validated release

Use the files in this directory as one set. See the project's [Apache-2.0 license](../../../LICENSE)
and [third-party terms](../../../docs/project/third-party.md). This is a publication
preview, not a new flight-validated release.

| File | Use |
|---|---|
| `Open32Drone-minimal-merged.bin` | Complete 8 MiB USB image; new/erased MCU, offset `0x0` |
| `Open32Drone-minimal-app.bin` | Application-only, ground-only A/B OTA; never flash it at `0x0` |
| `Open32Drone-Controller-0.1.apk` | Android 0.1 (`versionCode 1`), debug signed |
| `Open32Drone-ROS2-minimal.tar.gz` | ROS 2 source package 0.1.0 |

```bash
shasum -a 256 -c SHA256SUMS
```

### What changed in this preview

- Flight firmware source, parameters and hardware mappings are unchanged.
  Both binaries were rebuilt with anonymous compiler paths. Their hashes differ
  from earlier downloads, so previous flight results do not validate these bytes.
- The APK is unchanged. Its signing certificate is public verification material,
  not the private signing key. Installing another developer's debug build may
  require uninstalling this app first; export any settings you need.
- ROS runtime source is unchanged; the package project URL was updated and the
  archive regenerated. Package contents are checked against the `software/ros2/` tree.

### Versions and validation boundary

| Component | Build/identity | Remaining verification |
|---|---|---|
| Firmware | Arduino-ESP32 3.3.6, FlixPeriph 1.10.4, MAVLink 2.0.25; standard MPU6500/MPU9250 profile | Rebuilt preview requires boot, bench and flight checks; no hardware deployment was performed for this cleanup |
| Android | 0.1, API 26+, compiled with SDK 35, debug signed | No new APK build or device test in this cleanup |
| ROS 2 | 0.1.0, MAVROS; source archive | Test against the installed ROS/MAVROS environment; software checks do not prove a flight |

Firmware board options: `esp32:esp32:XIAO_ESP32S3:PSRAM=opi,PartitionScheme=default_8MB,FlashMode=dio`.
The source hash embedded in both binaries is checked by the repository tests.
Builds use AP mode with empty compiled STA defaults; do not distribute a
router-personalized image or a Flash/NVS backup.

### Known limitations

- A real FCU ACK and actual AUTO telemetry confirmation are required for ROS
  Offboard. Continuous velocity control can terminate if position, state,
  heartbeat or setpoint freshness is lost. Environment-specific timeout reports
  are not fully resolved. Land and preserve a short diagnostic excerpt rather
  than repeatedly restarting motion or increasing watchdog thresholds.
- Optical flow depends on lighting/texture; ToF is distance to the surface below.
- One Android or ROS network flight owner, and one camera viewer per aircraft.
- Alternate IMU profiles are source/build options, not additional validated downloads.
- Complete URDF/USD assets and pretrained policies are not part of this package.

Start with [Getting started](../../../docs/guide/04-firmware-flight.en.md), then
[ROS 2](../../../docs/guide/06-ros.en.md). See [Development](../../../docs/reference/source-build.md)
to rebuild the exact component you change.
