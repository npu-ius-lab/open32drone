# Open32Drone 配套下载

[English](README.md) · [简体中文](README.zh-CN.md)

下载文件统一使用 Open32Drone 项目名称，按 USB 刷写、OTA、Android 或 ROS 2 选择对应文件。

| 文件 | 用途 |
|---|---|
| [Open32Drone-20260928-190250-full.bin](Open32Drone-20260928-190250-full.bin) | 新板或完整擦除后的 8 MiB USB 镜像，从 `0x0` 刷写 |
| [Open32Drone-20260928-190250-app.bin](Open32Drone-20260928-190250-app.bin) | 配套分区上的地面 A/B OTA 应用镜像，不能代替完整镜像 |
| [Open32Drone-20260928-190250-android.apk](Open32Drone-20260928-190250-android.apk) | Android 0.1.2，`versionCode 3`，控制、图传与 OTA |
| [Open32Drone-20260928-190250-ros2.tar.gz](Open32Drone-20260928-190250-ros2.tar.gz) | ROS 2 0.1.2 源码包，含键盘、话题控制、遥测和测试工具 |

构建批次：20260928-190250（北京时间）。下载见 [GitHub Releases](https://github.com/npu-ius-lab/open32drone/releases)，刷写步骤见[刷写与首飞](../../docs/guide/04-firmware-flight.md)。

**验证状态：先做拆桨验证。** 本轮补齐急停硬件输出、近地盲区判断、ROS 急停调度和 Android 重连修复；主机回归及构建通过，尚未刷机或实飞。

## 本次整合与版本对应

本套件为 0.1.2 本地测试候选，修复 ToF 掉线油门累加、普通上锁与急停混用、Android 退出和急停状态、ROS 偏航方向，以及 IMU 异常采样、Offboard 接管、触地确认、光流零偏回退、循环超时和 UDP 回包目标问题。固件、APK 和 ROS 必须配套更新；应用包名与签名、实体遥控器手势、IMU 安装旋转和 NVS 参数布局不变。

固件适用于 MPU6500/MPU9250 配置，控制循环为 300 Hz。本次重编译产物尚未做刷机及飞行验证。

仓库测试逐文件比较 ROS 压缩包和 `ros2/`，核对四个文件的 SHA-256，并检查固件内嵌源码身份与 `firmware/` 一致。

## 下载后校验

在本目录执行：

```bash
shasum -a 256 -c SHA256SUMS   # macOS
# sha256sum -c SHA256SUMS    # Linux
```

## 安装与使用

- 固件和 APK 按[刷写与首飞](../../docs/guide/04-firmware-flight.md)操作。
- ROS 包按[ROS 2 教程](../../docs/guide/06-ros.md)安装、编译和启动；更新包后重新构建工作区并重启节点。
- 更换 APK 签名可能需要卸载旧应用；先保留所需设置。
- 每架飞机同一时刻只使用一个 Android 或 ROS 网络飞行控制端。

固件依赖 Arduino-ESP32 3.3.6、FlixPeriph 1.10.4 和 MAVLink 2.0.25；其他 IMU 配置只有构建检查，不代表对应硬件已实飞。
源码编译见[源码与编译](../../docs/reference/source-build.zh-CN.md)，许可范围见[许可证与来源](../../docs/project/third-party.zh-CN.md)。
