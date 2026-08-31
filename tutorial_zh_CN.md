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

**Open32Drone Minimal** 是一个基于 **ESP32-S3** 的开源微型无人机平台，面向科研教育、嵌入式飞控开发与低空机器人实验。

平台在轻量化嵌入式架构中集成惯性测量、光流/ToF、SBUS、四路有刷电机驱动、Wi-Fi/MAVLink 和 ROS 2 接口。单一 ESP32-S3 负责姿态稳定、辅助起降、低空定高/定点以及外部位置和速度控制；载板、传感器时序、相对状态估计、递进控制和配套应用共同构成一套匹配的系统实现。

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

##### IMU模块

<p align="center">
    <img src="img\imu.PNG" />
</p>

规格型号：GY-91 模块（MPU9250 + BMP280）。当前飞行配置关闭 BMP280，姿态使用 MPU9250，定高使用光流模块内置 ToF。

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
    <img src="img\tof.png" />
</p>

规格型号：CORVON link协议

需求：1个

参考价格：68元（1个价格）

##### 电机驱动芯片

<p align="center">
    <img src="img\mos.png" />
</p>

规格型号：MOS场效应管AO3400

需求：4个

参考价格：0.4元（4个价格）

##### 遥控器

<p align="center">
    <img src="img\controller.png" />
</p>

规格型号：福斯I6S单控

需求：1个

参考价格：249元（1个价格）

##### 接收器
<p align="center">
    <img src="img\receiver.png" />
</p>
规格型号：福斯A8S接收器，SBUS接收器

需求：1个

参考价格：65元（1个价格）

##### 其他物料

电机插座，电池，排针，排母若干

#### 2. 底板加工

<p align="center">
    <img src="img\pcb1.png" />
</p>

<p align="center">
    <img src="img\pcb2.png" />
</p>

规格型号：自制

需求：需要1个

图纸链接：https://oshwhub.com/fanchewang/open32drone

##### 2.1 打开设计图

<p align="center">
    <img src="img\design1.png" />
</p>

##### 2.2 PCB下单

<p align="center">
    <img src="img\design2.png" />
</p>

<p align="center">
    <img src="img\design3.png" />
</p>

<p align="center">
    <img src="img\design4.png" />
</p>

其他基本默认选择

<p align="center">
    <img src="img\design5.png" />
</p>

<p align="center">
    <img src="img\design6.png" />
</p>

#### 3. 无人机组装

##### 3.1 物料清点

电阻（0805规格10K 20K都可以）

<p align="center">
    <img src="img\all_stuff.png" />
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
    <img src="img\hanjie1.png" />
</p>

###### 步骤二：焊接模块插座 (排母与排针)

一旦贴片元件稳固，我们就可以焊接用于插接模块的接插件了。

- **ESP32-S3接口**：焊接两排2.54mm排母，确保高度水平，否则主控板插上后会倾斜。

- **传感器接口**：包含GY91模块接口、光流TOF二合一模块接口以及 5V 稳压模块接口。

- **焊接要点**：排母引脚较多，建议先焊对角线的两个引脚进行定位，确认位置垂直后再焊剩余引脚。

<p align="center">
    <img src="img\hanjie2.png" />
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
    <img src="img\software1.png" />
</p>


依次进入 **Tools > Board > Boards Manager...**，在搜索框中输入关键字 **esp32**，安装与工程验证环境一致的 **esp32 3.3.6**。

<p align="center">
    <img src="img\software2.png" />
</p>

- **步骤 4.** 选择你的开发板和端口。

在 Arduino IDE 顶部，你可以直接选择端口。它很可能是 COM3 或更高（**COM1** 和 **COM2** 通常保留给硬件串口）。同时，在左侧的开发板中搜索 **xiao**。选择 **XIAO_ESP32S3**。

<p align="center">
    <img src="img\software3.png" />
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
    <img src="img\software4.png" />
</p>

- **步骤 3.** 将开发板型号选择为 **XIAO ESP32-S3**，并选择正确的端口号后上传程序。

<p align="center">
    <img src="img\software5.png" />
</p>

当程序成功上传后，你会看到如下输出信息，并且可以观察到 XIAO ESP32-S3 右侧的橙色 LED 正在闪烁。

<p align="center">
    <img src="img\software6.png" />
</p>

##### 1.5 依赖库安装

<p align="center">
    <img src="img\software7.png" />
</p>

标准构建使用 Arduino-ESP32 `3.3.6`、`FlixPeriph 1.10.4`、`MAVLink 2.0.25` 和 SBUS。选择 `XIAO_ESP32S3` 后启用 OPI PSRAM，使用 `default_8MB` A/B 应用分区与 DIO Flash。默认 IMU 后端兼容 MPU6500/MPU9250；ICM20948 和 MPU6050 仅作为需要单独验证的编译配置。Minimal 固件没有气压计控制路径。

#### 2. 飞控代码架构

本节给出开发时需要的最短路径；完整的模块职责、控制权、后台服务与源码阅读顺序见[固件架构](docs/FIRMWARE_ARCHITECTURE.zh-CN.md)。

##### 2.1 运行时结构

当前固件不是“每个功能一个任务”的并行飞控。飞行关键路径集中在固定 300 Hz 的高优先级 `loopTask`：传感器采集完成后，状态估计、目标选择、飞行控制和电机输出按固定依赖顺序执行。CLI、MAVLink 与 OTA 启动验收在同一循环中限频服务；可选相机和 MJPEG HTTP 服务运行在低优先级 core-0 后台任务，不拥有控制权。

```mermaid
flowchart LR
  INPUT["imu · rc · flow"] --> EST["estimate"]
  EST --> SAFE["safety · ownership"]
  SAFE --> CTRL["auto · altitude · position · attitude · rate"]
  CTRL --> MOTOR["mix · PWM"]
  MOTOR --> SERVICE["CLI · MAVLink · log · NVS"]
  CAMERA["OV3660"] --> STREAM["低优先级 HTTPD 图传任务"]
```

| 层次 | 文件 | 代码边界 |
|---|---|---|
|入口与调度|`firmware.ino`、`time.ino`|编译开关、初始化顺序、64 位单调时间、主循环与任务优先级|
|输入|`imu_backend.h`、`imu.ino`、`rc.ino`、`flow.ino`|编译期选择的 IMU、SBUS 和 TF-0850 数据包；ToF 与 XY 光流分别维护序列、时间戳和健康状态|
|估计|`estimate.ino`|姿态、ToF 高度/垂直速度，以及带角速度时间对齐的水平速度/相对位置|
|控制|`control.ino`、`control_modes.ino`、`control_offboard.ino`、`control_auto_flight.ino`、`control_altitude.ino`、`control_position.ino`、`control_stabilization.ino`|共享控制状态，以及控制权、Offboard、自动起降、定高、定点、姿态和角速度各层|
|安全与电源|`safety.ino`、`power.ino`|解锁预检、失联下降、持续翻覆停桨、电池电压与悬停推力前馈|
|执行|`motors.ino`|Quad-X 映射与四路 LEDC PWM|
|通信|`mavlink.ino`、`wifi.ino`、`camera.ino`、`ota.ino`|MAVLink、AP/STA 网络、可选图传和仅地面 A/B OTA|
|观测|`cli.ino`、`log.ino`|本地串口诊断、25 Hz RAM 日志和每 16 圈一次的性能采样|
|配置与数学|`parameters.ino`、`*.h`|参数校验/NVS、PID、滤波、四元数和向量工具|

TF-0850 数据由 `flow.ino` 接收，高度状态由 `estimate.ino` 更新，定高控制位于 `control_altitude.ino`。光流/ToF 模块安装在偏航旋转中心前方约 24 mm，固件在水平估计中补偿该几何偏移。

##### 2.2 硬件引脚映射

以下引脚与底板硬件直接绑定，修改时务必确认底板电路图：

| 外设 | GPIO | 说明 |
|---|---|---|
|I2C SDA|2|编译期选择的 IMU 数据线，400 kHz|
|I2C SCL|43|编译期选择的 IMU 时钟线|
|光流 RX|8|Serial1 RX，接光流模块 TX。协议：帧头 0xDF，19字节包，115200bps|
|光流 TX|7|Serial1 TX，接光流模块 RX|
|SBUS RX|44|Serial2 RX，100Kbps，25字节帧|
|SBUS TX|9|Serial2 TX|
|LED|21|板载 NEOPIXEL|
|电池电压|1 / A0|`VBAT_SW × 0.5` 分压输入|
|MOTOR 0|4|LEDC PWM 10 kHz → AO3400 → 左后（CW）|
|MOTOR 1|3|右后（CW）|
|MOTOR 2|6|右前（CCW）|
|MOTOR 3|5|左前（CCW）|

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

##### 3.3 飞行控制（control.ino + control_*.ino）

**递进控制模式**

| 模式 | 值 | 行为 |
|---|---|---|
|STAB|2|默认姿态自稳模式。油门直接映射总推力，Roll/Pitch 摇杆映射姿态目标。|
|ALT_HOLD|4|ToF 定高模式。中位油门保持目标高度，偏离中位时给出有界垂直速度指令；总推力由悬停前馈与高度 PID 组成。|
|POS_HOLD|5|光流定点模式。在定高和水平估计合格后锁定位置；Roll/Pitch 摇杆以速度方式移动目标点。|

`AUTO` 是自动起降与经过验证的 Offboard 控制使用的内部控制权状态，不是飞手第四种模式。三种对外模式复用同一姿态、角速度和混控内环，并按 ToF 与光流状态的可用性逐层增加垂直和水平闭环。

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
|定高 PID|以当前固件注册值为准|设备参数可能由 NVS 覆盖；使用 `p ALT_P`、`p ALT_I`、`p ALT_D` 回读|
|`ALT_HOVER`|0.49|重启后的悬停推力基线；起飞后只在 RAM 中缓慢自适应|
|`ALT_VEL_MAX`|0.45 m/s|油门离开中位死区后的最大垂直速度指令|
|`POS_HOLD_P`|0.80|位置误差到水平速度目标的比例增益|
|`POS_STICK_V`|0.70 m/s|Roll/Pitch 满杆对应的目标点移动速度|
|`POS_VEL_P_X/Y`|0.30 / 0.30|水平速度闭环比例增益|
|`POS_VEL_I_X/Y`|0.10 / 0.10|水平速度闭环积分增益，单轴输出限制为 0.08 rad|
|`POS_VEL_D_X/Y`|0 / 0|水平速度闭环微分增益|
|`POS_CMD_RATE`|1.20 rad/s|定点姿态指令变化率限制|
|`FLOW_GYRO_P/R`|-0.78 / -0.77|Pitch/Roll 旋转光流补偿系数|
|`FLOW_GYRO_DLY`|40 ms|光流与角速度的时间对齐|

##### 3.4 MAVLink 调试（mavlink.ino）

**发送的消息**

| 消息 | 频率 | 说明 |
|---|---|---|
|HEARTBEAT / CURRENT_MODE|2 Hz|上报 GENERIC 四旋翼身份、当前自定义模式和解锁状态|
|EXTENDED_SYS_STATE / SYS_STATUS|2 Hz|上报着陆、传感器健康和系统状态|
|BATTERY_STATUS|2 Hz|配置电压采样引脚后上报实测电压；默认硬件返回未知值|
|ATTITUDE_QUATERNION|10 Hz|四元数姿态与角速度，按 MAVLink FRD 坐标约定转换|
|RC_CHANNELS_RAW (#35)|~10 Hz|16 通道原始 PWM 值|
|ACTUATOR_CONTROL_TARGET|10 Hz|当前 4 路电机归一化输出|
|SCALED_IMU|10 Hz|加速度计与陀螺仪数据|
|LOCAL_POSITION_NED / DISTANCE_SENSOR|10 Hz|上报机载相对位置/速度与有效 ToF 距离|

**接收的关键消息**

| 消息/命令 | 作用 |
|---|---|
|MANUAL_CONTROL|外部手动控制，映射到油门、俯仰、横滚、偏航|
|PARAM_REQUEST_LIST / PARAM_REQUEST_READ / PARAM_SET|参数读取与设置|
|MAV_CMD_COMPONENT_ARM_DISARM|MAVLink 解锁/上锁，油门高于 0.05 时拒绝解锁|
|MAV_CMD_DO_SET_MODE / DO_SET_STANDARD_MODE|切换受支持的自稳、定高、定点和 AUTO 模式|
|MAV_CMD_NAV_TAKEOFF / MAV_CMD_NAV_LAND|执行有传感器门控的自动起飞和降落|
|SET_ATTITUDE_TARGET|连续预热后在 AUTO 模式接收姿态、角速度和推力目标|
|SET_POSITION_TARGET_LOCAL_NED|在 AUTO 模式接收有界局部位置/速度和高度/垂直速度目标|
|SERIAL_CONTROL|仅把本地诊断文字镜像到 MAVLink；不接收入站远程 Shell|

Minimal 固件不接受 MAVLink 直接电机控制。标准参数协议只应在上锁状态使用；飞行中的控制入口限于受门控的生命周期命令、手动控制租约和 Offboard 位置/速度目标。

##### 3.5 ROS2/MAVROS 接入说明

仓库中通过 MAVROS 连接飞控 UDP 14550，并提供IMU/里程计/ToF/电池/RC 接口、`/cmd_vel`、`/goal_pose`、生命周期命令、TF、RViz2 和验收工具。飞控默认建立 AP `open32drone`，也可显式加入路由器 STA；同一架飞机同一时刻只能由 Android 或 ROS 2 控制。当前 ROS 包不转发实验性 HTTP MJPEG，也不提供 `camera_info`。安装、单机/多机命名、公共接口和受监督起降流程统一见[ROS 2 配套软件与自动飞行](docs/AUTOMATIC_FLIGHT_AND_ROS2.zh-CN.md)。

**架构**

```python
open32drone_driver
├── MAVROS                 ← UDP 14550 → Open32Drone
├── interface_bridge       ← Reliable IMU/odom/ToF/battery/RC + diagnostics + TF
├── flight_manager         ← arm/takeoff/land/mode 命令
├── offboard_control       ← 20 Hz 位置/速度目标与看门狗
└── rc_bridge              ← 显式启用的原始 RC 流
```

**关键参数**

| 参数 | 值 | 说明 |
|---|---|---|
|fcu_url|udp://0.0.0.0:14550@192.168.4.1:14550|AP 模式默认连接；STA 模式替换为串口 `wifi` 输出的地址|
|tgt_system / tgt_component|1 / 1|Open32Drone 飞控的 MAVLink 目标标识|
|setpoint rate|20 Hz|Offboard 节点持续刷新位置或速度目标|
|warmup|至少 0.35 s、5 个样本、10 Hz|飞控在切入 AUTO 前检查目标流连续性|
|watchdog|0.30 s|外部目标停更后触发机载 Offboard 失效处置|
|public QoS|Reliable|`/imu/data`、`/odom`、`/range/downward` 等桥接话题面向普通 ROS 工具|

##### 3.6 ROS 2 手动控制命令速查

教程只保留最短操作路径；接口表、多机命名、TF、验收规则和故障处理见独立 ROS 2 文档。自动起飞由固件统一完成预检、解锁、爬升和定点交接，成功后再发送有限时长的机体系速度目标。

| 动作 | 参数 | 命令 |
|---|---|---|
|起飞|相对起飞面 0.65 m|`ros2 run open32drone_driver control takeoff --height 0.65`|
|前进|+X，0.25 m/s，1.5 s|`ros2 run open32drone_driver control velocity 0.25 0.00 0.00 --duration 1.5`|
|左移|+Y，0.25 m/s，1.5 s|`ros2 run open32drone_driver control velocity 0.00 0.25 0.00 --duration 1.5`|
|上升|+Z，0.20 m/s，1 s|`ros2 run open32drone_driver control velocity 0.00 0.00 0.20 --duration 1`|
|降落|受控下降并自动上锁|`ros2 run open32drone_driver control land`|

`/cmd_vel` 使用机体系（+X 前、+Y 左、+Z 上）；`/goal_pose` 是 `open32drone/odom` 中的绝对目标。一次只保留一个控制源，自动试飞前先拆桨运行 `ros2 run open32drone_driver bench_test --duration 5`，并用 `control status` 核对真实连接。

##### 3.7 A/B 固件升级与 Minimal 套件

当前开发配置使用 ESP32-S3 8 MB `default_8MB` A/B 分区：`ota_0` 位于 `0x10000`，`ota_1` 位于 `0x340000`，每个应用槽大小为 `0x330000`。旧单应用分区必须先通过 USB 完成一次迁移；后续无线升级只写入非活动槽。

`releases/minimal/` 中的匹配文件用途如下：

| 文件 | 用途 |
|---|---|
|`Open32Drone-minimal-merged.bin`|完整 8 MiB USB 镜像；写入 `0x0`，用于新 MCU 或完整擦除后的恢复|
|`Open32Drone-minimal-app.bin`|仅用于地面 A/B OTA 的应用镜像|
|`Open32Drone-Controller-0.1.apk`|匹配 Android 客户端，`versionName 0.1`、`versionCode 1`|
|`SHA256SUMS`|刷写或上传前校验文件完整性|

首次迁移必须拆桨，并在需要保留校准时避免 `erase-flash`。迁移完成后执行 `sys`、`imu`、`flow` 和 `ota`，确认两个应用槽、陀螺仪校准、ToF、控制循环和本机 OTA 令牌均正常。无线升级前必须上锁、落地并停止自动飞行、Offboard 和图传；Android 或 ROS 2 上传器只能选择应用 `.bin`，不能选择 merged 镜像。上传器会发送令牌、文件长度和 SHA-256，新槽只有在启动后连续通过参数区、IMU、姿态、循环频率、ToF 和 MAVLink 健康检查才会被确认，否则 Bootloader 回滚到上一槽。

##### 3.8 CLI 调试命令

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
| `ap <ssid> <password>` / `sta <ssid> <password>` | 保存 AP 或路由器 STA 配置 | 重启后生效；STA 连接失败会打开恢复 AP |
| `arm` / `disarm` | 串口解锁 / 上锁 | 拆桨调试用 |
| `stab` / `alt` / `pos` | 切换三种飞行模式 | 串口拆桨调试控制链 |
| `mfr` / `mfl` / `mrr` / `mrl` | 单电机测试 | 必须拆桨，用于确认电机序号 |
| `log` / `log dump` | RAM 日志表头 / CSV 数据 | 飞后分析姿态、速度、位置和光流 |
| `perf` | 分阶段循环耗时统计 | 上锁状态下评估 300 Hz 主循环与后台负载 |
| `ota` | A/B 升级状态和设备令牌 | 配套 Android/ROS 2 上传器的本地鉴权信息 |
| `sys` / `reset` / `reboot` | 系统信息 / 姿态复位 / 重启 | 查看运行状态和维护固件 |

##### 3.9 Android 客户端与配套版本

要求 Android 8.0（API 26）及以上。一个可配置的飞机 IPv4 地址统一用于 MAVLink、可选图传和 OTA；客户端提供定高/定点、自动起降、双摇杆、状态诊断和 A/B 应用镜像上传。

使用顺序为：连接 `open32drone` Wi-Fi，等待 MAVLink 与 ToF 就绪，选择辅助模式，完成解锁/起飞，飞行结束后执行降落并确认上锁。图传只有收到新鲜 MJPEG 帧后才替换默认背景；断流不会冻结旧画面。实体 SBUS 飞手产生明确动作后取得普通控制权，手机停止发送普通摇杆指令；紧急上锁入口仍保留。

Minimal Android APK、固件和 ROS 2 包是一组匹配接口。安装或刷写前应核对 `SHA256SUMS`，不要混用不同提交的组件。Android 与 ROS 2 不能同时控制同一架飞机；QGC 仅用于地面参数维护。当前 APK 为 Debug 签名，适合开发和受控测试，不应作为正式签名的产品发行包。

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

当前定高采用“悬停推力前馈 + 高度 PID”，油门摇杆在辅助模式中解释为以 50% 为中位的垂直速度指令。首次调试应先在 `STAB` 估计悬停推力并写入 `ALT_HOVER`；进入 `ALT_HOLD` 后回中保持高度，上/下拨杆移动高度目标。地面解锁后电机保持怠速，油门超过起飞触发阈值并持续 0.20 s 才启动受限推力斜坡。

| 现象 | 优先调整 |
|---|---|
| 切入 `ALT_HOLD` 后推力不足 | 检查 `ALT_HOVER` 与运行时 `hoverEstimate`，不要用摇杆偏置代替前馈标定 |
| 高度上下震荡 | 检查 ToF 反射面，并适当降低 `ALT_P` / `ALT_D` |
| 高度响应迟钝 | 确认 `alt` 中定高已接入，再小幅调整 `ALT_P` 或 `ALT_VEL_MAX` |
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
| 门控与诊断 | `flowAgeMs`、`flowPosGate`、`posHoldGate`、`flowReject`、`posReject`、`altReject`、`actuatorOwner`、`autoPhase`、`posFallback` |
| 控制输出 | `thrustTarget`、`motor.rl/rr/fr/fl`、`mixerScale`、`posSaturated`、`motorSaturated` |

RAM 日志以 25 Hz 保存最近约 12 秒的飞行数据，解除锁定后使用 `log dump` 导出；飞行中禁止批量导出。`perf` 使用每 16 个控制周期一次的阶段采样评估 IMU、输入、估计、控制、CLI、MAVLink 和维护路径的耗时。安全模块保留失联下降和一个持续姿态门限的最小翻覆停桨，不进行碰撞分类，也没有独立事故缓冲。

#### 5. 飞行前检查与分阶段验收

每次新装机、改动飞行关键代码或更换传感器后，按以下顺序推进：

```mermaid
flowchart LR
  BUILD["编译与静态检查"] --> BENCH["拆桨检查"]
  BENCH --> RESTRAINED["低功率约束测试"]
  RESTRAINED --> FLIGHT["受控低空飞行"]
  FLIGHT --> EVIDENCE["参数 · 日志 · 版本记录"]
```

| 阶段 | 必做项目 | 进入下一阶段的条件 |
|---|---|---|
|软件|固定 ESP32 Core/分区配置编译；检查测试和凭据|构建成功，版本与参数来源明确|
|拆桨|`sys`、`imu`、`rc`、`flow`；校准；四路单电机；RC/Offboard 超时|传感器方向、电机顺序、拒绝逻辑和停桨路径正确|
|低功率约束|检查预转、起飞斜坡、定高接入、定点门控、自动降落和持续翻覆门限|输出连续且有界，人工接管有效，无异常饱和|
|受控飞行|依次验证 STAB、ALT_HOLD、POS_HOLD，再验证自动起降和外部控制|姿态稳定，高度/位置修正方向正确，失效时按预期退化|

飞行前确认机架方向、电机/桨叶安装、飞控固定、地面纹理与照明、ToF 反射条件和场地隔离。开机后保持静止，直到 `imu` 显示陀螺仪校准完成；用 `rc` 检查摇杆和三段开关，用 `flow` 分别确认 ToF 与水平光流状态。首次带桨只做短时低空 STAB，确认人工恢复后再进入定高和定点。每轮只改变一个因素，并记录固件版本、硬件、参数、环境和 RAM 日志。

#### 6. 故障排查表

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
