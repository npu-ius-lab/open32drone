# Open32Drone 完整教程

[English](tutorial.md) · [简体中文](tutorial_zh_CN.md)

## 目录

- [01 · 项目介绍](#chapter-01)
- [02 · 制作目标](#chapter-02)
- [03 · 开始制作](#chapter-03)
- [04 · 固件、校准与起飞](#chapter-04)
- [05 · 飞行调参](#chapter-05)
- [06 · ROS 2 控制](#chapter-06)
- [07 · 强化学习](#chapter-07)


---

<a id="chapter-01"></a>

## 01 · 项目介绍

<a id="chapter-01-section-1"></a>

### 从这里开始

第一次接触时按下表顺序阅读；已有飞机可以直接进入对应阶段。

| 你现在要做什么 | 对应入口 |
|---|---|
| 准备器件、打印机架、取得 PCB 并组装 | [采购清单](#chapter-03-purchasing) · [组装教程](#chapter-03) |
| 下载固件和 APK，完成刷写、校准及首飞 | [固件、校准与首飞](#chapter-04) |
| 已经飞起来，想调好或反馈问题 | [调参与排查](#chapter-05) · [问题反馈要求](docs/project/contributing.zh-CN.md) |
| 接入 ROS 2，读取数据并编程控制 | [ROS 2 控制](#chapter-06) |
| 编译或修改软件 | [源码与编译](docs/reference/source-build.zh-CN.md) · [参数与接口](docs/reference/firmware.zh-CN.md) |
| 进一步学习仿真与强化学习 | [独立数值练习与仿真](#chapter-07) |

没有物理遥控器，可以在首飞章节选择 Android 路线。

<a id="chapter-01-section-2"></a>

### 一架从制造开始的微型无人机

Open32Drone 是一套开源四旋翼项目。项目采用模块化电子结构，Open32Drone PCB 负责供电、四路有刷电机驱动和模块连接；XIAO ESP32-S3、IMU、光流/ToF 都是需要安装的独立模块。

参考样机使用 8520 空心杯电机和 1S 电池完成室内飞行，以 ESP32-S3 运行 300 Hz 飞控，通过 IMU 感知姿态，通过向下安装的光流/ToF 一体模块保持水平位置和离地高度。

项目最初的飞控核心来自 Oleg Kalachev 的 Flix。Open32Drone 在这个基础上加入了实际主控板的引脚映射、四路有刷电机输出、光流/ToF 定高定点、电池电压补偿、自动起降、参数保存，以及 Android 和 ROS 2 控制接口。

<a id="chapter-01-section-3"></a>

### 系统由哪些部分组成

飞机上的部件可以分成四层。

| 层次 | 主要部件 | 作用 |
| --- | --- | --- |
| 结构与动力 | 打印机架、4 个 8520 电机、4 个 60/65 mm 桨叶、橡胶电机圈 | 承载零件并产生升力和姿态力矩 |
| 飞控与传感 | Open32Drone PCB 底板、XIAO ESP32-S3、IMU、光流/ToF 一体模块 | 完成电机驱动、状态估计与控制计算 |
| 能源与通信 | 1S 电池、SBUS 接收机、Wi-Fi | 给系统供电，并接收遥控或程序命令 |
| 上位机与仿真 | Android APK、ROS 2、URDF/USD、Gazebo、Isaac Sim、PPO 示例 | 人工飞行、机器人编程、模型验证与学习控制 |

<a id="chapter-01-section-4"></a>

### 无人机数据链

IMU 提供角速度和加速度，ToF 提供离地高度，光流提供地面相对运动。飞控把这些测量组合成姿态、速度和位置估计，再根据驾驶员或 ROS 给出的目标计算四路电机输出。

```text
传感器测量 → 状态估计 → 姿态/高度/位置控制 → 电机混控 → 飞机运动
       ↑                                                   │
       └────────────────── 下一周期的新测量 ───────────────┘
```

<a id="chapter-01-section-5"></a>

### 参考样机

文中的尺寸、重量和参数以这台参考样机为例：

- 机架外形约 103.3 × 103.3 mm；
- 四个 8520 电机，8 × 20 mm，1 mm 轴；
- 四个 60 mm 桨叶，两只 CW、两只 CCW；
- 18350 1S 1300 mAh 电池，实测质量 25 g；
- 含电池起飞重量约 81 g，水平重心位于机体中央；
- XIAO ESP32-S3、MPU6500/MPU9250 IMU、光流/ToF 一体模块；
- 物理 SBUS、Android APK、ROS 2 三种控制入口。

<a id="chapter-01-section-6"></a>

### 仓库地图

| 目录 | 内容 |
| --- | --- |
| `software/hardware/` | 机架 3MF/STEP、机械规格和采购信息 |
| `software/firmware/` | ESP32-S3 飞控源码 |
| `software/android/` | 手机控制端源码 |
| `software/ros2/` | ROS 2 驱动、控制命令、RViz 配置 |
| `software/simulation/` | 教学实验、动力学、Gazebo/Isaac 与强化学习代码 |
| `software/releases/minimal/` | 相互匹配的完整固件、OTA 镜像、APK 和 ROS 包 |
| `docs/` | 安装、参数、故障排查与项目教程 |

制作所需的文件和代码都在上面这些目录中。下面先说明固件已有的功能，第二章再介绍制作顺序。

<a id="chapter-01-section-7"></a>

### 飞行功能和传感器

下面列出这套固件支持的主要功能。表中的 `time`、`imu` 等是串口命令，刷好固件后可以用它们查看运行情况。

| 功能 | 支持情况 | 查看或使用方法 |
|---|---|---|
| 固定 300 Hz 飞控循环 | 已启用 | `time`、`perf` |
| 姿态 / 定高 / 定点 | 已启用 | SBUS 模式开关、Android/ROS 控制命令 |
| 相对高度起飞与自动降落 | 已启用 | Android/ROS 命令或 SBUS 辅助起飞 |
| 默认 MPU6500/MPU9250 IMU | 已启用 | 编译期后端、`imu` |
| ICM20948 与 MPU6050 后端 | 编译选项 | 必须单独编译并完成对应硬件验证 |
| TF-0850 光流与 ToF | 已启用 | `flow`、MAVLink 遥测 |
| 电池电压检测与推力补偿 | 已启用，补偿幅度有限制 | `pw`、MAVLink 电池遥测 |
| 物理 SBUS 紧急上锁 | 已启用 | 独立于 Android/ROS 控制权 |
| 持续倾覆停桨 | 已启用 | 按倾斜角度和持续时间判断，不能识别所有碰撞情况 |

<a id="chapter-01-section-8"></a>

### 网络与维护

| 功能 | 支持情况 | 使用说明 |
|---|---|---|
| 飞机 Wi-Fi AP | 默认启用 | `ap <ssid> <pass>`，飞机固定为 `192.168.4.1` |
| 路由器 Wi-Fi STA | 已启用 | `sta <ssid> <pass>`，飞机地址由 DHCP 分配 |
| STA 启动恢复 | 已启用 | 启动后 8 秒内连不上路由器时开启原 AP，保存的 STA 配置仍然保留 |
| MAVLink 命令与遥测 | 已启用 | UDP `14550`；每架飞机同一时刻只能由 Android 或 ROS 之一控制 |
| ROS 2 多机控制 | 已启用 | 每架飞机使用唯一命名空间、System ID、本机 UDP 端口、IP 和 TF 前缀 |
| 标准 MAVLink 参数协议 | 已启用 | `PARAM_REQUEST_LIST`、`PARAM_REQUEST_READ`、`PARAM_SET`、`PARAM_VALUE` |
| MAVLink 诊断文字镜像 | 已启用、仅发送 | `SERIAL_CONTROL_DEV_SHELL`，不接收远程 CLI 输入 |
| 本地 USB 串口 CLI | 已启用 | 参数、校准、诊断、电机测试、网络配置 |
| 内存飞行日志 | 已启用 | 25 Hz、约 12 秒，可由串口/MAVLink 下载 |
| 循环采样分析器 | 已启用 | 每 16 次循环采样一次；未解锁时使用 `perf` |
| 地面 A/B OTA | 已启用 | HTTP `8080`；首次完整 USB 迁移后才可使用 app 固件 |
| 后台 MJPEG 图传 | 实验功能，仍需硬件和实飞验证 | HTTP `/stream`，一次只允许一个客户端观看，图像处理不放在 300 Hz 飞控循环内 |

Android 和 ROS 都通过 MAVLink 与飞机通信。固件会把回复发给最近一个有效的 UDP 发送端，所以控制同一架飞机时，要先关闭另一个控制端。图传同样一次只允许一个客户端观看：Android 正在看图时，先关闭预览，再用 OpenCV 读取。

<a id="chapter-01-section-9"></a>

### 哪些设置会在断电后保留

NVS 是芯片中用于保存参数的存储区。下面这些操作会把设置写进去，断电后仍然保留：

- 串口 `p <name> <value>` 或地面 MAVLink `PARAM_SET`；
- `ca` 加速度计校准；
- `cr` SBUS 校准；
- `ap` 或 `sta` 网络配置。

陀螺仪开机零偏、光流地面零偏、控制器积分、飞行目标和电压补偿只在本次运行中使用，不会覆盖已经保存的参数。`preset` 会重置注册参数，但保留 AP/STA 的网络名称和密码；完整擦除 Flash 则会把参数和网络设置一起清掉。

<a id="chapter-01-section-10"></a>

---

<a id="chapter-02"></a>

## 02 · 制作目标

第一次制作，先以完成一次稳定的起飞、悬停和降落为目标。熟悉这台飞机以后，再尝试修改参数、用 ROS 编写动作，最后做仿真和强化学习练习。每一部分都可以单独练习，不必一次完成全部内容。

<a id="chapter-02-section-1"></a>

### 最终能完成什么

<a id="chapter-02-section-2"></a>

#### 做出一架真实可飞的飞机

先打印机架，按配套生产文件下单 PCB 裸板，再对照 BOM 和位号图焊接器件、连接器、供电小板与 IMU。装好光流/ToF、XIAO、电机和电池后，标清机头、电机编号和转向。接线和安装位置可以拍照留存，后面排查问题时会用到。

<a id="chapter-02-section-3"></a>

#### 理解并刷入固件

第四章介绍 USB 完整刷写和后续 OTA 更新各用哪一个文件。刷好固件后，依次完成 IMU、遥控器和电池电压校准，再拆桨检查四路电机。首飞可以按手上的设备选择：

- 有 SBUS 接收机和遥控器时，使用实体摇杆与三段模式；
- 没有接收机时，手机直接连接飞机热点，使用配套 Android APK 自动起飞和降落。

<a id="chapter-02-section-4"></a>

#### 根据飞行现象调参

先观察问题出在哪里：机身快速抖动、缓慢摆动、定高时上下跳，还是水平漂移。第五章按这些现象介绍检查顺序。调参时每次只改一个值，再做一次相同的飞行动作，比较修改前后的变化。

<a id="chapter-02-section-5"></a>

#### 用 ROS 控制运动

连接 ROS 2 后，先查看 IMU、距离、电池和里程计数据，再试一次起飞和降落。之后可以发送速度和位置目标，把前进、横移、转向组合成自己的飞行动作，并用 rosbag 保存实验数据。

<a id="chapter-02-section-6"></a>

#### 建立仿真与强化学习流程

第七章先介绍怎样用 link、joint、质量和碰撞体描述飞机，再用 81 g 的近似动力学模型完成 CPU 悬停练习和 PPO 训练。数值练习可以直接运行；如果还想在 Isaac 中观看飞行，需要另行准备兼容的 USD 场景。仓库中没有打包完整场景和 Gazebo 飞行集成。

<a id="chapter-02-section-7"></a>

### 推荐学习路线

```mermaid
flowchart TD
    A[3D 打印机架] --> B[下单 PCB 裸板]
    B --> C[焊接 PCB]
    C --> D[整机组装]
    D --> E[USB 刷写与校准]
    E --> F{选择首飞方式}
    F -->|有 SBUS| G[遥控器定点首飞]
    F -->|无接收机| H[Android APK 定点首飞]
    G --> I[按现象调参]
    H --> I
    I --> J[ROS 起降与速度控制]
    J --> K[ROS 位置与航线]
    K --> L[URDF / USD 与动力模型]
    L --> M[PPO 训练与 Isaac 演示]
```

第一次制作建议从硬件部分开始。已经有能飞的样机，可以跳到 ROS 章节。做强化学习练习前，先熟悉坐标、速度和位置目标，能用 ROS 完成几次简单移动即可。

<a id="chapter-02-section-8"></a>

### 开始前需要的基础

硬件部分需要基本的焊接、万用表和锂电池使用经验。软件部分需要能够在终端中切换目录并运行命令。ROS 与强化学习章节会逐条给出命令，不要求预先会写复杂节点；读者如果了解 Python、向量和 PID，会更容易理解背后的原理。

真实飞行请使用有纹理、光照均匀的室内地面，周围至少留出 2 m 空间。焊接、刷写、校准和电机测试阶段均不安装桨叶；只有四路电机位置与转向确认后，才进入装桨首飞。

准备好后，就从下一章的机架打印和 PCB 制作开始。

---

<a id="chapter-03"></a>

## 03 · 开始制作

先打印机架并下单 PCB，等零件到齐后焊接电路板，再安装主控、传感器、电机和电池。下面按这个顺序介绍所需文件、零件和操作方法。装配期间先不装桨叶，等下一章刷好固件、校准并检查电机转向后，再回来装桨。

<a id="chapter-03-section-1"></a>

### 3.1 机架与 PCB

<a id="chapter-03-section-2"></a>

#### 打印机架

拓竹用户可以直接进入 [MakerWorld 机架打印页面](https://makerworld.com.cn/zh/models/2922108-open32drone-wu-ren-ji-8520kong-xin-bei-ji-jia-ros2#profileId-3425842)，选择“在 Bambu Studio 中打开”。页面提供的配置为 **0.2 mm 层高、6 层墙、25% 填充**；切片前选择自己的打印机和材料。其他切片软件可使用下方仓库文件。

仓库中的 `software/hardware/3d-model/open32drone-frame.3mf` 是推荐的打印工程文件。导入切片软件后保持 100% 比例，主机架外形应约为 103.3 × 103.3 mm。根据实际打印机、喷嘴和材料检查层高、壁厚、支撑与首层附着；打印完成后清理支撑，确认四个电机安装位没有变形，PCB 安装孔能够自然对齐。

`software/hardware/3d-model/open32drone-frame.stp` 用于修改结构或在其他 CAD 软件中检查尺寸。导入 STEP 后同样以 103.3 mm 左右的主机架外形复核单位，不要凭软件默认单位直接缩放。

<a id="chapter-03-section-3"></a>

#### PCB 制作

打开[嘉立创开源硬件 PCB 工程](https://oshwhub.com/fanchewang/open32drone)，在编辑器中打开或克隆设计，确认所用的硬件版本。介绍页没有显示电子 BOM 时，可以进入工程查看并导出。

下单前准备好同一版本的 Gerber 与钻孔包、电子 BOM、正反面位号图，以及接口和电压说明。板厚、铜厚和表面处理等选项按工程要求填写。

收到裸板后，先对照位号图检查板框、槽孔、通孔、焊盘和丝印，再开始焊接。后面的照片可以帮助辨认零件和安装方向，具体位置仍以配套 BOM 和位号图为准。

![主控 PCB 的正反面，丝印和接口清晰可见](img/pcb-bare-front-back.jpg)

图 3-1　Open32Drone PCB 底板正反面。底板负责电源、电机驱动和模块连接，XIAO、IMU 与光流/ToF 需要另行安装。

<a id="chapter-03-section-4"></a>

### 3.2 材料与工具

机架文件见上方“打印机架”，PCB 设计与板载电子件 BOM 从[嘉立创工程](https://oshwhub.com/fanchewang/open32drone)获取。下面准备模块、机械件和装配工具。

<a id="chapter-03-purchasing"></a>

<a id="chapter-03-section-5"></a>

#### 采购清单

表中数量为**一台飞机的用量**，选配件用量留空。打开商品链接后，按“规格参考”选择型号；商家的整包数量可能不同。橡胶圈等机械件的详细尺寸见[装配规格](#chapter-03-section-7)。

| 序号 | 部件 | 规格参考 | 单台用量 | 采购入口 |
| :---: | --- | --- | :---: | --- |
| 1 | 飞控底板 | 配套版本的生产文件、电子 BOM 与位号图 | 1 | [嘉立创工程](https://oshwhub.com/fanchewang/open32drone) |
| 2 | 打印机架 | 完整机架一套；主体约 103.3 × 103.3 mm，按 100% 比例打印 | 1 | [MakerWorld](https://makerworld.com.cn/zh/models/2922108-open32drone-wu-ren-ji-8520kong-xin-bei-ji-jia-ros2#profileId-3425842) |
| 3 | 主控板 | Seeed Studio XIAO ESP32-S3 Sense（含相机） | 1 | [商品页](https://item.taobao.com/item.htm?id=796226570709) |
| 4 | IMU 模块 | MPU9250；引脚排列与安装方向须匹配飞控底板 | 1 | [商品页](https://item.taobao.com/item.htm?id=867297908775) |
| 5 | 排母 | 1 × 7P；间距、高度须匹配飞控底板和主控板 | 2 | [商品选项](https://item.taobao.com/item.htm?id=1040276180385&skuId=6058024109270) |
| 6 | 跳线帽 | 间距 2.54 mm | 1 | [商品页](https://item.taobao.com/item.htm?id=1037786359471) |
| 7 | 升压板 | 输出标称 5 V / 1 A；输入适配 1S 电池 | 1 | [商品选项](https://item.taobao.com/item.htm?id=1020492920926&skuId=6194359034311) |
| 8 | 光流/ToF模块 | CORVON 纵川 TF-0850；UART 版，向下安装 | 1 | [商品页](https://item.taobao.com/item.htm?id=825567548453) |
| 9 | 光流线束 | 4P 双头反向线，长 60 mm；接口及引脚定义须匹配模块 | 1 | [商品页](https://item.taobao.com/item.htm?id=561435308484) |
| 10 | 8520 电机 | 机身 8 × 20 mm；轴径 1 mm；MX1.25 端子；线长 ≥ 100 mm | 4 | 暂未提供 |
| 11 | 桨叶 | 直径 60 mm；CW、CCW 各 2 | 4 | [商品页](https://item.taobao.com/item.htm?id=651317554058) |
| 12 | 电机橡胶圈 | Ø8 × 2 mm；建议两黑两白；[详细尺寸](#chapter-03-section-7) | 4 | [商品页](https://detail.tmall.com/item.htm?id=923643961535) |
| 13 | 固定螺丝 | 1 × 4 × 4 mm | 10 | [商品页](https://item.taobao.com/item.htm?id=658713209127&skuId=4755138613087) |
| 14 | 电池 | 1S 18350；JST 引出线须匹配飞控底板接口及极性 | 1 | [商品页](https://item.taobao.com/item.htm?id=900687087724) |
| 15 | 电池固定皮筋 | 直径 25 mm，宽 5 mm | 1 | [商品页](https://item.taobao.com/item.htm?id=583635067170) |
| 16 | SBUS 接收机 | 使用物理遥控器时需要，须与遥控器配套 | | 选配 |

板载电子元件（电阻、电容、MOSFET、二极管、连接器等）按[嘉立创工程](https://oshwhub.com/fanchewang/open32drone)中对应版本的电子 BOM 采购。

<a id="chapter-03-section-6"></a>

#### 器件配图

点击图片可查看大图，型号与数量见上方采购清单。

[![主控板](docs/public/media/purchasing/xiao-sense.webp)](docs/public/media/purchasing/xiao-sense.webp)

**主控板**

XIAO ESP32-S3 Sense · 1 个

[![IMU 模块](docs/public/media/purchasing/imu.webp)](docs/public/media/purchasing/imu.webp)

**IMU 模块**

MPU9250 · 1 个

[![排母](docs/public/media/purchasing/headers.webp)](docs/public/media/purchasing/headers.webp)

**排母**

1×7P · 2 个

[![跳线帽](docs/public/media/purchasing/jumper.webp)](docs/public/media/purchasing/jumper.webp)

**跳线帽**

2.54 mm · 1 个

[![升压板](docs/public/media/purchasing/power-module.webp)](docs/public/media/purchasing/power-module.webp)

**升压板**

5 V / 1 A · 1 个

[![桨叶](docs/public/media/purchasing/propellers.webp)](docs/public/media/purchasing/propellers.webp)

**桨叶**

60 mm · 4 个

[![固定螺丝](docs/public/media/purchasing/screws.webp)](docs/public/media/purchasing/screws.webp)

**固定螺丝**

1×4×4 mm · 10 个

[![光流/ToF 模块](docs/public/media/purchasing/flow-tof.webp)](docs/public/media/purchasing/flow-tof.webp)

**光流/ToF 模块**

CORVON 纵川光流测距 · 1 个

[![电机橡胶圈](docs/public/media/purchasing/grommets.webp)](docs/public/media/purchasing/grommets.webp)

**电机橡胶圈**

Ø8×2 mm · 4 个，建议两黑两白

[![电池](docs/public/media/purchasing/battery.webp)](docs/public/media/purchasing/battery.webp)

**电池**

18350 · 1 个，配 JST 引出线

[![光流线束](docs/public/media/purchasing/flow-cable.webp)](docs/public/media/purchasing/flow-cable.webp)

**光流线束**

4P 双头反向，60 mm · 1 根

[![电池固定皮筋](docs/public/media/purchasing/battery-band.webp)](docs/public/media/purchasing/battery-band.webp)

**电池固定皮筋**

直径 25 mm × 宽 5 mm · 1 个

<a id="chapter-03-section-7"></a>

#### 装配规格

- **电机橡胶圈：** 规格 Ø8×2 mm，开孔 10 mm、卡槽高 2 mm、总厚 6 mm、外径 15 mm，共 4 个。建议两黑两白，四个使用相同材料和硬度。
- **螺丝与桨叶：** 固定螺丝 1×4×4 mm，共 10 个；60 mm 桨叶共 4 个，CW、CCW 各 2 个。
- **光流线束：** 购买 4P、60 mm 配套线束。接线时按模块和底板的 GND、电源、TX、RX 定义对应连接，具体引脚见[固定主控板](#chapter-03-section-18)。
- **电池接口：** 选择与底板匹配的 JST 插头和引出线；首次插接前用万用表确认正负极。

<a id="chapter-03-section-8"></a>

#### 准备工具

- **焊接：** 恒温烙铁、焊锡、助焊剂、细头镊子、吸锡带；使用焊膏时另备可控温热台。
- **检查：** 万用表、放大镜。
- **装配：** 合适的螺丝刀、电子秤、非导电工作垫。

焊接温度按所用焊锡或焊膏的说明设置。

<a id="chapter-03-section-9"></a>

### 3.3 电路焊接

<a id="chapter-03-section-10"></a>

#### 1. 器件分组

把阻容、二极管、MOSFET、连接器、排针和模块分别放在小格中。每次只拿出一组器件，在贴装图上完成一组就勾掉一组。有极性的器件先找 Pin 1、阴极或连接器开口方向。

![PCB、连接器与模块展开](img/parts-layout.jpg)

图 3-2　焊接前的 PCB、连接器、供电小板和 IMU。

<a id="chapter-03-section-11"></a>

#### 2. 贴片器件

清洁焊盘，均匀施加焊膏或预上锡。按照“低矮、小封装在前，连接器和模块在后”的顺序贴装：

1. 电阻、电容和小信号器件；
2. MOSFET、二极管和其他有方向器件；
3. 电机接口、电源开关等连接器；
4. 排针、排母、供电小板和 IMU。

器件放下后先从正上方看是否居中，再从侧面看两端是否都落在焊盘上。偏移的器件在加热前调整；已经形成锡桥时，用助焊剂和吸锡带处理，不要反复用烙铁推挤相邻器件。

![贴片器件放置过程](img/smd-placement.jpg)

图 3-3　贴片器件完成定位后的状态。板上的机头箭头始终作为方向基准。

<a id="chapter-03-section-12"></a>

#### 3. 完成焊接

使用热台时，让 PCB 平整贴在工作面上，按焊料规定的预热、回流和冷却过程操作。观察焊料熔化后器件是否回正；焊完自然冷却，再移动电路板。使用烙铁时，先固定一个引脚，复查方向和位置，然后完成其余焊点。

![连接器与贴片器件的焊接状态](img/connectors-soldered.jpg)

图 3-4　连接器装好后的主板。连接器开口朝向要与外部线束的出线方向一致。

<a id="chapter-03-section-13"></a>

#### 4. 焊点检查

用放大镜沿着电源入口、四路电机驱动、排针、连接器逐区检查。合格焊点应完整润湿焊盘和引脚，没有相邻短路、虚焊、翘脚或多余锡珠。

![焊接后的主板正面](img/pcb-soldered.jpg)

图 3-5　焊后正面。检查重点是四路电机输出与中央器件区。

断电后用万用表检查电池正负极是否短路，并核对电源开关前后的连接。第一次供电使用限流电源或带保护的 1S 电池；发现异常发热、气味或电流快速上升时立即断电。

<a id="chapter-03-section-14"></a>

#### 5. 插件与模块

先装背面的供电小板，确认输入、输出和 GND 与主板丝印一致。再焊接 XIAO 使用的排母，让两排保持平行，XIAO 能够自然插入。

![背面供电小板](img/power-board.jpg)

图 3-6　背面供电小板与主板的安装关系。

![排母与板间连接](img/headers.jpg)

图 3-7　排母焊接完成后，从侧面检查高度和垂直度。

IMU 是独立模块，但属于主控板组件。将模块按板上的轴向标识安装，焊接后保持刚性，不能让厚软泡棉使它晃动。标准固件的 IMU 安装旋转为 `roll=π`、`pitch=0`、`yaw=π/2`；使用配套 PCB 与图示方向即可对应这一设置。

![IMU 模块的丝印与针脚](img/imu-module.jpg)

![IMU 安装到主控板](img/imu-installed.jpg)

图 3-8　IMU 模块及安装完成的主控板。

到这里，主控板应包含电机驱动、电源部分、XIAO 排母和 IMU。光流/ToF 通过线束连接，在下一步随机架安装。

<a id="chapter-03-section-15"></a>

### 3.4 整机装配

<a id="chapter-03-section-16"></a>

#### 方向与电机编号

把机头朝前，从机顶向下看：

```text
                         机头 / +X
                             ↑
              M3 前左                     M2 前右

          +Y（左）←        机体中心         → -Y（右）

              M0 后左                     M1 后右
                             ↓
                         机尾 / -X
```

固件与模型使用同一套编号：

| 位置 | 编号 | GPIO | 拆桨测试命令 | 仿真 link |
| --- | --- | ---: | --- | --- |
| 后左 | M0 | 4 | `mrl` | `rotor_0_link` |
| 后右 | M1 | 3 | `mrr` | `rotor_1_link` |
| 前右 | M2 | 6 | `mfr` | `rotor_2_link` |
| 前左 | M3 | 5 | `mfl` | `rotor_3_link` |

<a id="chapter-03-section-17"></a>

#### 光流与 ToF

把机架翻到底面朝上，将光流/ToF 模块放入前部安装位。镜头和测距窗口朝地面，窗口不能被螺丝、胶带或线束遮挡。模块平面应与四个电机的推力平面平行；标准位置位于机体偏航中心前方约 24 mm，固件会补偿这段偏置。

![光流与 ToF 一体模块的安装位置](img/flow-tof-install.jpg)

图 3-9　光流/ToF 一体模块固定在机架前部，线束穿入中央区域。

<a id="chapter-03-section-18"></a>

#### 固定主控板

把机架恢复到正常姿态。整理光流/ToF 线束后放上主控板，使机头箭头与机架机头一致。四个安装孔先全部带上螺丝，再按对角顺序轻轻拧到贴合。主控板应保持平整，下面没有被压住的导线。

![主控板固定到机架](img/mainboard-install.jpg)

图 3-10　主控板、IMU 和光流/ToF 的相对位置。

光流/ToF 使用 UART：模块 TX 接飞控 RX（GPIO8），模块 RX 接飞控 TX（GPIO7），波特率 115200。IMU 使用 I²C：SDA 为 GPIO2，SCL 为 GPIO43。使用配套线束时按 PCB 丝印插接，插拔时握住插头本体。

<a id="chapter-03-section-19"></a>

#### XIAO 与接收机

检查排针无弯折后，把 XIAO ESP32-S3 垂直插入两排排母。USB-C 口应留在机架外侧可接近的位置。使用 SBUS 时，将接收机固定到预留区域并连接 RX/TX 与供电；只使用手机或 ROS 时可以不装接收机。

![XIAO 安装到主控板](img/xiao-install.jpg)

图 3-11　XIAO 插入主控板排母。

<a id="chapter-03-section-20"></a>

#### 橡胶圈与电机

把四个 Ø8 mm 电机橡胶圈压入机架卡槽，沿一圈检查边缘完全就位。再把 8520 电机从正确方向压入橡胶圈，四个电机保持同一高度，轴线彼此平行。操作时握住电机外壳，不推压 1 mm 转轴，也不拉扯电机线。

![橡胶圈装入机架](img/motor-grommets.jpg)

![8520 电机与橡胶圈的侧面关系](img/motor-install.jpg)

图 3-12　橡胶圈与电机。橡胶圈既固定电机，也隔离部分振动。

把电机线沿机臂引到对应接口，依照 M0—M3 逐条连接。保留轻微活动余量，并把所有线束移出桨盘。此时仍然不要安装桨叶。

![四路电机线束接入主控板](img/motor-wiring.jpg)

图 3-13　电机线束接好后的状态。

<a id="chapter-03-section-21"></a>

#### 电池固定

参考样机使用 18350 1300 mAh 电池，实测 25 g。把电池固定在机体中央，使左右和前后重心都接近几何中心；电源线不会碰到桨叶，也不会压住光流/ToF 窗口。含电池、桨叶和实际附件称量，参考值约为 81 g。

![圆柱电池的中央安装方式](img/battery-install.jpg)

图 3-14　圆柱电池安装在中央区域。每次换电后保持相同位置。

如果增加相机、支架或更换软包电池，重新移动电池来恢复水平重心。相机的镜头朝向与排线弯曲半径按相机模块要求处理。

<a id="chapter-03-section-22"></a>

### 3.5 电机检查与装桨

上电前先检查电压采样接线：`VBAT_SW → 100 kΩ → GPIO1/A0 → 100 kΩ → GND`。这两个电阻把电池电压分成一半，例如电池为 3.70 V 时，ADC 引脚应约为 1.85 V。不要把电池或 5 V 直接接到 ESP32-S3 的 GPIO。

如果使用没有分压电路的旧板，把 `PWR_VOLT_PIN` 设为 `-1`。用万用表确认电源没有短路、供电电压和地线连接正确，再装入主控模块。

刷好固件后，在串口中依次运行：

```text
mrl
mrr
mfr
mfl
```

每条命令只让对应电机以低输出转动 1 秒。用一小条纸带或手机慢动作观察，从机顶向下记录每个电机是 CW 还是 CCW。M0 与 M2 应为同一方向，M1 与 M3 为相反方向；在四个橡胶圈旁贴上 `M0 CW`、`M1 CCW` 这样的可移除标签。

桨叶上的 CW/CCW 表示它设计的旋转方向。把 CW 桨装到实测 CW 的电机，把 CCW 桨装到实测 CCW 的电机。四只桨必须同一直径，桨毂压到位但不摩擦电机外壳。

![桨叶安装位置参考](img/prop-install.jpg)

图 3-15　桨叶与四个电机的安装关系。最终方向以拆桨实测标签为准。

安装前最后看一遍：主板方向正确，IMU 和光流/ToF 不松动，四个电机轴平行，电池居中，全部线束离开桨盘。下一章将先在无桨状态刷写和校准，完成后再回到这里安装桨叶。

![完成组装的 Open32Drone 参考样机](img/drone-complete.jpg)

图 3-16　完成组装的参考样机。相机为可选模块；普通定点飞行使用 IMU 与向下安装的光流/ToF。

---

<a id="chapter-04"></a>

## 04 · 固件、校准与起飞

硬件装好后，先保持四个电机都没有桨叶。本章会把完整固件写入 XIAO ESP32-S3，完成传感器与电压校准，再根据手上的设备选择 SBUS 遥控器或 Android 手机完成第一次定点起飞。

<a id="chapter-04-section-1"></a>

### 4.1 认识发布包

`software/releases/minimal/` 中最常用的三个文件是：

| 文件 | 用途 |
| --- | --- |
| `Open32Drone-minimal-merged.bin` | 第一次 USB 刷写使用，包含引导程序、分区表和应用 |
| `Open32Drone-minimal-app.bin` | 飞机已经安装完整分区后，用于 A/B OTA 更新 |
| `Open32Drone-Controller-0.1.apk` | Android 手机控制端 |

新 XIAO、整片擦除后的 XIAO，以及第一次安装这套分区时，都从地址 `0x0` 写入 merged 镜像。app 镜像只包含应用程序，要在已有完整分区的飞机上通过 OTA 更新使用。

先校验下载文件：

```bash
cd /path/to/open32drone/releases/minimal
shasum -a 256 -c SHA256SUMS       # macOS
# sha256sum -c SHA256SUMS         # Linux
```

<a id="chapter-04-section-2"></a>

### 4.2 USB 完整刷写

安装 Python 3 与 esptool：

```bash
python3 -m pip install --user esptool
```

用 USB 数据线连接 XIAO。在 macOS 上可用下面的命令找到串口：

```bash
ls /dev/cu.usb*
```

Windows 使用设备管理器显示的 `COMx`，Linux 通常是 `/dev/ttyACM0`。如果串口没有出现，让 XIAO 进入 Bootloader：按住 `BOOT`，按一下 `RESET`，然后松开 `BOOT`。

把示例端口替换为自己的端口：

```bash
python3 -m esptool --chip esp32s3 \
  --port /dev/cu.usbmodemXXXX erase-flash

python3 -m esptool --chip esp32s3 \
  --port /dev/cu.usbmodemXXXX --baud 921600 \
  write-flash 0x0 Open32Drone-minimal-merged.bin
```

出现传输错误时把波特率改为 `460800`，仍不稳定再改为 `115200`。写入完成后按一下 RESET，打开 115200 波特率串口：

```bash
screen /dev/cu.usbmodemXXXX 115200
```

开机时把飞机水平放在硬桌面上，不要触碰。串口会依次显示电机通道、Wi-Fi、IMU、光流/ToF 和陀螺仪初始化，最后出现：

```text
Gyro calibration complete
Initializing complete
```

<a id="chapter-04-section-3"></a>

### 4.3 检查传感器

第一次连接可以直接使用飞机热点：名称为 `open32drone`，默认密码为 `12345678`，飞机地址为 `192.168.4.1`。需要让电脑同时上网，或接入实验室路由器时，再按 4.6 节配置 STA。

在串口中输入以下命令，每行回车：

```text
sys
imu
flow
pw
```

`sys` 用来确认固件版本和 300 Hz 主循环的运行情况；`imu` 查看传感器型号、采样和陀螺仪校准结果；`flow` 查看光流/ToF 数据和高度；`pw` 查看 ADC 读数与换算后的电池电压。

飞机放在地面时，ToF 可能处于约 20 mm 的近距离盲区。将飞机平稳抬到 20–60 cm 后，距离应随高度变化；在有纹理地面上缓慢水平移动，光流数据也应变化。

<a id="chapter-04-section-4"></a>

### 4.4 校准这台飞机

<a id="chapter-04-section-5"></a>

#### 六面加速度计校准

第一次装机、更换 IMU 或完整擦除后，运行 `ca`。按照串口提示依次放置：

1. 水平；
2. 机头向上；
3. 机头向下；
4. 右侧着地；
5. 左侧着地；
6. 倒置。

每次摆好后松手，让飞机在刚性平面上静止采样。出现 `Accelerometer calibration accepted` 后，把飞机恢复水平，等待陀螺再次完成，再执行 `imu`。静止时加速度模长应接近 `9.81 m/s²`。

<a id="chapter-04-section-6"></a>

#### 电池电压校准

用万用表测量电池端电压，记为 `V_DMM`；运行 `pw` 读取飞控显示电压，记为 `V_FW`。先运行 `p PWR_VOLT_SCALE` 读取现值，再计算：

```text
新比例 = 旧比例 × V_DMM ÷ V_FW
```

写入后等待一秒，再用 `pw` 检查：

```text
p PWR_VOLT_SCALE 你的新数值
```

例如旧比例为 2.000，万用表为 4.10 V，飞控为 4.00 V，新比例就是 `2.000 × 4.10 ÷ 4.00 = 2.050`。

<a id="chapter-04-section-7"></a>

#### SBUS 校准（遥控器路线）

安装了接收机时，打开遥控器并运行 `cr`。完成串口给出的八个摇杆和开关动作，然后用 `rc` 查看结果：

| 操作 | 正常读数 |
| --- | --- |
| 横滚、俯仰、偏航回中 | 接近 0 |
| 油门最低 / 最高 | 接近 0 / 1 |
| 三段模式开关 | 接近 0 / 0.5 / 1 |

只用 Android 或 ROS 的飞机不需要执行 `cr`。

<a id="chapter-04-section-8"></a>

#### 校准与参数保存

开机后让飞机静止至少 2 秒，陀螺仪需要收集至少 500 个新样本才能完成校准。需要重新开始时运行 `cg`，它只重做本次陀螺仪校准。

`ca` 要等六个面的数据全部检查通过后才保存。如果其中一步不合格，会保留原来的校准结果，需要重新操作。每台 IMU 都应单独校准，不要复制另一台的数值。

`ca`、`cr` 和电压比例会保存在 NVS 中，正常重启和应用 OTA 后仍然有效。完整擦除会清掉参数和网络设置；`preset` 只重置注册参数，保留 AP/STA 的网络名称和密码。

光流不需要单独执行校准命令。飞机静止、上锁时，固件会估计本次运行的地面零偏。模块仍要装平，镜头保持干净，地面也要有可辨认的纹理。固件按模块前移 24 mm 做旋转补偿，安装位置应与此一致。

没有电压分压电路的旧板设 `PWR_VOLT_PIN=-1`。有采集电路时，按前面的步骤校准 `PWR_VOLT_SCALE` 即可；`PWR_COMP_REF=3.28`、`PWR_COMP_SLP=0.472`、`PWR_COMP_MAX=1.20` 是推力补偿参数，不需要随每次电压校准一起改。GPIO21 的低压闪灯只作提醒，飞机不会因此自动降落。

<a id="chapter-04-section-9"></a>

### 4.5 拆桨完成四路电机测试

飞机保持上锁，依次运行：

```text
mrl   # 后左 M0
mrr   # 后右 M1
mfr   # 前右 M2
mfl   # 前左 M3
```

每条命令只允许一个电机转动约 1 秒。把位置与从机顶观察到的 CW/CCW 写在电机标签上，然后按上一章的方法安装对应桨叶。

<a id="chapter-04-section-10"></a>

### 4.6 接入路由器 Wi-Fi

日常调试推荐使用路由器 STA 模式。飞机、Android 手机和 ROS 2 电脑接入同一个局域网后，电脑可以保持互联网连接，也不需要在飞机热点与实验室网络之间反复切换。第一次配置仍然通过 USB 串口完成；配置过程中保持拆桨和上锁。

准备一个飞机能够连接的 2.4 GHz Wi-Fi。SSID 长度为 1–32 个字符，密码长度为
8–63 个字符。打开 115200 波特率串口，先查看当前状态：

```text
wifi
```

默认完整镜像会显示 AP 模式和地址 `192.168.4.1`。把下面的示例名称和密码替换为路由器的实际参数：

```text
sta LAB_SSID LAB_PASSWORD
reboot
```

`sta` 会保存路由器凭据并把启动模式设为 STA；运行中的网络不会立即切换，执行 `reboot`
后才生效。飞机重新启动后继续通过 USB 串口执行：

```text
wifi
```

连接成功时应看到以下关键字段：

```text
Configured mode: STA (2)
Mode: Client (STA)
Connected: 1
SSID: LAB_SSID
IP: 192.168.31.42
MAVLink UDP: bound 1 local 14550
```

`IP` 由路由器 DHCP 分配，以飞机实际打印的地址为准。手机或 ROS 2 电脑接入同一路由器后，先测试该地址是否可达：

```bash
ping -c 3 192.168.31.42
```

建议在路由器管理页面按飞机的 Wi-Fi MAC 地址设置 DHCP 地址保留，使飞机每次上电都获得相同地址；同时关闭会阻止局域网设备互访的访客网络或客户端隔离。真实 SSID 和密码只写入飞机，不写入项目源码、教程或飞行日志。

Android 打开 **工具 → 飞机地址**，填入 `wifi` 输出的 `IP`。ROS 2 使用同一个地址：

```bash
ros2 launch open32drone_driver open32drone.launch.py \
  aircraft_ip:=192.168.31.42
```

Android 和 ROS 2 可以同时位于这个局域网，但一次飞行只保留一个 MAVLink 控制端。

如果飞机在启动后的 8 秒内没有连上路由器，会自动开启已保存的飞机热点作为恢复入口。串口中的 `wifi` 会显示 `Mode: Access Point (AP) - STA fallback`。修正路由器名称或密码后重新执行 `sta ...` 和 `reboot`；需要永久恢复直连模式时执行：

```text
ap open32drone 12345678
reboot
```

<a id="chapter-04-section-11"></a>

### 4.7 选择起飞方式

SBUS 和 Android 都能完成首飞。判断方法很简单：

| 手上设备 | 使用路线 | 需要什么 |
| --- | --- | --- |
| 有 SBUS 接收机和已配对遥控器 | 路线 A：遥控器 | 执行 `cr`，熟悉急停摇杆动作 |
| 没有接收机，或想快速体验 | 路线 B：Android APK | 一台 Android 手机，与飞机连接同一网络 |

第一次飞行只打开一个控制端。使用手机时关闭 ROS 与其他 MAVLink 客户端；使用遥控器时先让手机 App 停止控制。

<a id="chapter-04-section-12"></a>

#### 路线 A：SBUS 遥控器

三段开关对应三个模式：

| 开关位置 | 模式 | 操作感觉 |
| --- | --- | --- |
| 低 | STAB 姿态 | 油门直接控制推力，适合熟练飞手 |
| 中 | ALT_HOLD 定高 | 油门回中保持高度 |
| 高 | POS_HOLD 定点 | 光流保持水平位置，首飞推荐 |

把模式放到高档定点。油门最低、偏航最右完成解锁；电机会以约 10% 怠速转动。将油门保持在 62.5% 以上约 0.2 秒，飞机进入辅助起飞并爬升到默认 0.60 m。起飞后让油门回到中位，轻量修正水平位置。

降落时把油门保持在 5% 以下约 0.3 秒，飞机会自动下降并在接地后停桨。需要取消下降时将油门推到 60% 以上。

急停动作是油门最低、偏航最左保持至少 150 ms。急停会立即停桨，飞机在空中会直接下落，因此只在即将碰人、缠绕或姿态失控时使用。

<a id="chapter-04-section-13"></a>

#### 路线 B：Android APK

把 `Open32Drone-Controller-0.1.apk` 复制到手机并安装。Android 可能要求为文件管理器临时允许“安装未知应用”。

推荐使用上一节配置好的路由器 STA。手机连接同一路由器，在 App 的
**工具 → 飞机地址** 中填写飞机串口 `wifi` 命令显示的 DHCP 地址，然后等待顶部出现实时飞控状态。

首次配置或路由器不可用时，可以改用飞机直连热点。完整擦除后的默认网络是：

```text
Wi-Fi: open32drone
密码: 12345678
飞机地址: 192.168.4.1
MAVLink UDP: 14550
```

手机连接这个热点，系统提示“无互联网”时选择继续连接，并在
**工具 → 飞机地址** 中恢复 `192.168.4.1`。输入相对高度 `0.65`，长按“一键起飞”约
0.60 秒；固件会完成解锁、爬升并进入定点。

左摇杆控制升降与偏航，右摇杆控制前后与左右。第一次只做 5–10 秒小范围悬停，然后长按“降落”。如果飞机向人、墙或家具快速移动，优先按“降落”；已经无法安全降落时长按“紧急上锁”。

Android 不依赖实体遥控器。按钮变灰时先看顶部是否仍有 MAVLink 心跳，再确认手机和飞机仍在同一个网络、飞机地址与 `wifi` 输出一致。直连 AP 时还要确认手机没有切回蜂窝网络或其他 Wi-Fi。

<a id="chapter-04-section-14"></a>

### 4.8 第一次飞行

定高和定点模式下，油门中位是 50%。默认在 40–60% 之间保持高度，超出这个范围后控制升降速度，所以摇杆位置不再直接对应电机输出。自动起降过程中仍可以用横滚、俯仰和偏航输入修正方向；切换模式开关则会取消自动动作，回到所选模式。物理 SBUS 的有效操作优先于网络控制。

飞机放在地面时，ToF 可能只报告盲区，没有具体高度。只要盲区数据包仍在及时更新，固件就可以据此判断地面起飞条件，不需要拿着飞机在半空中解锁。

选择有纹理、光照均匀的地面，在飞机四周留出至少 2 m。将电池放在装机时确定的中央位置，镜头朝下且洁净。上电后等到陀螺校准完成，再走一遍：

1. 低高度起飞到 0.60–0.65 m；
2. 双手或摇杆回中，观察 5 秒；
3. 小幅向前、向后、向左、向右移动；
4. 回到原区域；
5. 自动降落，确认接地停桨。

起飞后马上向一侧翻通常是电机位置、转向、桨叶或 IMU 方向问题，应立即停桨并回到拆桨检查。能够平稳离地但有小幅抖动、漂移或高度变化，则进入下一章按现象调参。

---

<a id="chapter-05"></a>

## 05 · 飞行调参

先观察飞机怎样抖、往哪里漂，再决定检查什么。桨叶、电机、传感器和供电都正常后，才开始改控制参数。每次只改一个值，做一次相同的短飞行并记下结果，方便判断这次修改有没有帮助。

<a id="chapter-05-section-1"></a>

### 5.1 先判断是不是参数问题

下面这些现象应先回到硬件：

| 现象 | 先检查 |
| --- | --- |
| 起飞瞬间向一侧翻倒 | M0–M3 位置、转向、CW/CCW 桨、IMU 朝向 |
| 某一侧始终无力 | 桨叶损伤、电机弯轴、接头、电机温度与电池压降 |
| 高频细碎振动 | 桨叶变形、电机轴、橡胶圈、电机高度、IMU 固定 |
| 定点只在某种地面失效 | 地面纹理、反光、光照、光流窗口 |
| 高度值跳变 | ToF 窗口、模块倾斜、近距离盲区和线束 |
| 换电以后重心变化 | 电池位置、附件位置和实际起飞重量 |

机械状态稳定后，用同一块电池、同一处地面和同一高度做对比。首选的测试动作是“0.65 m 起飞 → 回中悬停 5 秒 → 降落”。

<a id="chapter-05-section-2"></a>

### 5.2 认识四层控制

Open32Drone 的控制器由内到外依次工作：

```text
角速度环 → 姿态角环 → 高度/速度环 → 水平位置环
```

内环必须先稳定，外环才能调好。P 决定纠正有多积极；I 用来消除持续偏差；D 抑制变化过快造成的过冲。调参时通常先看 P，再看 I，最后只在确有需要时调整 D。

查看所有参数：

```text
p
```

查看或写入单个参数：

```text
p CTL_R_P
p CTL_R_P 4.02
```

写入会保存到 NVS。改之前记录旧值，改完等待一秒再读取确认。

<a id="chapter-05-section-3"></a>

### 5.3 姿态抖动与回正

标准姿态参数如下：

| 功能 | Roll | Pitch | 默认值 |
| --- | --- | --- | ---: |
| 角度 P | `CTL_R_P` | `CTL_P_P` | 4.47 |
| 角速度 P | `CTL_R_RATE_P` | `CTL_P_RATE_P` | 0.05 |
| 角速度 I | `CTL_R_RATE_I` | `CTL_P_RATE_I` | 0.20 |
| 角速度 D | `CTL_R_RATE_D` | `CTL_P_RATE_D` | 0.001 |

<a id="chapter-05-section-4"></a>

#### 高频抖动

如果飞机能起飞，但机身快速、连续地抖动，先修复桨和电机振动。机械正常后，将对应轴的角速度 P 降低 5–10%。例如 Roll 从 `0.050` 改为 `0.045`：

```text
p CTL_R_RATE_P 0.045
```

重新做同样的 5 秒悬停。抖动减轻且控制仍有力度，再在 Pitch 轴按同样幅度处理；不要一次同时改 P、I、D。

<a id="chapter-05-section-5"></a>

#### 慢速来回摆动或回正过猛

低频、大幅度摆动更可能来自外层角度 P。把 `CTL_R_P` 或 `CTL_P_P` 降低约 10%，例如 `4.47 → 4.02`。如果飞机显得反应迟钝、松杆后很久才回平，可向原值方向小幅增加。

<a id="chapter-05-section-6"></a>

#### 持续偏向一侧

固定方向的倾斜通常由重心、弱电机、机架变形或加速度计偏置造成。先移动电池让重心回到中央，再重新运行 `ca`。只有机械与校准一致、偏差仍可重复时，才分析 I 项。

<a id="chapter-05-section-7"></a>

### 5.4 高度问题

高度控制的主要参数是：

| 参数 | 默认值 | 作用 |
| --- | ---: | --- |
| `ALT_P` | 0.747 | 高度误差的主要纠正力度 |
| `ALT_I` | 0.10 | 消除长期高度偏差 |
| `ALT_D` | 0.20 | 根据垂直速度抑制过冲 |
| `ALT_HOVER` | 0.49 | 标称悬停推力前馈 |
| `ALT_VEL_MAX` | 0.45 | 最大升降速度 |

飞机围绕目标高度缓慢上下摆动时，先确认 ToF 数据连续，再把 `ALT_P` 降低约 10%，例如：

```text
p ALT_P 0.67
```

起飞后稳定，但高度长期慢慢偏低或偏高，可以小幅检查 `ALT_I`。接近目标时明显冲过头再反向，则重点观察 ToF 速度和 `ALT_D`。

`ALT_HOVER` 表示标准电压附近维持高度所需的集体推力。81 g、60 mm 桨的参考值是 0.49。飞机在传感器正常、姿态平稳的情况下长期靠较大高度修正支撑，可从飞行日志估计悬停电机均值，再以很小幅度调整。不要用提高 `ALT_HOVER` 掩盖电池老化或弱电机。

<a id="chapter-05-section-8"></a>

### 5.5 水平漂移与定点

定点控制依赖光流，默认参数为：

| 参数 | 默认值 | 作用 |
| --- | ---: | --- |
| `POS_HOLD_P` | 0.85 | 位置误差转换为目标速度 |
| `POS_VEL_P_X/Y` | 0.35 | 水平速度 P |
| `POS_VEL_I_X/Y` | 0.04 | 水平速度 I |
| `POS_STICK_V` | 0.70 | 摇杆最大水平速度 |

先在纹理清晰的地面上运行 `flow`，确认数据持续更新。原地转向时若位置出现圆周漂移，检查模块是否位于标准的前移 24 mm 位置、安装是否水平。持续向固定方向漂移时，再检查光流零偏、电池重心和 IMU 校准。

飞机慢慢离开目标而不积极回来，可以小幅增加 `POS_HOLD_P`；围绕目标左右来回摆，则小幅降低。一次改 5–10%，每次使用相同的定点高度和飞行时间。

<a id="chapter-05-section-9"></a>

### 5.6 电池与动力变化

参考电池满电约 4.2 V，随着电量消耗，电机能提供的推力也会下降。主板通过 `GPIO1/A0` 读取 100 kΩ / 100 kΩ 分压后的电压，并在定高和定点时补偿一部分推力。补偿有上限，不能一直抵消电池衰减。

先用 `pw` 和万用表把 `PWR_VOLT_SCALE` 校准准确。新电池和低电量电池各做一次同样的 5 秒悬停，比较日志里的 `voltage`、`hoverFF`、`voltComp` 和四路电机输出。如果电压下降时四路同时接近饱和，优先检查电池内阻、桨叶和电机，而不是继续提高 PID。

<a id="chapter-05-section-10"></a>

### 5.7 用日志比较两次飞行

飞机上锁后运行：

```text
log dump
```

保存 CSV 后，可用仓库中的分析脚本快速检查：

```bash
python3 software/simulation/course/analyze_log.py \
  --csv /path/to/flight.csv \
  --output output/my-flight-analysis
```

至少比较以下曲线或字段：

- 姿态目标与实际 Roll/Pitch；
- ToF 高度与高度目标；
- 光流速度与位置误差；
- 四路电机输出及其是否饱和；
- 电池电压和补偿量；
- 问题发生前后的时间点。

一次有效调参记录只需要四项：原参数、修改值、相同飞行动作、观察结果。变好就保留并继续小幅调整；变差就恢复旧值。这样很快能形成适合这台飞机的参数表。

当飞机能重复完成定点起飞、5–10 秒悬停、小范围平移和自动降落，就可以把控制权交给 ROS。

<a id="chapter-05-section-11"></a>

### 5.8 按错误提示排查

<a id="chapter-05-section-12"></a>

### 启动与预检失败

<a id="chapter-05-section-13"></a>

#### 没有串口输出，或 LED 只闪一下

1. 确认在 `0x0` 写入的是完整 merged 镜像，而不是只用于 OTA 的 app 镜像；
2. 使用正确的 ESP32-S3 USB 端口和 115200 波特率；
3. 完整擦除后再通过 USB 刷写一次；
4. 查看启动日志中的分区、反复复位或欠压信息。

<a id="chapter-05-section-14"></a>

#### 初始化完成后 GPIO21 一直闪烁

先执行 `pw`，再用万用表核对电池电压。滤波后的电压不高于 `3.10 V` 并持续 `1.5 s` 时，GPIO21 会以 `2 Hz` 闪烁；电压恢复到不低于 `3.20 V` 并持续 `1.0 s` 后，闪烁才会停止。

闪灯只作低电压提醒，不会让飞机自动降落或上锁。如果旧板没有分压电路，应设 `PWR_VOLT_PIN=-1`，关闭悬空 ADC 引脚的采样。

<a id="chapter-05-section-15"></a>

#### `motor PWM unavailable`

这个提示表示四路电机的 LEDC 输出没有全部初始化成功，固件因此不允许解锁。先确认编译时使用了项目指定版本的 Arduino-ESP32 core，再检查相机或其他模块是否占用了同一组 LEDC 资源。电机引脚按左后、右后、右前、左前排列，应为 `4, 3, 6, 5`。

<a id="chapter-05-section-16"></a>

#### `gyro calibration incomplete`

把飞机放在坚硬、水平的桌面上，重新上电，至少两秒不要触碰。仍然不能完成时，运行 `imu` 查看失败原因和标准差，检查附近是否有振动、气流，桌面是否晃动，电机是否损坏。`cg` 可以重新开始陀螺仪校准；加速度计的六面校准仍要用 `ca`。

<a id="chapter-05-section-17"></a>

#### `invalid RC calibration/mapping`

给接收机供电，运行 `cr`，按提示完成八个动作。各个控制量应对应互不重复的 `0..7` 通道，这些通道的校准结果才能保存。只用 Android 或 ROS 控制时，不需要打开接收机。

<a id="chapter-05-section-18"></a>

#### 参数存储错误

`sys` 显示 `Parameter storage: ERROR` 时，说明参数存储出了问题，固件会拒绝解锁。先完整擦除并重新刷写，再执行 `ca`/`cr`。如果仍然报错，继续检查 Flash/NVS 硬件和分区，解决参数保存问题后再飞行。

<a id="chapter-05-section-19"></a>

#### 循环频率低或不稳定

正常飞控循环应接近 300 Hz。如果 `rate` 偏低，先记录各部分的耗时，查清原因后再改代码。

飞机上锁后执行 `perf reset`，保持一种运行状态 10-20 秒，再保存 `time` 和 `perf` 的输出。分别测试飞机单独运行、连接 Android、连接 ROS 和打开 QGC 参数页的情况。重点比较循环错过截止时间的次数、最大迟到量、p95/p99/最大时延，以及各阶段的耗时。

`imu acquire` 表示当前 IMU 后端执行 `read()` 所花的时间，不同传感器驱动还可能在内部完成数据传输。`perf` 没有把等待下一次循环的时间算进去，因此各阶段耗时之和不等于完整的 3.33 ms 周期。

如果 CLI、MAVLink 或后台阶段的最大耗时明显增大，再检查对应代码。25 Hz 飞行日志写在 RAM 环形缓冲中，是否拖慢循环也要看后台维护阶段的实际耗时，不必一开始就关闭日志或删除安全检查。

<a id="chapter-05-section-20"></a>

### TF-0850 与校准

<a id="chapter-05-section-21"></a>

#### 飞机放在地面，Android 提示 ToF 未就绪

TF-0850 在约 `20 mm` 以下无法给出准确距离，所以飞机放在地面时可能显示“ToF 未就绪”。只要模块持续发来盲区数据包，固件仍能判断地面起飞条件；这条提示本身不会额外禁止起飞。如果按钮也无法使用，再检查 MAVLink 是否连接，以及物理 SBUS 是否正在接管。

执行 `flow`，确认：

- `TOF UART healthy: 1`；
- 数据年龄小于 `150 ms`；
- 有数值距离或 `blind-zone: 1`；
- 数据包计数持续增加。

<a id="chapter-05-section-22"></a>

#### 加速度计校准被拒绝

拆桨并保持上锁，运行 `ca`，按提示依次放置六个面。每次放好后松手，让飞机静止完成采样。任何一面的数据无效，或噪声、重力模长、比例、残差没有通过检查，整组结果都不会保存，原来的校准参数会继续生效。

<a id="chapter-05-section-23"></a>

#### 相同套件装出的飞机表现不同

使用标准机架时，先从同一套默认控制参数开始。两台飞机表现不同，通常应先比较装配和校准，逐项检查：

- 电机/桨叶型号与方向；
- 弯轴、松动机臂或电机高度不一致；
- IMU 刚性固定并与推力平面平行；
- 电池位置与重心；
- 向下模块方向和标准 `24 mm` 前向偏置；
- 校准平面与振动。

当前固件不会通过自动配置迁移或悬停 Trim 学习修改已设置的控制参数。确认机械状态一致，再分别完成 `ca`/`cr`，最后考虑是否需要调参。

<a id="chapter-05-section-24"></a>

### Android 链路与控制

<a id="chapter-05-section-25"></a>

#### `ENETUNREACH (Network is unreachable)`

这个错误通常表示手机当前的 Wi-Fi 无法访问所填的飞机地址，也可能出现在 Android 切换或重新连接网络时。

直连 AP 时，确认手机连接的是飞机热点，地址为 `192.168.4.1`。使用路由器 STA 时，让手机接入同一路由器，再把串口 `wifi` 命令显示的 DHCP 地址填入 **工具 > 飞机地址**。关闭 VPN，授予 App 局域网权限，然后用浏览器试着访问：

```text
http://<飞机地址>:8080/api/ota/status
```

App 会用能够访问飞机地址的 Wi-Fi 发送 MAVLink、图传和 OTA 数据。网络断开后，它会关闭旧连接，等这条 Wi-Fi 路由恢复后重连，不会改走蜂窝网络。

<a id="chapter-05-section-26"></a>

#### 按钮全部是灰色

查看顶部状态：

- 未连接：没有 MAVLink 心跳；
- 物理 SBUS 优先：松开摇杆等待控制权空闲；若只用 Android，也可关闭接收机；
- App 进入后台：回到前台，进入后台会有意停止手动控制流。

Android 控制不需要先打开物理遥控器。

<a id="chapter-05-section-27"></a>

#### 起飞后约几秒自动降落

先检查 App 是否退到了后台、Wi-Fi 是否切换，以及状态文字中有没有 `link loss`。旧版客户端还可能在起飞后发出过时的零油门数据，因此 APK 和固件要使用配套版本。

让 App 保持前台，恢复稳定连接后再试。直接延长失联等待时间并不能解决控制数据中断的问题。

<a id="chapter-05-section-28"></a>

### ROS 2 连接与命令

<a id="chapter-05-section-29"></a>

#### 有话题名字但没有数据

节点可以在飞控未连接前创建话题。检查：

```bash
ping -c 3 <飞机地址>
ros2 run open32drone_driver control status
ros2 topic echo /open32drone/connected --once
```

飞机直连 AP 使用 `192.168.4.1`；STA 模式启动 ROS 时传入同一 DHCP 地址：
`aircraft_ip:=<飞机地址>`。

关闭控制这架飞机的 Android 和其他客户端，再确认 `local_udp_port` 没有被另一个 MAVROS 进程占用。如果只能看到旧的 `/open32drone/UAS1/state` 消息，而且其中仍是 `connected: false`，说明连接还没有建立。

<a id="chapter-05-section-30"></a>

#### `rqt` 报 QoS 不兼容

在 `rqt` 中选择桥接后的话题：`/open32drone/imu/data`、`/open32drone/odom`、`/open32drone/pose` 或 `/open32drone/range/downward`。这些话题使用 Reliable QoS，可以避免直接订阅 MAVROS sensor-data 话题时的兼容问题。仍有告警时，确认使用了完整的 `open32drone.launch.py`，并检查 `interface_bridge` 是否正在运行。

<a id="chapter-05-section-31"></a>

#### `/open32drone/cmd_vel` 没反应

速度命令需要飞机已经连接、解锁，位置和姿态数据及时更新，并进入 Offboard ACTIVE 状态。先用 `control velocity` 测试，再查看：

```bash
ros2 topic echo /open32drone/offboard/status
ros2 topic echo /open32drone/flight/status
```

自己向 `/open32drone/cmd_vel` 发布消息时，需要连续发送。物理 SBUS 操作会优先接管飞机，也要一并检查。Offboard 没有激活时，先解决连接和模式问题，调整固件增益不会让命令生效。

<a id="chapter-05-section-32"></a>

### OTA 失败

先确认飞机已落地、上锁，电机已经停止，自动飞行和 Offboard 都已退出。当前镜像还需要通过启动验证，才能接受 OTA。上传时选择 app 镜像，USB 刷写用的 merged 镜像不能用于 OTA。可以查看下面的地址了解当前状态：

```text
http://<飞机地址>:8080/api/ota/status
```

OTA 传输失败时，飞机应继续使用当前分区；新镜像启动验证失败时，应自动回滚。若不能正常恢复，仍需要用 USB 重新刷写，因此主控的 USB 接口要保持可用。

---

<a id="chapter-06"></a>

## 06 · ROS 2 控制

飞机能用遥控器或手机稳定飞行后，就可以试着用 ROS 2 控制。Open32Drone 通过 MAVROS 与飞控通信，把 IMU、距离、电池和里程计数据发布成 ROS 话题，也提供起飞、降落、速度和位置命令。

本章不需要 QGC。开始前先关闭 Android 控制端，让 ROS 单独控制这架飞机。ROS 包版本为 `0.1.0`，应与同一源码版本的固件和 Android 配套使用。如果在同一台电脑上启动多个 MAVROS 进程，每个进程要使用不同的本地 UDP 端口，具体配置见本章多机部分。

下面的命令用于连接真实飞机。如果想先做仿真，可以阅读第七章的 [URDF / USD 模型说明](#chapter-07)；当前 ROS 包还没有接入仿真后端。

第一次使用 ROS，可以按下面的顺序操作：

1. 普通遥控或 Android 首飞已经通过，并关闭 Android 控制端；
2. ROS 电脑直连飞机热点，或与 STA 模式飞机连接同一个路由器；
3. 按第 2 节安装，在一个终端按第 3 节启动；
4. 在另一个终端确认 `/open32drone/connected` 为 `true`；
5. 第 4–5 节先作为参考，直接跳到第 6 节，只执行一次“起飞 → 悬停 → 降落”。

多机配置可以等单机起降成功后再看。测试期间，让 ROS 成为这架飞机唯一的 MAVLink 控制端。

<a id="chapter-06-section-1"></a>

### 1. 环境要求

- 已安装 ROS 2、`colcon` 和 MAVROS；
- 主机直连飞机 AP，或者与已经配置 STA 的飞机连接同一个可信路由器；
- ROS 主机可以访问所选的飞机 IPv4 地址；
- 关闭 Android 和其他 MAVLink 客户端；
- 安装和台架检查期间必须拆桨。

启动 ROS 前先确认网络：

```bash
ping -c 3 192.168.4.1  # 路由器模式替换为飞机的 STA 地址
```

<a id="chapter-06-section-2"></a>

### 2. 安装

使用仓库 `software/ros2/` 目录，或同一构建套件中的匹配 ROS 2 源码包：

```bash
mkdir -p ~/osdrone_ws/src
cp -a /path/to/open32drone/software/ros2 ~/osdrone_ws/src/open32drone_driver
cd ~/osdrone_ws
rosdep install --from-paths src --ignore-src -r -y
colcon build --symlink-install
source install/setup.bash
```

每次打开新终端都要加载工作空间：

```bash
source ~/osdrone_ws/install/setup.bash
```

<a id="chapter-06-section-3"></a>

#### 修改源码后怎样重新运行

编写自己的节点时，可以订阅 `/open32drone/odom` 读取状态，向 `/open32drone/cmd_vel` 发送速度，或调用已有的命令话题和服务。起飞、降落的 MAVLink 处理已经由驱动完成，无需再写一套。

按上面的复制方式安装后，直接修改 `~/osdrone_ws/src/open32drone_driver/` 中的源码，再执行：

```bash
cd ~/osdrone_ws
colcon build --symlink-install --packages-select open32drone_driver
source install/setup.bash
```

新增 Python 节点放在源码的 `open32drone_driver/` 包中，并在 `setup.py` 注册入口；增加启动参数时，同步修改 `launch/`。按第 3 节启动后，先在第二个终端拆桨运行 `ros2 run open32drone_driver bench_test --duration 5`。检查通过，再按第 6 节做一次起飞和降落。

普通 ROS 应用通常只需修改节点。如果改动了共享的 MAVLink 协议，还要同步修改固件、Android 和相关协议测试。详细编译方法见[开发指南](docs/reference/source-build.zh-CN.md)。

<a id="chapter-06-section-4"></a>

### 3. 启动并检查连接

先使用仓库提供的启动文件。连接后除了查看 `connected=true`，还要确认 IMU、odom 和测距数据持续更新，再准备起飞。

如果自行修改启动文件，不要给 `mavros_node` 添加全局 `name="mavros"`，否则内部插件也会被重命名，导致话题路径和配置不匹配。配套启动文件已经把 ToF 输出映射到 `UAS1/distance_sensor/tof`，再由桥接节点发布为 `range/downward`。

执行起飞命令时，程序会先发送一次只读的 `status` 请求，确认飞控能正常回复。这里超时，就先检查连接，不要连续重发起飞。

需要记录 ROS bag 时，飞行过程中通过实时话题查看状态；录制结束并正常关闭后，再读取 SQLite 数据库，避免直接查询仍在写入的文件。

直连飞机热点时，运行：

```bash
ros2 launch open32drone_driver open32drone.launch.py
```

默认 MAVROS 地址是：

```text
udp://0.0.0.0:14550@192.168.4.1:14550
```

使用路由器 STA 时，把固件 `wifi` 命令打印的 DHCP 地址传给启动文件：

```bash
ros2 launch open32drone_driver open32drone.launch.py \
  aircraft_ip:=192.168.31.42
```

需要自定义 MAVROS 连接地址时，也可以传入 `fcu_url:=...`。日常使用建议在路由器中为飞机保留固定 DHCP 地址，省去每次上电重新查地址的麻烦。Android 手机可以连接同一路由器，但用 ROS 控制时应关闭 Android 控制端。

再打开一个终端，检查下面几项数据：

```bash
source ~/osdrone_ws/install/setup.bash
ros2 run open32drone_driver control status
ros2 topic echo /open32drone/connected --once
ros2 topic hz /open32drone/imu/data
ros2 topic echo /open32drone/range/downward --once
```

正常情况下，`/open32drone/connected` 为 `true`，IMU 数据持续更新，向下测距也能收到及时更新的 TF-0850 数据包。如果只有话题名称、没有数据，连接还没有完成，先按本章末尾的排查步骤检查。

<a id="chapter-06-section-5"></a>

### 4. 对外接口

<a id="chapter-06-section-6"></a>

#### 遥测

| 话题 | 类型 | 含义 |
|---|---|---|
| `/open32drone/connected` | `std_msgs/Bool` | 实时心跳连接状态 |
| `/open32drone/state` | `mavros_msgs/State` | 连接、解锁和模式 |
| `/open32drone/imu/data` | `sensor_msgs/Imu` | 姿态和滤波后 IMU |
| `/open32drone/imu/data_raw` | `sensor_msgs/Imu` | MAVROS 原始 IMU 接口 |
| `/open32drone/odom` | `nav_msgs/Odometry` | 本地位置和速度 |
| `/open32drone/pose` | `geometry_msgs/PoseStamped` | 本地位姿 |
| `/open32drone/range/downward` | `sensor_msgs/Range` | 向下 TF-0850 距离 |
| `/open32drone/battery` | `sensor_msgs/BatteryState` | 实测电压；电流和剩余百分比保持未知；辅助推力补偿由固件负责 |
| `/open32drone/rc/in` | `mavros_msgs/RCIn` | 物理 SBUS 通道 |
| `/open32drone/rc/channels` | `std_msgs/UInt16MultiArray` | 简单数组形式的 RC 通道 |
| `/open32drone/diagnostics` | `diagnostic_msgs/DiagnosticArray` | 连接诊断 |
| `/tf` | TF | `open32drone/odom -> open32drone/base_link` |

桥接节点已经把关键传感器话题转成 Reliable QoS。用 RViz 和 `rqt` 查看时，选择这些桥接后的话题即可，不需要再处理 MAVROS sensor-data QoS 的差异。

<a id="chapter-06-section-7"></a>

#### 控制

| 接口 | 含义 |
|---|---|
| `/open32drone/command` | 起飞、降落等单次文本命令 |
| `/open32drone/command/result` | 与原命令匹配的 JSON 结果 |
| `/open32drone/cmd_vel` | 机体系速度：`x` 前、`y` 左、`z` 上 |
| `/open32drone/goal_pose` | `open32drone/odom` 中的绝对位置目标 |
| `/open32drone/rc/override` | SBUS 风格原始通道测试输入 |

同时提供简短服务：

```text
/open32drone/arm  /open32drone/disarm  /open32drone/takeoff
/open32drone/land  /open32drone/emergency_stop
```

`/open32drone/takeoff` 服务使用 `flight_manager.takeoff_height` 参数；需要明确指定高度时，使用
CLI 或文本话题。

<a id="chapter-06-section-8"></a>

### 5. 高级：同一局域网控制多架飞机

同时连接多架飞机时，要让它们通过 STA 模式接入路由器。直连热点模式下，每架飞机的默认地址都是 `192.168.4.1`，不能用这个地址在同一局域网中区分它们。

除了 IP 地址，还要为每架飞机分别设置系统 ID、ROS 名称和端口。下面是两架飞机的配置示例，用来区分各自的话题、服务、MAVROS 接口和 TF：

| 配置 | 飞机 1 | 飞机 2 | 作用 |
|---|---:|---:|---|
| 飞机 STA 地址 | `192.168.31.101` | `192.168.31.102` | 找到具体物理飞机 |
| 固件 `MAV_SYS_ID` | `1` | `2` | 区分 MAVLink 系统 |
| `robot_name` / TF 前缀 | `drone01` | `drone02` | 隔离 ROS 名称和坐标系 |
| ROS 主机 UDP 本地端口 | `14551` | `14552` | 两套 MAVROS 在同一主机运行时避免本机套接字冲突 |

先通过每架飞机的本地串口 CLI 设置一次系统 ID，重启后用 `p MAV_SYS_ID` 核对：

```text
p MAV_SYS_ID 1
```

第二架飞机使用不同的系统 ID。同时在路由器中为每架飞机保留固定 DHCP 地址，避免重新上电后地址变化，控制程序连错飞机。

中央 ROS 程序需要同时发现两架飞机时，两套进程使用相同的 `ROS_DOMAIN_ID`。下面示例让两套 MAVROS 运行在同一主机，所以本地 UDP 端口必须不同：

```bash
export ROS_DOMAIN_ID=32

# 终端 1
ros2 launch open32drone_driver open32drone.launch.py \
  robot_name:=drone01 frame_prefix:=drone01 \
  aircraft_ip:=192.168.31.101 mav_sys_id:=1 local_udp_port:=14551

# 终端 2
ros2 launch open32drone_driver open32drone.launch.py \
  robot_name:=drone02 frame_prefix:=drone02 \
  aircraft_ip:=192.168.31.102 mav_sys_id:=2 local_udp_port:=14552
```

所有需要看到该集群的终端和中央控制进程都必须导出同一个 Domain ID。

此时同一 Domain 中的每台 ROS 电脑都会在话题列表中看到两套名称，这是正常现象：

```text
/drone01/state       /drone02/state
/drone01/cmd_vel     /drone02/cmd_vel
/drone01/odom        /drone02/odom
```

在 `ros2 topic list` 中看到两套话题，表示 DDS 已经发现两组节点，控制程序可以分别访问它们。具体控制哪一架，由命名空间决定：发到 `/drone01/cmd_vel` 的命令不会进入 `/drone02/cmd_vel`。运行命令行工具时，也要指定飞机名称：

```bash
ros2 run open32drone_driver control --robot-name drone01 status
ros2 run open32drone_driver control --robot-name drone02 takeoff --height 0.65
ros2 run open32drone_driver bench_test --robot-name drone01 --duration 5
```

需要让两个实验互不发现时，再使用不同的 `ROS_DOMAIN_ID`。同一个中央程序要同时控制多架飞机，通常应让它们使用相同的 Domain ID，并用命名空间区分。不同域之间需要额外的 DDS/域桥才能通信。

如果每架飞机各用一台独立伴随计算机，它们可以都使用本机 UDP 端口 `14550`，因为不同主机上的套接字不会冲突；飞机 IP、固件 `MAV_SYS_ID`、`robot_name` 和 TF 前缀仍必须对应正确的飞机。以后增加 ROS 图像节点时也必须放在该飞机命名空间下，例如
`/drone01/camera/image_raw`。

后台进程管理也按飞机隔离：

```bash
ros2 run open32drone_driver system start \
  --robot-name drone01 --aircraft-ip 192.168.31.101 \
  --mav-sys-id 1 --local-udp-port 14551
ros2 run open32drone_driver system status --robot-name drone01
ros2 run open32drone_driver system stop --robot-name drone01
```

工具在显示进程状态或停止进程前，会核对记录的 PID 是否仍对应这架飞机的命名空间。电脑重启后，如果旧 PID 已被其他进程使用，这条旧记录会被忽略，避免误停别的程序。

<a id="chapter-06-section-9"></a>

### 6. 正常飞行流程

ROS 自动起飞后默认进入定点模式。直接发送 `takeoff` 即可，固件会依次完成预检、解锁、爬升和定点保持，不需要提前单独发送 `arm`，也不需要先改变待机时显示的模式。

<a id="chapter-06-section-10"></a>

#### 先做一次起飞和降落

飞机放在空旷安全区并有人监护：

```bash
ros2 run open32drone_driver control status
ros2 run open32drone_driver control takeoff --height 0.65
ros2 topic echo /open32drone/odom
ros2 run open32drone_driver control land
```

收到起飞成功结果后再发送移动命令。降落后确认 `armed: false` 和落地状态。

<a id="chapter-06-section-11"></a>

#### 速度控制

ROS 显示命令成功，表示已经收到了飞控的实际应答。速度控制还要等飞控确认进入 AUTO 模式后才会开始。

用 `control velocity` 可以指定前后、左右、升降速度和持续时间。工具会先进入 Offboard，按指定时间持续发送速度，结束后让飞机在当前位置保持：

```bash
# 前、后、左、右；每次 0.25 m/s，持续 1.5 s。
ros2 run open32drone_driver control velocity  0.25  0.00 0.00 --duration 1.5
ros2 run open32drone_driver control velocity -0.25  0.00 0.00 --duration 1.5
ros2 run open32drone_driver control velocity  0.00  0.25 0.00 --duration 1.5
ros2 run open32drone_driver control velocity  0.00 -0.25 0.00 --duration 1.5

# 上升、下降、原地旋转。
ros2 run open32drone_driver control velocity 0 0  0.20 --duration 1.0
ros2 run open32drone_driver control velocity 0 0 -0.20 --duration 1.0
ros2 run open32drone_driver control velocity 0 0 0 --yaw-rate 0.50 --duration 2.0
```

ROS 节点把水平合速度限制为 `0.70 m/s`，与固件默认的 `POS_STICK_V` 一致，固件收到后还会再检查一次限幅。垂直速度限值为 `0.35 m/s`，偏航角速度限值为 `1.0 rad/s`。超过 `0.50 s` 没有收到新命令时，节点会记录当前位置并转为位置保持。

直接发布 `/open32drone/cmd_vel` 时必须连续发送，通常使用 20 Hz：

```bash
ros2 topic pub -r 20 /open32drone/cmd_vel geometry_msgs/msg/Twist \
  '{linear: {x: 0.20, y: 0.0, z: 0.0}, angular: {z: 0.0}}'
```

按 `Ctrl-C` 停止发布。速度消息需要持续发送，只发一帧会很快触发命令超时。

<a id="chapter-06-section-12"></a>

#### 位置控制

```bash
ros2 run open32drone_driver control position 0.30 0.00 0.65
```

发送前先查看 `/open32drone/odom`，确认当前位置。命令中的坐标是 `open32drone/odom` 坐标系里的绝对位置；例如 `x=0.30` 表示到达这个坐标，不是从当前位置再向前走 0.30 m。

新目标与当前位置的水平距离限制在 `0.80 m` 内，接近速度不超过 `0.15 m/s`。

<a id="chapter-06-section-13"></a>

### 7. 文本命令与服务

文本话题适合写教学脚本：

```bash
ros2 topic pub --once /open32drone/command std_msgs/msg/String \
  '{data: "takeoff 0.65"}'
ros2 topic echo /open32drone/command/result
ros2 topic pub --once /open32drone/command std_msgs/msg/String '{data: "land"}'
```

支持的文本命令：

```text
status
arm
disarm
emergency_stop
takeoff [height_m]
land
mode stabilize|altitude|position
offboard start|stop
rc start|stop
```

简短服务示例：

```bash
ros2 service call /open32drone/takeoff std_srvs/srv/Trigger '{}'
ros2 service call /open32drone/land std_srvs/srv/Trigger '{}'
ros2 service call /open32drone/emergency_stop std_srvs/srv/Trigger '{}'
```

起飞、降落这类命令发送一次后，等待对应结果再继续。如果没有收到回复，先检查丢包或飞控返回的拒绝原因，不要连续重复发送。

<a id="chapter-06-section-14"></a>

### 8. 原始 RC 通道测试

需要检查原始遥控通道和协议转换时，可以使用下面的接口。编写常规自主飞行程序，仍建议使用前面的速度或位置命令：

```bash
ros2 run open32drone_driver control rc \
  --roll 1023 --pitch 1023 --throttle 1100 --yaw 1023 --duration 1.0
```

通道数值使用 SBUS 的 `[240, 1807]` 范围。桥接节点会把它们转换成 MAVLink `MANUAL_CONTROL`，因此测试时要持续更新数据。命令结束后，工具会停止发送并回到定点。

物理 SBUS 操作优先；遥控器正在发送有效操作时，ROS RC 不会启动。有人在旁监护时，可以保留遥控器急停作为应急措施，正常起飞和降落仍按 ROS 流程完成。

查看物理遥控通道：

```bash
ros2 topic echo /open32drone/rc/in
ros2 topic echo /open32drone/rc/channels
```

<a id="chapter-06-section-15"></a>

### 9. RViz 与 TF

```bash
ros2 launch open32drone_driver open32drone.launch.py use_rviz:=true
```

默认 RViz 配置会显示里程计、位姿、TF 和向下距离。固定坐标系是 `open32drone/odom`，桥接节点发布 `open32drone/odom -> open32drone/base_link`。多机启动时，换成各自的 `frame_prefix`，例如 `drone01/odom -> drone01/base_link`。

当前 ROS 包还没有把实验性的 HTTP MJPEG 图传转成 ROS 图像话题，也不提供 `camera_info`。需要读取图像时，可以用 OpenCV 打开 `http://<飞机地址>/stream`。固件一次只允许一个客户端观看图传；用 ROS 控制飞机时，仍需关闭 Android 控制端。

<a id="chapter-06-section-16"></a>

### 10. 用脚本检查悬停和位置控制

测试时让飞机下方保持空旷，不要伸脚或移动物体，否则 ToF 距离和光流读数都会改变。如果专门测试传感器遮挡，应另存一份记录，与正常悬停数据分开分析。

拆桨台架：

```bash
ros2 run open32drone_driver bench_test --duration 5
```

只有确实安装并校准物理遥控器时才加 `--require-rc`；只有配置电压采样硬件时才加
`--require-battery`。

有人监护的飞行测试：

```bash
ros2 run open32drone_driver flight_test --height 0.65 --hover 5
```

默认的 `--pattern hover` 会完成起飞、悬停和降落，不做水平移动。脚本会等飞机稳定下来：水平和高度误差在 0.10 m 内，水平与垂直速度均不超过 0.08 m/s，并持续 1 秒。达到这些条件后，才开始计算 `--hover` 指定的悬停时间。

位置到点测试（有人监护，留出移动空间）：

```bash
ros2 run open32drone_driver flight_test --pattern cross --height 0.65 --distance 0.4 --hover 5 --output flight-cross.json
```

这次测试依次完成：稳定起飞 → 悬停 → 向前 0.4 m → 回原点 → 向左 0.4 m → 回原点 → 悬停 → 降落。脚本以初始悬停结束时的机头方向为参考，把目标点固定在里程计坐标系中，目标不会随着飞机漂移而移动。

每到一个点，飞机需要在允许误差内停稳并保持 1 秒，随后再观察 1 秒，才进入下一步。如果 20 秒仍未到位，测试会报告失败并请求降落。

`--height-tolerance` 设置本次测试允许的 XY/Z 误差，默认 0.10 m；它不会修改飞控参数。`--distance` 可设为 0.1–0.7 m，也不会改变日常控制的距离限制。`--output` 用于保存各阶段的时间和原始位置样本。如果文件已存在，脚本会在起飞前报错，请另取文件名，以免覆盖上次记录。

需要检查连续速度控制时，使用定时速度命令；它与位置到点测试不是同一项：

```bash
ros2 run open32drone_driver control velocity 0.15 0.00 0.00 --duration 10
```

先起飞，再执行这条命令。工具以约 20 Hz 发送速度，10 秒后发送零速度。实际移动距离未必正好是 1.5 m，发出零速度后也要继续观察飞机是否停住。

运行中如果 Offboard 状态长时间不更新、退出激活状态或飞机上锁，命令会报告失败。ROS 收到 AUTO 命令的 ACK 后，还要等飞控反馈实际 AUTO 模式，才会显示 ACTIVE。测试脚本不会自动重试起飞，失败后的降落请求也不会循环重发。

这些结果使用机载里程计判断。如果需要测量真实的位置精度，还要用外部定位设备进行对照。

<a id="chapter-06-section-17"></a>

### 11. 无响应时

如果看到 `fresh local position is required`，先检查 ROS 位置话题：消息可能没有收到，也可能已超过 0.5 秒没有更新，不能仅凭这条提示判断 ToF 损坏。Offboard 预热被拒后，程序会在激活期限内重试；仍然失败时先降落，再保存启动日志，以及该飞机命名空间下的 `offboard/status`、`UAS1/local_position/pose` 和 `UAS1/setpoint_raw/local` 数据。查清消息在哪一步中断，再处理连接问题，无需先改 PID 或超时参数。

1. 确认 `ping <飞机地址>` 成功，并核对启动参数 `aircraft_ip`；
2. ROS 需要接管该飞机时关闭 Android，并确认没有其他进程占用本次启动的
   `local_udp_port`；
3. 运行 `control status` 查看实际连接状态，确认数据正在更新；
4. 查看 `/open32drone/command/result` 和 MAVROS `statustext` 中的预检拒绝原因；
5. ROS 需要控制时停止物理 SBUS 操作；
6. 修改固件参数前先阅读[故障排查](#chapter-05)。

---

<a id="chapter-07"></a>

## 07 · 强化学习

上一章用 ROS 发送速度和位置目标，再通过里程计观察飞机的运动。这一章把飞机放进仿真环境，让程序反复尝试同一个任务，学习怎样减小风、动力差异和模型误差造成的偏移。

示例采用残差强化学习：几何 PD 控制器仍负责姿态稳定和四个电机的推力分配，PPO 网络只给出三轴加速度修正。这样可以沿用已有控制器，再比较加入学习策略后，81 g 模型在不同扰动下的表现。

<a id="chapter-07-section-1"></a>

### 7.1 先把真实飞机变成机器人模型

仓库提供了数值模型和 PPO 练习，可以先从 7.4 节的 CPU 示例开始。完整的飞机 URDF/USD 场景、Gazebo 飞行后端和预训练权重需要另外准备。下面先用参考模型和视频说明建模方法；准备好兼容场景后，再运行 Isaac 部分。

本章视频和曲线来自仿真教学示例，用来说明训练与比较方法，不代表下载固件在真机上的飞行性能。

参考 URDF/USD 模型把飞机分成一个刚性机身和四个旋翼关节：

```text
base_link
├── rotor_0_link  — continuous — 后左 M0
├── rotor_1_link  — continuous — 后右 M1
├── rotor_2_link  — continuous — 前右 M2
├── rotor_3_link  — continuous — 前左 M3
├── battery_link — fixed
├── camera_link  — fixed
├── imu_link     — fixed sensor frame
└── flow_tof_link — fixed
    ├── flow_link — fixed optical frame
    └── tof_link  — fixed range frame
```

`base_link` 包括打印机架、主控 PCB、XIAO、橡胶圈、电机外壳，以及供电和固定结构，这些部件相对机身不动。IMU 和光流/ToF 的质量也计入机身，同时各自保留固定坐标系，供 ROS 和仿真传感器使用。

电池单独保留为 `battery_link`，方便修改质量和安装位置；相机也作为固定 link。四副桨叶分别通过 `continuous` 关节与机身相连，可以连续旋转。

参考模型的质量分配为：

| 部分 | 质量 |
| --- | ---: |
| 刚性机身 `base_link` | 54.2547 g |
| 4 副桨叶 | 约 1.4542 g |
| 18350 电池 | 25.0000 g |
| 相机 | 约 0.2911 g |
| 合计 | 81.0000 g |

下面的视频把四段 Isaac Sim 检查合在一起：外观与 81 g 配置、主控 PCB 近景、无动力自由落体、四个旋翼关节运动。


[![播放视频：model-checks](img/model-checks-poster.png)](img/videos/model-checks.mp4)


<a id="chapter-07-section-2"></a>

### 7.2 没有完整电机曲线，怎样先建立模型

8520 电机只给出尺寸和最高 50,000 rpm，不能直接得到带 60 mm 桨时的推力。最高转速换算为角速度是：

```text
50,000 × 2π ÷ 60 = 5,235.99 rad/s
```

这个数可以用作关节速度上限，但还不能据此算出带桨推力。要先得到一个能运行的近似模型，可以从悬停时的受力开始估算：

```text
单电机平均悬停推力
= 总质量 × 重力加速度 ÷ 4
= 0.081 kg × 9.80665 m/s² ÷ 4
≈ 0.1986 N
≈ 20.25 gf
```

再查看带电压记录的飞行日志，取稳定悬停时四路电机命令的平均值。参考日志约为 47.4%，据此粗略外推，满命令推力约为每个电机 0.419 N；电机响应时间先取 40 ms。

这些值只是模型初值。训练时随机改变推力增益、质量、惯量、电压和响应时间，让策略在不同参数下都练习，减少它对某一组估计值的依赖。

有了这个近似模型，就可以先运行训练和评估，准备好场景后再做 Isaac 展示。后续可用单电机推力台，在 4.2、3.9、3.7、3.5 V 下分别记录多个 PWM 点，逐步用实测数据替换估计值。

<a id="chapter-07-section-3"></a>

### 7.3 训练任务是什么

悬停练习的观测有 35 维，包含位置/速度误差、姿态矩阵、角速度、参考速度与加速度、上一步动作、误差积分、电机估计力和电压。网络输出 3 维动作，分别是 x、y、z 方向的残差加速度，范围为 `±4 m/s²`。

每个仿真环境都从略有不同的初始姿态、动力参数和风扰开始。策略在每一步得到奖励：

- 接近目标位置与速度；
- 保持姿态和飞行高度；
- 动作平滑，不频繁大幅修正；
- 不发生翻覆、撞地或飞出范围。

PPO 同时在多个仿真环境中运行，收集每一步的观测、动作和结果，再据此更新策略。训练结束后，换用训练时没有用过的随机种子，并增大扰动，比较基础 PD 和 PPO 残差控制的表现。

<a id="chapter-07-section-4"></a>

### 7.4 在普通电脑上跑第一个 PPO

从仓库根目录创建 Python 环境：

```bash
python3 -m venv .venv
source .venv/bin/activate
python3 -m pip install --upgrade pip
python3 -m pip install numpy torch matplotlib
```

运行 CPU 悬停练习：

```bash
python3 software/simulation/course/hover_lab.py \
  --output output/my-first-hover \
  --iterations 400 --envs 128 --device cpu
```

输出目录会包含：

| 文件 | 内容 |
| --- | --- |
| `training.csv` | 每轮奖励、位置误差和失败数 |
| `policy_initial.pt` / `policy_final.pt` | 初始与最终策略 |
| `actor.pt` | 可独立加载的 TorchScript 策略 |
| `evaluation.json` | PD 与 PPO 在未参与训练的工况下的测试结果 |
| `config.json` | 训练使用的全部设置 |

参考运行的结果如下，数值是 96 个完整回合的平均 RMS 位置误差：

| 水平扰动 | 基础 PD | PPO 残差 |
| ---: | ---: | ---: |
| 0.0 m/s² | 1.58 cm | 3.64 cm |
| 0.8 m/s² | 16.51 cm | 6.03 cm |
| 1.5 m/s² | 30.48 cm | 10.56 cm |

平静环境里，简单 PD 更准确；风扰增强后，PPO 学到的补偿明显降低了位置误差。残差策略的主要作用是处理持续扰动和模型误差，基础控制器仍负责稳定飞行。

![基础 PD 与 PPO 在三种扰动下的位置误差](img/hover-evaluation.png)

图 7-1　CPU 悬停练习在未参与训练的工况下的表现。

![PPO 训练曲线](img/training-curves.png)

图 7-2　训练过程中的奖励与误差变化。

<a id="chapter-07-section-5"></a>

### 7.5 从悬停到轨迹跟踪

完成悬停练习后，可以把固定目标换成连续轨迹。下面的演示先沿八字曲线穿过 10 个环，再螺旋爬升，最后在阵风中悬停。轨迹由程序预先给出，PPO 学习怎样跟上轨迹，并减小扰动造成的偏差。

先看相同扰动下的固定镜头悬停。第一段是基础 PD，第二段是 PPO 残差：


[![播放视频：hover-pd](img/hover-pd-poster.png)](img/videos/hover-pd.mp4)

基础 PD：受到持续扰动后出现较大的稳态偏移。



[![播放视频：hover-ppo](img/hover-ppo-poster.png)](img/videos/hover-ppo.mp4)

PPO 残差 + PD：策略主动补偿扰动并回到目标附近。



下面是 60 秒完整演示，包括模型、训练流程、悬停对照、八字穿环、螺旋和阵风恢复：


[![播放视频：rl-demo-60s](img/rl-demo-poster.png)](img/videos/rl-demo-60s.mp4)


这次 Isaac Sim 演示持续 34 秒，通过了 10/10 个环，位置 RMS 误差约为 12.11 cm，最高速度约为 1.10 m/s。另几组工况下的对比如下：

| 工况 | 基础 PD | PPO 残差 + PD |
| --- | ---: | ---: |
| 平静 | 2.72 cm | 8.19 cm |
| 持续扰动 | 51.33 cm | 18.43 cm |
| 电机差异与质量误差 | 53.81 cm | 14.28 cm |
| 突变阵风 | 34.46 cm | 26.92 cm |

<a id="chapter-07-section-6"></a>

### 7.6 数值训练与可选 Isaac Sim

下面的数值训练不需要外部场景。最后运行 Isaac 时，需要自行准备兼容的 USD 模型：资产目录中应有 `USD/open32droe/robot.usd`，以及它引用的网格、材质等文件。示例命令中的资产路径要换成自己的实际路径。

完整训练脚本使用 CUDA。先在训练工作站运行物理检查：

```bash
cd /path/to/open32drone/software/simulation/rl_demo
python3 physics_checks.py \
  --output ../../output/rl-demo/my-run/physics-checks.json
```

然后训练、评估并做赛道预检：

```bash
python3 train.py \
  --output ../../output/rl-demo/my-run \
  --iterations 1200 --envs 1024

python3 evaluate.py --run ../../output/rl-demo/my-run
python3 preflight.py --run ../../output/rl-demo/my-run
```

Isaac Sim 的独立 Python 环境用它自带的 `python.sh` 启动：

```bash
/path/to/isaac-sim/python.sh \
  /path/to/open32drone/software/simulation/rl_demo/native_isaac.py \
  --package /path/to/open32drone/output/simulation-model/OPEN32DRON_fixed_81g \
  --run /path/to/open32drone/output/rl-demo/my-run \
  --output /path/to/open32drone/output/rl-demo/my-run/native \
  --seconds 34 --record --visible
```

`native_isaac.py` 每 5 ms 向刚体施加四电机合力与力矩，飞机的位置和姿态来自 PhysX 积分；轨迹、环和摄影机用于展示，不会逐帧拖动飞机。

<a id="chapter-07-section-7"></a>

### 7.7 怎样继续走向真机策略

要进一步把策略用到真机上，还需要补充几项工作：

1. 用单电机推力台替换满推力、响应时间和反扭矩初值；
2. 把 IMU、光流和 ToF 的噪声、延迟、丢帧加入训练环境；
3. 将 ROS 记录的状态整理成与策略 35 维观测一致的输入，先做回放推理，再做受限台架与低高度试验。

示例直接读取仿真状态，圆环位置也由任务提供。换成真实飞机后，需要先解决状态和目标从哪里获取的问题，例如加入相机定位或目标识别。

初步接入时，可以先让策略输出限幅后的加速度或速度修正，通过 ROS 使用已有飞控。完成回放和台架检查后，再逐步尝试低高度飞行；更底层的电机控制留到数据和测试充分以后再研究。

完成本章后，可以先比较自己的训练曲线与示例结果，再根据差异改进模型。完整场景、传感器仿真和真机策略迁移仍需另外实现，数值练习的结果不能直接作为真机飞行结论。

<a id="chapter-07-section-8"></a>

### 7.8 模型坐标与检查顺序

建模时统一使用米、千克、秒，机体系采用 ROS FLU：X 向前、Y 向左、Z 向上。CAD 软件的坐标定义可能不同，接入固件接口前先确认转换关系。

导入模型后，分别检查尺寸比例、重心和惯量。机架文件只描述部分结构，完整质量还包括电路、线束、电机和电池，应以装配后的整机测量结果为准。

碰撞体尽量简化，并检查是否互相重叠。调试时先看尺寸和质量，再检查重力、接触、旋翼轴向和力的正负方向，最后接入控制器运行闭环仿真。参数保存在 `software/simulation/rl_demo/model.json` 中，这些是模型设置，不是程序自动标定出的数值。

适配器目前查找的路径就是 `USD/open32droe/robot.usd`，其中 `open32droe` 的拼写需要保留。更换场景时，一并核对网格、材质引用和 Isaac 版本。传感器仿真、固件在环、Gazebo 接入，以及从仿真迁移到真机的部分，都需要分别实现和测试。

## 补充参考

- [源码与编译](docs/reference/source-build.zh-CN.md)
- [参数与接口](docs/reference/firmware.zh-CN.md)
