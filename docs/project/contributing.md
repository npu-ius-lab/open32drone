# Contributing

## Report a reproducible problem

Use [GitHub Issues](https://github.com/npu-ius-lab/open32drone/issues). Include:

1. Download/source version; firmware, APK or ROS package in use.
2. Board, IMU, propeller diameter and control method (SBUS/Android/ROS).
3. Exact steps or command, expected behavior and actual behavior.
4. A short relevant log excerpt; for ROS, include the launch error and Offboard
   status around the failure. Include ROS/MAVROS versions and loaded package path
   with the personal part of the path replaced by a placeholder.

Do not post router credentials, tokens, full flash/NVS dumps, signing keys,
personal home-directory names, unrelated people in photos or complete desktop
captures. Redact these before upload. Do not rerun a hazardous flight merely
to obtain a log.

## Change code

Keep one issue per change. Preserve the flight/client ownership contract and
update both language references when behavior changes. For tuning, record the
old/new values and the measured effect; do not bundle unrelated gain changes.

From the repository root:

```bash
python3 -m unittest discover -s software/tests -v
git diff --check
npm ci
npm run docs:build
```

Firmware, APK and ROS builds are described in [Development](../reference/source-build.md).
A software check does not prove a new binary's hardware behavior. Document what
was actually tested and what remains unverified.

Keep local builds under ignored `output/`; do not add flight captures or
machine configuration. Put reusable regression tests in `software/tests/` or Android's
test source, not one-off experiment scripts.

Check [source/license status](third-party.md) before contributing third-party
code or assets. Credit the original author and include the applicable terms.
