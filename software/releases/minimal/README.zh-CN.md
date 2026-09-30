# Open32Drone 下载

[English](README.md) · [简体中文](README.zh-CN.md)

目录及文件名中的 `minimal` 暂时保留，用于兼容现有下载链接和升级流程。
项目统一称为 Open32Drone，不再区分所谓 Minimal 版本。

## 公开整理预览，不是新的实飞确认版本

本目录文件配套使用，项目采用 [Apache-2.0](../../../LICENSE)，保留[第三方许可](../../../docs/project/third-party.zh-CN.md)。
此版本用于公开预览，不是新的实飞确认版本。

| 文件 | 用途 |
|---|---|
| `Open32Drone-minimal-merged.bin` | 完整 8 MiB USB 镜像，新板/擦除后从 `0x0` 刷写 |
| `Open32Drone-minimal-app.bin` | 仅用于地面 A/B OTA 的应用镜像，不能刷到 `0x0` |
| `Open32Drone-Controller-0.1.apk` | Android 0.1，`versionCode 1`，调试签名 |
| `Open32Drone-ROS2-minimal.tar.gz` | ROS 2 0.1.0 源码包 |

```bash
shasum -a 256 -c SHA256SUMS
```

### 本次预览改了什么

- 飞控源码、参数和映射不变。两个二进制仅用匿名编译路径重新构建，
  哈希已改变，不能用此前实飞结果代替这些新文件的验证。
- APK 原样保留。签名证书属于公开验证材料，不是签名私钥。
  自行编译的调试 APK 可能需要先卸载此应用才能安装，注意保留需要的设置。
- ROS 运行逻辑不变，只更新项目 URL 并重打包，压缩包逐文件对应 `software/ros2/`。

### 版本与验证范围

| 部分 | 构建与身份 | 未完成范围 |
|---|---|---|
| 固件 | Arduino-ESP32 3.3.6、FlixPeriph 1.10.4、MAVLink 2.0.25，标准 MPU6500/MPU9250 配置 | 重编译预览需启动、台架及飞行验证；本次未部署硬件 |
| Android | 0.1，最低 API 26、编译 SDK 35、调试签名 | 本次未重编 APK，也未做手机测试 |
| ROS 2 | 0.1.0、MAVROS、源码压缩包 | 应核对实际 ROS/MAVROS 环境；软件测试不等于飞行测试 |

主板参数为 `esp32:esp32:XIAO_ESP32S3:PSRAM=opi,PartitionScheme=default_8MB,FlashMode=dio`。
仓库测试核对两个固件内嵌的源码哈希。公开构建默认 AP、编译时 STA 信息为空；
不要分发含个人路由器设置的固件或 Flash/NVS 备份。

### 已知限制

- ROS Offboard 需要真实 FCU ACK 和实际 AUTO 遥测确认；位置、状态、心跳或设定值
  失去新鲜度会终止持续速度控制。部分环境的超时反馈尚未完全闭环。
  出现问题先降落并保留短日志，不连续重启移动，也不加大看门狗阈值掩盖问题。
- 光流依赖纹理和光照；ToF 测下方表面。
- 每机仅一个 Android 或 ROS 网络飞行控制端，相机仅一个观看端。
- 其他 IMU 是源码/构建选项，不是另外的已验证下载。
- 完整 URDF/USD 资产与预训练策略不包含在本包。

先按[入门说明](../../../docs/guide/04-firmware-flight.md)完成首飞，再用
[ROS 2](../../../docs/guide/06-ros.md)。重编译见[开发指南](../../../docs/reference/source-build.zh-CN.md)。
