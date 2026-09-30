# Contributing

Contributions are welcome, whether reporting issues, fixing code or improving the tutorials.

## Reporting issues

Search [GitHub Issues](https://github.com/npu-ius-lab/open32drone/issues) before opening a new issue. If the problem has not been reported, include:

1. Firmware, app and ROS 2 software versions; include the source revision if you built it yourself.
2. Controller board, IMU, propeller diameter and control method.
3. Complete steps or commands, expected behavior and actual behavior.
4. Short logs from before and after the fault; for ROS issues, include launch errors, Offboard status and ROS/MAVROS versions.

## Feature requests

Open an Issue describing the use case, what the current implementation is missing and the behavior you would like to see. Discuss the scope and submission approach with the maintainers before starting a large feature or interface change.

## Submitting changes

Create a working branch and keep each pull request focused on one issue. Follow the existing style and avoid unrelated formatting or parameter changes.

A pull request should describe:

- The problem it addresses, with a link to the issue if available.
- What changed and which functions are affected.
- Test methods and results, including anything that has not been verified.

For bug fixes, add a regression test where possible. If an interface changes, check the firmware, Android app and ROS 2 package together. For parameter changes, record the old and new values and the measured effect.

See [Source and build](../reference/source-build.md) for build and test commands.

## License

Contributions follow the licensing scope in [LICENSE.txt](../../LICENSE.txt). When adding third-party material, identify its source and retain its copyright and license notices and update the [Third-party notices](third-party.md).
