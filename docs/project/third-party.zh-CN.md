# 第三方声明

Open32Drone 沿用原公开项目的 **Apache License 2.0**，完整条款见 [LICENSE](../../LICENSE)。
本页说明来源和分组件例外，不替代第三方原有许可证。

| 部分 | 来源与当前状态 |
|---|---|
| 飞控核心 | 派生自 Oleg Kalachev 的 [Flix](https://github.com/okalachev/flix)，修改文件保留原声明；分发前需确认适用授权。 |
| Open32Drone 新增部分 | 项目自有的固件扩展、客户端和文档采用 Apache-2.0；文件或组件另有声明的除外。 |
| ROS 2 包 | 保留现有 manifest 的 MIT 分组件声明。 |
| IMU/SBUS 库 | 构建依赖 FlixPeriph 1.10.4，由上游提供，不在此仓库内复制。 |
| MAVLink | 依赖 MAVLink Arduino 2.0.25 与 ROS MAVROS，各自遵循上游条款。 |
| 其他依赖 | Arduino-ESP32、Android/Gradle、Vue/VitePress、PyTorch、Isaac Sim 有各自条款；分发二进制时保留必要声明。 |
| CAD、图片、视频 | 贡献者与供应商素材权利独立于软件许可，保留适用署名和条款。 |

根目录 LICENSE 与原公开 Open32Drone 仓库保持一致，不对第三方代码和素材重新授权。
保留原作者和来源声明；派生代码公开分发前仍需确认适用的 Flix 上游授权。

## 硬件资源页面

| 资源 | 来源与页面标注 |
|---|---|
| 机架打印配置 | [MakerWorld](https://makerworld.com.cn/zh/models/2922108-open32drone-wu-ren-ji-8520kong-xin-bei-ji-jia-ros2#profileId-3425842)标注 CC BY-NC 4.0，保留来源署名及适用模型条款。 |
| PCB 设计 | [嘉立创开源硬件](https://oshwhub.com/fanchewang/open32drone)标注 MIT License，同时存在平台转载和非商业使用提示。使用时核对工程文件与适用条款，本表不替代对这些差异的确认。 |

以上为 2026-09-15 核对的资源页面标注，不表示将硬件改为仓库的 Apache-2.0 许可。引用链接不改变资源许可，二进制和媒体分发保留适用的第三方条款。
