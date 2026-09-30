# 第三方声明

Open32Drone 自有代码与文档默认采用 **Apache License 2.0**。完整条款见 [LICENSE](../../LICENSE)，适用范围见 [LICENSE.txt](../../LICENSE.txt)。第三方代码、依赖和素材保留原有许可及版权声明。

## 软件与依赖

| 组件 | 来源与许可 |
|---|---|
| 飞控核心 | 派生自 Oleg Kalachev 的 [Flix](https://github.com/okalachev/flix)。 |
| ROS 2 驱动 | MIT，见 [ros2/package.xml](../../ros2/package.xml)。 |
| IMU/SBUS 库 | FlixPeriph 1.10.4，按上游许可使用；作为构建依赖获取，未随仓库附带。 |
| MAVLink 通信 | MAVLink Arduino 2.0.25、ROS MAVROS，分别遵循各自许可。 |
| 开发工具与运行依赖 | Arduino-ESP32、Android/Gradle、Vue/VitePress、PyTorch 和 Isaac Sim，分别遵循各自许可。 |

## 硬件资源页面

| 资源 | 来源与许可说明 |
|---|---|
| 机架打印配置 | [MakerWorld](https://makerworld.com.cn/zh/models/2922108-open32drone-wu-ren-ji-8520kong-xin-bei-ji-jia-ros2#profileId-3425842)：页面标注 CC BY-NC 4.0，使用时保留来源署名并遵守模型条款。 |
| PCB 设计 | [嘉立创开源硬件](https://oshwhub.com/fanchewang/open32drone)：页面标注 MIT License，另有平台转载和非商业使用提示；使用前需核对工程文件与平台条款。 |
