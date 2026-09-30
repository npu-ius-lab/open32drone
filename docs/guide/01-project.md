# 01 · 项目介绍

## Open32Drone 是什么

Open32Drone 是一个用于教学、科研和个人制作的开源微型四旋翼项目。从打印机架、焊接电路开始，你可以组装一架飞机，再用手机、遥控器或 ROS 2 控制它。

项目提供机架模型、PCB 设计入口、飞控固件、Android 应用、ROS 2 功能包和图文教程。制作过程中可以结合源码了解原理，也可以修改代码，尝试自己的实验。

![Open32Drone 整机：打印机架、飞控底板与四个空心杯电机](/media/photos/drone-complete.jpg)

## 可以做什么

- **室内飞行**：利用 IMU、光流和 ToF 实现姿态稳定、定高、定点及自动起降。
- **手机或遥控器控制**：使用 Android 应用，也可以选配 SBUS 接收机和遥控器。
- **ROS 2 编程**：读取传感器和飞行状态，用速度、位置及起降指令编写自己的飞行实验程序。
- **仿真与强化学习**：通过独立的数值控制和 PPO 练习，学习模型、控制器与策略训练。

## 系统组成

飞机以 XIAO ESP32-S3 为主控，搭配 IMU、光流/ToF 模块、四个 8520 空心杯电机和 1S 电池。3D 打印机架承载零件，飞控底板负责模块连接、供电和电机驱动。

手机、遥控器和 ROS 2 与飞机的连接方式如下：

![系统组成：遥控器、Android 或 ROS 2 连接飞控，传感器提供测量，飞控底板驱动四个电机](/media/figures/system-overview.svg)

Android 和 ROS 2 通过 Wi-Fi 连接飞机，同一时刻选择其中一个控制。手机控制不需要另配遥控器。

## 开始制作

第一次制作时按下表顺序操作；已经完成组装和首飞，可以直接进入调参、ROS 2 或仿真章节。

| 阶段 | 主要内容 |
|---|---|
| [02 · 开始制作](03-hardware.md) | 采购器件、打印机架、制作 PCB、焊接与组装 |
| [03 · 固件刷写与首飞](04-firmware-flight.md) | 下载固件和 APK，刷写、校准并完成首次飞行 |
| [04 · 调参与排查](05-tuning.md) | 根据飞机异常现象检查和调整代码与参数 |
| [05 · ROS 2 控制](06-ros.md) | 联网、读取数据并编程控制飞机 |
| [06 · 仿真与强化学习](07-rl.md) | 运行数值仿真，学习控制与策略训练 |

软件可以在 [GitHub Releases](https://github.com/npu-ius-lab/open32drone/releases) 下载；准备修改程序时，再看[源码与编译](../reference/source-build.zh-CN.md)。

## 参与项目

欢迎分享制作经验、反馈问题、完善教程或贡献代码。

[开源仓库](https://github.com/npu-ius-lab/open32drone) · [问题反馈](https://github.com/npu-ius-lab/open32drone/issues) · [参与贡献](../project/contributing.zh-CN.md)

本项目自有代码与文档默认采用 [Apache License 2.0](../../LICENSE)。测试方法或其他组件另附许可证时，遵循各自条款；第三方代码、硬件资源和媒体素材保留原有许可。适用范围见 [LICENSE.txt](../../LICENSE.txt)，来源及例外见[第三方声明](../project/third-party.zh-CN.md)。
