# 参与项目

## 反馈可复现问题

在 [GitHub Issues](https://github.com/npu-ius-lab/open32drone/issues) 提供：

1. 下载文件/源码版本，以及固件、APK、ROS 包身份。
2. 主板、IMU、桨叶直径、控制方式（SBUS/Android/ROS）。
3. 完整操作步骤或命令、预期和实际结果。
4. 故障前后短日志；ROS 问题附启动错误、Offboard 状态及 ROS/MAVROS 版本。
   加载包路径中的个人目录部分用占位符替换。

不要上传路由器密码、令牌、Flash/NVS 全量备份、签名私钥、个人主目录名、
含无关人员的照片或完整桌面截图。先脱敏再提交，不为补日志重复危险飞行。

## 修改代码

一个改动对应一个问题。保留飞控/客户端控制权契约，行为变化同步更新中英文说明。
调参需要记录改前改后及测量效果，不混入无关增益修改。

仓库根目录运行：

```bash
python3 -m unittest discover -s software/tests -v
git diff --check
npm ci
npm run docs:build
```

固件、APK、ROS 构建见[开发指南](../reference/source-build.zh-CN.md)。
软件检查不能代表新二进制的硬件效果；说明实际测试范围和未验证项。

本地构建进入忽略的 `output/`，不要提交飞行采集数据和机器配置。
可重复回归测试放在 `software/tests/` 或 Android 测试源码，不加入一次性实验脚本。

引入第三方代码和素材前阅读[来源与许可](third-party.zh-CN.md)，
保留作者署名和适用条款。
