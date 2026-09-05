# Open32Drone Minimal 下载

[English](README.md) · [简体中文](README.zh-CN.md)

这是 `feat-minimal` 分支保留的安装套件。请只使用本目录中的同套文件，不要混入旧提交
恢复出的固件或客户端。

| 文件 | 用途 |
|---|---|
| `Open32Drone-minimal-merged.bin` | 新 MCU 或完整擦除后的 8 MiB 完整镜像；通过 USB 写入 `0x0` |
| `Open32Drone-minimal-app.bin` | 地面 A/B OTA 使用的应用镜像；禁止通过 OTA 上传 merged 镜像 |
| `Open32Drone-Controller-0.1.apk` | Android 控制/图传客户端（`versionName 0.1`、`versionCode 1`） |
| `Open32Drone-ROS2-minimal.tar.gz` | ROS 2 `0.1.0` 源码包，包含启动、控制、遥测、TF 和测试工具 |

## 本次更新

- 两份固件已从当前 `feat-minimal` 源码重新构建。标准镜像使用
  MPU6500/MPU9250 FlixPeriph 后端、固定 `300 Hz` 完整控制节拍，以及有界的
  `150/100/50 Hz` MAVLink/CLI/OTA 服务；内嵌源码 SHA-256 会把两份下载文件绑定到
  当前固件源码树；完整擦除后仍默认启动飞机 AP，公开产物不包含私有路由器默认凭据；
- 新增由操作员明确选择的路由器 STA 模式；八秒连接失败后打开恢复 AP。MAVLink、
  Android 图传、Android OTA 和 ROS 都使用所选飞机 IPv4 地址；同一架飞机同一时刻
  只能由 Android 或 ROS 之一控制；
- 新增有界 QVGA、JPEG 质量 10、10 FPS MJPEG 图传候选。摄像头使用独立 LEDC
  定时器/通道、一个低优先级 core-0 HTTP 任务、一个观看端，`300 Hz` 飞控循环中没有
  摄像头工作；摄像头失败不会阻止飞控启动；
- 定点现在只在最终 XY 命令向量上执行一次 `POS_STICK_V=0.70 m/s` 限幅；
  `2.5 m/s` 光流样本合理性检查不再充当飞行速度上限；光流门短时不可用时仍保持在
  原有 `12°` 定点姿态范围内，不会跌入 `30°` 姿态模式范围；日志用 `posFallback`
  标记这条路径；
- 标准机架编译默认值同步为成功实飞参数：`CTL_R_P=4.47`、`CTL_P_P=4.47`、
  `ALT_P=0.747`；加速度计、遥控器和电压分压校准仍按设备分别完成，不复制测试飞机的
  单机校准值；
- ICM20948 和 MPU6050 只保留为源码/CI 编译配置，不额外发布下载；必须先完成对应板卡
  和飞行验证才能分发；
- Android 使用一个可配置飞机 IPv4 地址统一访问 MAVLink、图传和 OTA，并拒绝同一
  路由器上其他地址发来的 UDP 遥测/ACK。紧凑控制面板和 `180 x 135 dp` 4:3 图传窗口
  位于两个摇杆之间；画面使用 `fitCenter` 和后台解码优先级；定点起飞固定交接
  `POS_HOLD`；
- APK 已从当前 Android 源码重新构建；包名仍是
  `com.osrbot.open32drone.controller`，签名与上一包一致，可以直接覆盖安装；
- ROS 2 按飞机 IPv4、MAVLink System ID、本机 UDP 端口、ROS 命名空间和 TF 前缀
  隔离多机。压缩包保留可靠遥测桥、生命周期服务、`cmd_vel`、绝对位置、原始 RC、
  RViz、拆桨测试和受监督飞行测试；服务/客户端回调组与 MAVROS 传感器 QoS 已适配
  异步生命周期命令，进程工具也不会向被复用的旧 PID 发送信号。

## 安装前校验

```bash
shasum -a 256 -c SHA256SUMS
```

当前身份：

- 固件：Arduino-ESP32 `3.3.6`、FlixPeriph `1.10.4`、MAVLink `2.0.25`；
- 开发板：`esp32:esp32:XIAO_ESP32S3`，`PSRAM=opi`、
  `PartitionScheme=default_8MB`、`FlashMode=dio`；
- Android：`0.1`（`versionCode 1`，调试签名）；
- ROS 2 包：`0.1.0`。

标准固件和两个替代 IMU 配置均已用固定 ESP32-S3 工具链干净、顺序编译；本目录只打包
标准固件。标准镜像已通过源码身份、镜像布局、分区和 A/B 回滚符号检查。操作员此前已
实飞确认保留的 `4.47 / 4.47 / 0.747` 控制默认值，但这次重新打包的网络/摄像头产物
仍需单独完成板级、链路、图传和带桨实飞确认；替代 IMU 配置仍然只有编译证据。
APK 已通过单元测试、Lint、构建、签名校验和清单回读；APK 使用调试签名，手机需要
允许当前文件来源。ROS 2 压缩包与当前 `ros2/` 源码树逐文件一致；打包检查不等于已经
部署 ROS 或完成 ROS 飞行。

这套精确固件/APK 仍需分别在飞机直连 AP 和路由器 STA 下做设备确认：核对遥测只来自
所选飞机，检查小窗图传与断线重连，长按“定点起飞”，观察爬升期间为 AUTO、交接后
变成定点，再在扩大动作前确认双摇杆均可用。

第一次使用只看[快速开始](../../docs/GETTING_STARTED.zh-CN.md)：其中第 4 节说明
飞机直连热点和路由器 STA 如何二选一，第 8 节说明 Android。普通首飞通过后，再进入
[ROS 2 控制与开发](../../docs/ROS2.zh-CN.md)。
