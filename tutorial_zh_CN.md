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

<p align="center">
    <img src="img\seed-s3.PNG" />
</p>

规格型号：Seeed Studio XIAO ESP32-S3 Sense

参考价格：90元

模块说明：https://wiki.seeedstudio.com/cn/xiao_esp32s3_getting_started/

##### 机架桨叶

<p align="center">
    <img src="img\paddle1.PNG" />
</p>

<p align="center">
    <img src="img\paddle2.PNG" />
</p>

规格型号：12.3cm轴距机架，76mm桨叶（适配1mm轴径电机）

需求：1个机架，4个桨叶

参考价格：19元（1套价格）

##### 空心杯电机

<p align="center">
    <img src="img\electric.PNG" />
</p>

规格型号：8520空心杯电机，轴径1mm

需求：4个

参考价格：24元（4个价格）

##### IMU模块 十轴传感器

<p align="center">
    <img src="img\imu.PNG" />
</p>

规格型号：GY-91 九轴MPU9250+BMP280气压计

需求：1个

参考价格：14元（1个价格）

##### 升压模块

<p align="center">
    <img src="img\up_voltage.PNG" />
</p>

规格型号：3.3V升压5V模块

需求：1个

参考价格：4.5元（1个价格）

##### TOF光流模块

<p align="center">
    <img src="img\tof.PNG" />
</p>

规格型号：CORVON link协议

需求：1个

参考价格：68元（1个价格）

##### 电机驱动芯片

<p align="center">
    <img src="img\mos.PNG" />
</p>

规格型号：MOS场效应管AO3400

需求：4个

参考价格：0.4元（4个价格）

##### 遥控器

<p align="center">
    <img src="img\controller.PNG" />
</p>

规格型号：福斯I6S单控

需求：1个

参考价格：249元（1个价格）

##### 接收器
<p align="center">
    <img src="img\receiver.PNG" />
</p>
规格型号：福斯A8S接收器，SBUS接收器

需求：1个

参考价格：65元（1个价格）

##### 其他物料

电机插座，电池，排针，排母若干

#### 2. 底板加工

<p align="center">
    <img src="img\pcb1.PNG" />
</p>

<p align="center">
    <img src="img\pcb2.PNG" />
</p>

规格型号：自制

需求：需要1个

图纸链接：https://oshwhub.com/fanchewang/open32drone

##### 2.1 打开设计图

<p align="center">
    <img src="img\design1.PNG" />
</p>

##### 2.2 PCB下单

<p align="center">
    <img src="img\design2.PNG" />
</p>

<p align="center">
    <img src="img\design3.PNG" />
</p>

<p align="center">
    <img src="img\design4.PNG" />
</p>

其他基本默认选择

<p align="center">
    <img src="img\design5.PNG" />
</p>

<p align="center">
    <img src="img\design6.PNG" />
</p>

#### 3. 无人机组装

##### 3.1 物料清点

电阻（0805规格10K 20K都可以）

<p align="center">
    <img src="img\all_stuff.PNG" />
</p>

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

<p align="center">
    <img src="img\hanjie1.PNG" />
</p>

###### 步骤二：焊接模块插座 (排母与排针)

一旦贴片元件稳固，我们就可以焊接用于插接模块的接插件了。

- **ESP32-S3接口**：焊接两排2.54mm排母，确保高度水平，否则主控板插上后会倾斜。

- **传感器接口**：包含GY91模块接口、光流TOF二合一模块接口以及 5V 稳压模块接口。

- **焊接要点**：排母引脚较多，建议先焊对角线的两个引脚进行定位，确认位置垂直后再焊剩余引脚。

<p align="center">
    <img src="img\hanjie2.PNG" />
</p>


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

当前固件使用 `arduino-esp32 3.3.6` 构建，建议教程与实际编译环境保持同一版本。

- **步骤 1.** 根据你的操作系统下载并安装稳定版本的 Arduino IDE。

https://www.arduino.cc/en/software/

- **步骤 2.** 启动 Arduino 应用程序。

- **步骤 3.** 向 Arduino IDE 中添加 ESP32 开发板包。

依次进入 **File > Preferences**，在 **"Additional Boards Manager URLs"** 中填入以下链接：

```text
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```

<p align="center">
    <img src="img\software1.PNG" />
</p>


依次进入 **Tools > Board > Boards Manager...**，在搜索框中输入关键字 **esp32**，选择最新版本的 **esp32** 并安装。

<p align="center">
    <img src="img\software2.PNG" />
</p>

- **步骤 4.** 选择你的开发板和端口。

在 Arduino IDE 顶部，你可以直接选择端口。它很可能是 COM3 或更高（**COM1** 和 **COM2** 通常保留给硬件串口）。同时，在左侧的开发板中搜索 **xiao**。选择 **XIAO_ESP32S3**。

<p align="center">
    <img src="img\software3.PNG" />
</p>

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

<p align="center">
    <img src="img\software4.PNG" />
</p>

- **步骤 3.** 将开发板型号选择为 **XIAO ESP32-S3**，并选择正确的端口号后上传程序。

<p align="center">
    <img src="img\software5.PNG" />
</p>

当程序成功上传后，你会看到如下输出信息，并且可以观察到 XIAO ESP32-S3 右侧的橙色 LED 正在闪烁。

<p align="center">
    <img src="img\software6.PNG" />
</p>

##### 1.5 依赖库安装

<p align="center">
    <img src="img\software7.PNG" />
</p>

工程依赖 `FlixPeriph`、`SBUS`、`MAVLink` 和 `Adafruit BMP280`。选择 `XIAO_ESP32S3` 后启用 OPI PSRAM，并使用 `max_app_8MB` 分区，以容纳相机、Wi-Fi 和飞控功能。

#### 2. 飞控代码架构解读

##### 2.1 文件结构总览

本实验固件基于 Flix 架构二次开发，加入串口光流 ToF、定高、定点、MAVLink UDP、串口 CLI、NVS 参数持久化等功能。以下表格列出当前源文件及其职责：

| 文件 | 职责 | 详细介绍 |
|---|---|---|
|`open32drone_v3.ino`|主入口|初始化参数、电机、相机、WiFi、IMU、BMP280、SBUS 和光流；实时循环依次执行采集、估计、控制、电机输出、通信、日志和参数同步|
|`control.ino`|飞控控制|包含 STAB、ALT_HOLD、POS_HOLD、ACRO、AUTO；姿态/角速度串级控制、ToF 定高和光流位置/速度控制，并统一处理控制接入与复位|
|`estimate.ino`|状态估计|四元数陀螺积分与门控重力修正；纯 ToF 相对高度及垂直速度；带时延角速度补偿的光流水平速度和位置估计|
|`flow.ino`|光流 ToF 驱动|`Serial1`，GPIO8 接光流 TX，GPIO7 接光流 RX；解析 0xDF 帧头 19 字节数据包；有效高度 0.05-6.0 m；150 ms 超时判定失效|
|`imu.ino`|IMU 驱动|MPU9250 I2C，SDA=GPIO2，SCL=GPIO43，400kHz；陀螺静止自学习；六面加速度计标定|
|`rc.ino`|遥控输入|SBUS，`Serial2` RX=GPIO44，TX=GPIO9；默认 Roll/Pitch/Throttle/Yaw/Mode 通道为 0/1/2/3/6|
|`motors.ino`|电机输出|4 路 LEDC PWM，10kHz、10-bit；GPIO4/3/6/5 对应左后/右后/右前/左前|
|`wifi.ino`|WiFi、UDP 与图传|默认建立 AP `open32drone`，地址 `192.168.4.1`；UDP 14550 用于 MAVLink，HTTP `/stream` 输出 QVGA MJPEG|
|`mavlink.ino`|MAVLink 通信|发送心跳、姿态、RC、执行器、IMU 和系统状态；支持 MANUAL_CONTROL、参数读写、解锁、外部控制、日志读取和 CLI 透传|
|`cli.ino`|串口命令行|115200 bps，提供参数、姿态、IMU、RC、光流、定高、电机、网络、日志、校准和系统状态命令|
|`parameters.ino`|参数存储|使用 ESP32 NVS 命名空间 `flix`；启动时读取已有参数，参数变化后低频写入，避免启动时写满 NVS|
|`log.ino`|飞行日志|50 Hz、300 样本 RAM 循环日志，记录估计、目标、光流补偿、控制门控、饱和状态和电机输出|
|`safety.ino`|安全保护|RC 超时后进入平滑下降；AUTO 外部目标超时后立即解除锁定|
|`led.ino` / `time.ino`|辅助模块|LED 状态指示、`dt` 与 `loopRate` 统计|
|`pid.h` / `lpf.h` / `quaternion.h` / `vector.h` / `util.h`|数学与工具|PID、一阶低通、四元数、向量、Rate/Delay 等基础工具|

##### 2.2 硬件引脚映射

以下引脚与底板硬件直接绑定，修改时务必确认底板电路图：

| 外设 | GPIO | 说明 |
|---|---|---|
|I2C SDA|2|MPU9250 与 BMP280 数据线，400 kHz；BMP280 用于诊断，飞行高度来自 ToF|
|I2C SCL|43|MPU9250 时钟线|
|光流 RX|8|Serial1 RX，接光流模块 TX。协议：帧头 0xDF，19字节包，115200bps|
|光流 TX|7|Serial1 TX，接光流模块 RX|
|SBUS RX|44|Serial2 RX，100Kbps，25字节帧|
|SBUS TX|9|Serial2 TX|
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

主循环保持固定的数据依赖顺序：先采集传感器，再更新时间、估计状态和执行控制，最后输出电机并处理通信与日志。相机 MJPEG 服务运行在较低优先级任务中，飞控 `loopTask` 保持更高优先级：

```cpp
void loop() {
    readIMU();
    step();
    readRC();
    readOpticalFlow();
    updateBaro();
    estimate();
    estimateHeight();
    estimateHorizontalVelocity();
    control();
    sendMotors();
    handleInput();
    processMavlink();
    readVoltage();
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

- height: 0.05 m ~ 6 m

- integrationTime > 0

- 健康超时：150 ms 无有效数据 → unhealthy

##### 3.2 姿态解算（quaternion.h + estimate.ino）

**四元数姿态估计**

- 陀螺仪角速度先经过低通滤波，再以旋转向量形式积分到姿态四元数。

- 落地静止时，加速度计以 `EST_ACC_WEIGHT=0.003` 修正估计重力方向。

- 飞行中仅在推力、加速度模长和角速度满足条件时，以较小的 `EST_LVL_WEIGHT=0.0002` 进行重力方向修正。

- Roll、Pitch 和 Yaw 统一由四元数表示，避免欧拉角直接积分带来的轴间耦合。

**高度估计**

- 高度直接来自光流模块内置 ToF，`position.z` 保存滤波后的相对离地高度。

- 相邻有效样本的高度差用于计算 `velocity.z`，再通过低通滤波抑制测距抖动。

- 单次高度跳变超过 0.45 m 时只接收 10% 的变化，并将最大高度变化率限制为 2 m/s。

- 电机停止且 ToF 无效时，高度和垂直速度复位。

**水平速度与位置**

- 光流位移按 ToF 高度和积分时间换算为速度，再使用 40 ms 历史角速度补偿纯旋转串扰，得到 `flowCompBodyVel`。

- 补偿速度依次经过地面偏置学习、三点中值滤波、创新限幅和低通平滑，再转换到世界坐标系。

- 控制用 `velocity.x/y` 和 `position.x/y` 只有在 `flowAirborne=true` 后才更新；未起飞时会进入地面零速锁定，避免地面纹理噪声让定点目标漂移。

- 起飞判定由解锁、油门大于 0.12、光流数据新鲜和 ToF 有效共同决定，并持续满足 250 ms 后生效。

##### 3.3 飞行控制（control.ino）

**五种飞行模式**

| 模式 | 值 | 行为 |
|---|---|---|
|STAB|2|默认。油门直通+姿态自稳。RC 摇杆映射角度指令。|
|ALT_HOLD|4|ToF 定高模式。进入后记录当前高度，飞手油门作为基础推力，高度 PID 在其上叠加修正。|
|POS_HOLD|5|光流定点模式。定高接入且光流、ToF、姿态和偏航条件满足后锁定当前位置；移动 Roll/Pitch 摇杆时平移目标点。|
|ACRO|1|角速度控制模式，直接把摇杆映射到角速度目标|
|AUTO|3|MAVLink 外部控制模式，可接收 SET_ATTITUDE_TARGET 或 SET_ACTUATOR_CONTROL_TARGET|

**模式切换（RC 通道 6）**

```cpp
ch6 < 25%       → STAB
ch6 25% ~ 75%  → ALT_HOLD
ch6 > 75%      → POS_HOLD
```

**解锁/上锁**

```cpp
解锁：油门最低 + 偏航右打到底
上锁：油门最低 + 偏航左打到底
```

**定高与定点主要参数**

| 参数 | 默认值 | 说明 |
|---|---:|---|
|定高 PID|P=0.8, I=0.1, D=0.2|编译常量；修正量限制为 ±0.2，积分限制为 ±0.3|
|`POS_HOLD_P`|0.50|位置误差到水平速度目标的比例增益|
|`POS_STICK_V`|0.50 m/s|Roll/Pitch 满杆对应的目标点移动速度|
|`POS_VEL_P_X/Y`|0.20 / 0.20|水平速度闭环比例增益|
|`POS_VEL_I_X/Y`|0 / 0|水平速度闭环积分增益|
|`POS_VEL_D_X/Y`|0 / 0|水平速度闭环微分增益|
|`POS_CMD_RATE`|1.20 rad/s|定点姿态指令变化率限制|
|`FLOW_GYRO_P/R`|-0.78 / -0.77|Pitch/Roll 旋转光流补偿系数|
|`FLOW_GYRO_DLY`|40 ms|光流与角速度的时间对齐|

##### 3.4 MAVLink 调试（mavlink.ino）

**发送的消息**

| 消息 | 频率 | 说明 |
|---|---|---|
|HEARTBEAT (#0)|2 Hz|type=QUADROTOR, autopilot=GENERIC, base_mode=armed/disarmed|
|EXTENDED_SYS_STATE|2 Hz|上报 landed / in-air 状态|
|BATTERY_STATUS|2 Hz|上报电池字段；启用电压采样引脚后使用|
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
|MAV_CMD_DO_SET_MODE|切换 `RAW/ACRO/STAB/AUTO`；定高与定点由 RC 三段开关选择|
|SET_ATTITUDE_TARGET|AUTO 模式下接收姿态、角速度和推力目标|
|SET_ACTUATOR_CONTROL_TARGET|AUTO 模式下直接接收电机控制量|
|SERIAL_CONTROL|通过 MAVLink 透传 CLI 命令|

##### 3.5 ROS2/MAVROS 接入说明

仓库中的 `ros2_open32drone` 功能包在上位机运行，通过 MAVROS 连接飞控 UDP 14550，并将板载相机的 MJPEG 视频发布为 ROS 2 图像话题。飞控默认建立 AP `open32drone`，密码 `12345678`，地址为 `192.168.4.1`。

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

启用的 MAVROS 插件：sys_status / command / param / manual_control / imu。`open32drone.launch.py` 还会启动 MJPEG 相机节点，发布 `image_raw` 与 `image_raw/compressed`。

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

- 油门 z 范围 [0, 1000]，对应飞控中 0~100% 推力

- 俯仰/横滚 [-1000, 1000] 映射到最大倾角 `CTL_TILT_MAX`（默认 30°）；偏航映射到 `CTL_Y_RATE_MAX`

- 切换模式后需等待心跳确认，QGC 或 `ros2 topic echo /osdrone/state` 可查看当前模式

- 上锁前确保油门=0，否则飞控不会响应上锁命令

##### 3.7 CLI 调试命令

USB 串口 115200bps，当前 `cli.ino` 同时兼容 UART0 与 ESP32-S3 USB Serial/JTAG，提供以下常用命令：

| 命令 | 输出内容 | 典型用途 |
|---|---|---|
| `help` | 全部命令 | 查看当前固件支持的 CLI |
| `p` / `p <name>` / `p <name> <value>` | 参数列表、单参数、写参数 | 调 PID、估计器和光流参数；电机未转时保存到 NVS |
| `ps` / `psq` | 欧拉角 / 四元数 | 快速确认姿态方向 |
| `imu` | IMU、校准值和 landed 状态 | 检查传感器与六面标定 |
| `rc` | 16 通道、归一化输入、模式和解锁状态 | 检查 SBUS 映射和三段开关 |
| `flow` | ToF、光流速度、角速度补偿、位置、门控和拒绝码 | 检查定点估计链 |
| `alt` | 定高接入、目标高度、ToF 高度、推力和拒绝码 | 检查定高控制链 |
| `mot` | 四路电机输出 | 检查电机映射与混控方向 |
| `time` | 循环频率、平均/最大周期和 overrun | 检查实时循环负载 |
| `ca` / `cr` | 加速度计六面标定 / SBUS 遥控器标定 | 首次装机或重装后使用 |
| `wifi` | AP/STA、IP、客户端和 MAVLink 状态 | 检查网络连接 |
| `ap <ssid> <password>` | 保存 AP 名称和密码 | 修改无线网络配置，重启后生效 |
| `arm` / `disarm` | 串口解锁 / 上锁 | 拆桨调试用 |
| `raw` / `stab` / `acro` / `auto` | 手动切换调试模式 | 串口调试控制链 |
| `mfr` / `mfl` / `mrr` / `mrl` | 单电机测试 | 必须拆桨，用于确认电机序号 |
| `log` / `log dump` | RAM 日志表头 / CSV 数据 | 飞后分析姿态、速度、位置和光流 |
| `sys` / `reset` / `reboot` | 系统信息 / 姿态复位 / 重启 | 查看运行状态和维护固件 |

#### 4. 调参建议

调参建议每次只改一类参数，并记录修改前后的日志。

##### 4.1 调试输出怎么用

| 阶段 | 推荐命令 | 关注字段 | 判断标准 |
|---|---|---|---|
| 上电静止 | `imu`、`ps` | 加速度计、角速度和姿态 | 机体静止时输出稳定，手持倾斜方向与机体一致 |
| 遥控检查 | `rc` | 通道、归一化输入、模式和 armed | 摇杆及三段开关映射正确 |
| 手持平移/旋转 | `flow` | RawVel、Filtered body、Gyro apparent、Position | 平移方向正确；原地旋转时补偿后速度接近零 |
| 电机映射 | `mfr/mfl/mrr/mrl` | 对应电机 | 四个命令与机体电机位置一致，必须拆桨 |
| STAB 低空 | `log dump` | rates、ratesTarget、attitude、motor | 角速度和姿态跟随目标，电机没有持续饱和 |
| ALT_HOLD | `alt`、`log dump` | Target、Alt(TOF)、velocity.z、altReject | 高度围绕进入模式时的目标收敛 |
| POS_HOLD | `flow`、`log dump` | UsingFlow、PosGate、HoldGate、Locked、position.x/y | 门控和锁点建立后位置控制生效 |

##### 4.2 基础姿态环

1. 保持 `STAB` 模式，先确认不会自旋、不会单方向持续倾倒。
2. 若高频抖动，先降低 `CTL_R_RATE_D` / `CTL_P_RATE_D` 或检查电机、桨叶、机架震动。
3. 若姿态响应慢，再小幅提高 `CTL_R_RATE_P` / `CTL_P_RATE_P`。
4. 若姿态能回正但有慢性偏差，再考虑小幅增加 Rate I，不要先动积分。

##### 4.3 定高

当前定高采用“飞手基础油门 + PID 修正”。先在 `STAB` 中找到接近悬停的油门，再以相近油门切入 `ALT_HOLD`；高度目标在模式接入时自动记录。

| 现象 | 优先调整 |
|---|---|
| 切入 `ALT_HOLD` 后修正量不足 | 将基础油门保持在接近悬停位置 |
| 高度上下震荡 | 检查 ToF 反射面，并适当降低定高 P/D 编译常量 |
| 高度响应迟钝 | 确认 `alt` 中 `AltHold=1`，再适当增加定高 P 编译常量 |
| 无法接入定高 | 查看 `alt` 的 `Reject` 和 ToF 高度 |

##### 4.4 光流方向与定点

当前控制用 XY 不是原始像素，而是经过高度换算、陀螺补偿、bias 扣除和门控后的相对位置估计。调光流时要区分三类量：

| 字段 | 含义 |
|---|---|
| `flow` 中的 `RawVel` | 光流模块原始轴向速度 |
| `Filtered body` | 角速度补偿和滤波后的机体系速度 |
| `Gyro apparent` | 机体旋转在光流中产生的表观速度 |
| `Position X/Y` | 飞控积分的水平相对位置 |
| `UsingFlow/PosGate/HoldGate/Locked` | 光流使用、位置估计和定点控制状态 |

校准顺序：

1. 拆桨，手持前后和左右平移，每个方向执行一次 `flow`，确认速度符号。
2. 原地俯仰和横滚后执行 `flow`，观察 `Filtered body` 是否接近零；需要时调整 `FLOW_GYRO_P/R` 和 `FLOW_GYRO_DLY`。
3. 完成 STAB 与 ALT_HOLD 检查后，再低空切入 `POS_HOLD`。
4. 使用 `flow` 查看 `UsingFlow`、`PosGate`、`HoldGate` 和 `Locked`，使用 `log dump` 分析连续过程。
5. 定点横向发散时切回 `STAB`，先检查方向和旋转补偿，再调整 `POS_HOLD_P` 与 `POS_VEL_P_X/Y`。

##### 4.5 RAM 日志

`log dump` 会输出 CSV 格式数据。当前日志列包括：

| 类别 | 字段 |
|---|---|
| 时间 | `t` |
| 角速度 | `rates.x/y/z`、`ratesTarget.x/y/z` |
| 姿态 | `attitude.x/y/z`、`attitudeTarget.x/y/z` |
| 控制用位置 | `position.x/y/z` |
| 控制用速度 | `velocity.x/y/z` |
| 光流链 | `flowRaw.x/y`、`flowComp.x/y`、`flowFilt.x/y`、`flowGyro.x/y` |
| 位置控制 | `targetPosX/Y`、`targetVel.x/y`、`velError.x/y`、`posRollCmd`、`posPitchCmd` |
| 门控与诊断 | `flowAgeMs`、`flowPosGate`、`posHoldGate`、`flowReject`、`posReject`、`altReject` |
| 控制输出 | `thrustTarget`、`motor.rl/rr/fr/fl`、`posSaturated`、`motorSaturated` |

日志以 50 Hz 保存最近约 6 秒的飞行数据。解除锁定后执行 `log dump`，用表格或 Python/Matlab 绘制姿态、高度、光流、位置目标、控制指令和电机输出。

#### 5. 故障排查表

| 现象 | 可能原因 | 解决方法 |
|---|---|---|
|起飞即翻|桨叶方向错误|检查 X 布局：对角线同向，相邻反向|
|STAB 稳但 XY 漂移|未进入 POS_HOLD 或光流门控未打开|串口 `flow` 查看 `UsingFlow/PosGate/HoldGate/Locked` 和拒绝码|
|定点越修越跑|光流 X/Y 方向或角速度补偿错误|手持平移和原地旋转后执行 `flow`，检查 RawVel 与 Filtered body|
|高度波动大|ToF 数据抖动、反射面不合适或定高增益偏大|运行 `alt` 并结合日志检查 `flowHeight`、`position.z` 和 `velocity.z`|
|悬停震荡|位置/速度环增益过大|降低 `POS_HOLD_P` 或 `POS_VEL_P_X/Y`|
|电机输出明显偏一侧|混控方向、电机顺序或姿态估计方向错误|拆桨依次执行四个单电机测试命令，并在日志中确认修正方向|
|姿态环抖动|机架震动或角速度环 D 项过大|检查机架与桨叶，并结合 `log dump` 分析 rates 和 motor 输出|
|解锁失败|RC 信号异常或油门未归零|检查 SBUS 接线，串口 `rc` 查看通道值|
|WiFi 连不上|SSID/密码设置错误或供电不稳|连接默认 AP `open32drone`；修改凭据时使用 `ap <ssid> <password>` 并重启|
|MAVROS 连不上|电脑未连接飞控 AP，或 IP/UDP 端口错误|确认飞控地址为 192.168.4.1、端口为 14550，并用 `wifi` 查看状态|

## 三、其他补充内容

### 参考链接汇总

- Flix 原项目：[github.com/okalachev/flix](https://github.com/okalachev/flix)

- MAVLink 协议文档：[mavlink.io](https://mavlink.io)

- QGroundControl：[qgroundcontrol.com](https://qgroundcontrol.com)

- ESP32-S3 技术手册：[espressif.com](https://www.espressif.com)

- PX4 开发指南（PID 调优）：[docs.px4.io](https://docs.px4.io)

- Crazyflie 技术文档（参考 Lee 控制器）：[bitcraze.io](https://www.bitcraze.io)
