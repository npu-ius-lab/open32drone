# Third-party notices

Open32Drone follows the existing public project's **Apache License 2.0**;
see [LICENSE](../../LICENSE). This file documents provenance and exceptions, rather
than replacing third-party licenses.

| Component | Source / current status |
|---|---|
| Flight-control core | Derived from [Flix](https://github.com/okalachev/flix), by Oleg Kalachev. Original notices remain in the modified files. Confirm the applicable upstream grant before redistribution. |
| Open32Drone additions | Project-owned firmware extensions, clients and documentation use Apache-2.0, except where a file or component states otherwise. |
| ROS 2 package | Its existing manifest declares MIT; that component-specific declaration is retained. |
| IMU/SBUS library | Build dependency FlixPeriph 1.10.4; distributed by its own upstream, not vendored here. |
| MAVLink | Build dependency MAVLink Arduino 2.0.25 and ROS MAVROS; each retains its applicable upstream terms. |
| Other dependencies | Arduino-ESP32, Android/Gradle, Vue/VitePress, PyTorch and Isaac Sim have separate terms. Pin versions and preserve required notices when distributing binaries. |
| CAD, images and video | Contributor and supplier rights are distinct from the software license. Preserve applicable attribution and terms. |

The root LICENSE matches the existing public Open32Drone repository. It does
not relicense third-party code or media. Retain original authorship and notices;
confirm the applicable Flix grant before public redistribution of derived code.

## Hardware resource pages

| Resource | Source and displayed terms |
|---|---|
| Frame print profile | [MakerWorld](https://makerworld.com.cn/zh/models/2922108-open32drone-wu-ren-ji-8520kong-xin-bei-ji-jia-ros2#profileId-3425842) displays CC BY-NC 4.0. Preserve the source attribution and applicable model terms. |
| PCB design | [JLC Open Hardware](https://oshwhub.com/fanchewang/open32drone) displays MIT License, alongside platform notices concerning reproduction and non-commercial use. Consult the project files and applicable terms; this table does not resolve those differences. |

These are the resource-page labels checked on 2026-09-15, not a relicensing of the hardware under the repository's Apache-2.0 license. Linking to a resource does not change its license. Distributed binaries and media retain applicable third-party terms.
