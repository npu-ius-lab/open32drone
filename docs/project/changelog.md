# Changelog

User-facing changes for the matched Open32Drone stack.

## Documentation updates

- Added a per-aircraft shopping list with quantities, selection notes and supplier links.
- Added direct MakerWorld printing and JLC PCB project links.
- Organized the guide from building and first flight to tuning, ROS 2 and source development.
- Simplified chapter navigation and the assembly page outline; each sidebar uses one language.
- Kept detailed instructions in the documentation site, with a concise project introduction in README.

These updates do not change flight firmware or client behavior. See the [download notes](../../software/releases/minimal/README.md) for installation requirements and outstanding validation.

## Current capabilities

- Fixed 300 Hz ESP32-S3 control, build-selected I²C IMU, TF-0850 estimation,
  altitude/position hold and automatic takeoff/landing.
- Manual stick authority during automatic flight, SBUS priority, failsafe
  descent, tip-over stop, voltage sensing and bounded compensation.
- Android 0.1 with atomic Position Hold takeoff, selected-aircraft UDP routing,
  configurable aircraft address and a compact optional camera preview.
- ROS 2 package 0.1.0 with telemetry, TF/RViz, lifecycle, velocity, position
  and raw RC interfaces.
- Real FCU ACKs for ROS mode/lifecycle requests; fresh position feedback,
  streamed warmup and actual AUTO confirmation before Offboard activation.
  Rejections retry within a bounded window; late ACKs cannot restart an abort.
- Ground-only A/B OTA, AP-first networking, optional STA and recovery AP.
- Frame models and BOM for 8520 motors with matched 60 mm or 65 mm propellers.

These entries describe implementation, not universal hardware validation.
See the download notes for the exact artifacts' validation status.
