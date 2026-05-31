# Open32Drone：从 0 到稳飞

<p align="center">
    <img src="img\drone.PNG" alt="Full drone view" />
</p>

<p align="center">
  <strong>
    <a href="./tutorial_zh_CN.md">简体中文</a> &nbsp;|&nbsp;
    <a href="./tutorial.md">English</a>
  </strong>
</p>


## 一、项目简介

**Open32drone** 是一个基于**ESP32-S3**开发的高性能、低成本、科研教育级开源微型无人机平台。

本项目基于开源项目[Flix](https://github.com/okalachev/flix/tree/master)进行二次开发，保留了其轻量化的代码架构，并在此基础上引入了光流与 ToF 传感器，实现无人机室内环境下的定点与定高飞行。Open32drone 支持 MAVLink 协议与 ROS 接入，旨在为开发者提供一个低成本、高可扩展性的微型飞行器实验平台，适用于无人机控制理论学习、集群算法验证及室内导航研究。

## 二、项目教程

### 阶段一：硬件与装配

#### 器材准备

##### 主控模块

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=ZjQ5YjI1YTI2MjQ1N2M0OWY2MTRjNWMyM2RlNGQyOTJfMTc0ZGZlN2M4MDFmM2YwZWJiZDcxNzhiMjJmOTkzZDRfSUQ6NzY0NTg5MzkzODIwNDc4OTkzOV8xNzgwMjIxMTU1OjE3ODAzMDc1NTVfVjM)

规格型号：Seeed Studio XIAO ESP32-S3 Sense

参考价格：90元

模块说明：https://wiki.seeedstudio.com/cn/xiao_esp32s3_getting_started/

##### 机架桨叶

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=MDgwYTRlNDJhMTE2MGVlMWRjNzE1ZDM1YWRlMzdmY2NfM2ZlOTA0NTZiOTc0NzUwZjUwODg3ZjA5YTNhMzIzMDlfSUQ6NzY0NTg5MzkzNjIwNDMwMzU2NF8xNzgwMjIxMTU1OjE3ODAzMDc1NTVfVjM)

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=ZDQ3OTMwMjg5ZTk3ZWNhYzU3YTJjZWUzNzQ1MWFiYWRfODQ5MTVkNDMyMWMyMDM5MzQ0YTQ1MjgyNzI1YWZlMjdfSUQ6NzY0NTg5MzkzNzY0Njk5NjY3Nl8xNzgwMjIxMTU1OjE3ODAzMDc1NTVfVjM)

规格型号：12.3cm轴距机架，76mm桨叶（适配1mm轴径电机）

需求：1个机架，4个桨叶

参考价格：19元（1套价格）

##### 空心杯电机

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=NTMzZGQ3MDNiOWQwYzgzNWFjYzc0YzkwZGVkYmZlMjBfZDU2YzcyM2Q5ZWM4MmZmZTRkZjlkNWNlNzUyNTA5OTNfSUQ6NzY0NTg5MzkzNTU5MTk1MTUzOF8xNzgwMjIxMTU1OjE3ODAzMDc1NTVfVjM)

规格型号：8520空心杯电机，轴径1mm

需求：4个

参考价格：24元（4个价格）

##### IMU模块 十轴传感器

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=YWY1MGU4MDNjZTE0NzU4MGI5NDU4NTRjYjBiNDdjNjJfNGNjNGVlOGI1YWFmMTQwMmQzYzIxMzY1NjNhZWJkYzFfSUQ6NzY0NTg5MzkzNzE2MDM5MTg3NF8xNzgwMjIxMTU1OjE3ODAzMDc1NTVfVjM)

规格型号：GY-91 九轴MPU9250+BMP280气压计

需求：1个

参考价格：14元（1个价格）

##### 升压模块

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=NWJkNGUwODQxZmZkNmJkODZkZjgyMjMwOGUzMmJhYzVfNDhhN2ZiM2I5MzEwYTFmMjQ5MzRiZDllNGZlODczZGJfSUQ6NzY0NTg5MzkzNjQyNjYwMTY2NV8xNzgwMjIxMTU1OjE3ODAzMDc1NTVfVjM)

规格型号：3.3V升压5V模块

需求：1个

参考价格：4.5元（1个价格）

##### TOF光流模块

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=MzZiZDhmYjk5YmI5NmEwZjQ4ZDgyMWU5YzdiYzM3MGZfZTg0Y2JmNzJiY2IzNmUzMDUzMTJkYmZlMWNjMDU4YjNfSUQ6NzY0NTg5MzkzNjY3OTgwMDAxMl8xNzgwMjIxMTU1OjE3ODAzMDc1NTVfVjM)

规格型号：CORVON link协议

需求：1个

参考价格：68元（1个价格）

##### 电机驱动芯片

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=YzI3NDQyMGIyZTU0ZmQ0MmJkYzYxNTdmNjIyZjlmZTlfZWY3YmM1OWQxOWNkYTQ5Y2EyNDMxM2E3YWI3MGQ1MzNfSUQ6NzY0NTg5MzkzNzQ0MTUwODU3Nl8xNzgwMjIxMTU1OjE3ODAzMDc1NTVfVjM)

规格型号：MOS场效应管AO3400

需求：4个

参考价格：0.4元（4个价格）

##### 遥控器

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=ZjBmMjJmNzlmNGJlMDVkZTBiYTMyZTQxZTQ4ZTI2M2VfMjc2YzRiYmU0MjhiZmZkMmM2ZmIyMWYxODVkNmZkNzJfSUQ6NzY0NTg5MzkzNjg5MjAzODM0OF8xNzgwMjIxMTU1OjE3ODAzMDc1NTVfVjM)

规格型号：福斯I6S单控

需求：1个

参考价格：249元（1个价格）

##### 接收器

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=MDQ3ZjM4NDVmN2U1OWRmNDk4NmEzNmUxNjc4YjE3N2JfYzgyYmEwYTk3NTY5OGYxOTU0YWQ2MWUxYmFhN2VmZGVfSUQ6NzY0NTg5MzkzOTA3NzIyMTU5M18xNzgwMjIxMTU1OjE3ODAzMDc1NTVfVjM)

规格型号：福斯A8S接收器，SBUS接收器

需求：1个

参考价格：65元（1个价格）

##### 其他物料

电机插座，电池，排针，排母若干

#### 2. 底板加工

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=MTFjZmRkZTRiOWVhNzM4ZjU1MGZhODM5YWU1Mjg5MmFfMGUyNTM4Y2QzODllMTAyM2U2Zjk2NWY3ODYyMjRjMjBfSUQ6NzY0NTg5MzkzNjY3MzgwMzQ4NF8xNzgwMjIxMTU1OjE3ODAzMDc1NTVfVjM)

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=N2FkMmNkOWYyMGY2MTUwMTNhYzI0YTI0ODViN2Y1NDlfNjEzYTRhZDc4NTAxZjFhMTNmZjBlYmUyM2ZjZDNhZmZfSUQ6NzY0NTg5MzkzODU4NjQyMjQ4N18xNzgwMjIxMTU1OjE3ODAzMDc1NTVfVjM)

规格型号：自制

需求：需要1个

图纸链接：https://oshwhub.com/fanchewang/open32drone

##### 2.1 打开设计图

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=MGI5ZDMxZjViNDMxZDU1Zjk1ZjE0MTMwNjM3MzUyZDVfM2I3NjliMmEwNThmMGEyZTkwNzg3YzZjZDAzMjE1ODNfSUQ6NzY0NTg5MzkzNzc1MTkxOTgwOV8xNzgwMjIxMTU1OjE3ODAzMDc1NTVfVjM)

##### 2.2 PCB下单

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=NzZlY2RmMGUzZTE1YWUwNzM0ZTM3NGMwMjhjY2FlMGFfZTMyMDNlM2NkODNjM2M5MThiYThkODViNjI1YjI3OWZfSUQ6NzY0NTg5MzkzNTUzMzI5NjgzM18xNzgwMjIxMTU1OjE3ODAzMDc1NTVfVjM)

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=MTI0ZTgyMTYxMzBiZmU3YjM2Y2YxMDNkZWM4ZjEzZWFfMTdkNTU3ZTZlODVjZGM5NGIxMGYzNzE5Zjk3MGI3ZmFfSUQ6NzY0NTg5MzkzODI5NzA0ODI2OF8xNzgwMjIxMTU1OjE3ODAzMDc1NTVfVjM)

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=YjFjNTkyMGNmOWM3ZTlhNmFhZGUyODAwYzUwYmVjMDFfNmJjYTgxNzFhZmMxMDQyNTJjYjFlYTA1ZmYxZTVhMWNfSUQ6NzY0NTg5MzkzNjIyNjg2NDM0OF8xNzgwMjIxMTU1OjE3ODAzMDc1NTVfVjM)

其他基本默认选择

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=NzZiMmJiNTY5YTdkMDUzODcyNjhjYjVkZjY2YWQzMTNfMDlkNWNjMGNmOTU3ODczNmNjMmY2NWViODU0ZDg5MTZfSUQ6NzY0NTg5MzkzODM0NzM3OTkwM18xNzgwMjIxMTU1OjE3ODAzMDc1NTVfVjM)

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=ZjhmYjdkM2VjYTVlZTRmNzJlZGIzYjAwZmY2ZTZmNGFfZjNmMGJkODI1NWYyNmYyYmQ3NGY3YmYyNTNmY2I3MDFfSUQ6NzY0NTg5MzkzNjY3OTg4MTkzMl8xNzgwMjIxMTU1OjE3ODAzMDc1NTVfVjM)

#### 3. 无人机组装

##### 3.1 物料清点

电阻（0805规格10K 20K都可以）

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=YzAzMDU2ZjI5OWI1ODE4MGY5Mzc1NTQ1ODI2MjEwZTlfNWI2OTEyZmQzNGZiZmQ4NjRkY2MzOTdhMzllZTkxZmVfSUQ6NzY0NTg5MzkzNjc4NzI0NjI5Nl8xNzgwMjIxMTU1OjE3ODAzMDc1NTVfVjM)

##### 3.2 工具准备

烙铁，焊锡丝

##### 3.3 焊接流程

###### 焊接核心原则：先贴片，后插件

如果先焊排母，高耸的塑胶座会挡住烙铁头，导致贴片元件极难焊接,**务必先焊接底板上的 MOS 管、电阻等贴片元件，最后焊接排母和排针**。

###### 步骤一：焊接动力 (MOSFET与电阻)

**焊接技巧**：先焊接电阻，再焊接MOSFET

1. 先给PCB上的一个焊盘上少许焊锡。

2. 用镊子夹住元件对齐，加热焊盘使元件固定。

3. 最后补焊剩余的引脚，确保焊点圆润。

**⚠️ 避坑指南**：MOS 管具有方向性，请务必核对 PCB丝印（图标）的方向，焊反将导致上电后电机直接全速旋转，极易炸机。

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=ZWE1MWFmOGM4Mzc4NDQ3YTEyZmQwNzE5ZjQwZTAzYzNfMmMwODEzMzdmZmRmN2I3MzEzODRhNjY3Mjg0ZWRlOTlfSUQ6NzY0NTg5MzkzODM1MTQ5MjMwOV8xNzgwMjIxMTU1OjE3ODAzMDc1NTVfVjM)

###### 步骤二：焊接模块插座 (排母与排针)

一旦贴片元件稳固，我们就可以焊接用于插接模块的接插件了。

- **ESP32-S3接口**：焊接两排2.54mm排母，确保高度水平，否则主控板插上后会倾斜。

- **传感器接口**：包含GY91模块接口、光流TOF二合一模块接口以及 5V 稳压模块接口。

- **焊接要点**：排母引脚较多，建议先焊对角线的两个引脚进行定位，确认位置垂直后再焊剩余引脚。

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=NzljZTkzNTI3NTZlNGVlMzgzZjMyOThjNjBiNThkNDZfYjgwZTM4M2VhNjJmMTQxZWE0OGI0YmQ1YjYzZDcwODBfSUQ6NzY0NTg5MzkzODczMzIyMzEzMV8xNzgwMjIxMTU1OjE3ODAzMDc1NTVfVjM)

###### 步骤三：焊接接口组件 (电池与电机)

最后，我们需要完成输入与输出的连接。

- **电机插座 (4个)**：焊接在四个角落。使用插座的好处是，当空心杯电机这种易损耗品出现问题时，可以像拔插座一样快速更换。

- **电池连接线**：注意检查**电池正负极 (VCC/GND)**，并确保线材能够承受8520电机全速旋转时的瞬间大电流。

###### 焊接后的检查清单 (Checklist)

在插上ESP32-S3之前，请务必进行以下测试：

1. **短路测试**：使用万用表蜂鸣档，检查电源正负极（5V与GND）是否短路。

2. **导通测试**：检查 MOS 管的输出端（电机口）是否与对应的控制引脚通畅。

3. **目测检查**：是否有连锡（引脚粘在一起）的情况，特别是MOS管和排母的密集引脚处。

### 阶段三：软件与开发

#### 1. 嵌入式开发环境搭建

##### 1.1 Arduino IDE 安装

Arduino IDE 是嵌入式开发中最常用的集成开发环境，支持 Windows、macOS 和 Linux 三大主流操作系统。本实验推荐使用 **Arduino IDE 2.x** 版本，相较于 1.x 版本，2.x 版本引入了现代化的编辑器内核，支持自动补全、智能提示、改进的库管理器以及实时串口监视器等功能，能够显著提升开发效率。

XIAO ESP32-S3 的板载包要求版本为 **2.0.8** 及以上才可用。

- **步骤 1.** 根据你的操作系统下载并安装稳定版本的 Arduino IDE。

https://www.arduino.cc/en/software/

- **步骤 2.** 启动 Arduino 应用程序。

- **步骤 3.** 向 Arduino IDE 中添加 ESP32 开发板包。

依次进入 **File > Preferences**，在 **"Additional Boards Manager URLs"** 中填入以下链接：

```text
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=OGY2MTFkMjEwM2QyOTdkNTcwNjljNTA0OTgyOTFlNzRfZmE3N2FjYWQwYTc3N2JjMzUwZTYwN2RmYjNlNjM1ZmVfSUQ6NzY0NTg5MzkzNzMxOTc5MTgxNl8xNzgwMjIxMTU1OjE3ODAzMDc1NTVfVjM)

依次进入 **Tools > Board > Boards Manager...**，在搜索框中输入关键字 **esp32**，选择最新版本的 **esp32** 并安装。

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=YzJhMmY0MzcyZjJkYzljNzQ3OTkzYzA1NjQxOGQ1ZjBfYjk5YTM5NzBkNTgyMzQ2NmMxODA2ZDBkYWY1ODMxNTRfSUQ6NzY0NTg5MzkzODU4NjQwNjEwM18xNzgwMjIxMTU1OjE3ODAzMDc1NTVfVjM)

- **步骤 4.** 选择你的开发板和端口。

在 Arduino IDE 顶部，你可以直接选择端口。它很可能是 COM3 或更高（**COM1** 和 **COM2** 通常保留给硬件串口）。同时，在左侧的开发板中搜索 **xiao**。选择 **XIAO_ESP32S3**。

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=MmUxY2UyZjZlYjBjNDM2MzM5MWRiYTZkZjgzMWVjNTNfNzJjNmEyM2E0ZDllM2FiZGI4NTUxMjZjZGMwZjcwMjVfSUQ6NzY0NTg5MzkzNjM3MTk3NzQwNF8xNzgwMjIxMTU1OjE3ODAzMDc1NTVfVjM)

完成以上准备后，你就可以开始为 XIAO ESP32-S3 编写程序并进行编译和上传了。

##### 1.2 BootLoader 模式

有时，使用了错误的程序会导致 XIAO 丢失端口或无法正常工作。常见问题包括：

- XIAO 已连接到电脑，但找不到端口号。

- XIAO 已连接并出现端口号，但程序上传失败。

当你遇到以上两种情况时，可以尝试让 XIAO 进入 BootLoader 模式，这可以解决大多数设备无法识别和上传失败的问题。具体方法如下：

- **步骤 1**. 按住 XIAO ESP32-S3 上的 `BOOT` 按钮不要松开。

- **步骤 2**. 保持按住 `BOOT` 按钮，然后通过数据线连接电脑。连接电脑后再松开 `BOOT` 按钮。

- **步骤 3**. 上传 **File > Examples > 01.Basics > Blink** 程序来检查 XIAO ESP32-S3 的运行情况。

##### 1.3 复位

当程序运行异常时，你可以在上电时按一次 `Reset`，让 XIAO 重新执行已上传的程序。

当你在上电时按住 `BOOT` 键，然后再按一次 `Reset` 键，也可以进入 BootLoader 模式。

##### 1.4 运行你的第一个 Blink 程序

到现在为止，相信你已经对 XIAO ESP32-S3 的特性和硬件有了较好的了解。接下来，我们以最简单的 Blink 程序为例，让你的 XIAO ESP32-S3 完成第一次闪烁！

- **步骤 1.** 启动 Arduino 应用程序。

- **步骤 2.** 依次进入 **File > Examples > 01.Basics > Blink**，打开该程序。

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=MzlkMjAxMzlkNmM1OWFiOWFmZTYzNWUzZDQyM2QxZTVfOGYxMDk3MmFjMGUzMWVhMjhjOTE3MzNiNDdlMDhkODNfSUQ6NzY0NTg5MzkzNTI3MzIwMDg0NV8xNzgwMjIxMTU1OjE3ODAzMDc1NTVfVjM)

- **步骤 3.** 将开发板型号选择为 **XIAO ESP32-S3**，并选择正确的端口号后上传程序。

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=YmVkOTJmNmEwMTIxNWU3NjY4MDFlZDY1MTgzMzUyY2VfMmNkNjc0ZGE3ZTljNjYxYTNjMDhjMWI3MTViNTdmMGRfSUQ6NzY0NTg5MzkzODY5NTQwODg2MF8xNzgwMjIxMTU1OjE3ODAzMDc1NTVfVjM)

当程序成功上传后，你会看到如下输出信息，并且可以观察到 XIAO ESP32-S3 右侧的橙色 LED 正在闪烁。

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=NDQ1MTdhMTZjMTQ5Y2RhYTY4ZDVmYmQwZmQ0MGVlZDRfYmJhNThmYjAwZWJjYTRkNDk1MWM0YmZlNWVkODQyZGNfSUQ6NzY0NTg5MzkzOTQyMTAzOTc5NV8xNzgwMjIxMTU1OjE3ODAzMDc1NTVfVjM)

##### 1.5 依赖库安装

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=YzdiNzQ0MDU5YjZiNDg5NjQ1NTQ3ZGU0MTI5ZWQwMWJfZWVjMzAwODMwYmU3YmNhZjA4NjhmMGEzMDQzNDFmNWRfSUQ6NzY0NTg5MzkzNTMwMjU0NDU4MF8xNzgwMjIxMTU1OjE3ODAzMDc1NTVfVjM)

#### 2. 飞控代码架构解读

##### 2.1 文件结构总览

本实验固件基于 Flix 架构二次开发，加入串口光流 ToF、定高、定点、MAVLink UDP、串口 CLI、NVS 参数持久化等功能。以下表格列出当前源文件及其职责：

| 文件 | 职责 | 详细介绍 |
|---|---|---|
|`proj_op32drone.ino`|主入口|定义 `WIFI_ENABLED`、`OPTICAL_FLOW_ENABLED`，声明全局位姿/光流变量；`setup()` 初始化参数、LED、电机、WiFi、IMU、RC、光流；`loop()` 按不同频率执行估计与控制|
|`control.ino`|飞控控制|包含 STAB、ALT_HOLD、POS_HOLD、ACRO、AUTO；姿态环、角速度环、定高环、光流定点速度/位置环；支持地面起飞门控和光流失效保护|
|`estimate.ino`|状态估计|KalmanAngle 姿态估计；ToF 与惯性融合高度；串口光流水平速度估计；地面零速锁定、光流门控、原始光流积分调试量|
|`flow.ino`|光流 ToF 驱动|`Serial1`，GPIO8 接光流 TX，GPIO7 接光流 RX；解析 0xDF 帧头 19 字节数据包；有效高度 0.3-6.0m；120ms 超时判定失效|
|`imu.ino`|IMU 驱动|MPU9250 I2C，SDA=GPIO2，SCL=GPIO43，400kHz；陀螺静止自学习；六面加速度计标定|
|`rc.ino`|遥控输入|SBUS，`Serial2` RX=GPIO44，TX=GPIO1；默认 Roll/Pitch/Throttle/Yaw/Mode 通道为 0/1/2/3/6|
|`motors.ino`|电机输出|4 路 LEDC PWM，10kHz、10-bit；GPIO4/3/6/5 对应左后/右后/右前/左前|
|`wifi.ino`|WiFi 与 UDP|优先连接保存的 STA 网络，其次尝试内置 2.4G 网络；失败后回退 AP `flix/flixwifi`；UDP 14550 用于 MAVLink|
|`mavlink.ino`|MAVLink 通信|发送 HEARTBEAT、ATTITUDE_QUATERNION、RC_CHANNELS_RAW、ACTUATOR_CONTROL_TARGET、SCALED_IMU；支持 MANUAL_CONTROL、参数读写、解锁、模式切换和日志读取|
|`cli.ino`|串口命令行|115200bps，同时兼容 UART0 与 ESP32-S3 USB Serial/JTAG；支持参数、姿态、位姿、WiFi、日志、电机、校准、重启等命令|
|`parameters.ino`|参数存储|使用 ESP32 NVS 命名空间 `flix`；启动时读取已有参数，参数变化后低频写入，避免启动时写满 NVS|
|`log.ino`|飞行日志|RAM 循环日志，记录姿态、目标、位置、速度、原始光流积分、光流速度和推力|
|`safety.ino`|安全保护|RC 超时后推力归零并切入 AUTO|
|`led.ino` / `time.ino`|辅助模块|LED 状态指示、`dt` 与 `loopRate` 统计|
|`pid.h` / `kalman_angle.h` / `lpf.h` / `quaternion.h` / `vector.h` / `util.h`|数学与工具|PID、两状态卡尔曼、一阶低通、四元数、向量、Rate/Delay 等基础工具|

##### 2.2 硬件引脚映射

以下引脚与底板硬件直接绑定，修改时务必确认底板电路图：

| 外设 | GPIO | 说明 |
|---|---|---|
|I2C SDA|2|MPU9250 数据线，400KHz；GY-91 上的 BMP280 当前固件未使用|
|I2C SCL|43|MPU9250 时钟线|
|光流 RX|8|Serial1 RX，接光流模块 TX。协议：帧头 0xDF，19字节包，115200bps|
|光流 TX|7|Serial1 TX，接光流模块 RX|
|SBUS RX|44|Serial2 RX，100Kbps，25字节帧|
|SBUS TX|1|Serial2 TX|
|LED|21|板载 NEOPIXEL|
|MOTOR 0|4|LEDC PWM 10KHz → A03400 → 左后（CW）|
|MOTOR 1|3|右后（CW）|
|MOTOR 2|6|右前（CCW）|
|MOTOR 3|5|左前（CCW）|

##### 2.3 循环执行顺序

```cpp
#define WIFI_ENABLED 1       // 启用 WiFi MAVLink
#define OPTICAL_FLOW_ENABLED 1 // 启用光流传感器
```

主循环按功能拆分为多级频率调度。`step()` 负责更新 `dt` 与 `loopRate`，控制部分通过 `Rate` 定时器分层运行：

```cpp
void loop() {
    static Rate pilotRate(80);
    static Rate positionRate(40);
    static Rate attitudeRate(150);
    static Rate innerRate(400);

    readIMU();
    step();
    readRC();
    readOpticalFlow();
    estimate();
    if (pilotRate) controlPilotLoop();
    if (positionRate) controlPositionLoop();
    if (attitudeRate) controlAttitudeLoop();
    if (innerRate) controlRateTorqueLoop();
    sendMotors();
    handleInput();
    processMavlink();
    logData();
    syncParameters();
}
```

#### 3. 核心子系统详解

##### 3.1 光流传感器（flow.ino）

**数据包格式**

| 字节 | 字段 | 说明 |
|---|---|---|
|0|Header|0xDF|
|1~3|ID/Dev/Sta|0x15, 0x00, 0x55|
|4|Len|0x0C（数据长 12 字节）|
|6~7|ToF|uint16，mm 单位|
|10~11|FlowX|int16，像素位移|
|12~13|FlowY|int16，像素位移|
|14~15|IntTime|uint16，μs 单位|
|16|Valid|245 = 数据有效|
|18|Checksum|前18字节和 & 0xFF|

**速度换算公式**

```text
v = flow × (1/10000) × height(m) / dt(s)
```

**有效性条件**

- dataValid == true (byte 16 = 245)

- height: 0.3m ~ 6m

- integrationTime: 5000us ~ 100000us

- 健康超时：120ms 无有效数据 → unhealthy

##### 3.2 姿态解算（kalman_angle.h + estimate.ino）

**卡尔曼滤波器**

- **类型**：两状态（Angle + Gyro Bias）

- **输入**：加速度计角度（观测值，atan2 计算）+ 陀螺仪角速度（预测值）

- **输出**：滤波角度 angle、无偏角速度 rate、实时零偏 bias

- **默认参数**：Q_angle=0.001, Q_bias=0.003, R_measure=0.03

- **实例**：Roll 和 Pitch 各一个 KalmanAngle，Yaw 纯积分

**高度估计**

- ToF 有效时，`position.z` 会向光流模块测得的 `opticalFlowHeight` 收敛。

- ToF 暂不可用时，使用世界系垂直加速度进行短时间惯性积分，并对速度做阻尼。

- 低空电机噪声较强时，会降低加速度计对高度和姿态的影响，避免起飞前姿态被震动带偏。

- `position.z < 0` 时强制归零，避免高度估计向地下漂移。

**水平速度与位置**

- 光流原始速度先按高度换算，再做陀螺补偿，得到 `flowCompBodyVel`。

- `raw_flow pos_xy` 是原始光流速度积分，用于调试传感器方向和比例。

- 控制用 `velocity.x/y` 和 `position.x/y` 只有在 `flowAirborne=true` 后才更新；未起飞时会进入地面零速锁定，避免地面纹理噪声让定点目标漂移。

- 起飞判定主要由 `armed`、`controlThrottle > FLOW_ARM_T`、`position.z > FLOW_ARM_Z` 决定；默认 `FLOW_ARM_T=0.12`、`FLOW_ARM_Z=0.22`。

##### 3.3 飞行控制（control.ino）

**五种飞行模式**

| 模式 | 值 | 行为 |
|---|---|---|
|STAB|2|默认。油门直通+姿态自稳。RC 摇杆映射角度指令。|
|ALT_HOLD|4|定高模式。油门中位附近保持当前高度，推高/拉低转换为爬升/下降速度目标。默认 `ALT_HOVER=0.45`、`ALT_P=1.00`、`ALT_V_P=0.50`|
|POS_HOLD|5|定点模式。在光流健康、已起飞、高度足够、姿态不过度倾斜时打开位置门控；松杆锁定当前位置，动杆接管并刷新目标|
|ACRO|1|角速度控制模式，直接把摇杆映射到角速度目标|
|AUTO|3|MAVLink 外部控制模式，可接收 SET_ATTITUDE_TARGET 或 SET_ACTUATOR_CONTROL_TARGET|

**模式切换（RC 通道 6）**

```cpp
ch6 < 33%  → STAB
ch6 33~66% → ALT_HOLD
ch6 > 66%  → POS_HOLD
```

**解锁/上锁**

```cpp
解锁：油门最低 + 偏航右打到底
上锁：油门最低 + 偏航左打到底
```

**定高与定点主要参数**

| 参数 | 默认值 | 说明 |
|---|---:|---|
|`ALT_HOVER`|0.45|悬停油门基准，定高是否稳住首先取决于该值|
|`ALT_P` / `ALT_I` / `ALT_V_P`|1.00 / 0.010 / 0.50|高度误差到速度目标、积分补偿、垂直速度阻尼|
|`POS_P` / `POS_V_MAX`|0.25 / 0.12|位置误差到水平速度目标，以及最大水平速度限制|
|`HOLD_P_X/Y`|0.015 / 0.014|光流速度闭环比例项|
|`HOLD_I_X/Y`|0.006 / 0.006|速度闭环积分项|
|`HOLD_D_X/Y`|0.0010 / 0.0010|速度闭环微分刹车项|
|`HOLD_A_MAX`|0.04 rad|定点控制最大倾角输出|
|`POS_MIN_Z`|0.22 m|定点门控最低高度|

##### 3.4 MAVLink 调试（mavlink.ino）

**发送的消息**

| 消息 | 频率 | 说明 |
|---|---|---|
|HEARTBEAT (#0)|1 Hz|type=QUADROTOR, autopilot=GENERIC, base_mode=armed/disarmed|
|EXTENDED_SYS_STATE|1 Hz|上报 landed / in-air 状态|
|ATTITUDE_QUATERNION|10 Hz|四元数姿态与角速度，按 MAVLink FRD 坐标约定转换|
|RC_CHANNELS_RAW (#35)|~10 Hz|16 通道原始 PWM 值|
|ACTUATOR_CONTROL_TARGET|10 Hz|当前 4 路电机归一化输出|
|SCALED_IMU|10 Hz|加速度计与陀螺仪数据|

**接收的关键消息**

| 消息/命令 | 作用 |
|---|---|
|MANUAL_CONTROL|外部手动控制，映射到油门、俯仰、横滚、偏航|
|PARAM_REQUEST_LIST / PARAM_REQUEST_READ / PARAM_SET|参数读取与设置|
|MAV_CMD_COMPONENT_ARM_DISARM|MAVLink 解锁/上锁，油门高于 0.05 时拒绝解锁|
|MAV_CMD_DO_SET_MODE|切换 `RAW/ACRO/STAB/AUTO/ALT_HOLD/POS_HOLD`|
|SET_ATTITUDE_TARGET|AUTO 模式下接收姿态、角速度和推力目标|
|SET_ACTUATOR_CONTROL_TARGET|AUTO 模式下直接接收电机控制量|
|SERIAL_CONTROL|通过 MAVLink 透传 CLI 命令|

##### 3.5 ROS2/MAVROS 接入说明

当前 `proj_op32drone` 固件侧只负责 MAVLink v2 的 UDP 收发，不内置 ROS 2 节点；ROS 2 接入需要在上位机侧通过 MAVROS 或自定义 MAVLink 程序连接飞控 UDP 14550。飞控默认会优先连接保存的 2.4G WiFi；连接失败才回退 AP `flix/flixwifi`，AP 模式下飞控地址为 `192.168.4.1`。如果飞控已接入路由器 STA 网络，应以串口 `wifi` 命令打印出的 `STA IP` 为准。

**架构**

```python
ComponentContainer("open32drone")
├── mavros::Router  ← UDP → ESP32 (AP: 192.168.4.1:14550 / STA: 查看串口 wifi 输出)
│   ├── fcu_urls: udp://:14550@<飞控IP>:14550
│   ├── gcs_urls: udp://0.0.0.0:14551@            (向地面站转发)
│   └── uas_urls: /open32drone_uas                (内部)
└── mavros::UAS     ← Router → ROS topics
    namespace: /open32drone
```

**关键参数**

| 参数 | 值 | 说明 |
|---|---|---|
|fcu_urls|udp://:14550@<飞控IP>:14550|AP 模式使用 192.168.4.1；STA 模式使用串口 `wifi` 输出的 STA IP|
|gcs_urls|udp://0.0.0.0:14551@|同时向地面站转发，用 14551 避免与飞控 14550 冲突|
|system_id|255|GCS 系统 ID（标准 MAVLink 约定）|
|component_id|240|MAVROS 组件 ID（标准值）|
|target_system_id|1|飞控 SYSTEM_ID=1（与代码 mavlink.ino 一致）|
|target_component_id|1|飞控组件号|
|fcu_protocol|v2.0|MAVLink v2|
|connection_timeout|10.0|连接超时 10 秒|
|heartbeat_interval|1.0|心跳 1 Hz|
|timeout_heartbeat|5.0|5 秒无心跳判定离线|
|enable_autopilot_version_check|false|跳过版本检查（自定义飞控无标准版本号）|

**插件白名单**

启用的 MAVROS 插件：sys_status / command / param / manual_control / imu

command_long 和 rc_io 已被注释。command_long 用于复杂命令（航点/相机），rc_io 用于 RC 覆写。如需这些功能取消注释即可。

**IMU 噪声参数**

```text
imu/frame_id: base_link
imu/linear_acceleration_stdev: 0.0003
imu/angular_velocity_stdev: 0.000349
imu/orientation_stdev: 1.0
```

**Topic 重映射**

```text
/open32drone/UAS1/imu/data → /imu/data
/open32drone/UAS1/imu/data_raw → /imu/data_raw
/open32drone/UAS1/manual_control/send → /manual_control
```

**QoS 配置（IMU）**

```text
history: keep_last, depth: 10
reliability: best_effort
durability: volatile
```

##### 3.6 ROS 2 手动控制命令速查

**解锁与上锁**

```text
# 解锁
ros2 service call /osdrone/arming mavros_msgs/srv/CommandBool "{value: true}"

# 上锁
ros2 service call /osdrone/arming mavros_msgs/srv/CommandBool "{value: false}"
```

**手动飞行控制**

通过 `/osdrone/send` topic 发送 ManualControl 消息，四轴参数范围均为 **[-1000, 1000]**。

| 动作 | 参数 | 命令 |
|---|---|---|
|悬停|油门 200|`ros2 topic pub /osdrone/send mavros_msgs/msg/ManualControl "{x:0,y:0,z:200,r:0,buttons:0}" --once`|
|前进|俯仰 +300|`ros2 topic pub /osdrone/send mavros_msgs/msg/ManualControl "{x:300,y:0,z:500,r:0,buttons:0}" --once`|
|后退|俯仰 -300|`ros2 topic pub /osdrone/send mavros_msgs/msg/ManualControl "{x:-300,y:0,z:500,r:0,buttons:0}" --once`|
|右移|横滚 +300|`ros2 topic pub /osdrone/send mavros_msgs/msg/ManualControl "{x:0,y:300,z:500,r:0,buttons:0}" --once`|
|左移|横滚 -300|`ros2 topic pub /osdrone/send mavros_msgs/msg/ManualControl "{x:0,y:-300,z:500,r:0,buttons:0}" --once`|
|右转|偏航 +200|`ros2 topic pub /osdrone/send mavros_msgs/msg/ManualControl "{x:0,y:0,z:500,r:200,buttons:0}" --once`|
|左转|偏航 -200|`ros2 topic pub /osdrone/send mavros_msgs/msg/ManualControl "{x:0,y:0,z:500,r:-200,buttons:0}" --once`|

**注意**：每次 `--once` 命令仅发送一帧。持续飞行需循环发送，或写节点代码以一定频率发布。

**飞行模式切换**

使用 MAVLink `MAV_CMD_DO_SET_MODE`（command=176），param1=1.0 表示 base_mode=CUSTOM，param2 为子模式编号。

| 模式 | param2 | ROS 2 命令 |
|---|---|---|
|MANUAL|0.0|`ros2 service call /osdrone/command mavros_msgs/srv/CommandLong "{command:176,param1:1.0,param2:0.0}"`|
|ACRO|1.0|`ros2 service call /osdrone/command mavros_msgs/srv/CommandLong "{command:176,param1:1.0,param2:1.0}"`|
|STAB|2.0|`ros2 service call /osdrone/command mavros_msgs/srv/CommandLong "{command:176,param1:1.0,param2:2.0}"`|
|AUTO|3.0|`ros2 service call /osdrone/command mavros_msgs/srv/CommandLong "{command:176,param1:1.0,param2:3.0}"`|
|ALT_HOLD|4.0|`ros2 service call /osdrone/command mavros_msgs/srv/CommandLong "{command:176,param1:1.0,param2:4.0}"`|
|POS_HOLD|5.0|`ros2 service call /osdrone/command mavros_msgs/srv/CommandLong "{command:176,param1:1.0,param2:5.0}"`|

**参数读写**

```text
# 拉取全部参数到本地
ros2 service call /osdrone/pull mavros_msgs/srv/ParamPull "{force_pull: true}"

# 监听参数事件获取参数列表
ros2 topic echo /parameter_events

# 读取指定参数
ros2 service call /osdrone/get_parameters rcl_interfaces/srv/GetParameters "{names: ["CTL_R_RATE_P", "CTL_P_RATE_P", "CTL_Y_RATE_P"]}"
```

**写入参数**

```text
ros2 service call /osdrone/set mavros_msgs/srv/ParamSetV2 "{force_set: true, param_id: "CTL_R_RATE_P", value: {type: 3, double_value: 0.15}}"
```

type 对应 MAVLink MAV_PARAM_TYPE：1=uint8, 2=int8, 3=uint16, 4=int16, 5=uint32, 6=int32, 9=float（实际 double_value）。飞控代码中参数多为 float（type=9）。

**使用注意事项**

- ManualControl 每 `--once` 只发一帧，需要代码循环才能持续控制

- 油门 z 范围 [0, 1000]，对应飞控中 0~100% 推力。当前 `ALT_HOVER=0.45`，约对应 z=450

- 俯仰/横滚 [-1000, 1000] 映射到最大倾角 `CTL_TILT_MAX`（默认约 25°）；偏航映射到 `CTL_Y_RATE_MAX`

- 切换模式后需等待心跳确认，QGC 或 `ros2 topic echo /osdrone/state` 可查看当前模式

- 上锁前确保油门=0，否则飞控不会响应上锁命令

##### 3.7 CLI 调试命令

USB 串口 115200bps，当前 `cli.ino` 同时兼容 UART0 与 ESP32-S3 USB Serial/JTAG，提供以下常用命令：

| 命令 | 输出内容 | 典型用途 |
|---|---|---|
| `help` | 全部命令 | 查看当前固件支持的 CLI |
| `p` / `p <name>` / `p <name> <value>` | 参数列表、单参数、写参数 | 调 PID、光流比例、悬停油门；电机未转时会由 `syncParameters()` 保存到 NVS |
| `ps` / `psq` | 欧拉角 / 四元数 | 快速确认姿态方向 |
| `pose` / `pose <hz>` / `poseoff` | 姿态、位置、速度、光流门控、原始光流积分 | 总览位姿是否可信 |
| `flowraw` / `flowraw <hz>` | 光流测量速度、陀螺补偿速度、补偿后速度、bias、innovation | 判断光流方向、比例、陀螺补偿是否正确 |
| `flowctrl` / `flowctrl <hz>` | `stationary/air/zero/use/gate/reject`、控制用位置/速度、高度目标 | 判断 POS_HOLD 为什么不开门控或 XY 为什么不更新 |
| `rate` / `rate <hz>` | 角速度、角速度目标、力矩输出、PID 参数 | 调姿态环和角速度环 |
| `mot` / `mot <hz>` | 四电机输出、左右/前后差分、推力目标、力矩目标 | 检查混控方向、电机方向和姿态修正方向 |
| `diag` / `diag <hz>` | 加速度模长、震动、加速度计信任度、姿态观测角、陀螺状态 | 判断 IMU 震动、起飞地面效应和估计器健康 |
| `monoff` / `flowoff` / `rateoff` / `motoff` / `diagoff` | 停止滚动监视器 | 防止串口刷屏 |
| `monpause` / `monresume` | 暂停/恢复当前滚动监视器 | 调参时临时停住输出 |
| `imu` | IMU 状态、校准值、原始加速度计和陀螺仪 | 检查传感器与六面标定 |
| `ca` / `cr` | 加速度计六面标定 / SBUS 遥控器标定 | 首次装机或重装后使用 |
| `wifi` / `wifiscan` | WiFi/AP/STA/MAVLink 状态，附近网络 | 排查连接问题 |
| `wifi <ssid> <password>` / `wifi reset` | 保存网络 / 清除保存网络 | 切换路由器或密码 |
| `arm` / `disarm` | 串口解锁 / 上锁 | 拆桨调试用 |
| `raw` / `stab` / `acro` / `auto` / `alt` / `pos` | 手动切换模式 | 室内调试时快速切模式 |
| `mfr` / `mfl` / `mrr` / `mrl` | 单电机测试 | 必须拆桨，用于确认电机序号 |
| `log` / `log dump` | RAM 日志表头 / CSV 数据 | 飞后分析姿态、速度、位置和光流 |
| `sys` / `reboot` | 系统信息 / 重启 | 查看堆内存、芯片温度或软重启 |

#### 4. 调参建议

调参建议每次只改一类参数，并记录修改前后的日志。

##### 4.1 调试输出怎么用

| 阶段 | 推荐命令 | 关注字段 | 判断标准 |
|---|---|---|---|
| 上电静止 | `diag 5` | `accNorm`、`vibe`、`trust`、`bias` | 静止时 `accNorm` 接近 9.8，`vibe` 不应很大，`trust` 不应长期接近 0 |
| 手持倾斜 | `pose 5` | `att_deg r/p/y` | 向右倾 Roll 方向、向前倾 Pitch 方向应符合机体定义 |
| 手持平移 | `flowraw 10` | `body_vel`、`comp`、`pos_xy` | 向前/向右移动时光流方向应与实际运动方向一致 |
| 解锁低油门 | `mot 10` | `FL/FR/RL/RR`、`LRdiff`、`FBdiff` | 姿态修正时电机差分方向正确，没有某一路异常饱和 |
| STAB 低空 | `rate 10` | `cur/tgt/tq` | 角速度能跟随目标，力矩输出不过度抖动 |
| ALT_HOLD | `pose 10`、`flowctrl 10` | `pos.z`、`vel.z`、`targetZ`、`vzT` | 高度围绕目标收敛，不持续上飘或下沉 |
| POS_HOLD | `flowctrl 10`、`flowraw 10` | `air/use/gate/rej`、`position.x/y` | `air=1`、`use=1`、`gate=1` 后 XY 才会进入控制闭环 |
| 飞后分析 | `log dump` | `position`、`velocity`、`rawFlowPos`、`flowCompVel`、`thrustTarget` | 将输出保存为 CSV，用表格或绘图工具看趋势 |

##### 4.2 基础姿态环

1. 保持 `STAB` 模式，先确认不会自旋、不会单方向持续倾倒。
2. 若高频抖动，先降低 `CTL_R_RATE_D` / `CTL_P_RATE_D` 或检查电机、桨叶、机架震动。
3. 若姿态响应慢，再小幅提高 `CTL_R_RATE_P` / `CTL_P_RATE_P`。
4. 若姿态能回正但有慢性偏差，再考虑小幅增加 Rate I，不要先动积分。

##### 4.3 定高

定高优先调 `ALT_HOVER`。如果悬停油门不准，`ALT_P` 和 `ALT_V_P` 再怎么调也会很吃力。

| 现象 | 优先调整 |
|---|---|
| 切入 `ALT_HOLD` 后缓慢下沉 | 增大 `ALT_HOVER` |
| 切入 `ALT_HOLD` 后缓慢上飘 | 减小 `ALT_HOVER` |
| 高度上下震荡 | 降低 `ALT_P`，或略增 `ALT_V_P` |
| 高度响应迟钝 | 略增 `ALT_P`，确认 ToF 高度稳定 |

##### 4.4 光流方向与定点

当前控制用 XY 不是原始像素，而是经过高度换算、陀螺补偿、bias 扣除和门控后的相对位置估计。调光流时要区分三类量：

| 字段 | 含义 |
|---|---|
| `flowraw meas` | 光流模块原始速度换算值 |
| `flowraw comp` | 扣除机体转动影响后的速度 |
| `flowraw pos_xy` | 原始光流积分，只用于判断方向和比例 |
| `flowctrl pos/vel` | 飞控控制用 XY 位置/速度 |
| `flowctrl air/use/gate` | 是否已起飞、是否使用光流、POS_HOLD 门控是否打开 |

校准顺序：

1. 拆桨，手持平移，运行 `flowraw 10`。
2. 如果 `pos_xy` 方向反了，优先检查光流模块安装方向，其次调整 `FLOW_SCL_X/Y` 符号。
3. 如果手持原地俯仰/横滚时 `comp` 明显漂移，微调 `FLOW_K_PIT` / `FLOW_K_ROL`。
4. STAB 已经稳后，再低空短切 `POS_HOLD`，运行 `flowctrl 10`。
5. 只有 `air=1`、`use=1`、`gate=1` 后，`position.x/y` 才是参与定点控制的 XY。
6. 定点横向发散时先切回 `STAB`，不要在发散状态继续加大 `HOLD_P_X/Y`。

##### 4.5 RAM 日志

`log dump` 会输出 CSV 格式数据。当前日志列包括：

| 类别 | 字段 |
|---|---|
| 时间 | `t` |
| 角速度 | `rates.x/y/z`、`ratesTarget.x/y/z` |
| 姿态 | `attitude.x/y/z`、`attitudeTarget.x/y/z` |
| 控制用位置 | `position.x/y/z` |
| 控制用速度 | `velocity.x/y/z` |
| 原始光流 | `rawFlowPos.x/y`、`rawBodyVel.x/y` |
| 补偿光流 | `flowCompVel.x/y` |
| 推力 | `thrustTarget` |

建议把 `log dump` 输出保存为 `.csv`，用表格或 Python/Matlab 绘制 `position.z`、`velocity.z`、`position.x/y`、`rawFlowPos.x/y`、`flowCompVel.x/y`。这样比只看飞行视频更容易定位是高度环、光流方向、速度闭环还是门控问题。

#### 5. 故障排查表

| 现象 | 可能原因 | 解决方法 |
|---|---|---|
|起飞即翻|桨叶方向错误|检查 X 布局：对角线同向，相邻反向|
|STAB 稳但 XY 漂移|未进入 POS_HOLD 或光流门控未打开|串口 `flowctrl 10` 查看 `air/use/gate/rej`，确认油门、高度和光流健康|
|定点越修越跑|光流 X/Y 方向、比例或陀螺补偿方向错误|手持测试 `flowraw 10`，必要时调整 `FLOW_SCL_X/Y`、`FLOW_K_PIT/ROL`|
|高度波动大|`ALT_HOVER` 不准、ToF 数据抖动或地面反射差|先调 `ALT_HOVER`，再微调 `ALT_P`、`ALT_V_P`|
|悬停震荡|位置/速度环增益过大|降低 `HOLD_P_X/Y` 或 `HOLD_A_MAX`|
|电机输出明显偏一侧|混控方向、电机顺序或姿态估计方向错误|拆桨运行 `mot 10`，轻轻倾斜机体，确认修正方向正确|
|姿态环抖动|机架震动、D 项过大或 IMU 信任度下降|运行 `rate 10` 和 `diag 10`，看 `tq`、`vibe`、`trust`|
|解锁失败|RC 信号异常或油门未归零|检查 SBUS 接线，串口 `rc` 查看通道值|
|WiFi 连不上|旧 NVS 凭据、5GHz/加密不兼容或信号弱|执行 `wifi reset`、`wifiscan`，确认 2.4G SSID、WPA2/WPA2-WPA3 mixed|
|MAVROS 连不上|飞控 IP 或 UDP 端口错误|AP 模式使用 192.168.4.1，STA 模式用串口 `wifi` 输出的 `STA IP`，端口 14550|

## 三、其他补充内容

### 参考链接汇总

- Flix 原项目：[github.com/okalachev/flix](https://github.com/okalachev/flix)

- MAVLink 协议文档：[mavlink.io](https://mavlink.io)

- QGroundControl：[qgroundcontrol.com](https://qgroundcontrol.com)

- ESP32-S3 技术手册：[espressif.com](https://www.espressif.com)

- PX4 开发指南（PID 调优）：[docs.px4.io](https://docs.px4.io)

- Crazyflie 技术文档（参考 Lee 控制器）：[bitcraze.io](https://www.bitcraze.io)
