# 源码与编译

该文档从主循环开始，介绍飞机怎样读取传感器、保持平稳，并响应手机、遥控器和 ROS 的指令。后半部分介绍固件、Android App 和 ROS 2 的编译方法。参数的具体含义见[参数与接口](firmware.zh-CN.md)。

## 仓库目录

| 目录 | 内容 |
|---|---|
| `hardware/` | 机架模型、机械规格与采购资料 |
| `firmware/` | ESP32-S3 飞控源码 |
| `android/` | Android 控制器源码 |
| `ros2/` | ROS 2 驱动、控制命令与 RViz 配置 |
| `simulation/` | 数值动力学、控制与强化学习实验 |
| `docs/` | 中英文教程及网站配置 |
| `tests/` | 可重复执行的源码与文档检查 |
| `releases/open32drone/` | 仓库内的软件包、说明和校验清单；软件下载见 [GitHub Releases](https://github.com/npu-ius-lab/open32drone/releases) |

## 代码架构预览

[![飞控逻辑总览](/media/figures/firmware-map.svg)](/media/figures/firmware-map.svg)

可以把飞控的工作分成三件事：

1. **知道飞机现在怎么样。** 读取 IMU、光流和 ToF，计算飞机朝哪个方向倾斜、离地多高、正在向哪里移动。
2. **知道接下来要做什么。** 根据摇杆、起降按钮或 ROS 指令，确定想要的姿态、高度或移动速度。
3. **调整四个电机。** 比较当前状态和目标，分别增加或减小电机输出，让飞机接近目标。

下面沿着这三件事看代码。

## 阅读代码顺序

第一次看源码，按下面顺序读函数：

1. `firmware/firmware.ino` 的 `setup()` 和 `loop()`：程序如何启动，每轮按什么顺序运行；
2. `firmware/control.ino` 的 `control()`：各个控制步骤如何接在一起；
3. `firmware/control_modes.ino` 的 `interpretControls()`：摇杆和模式选择怎样变成控制目标；
4. `firmware/control_auto_flight.ino` 的 `updateAutoFlightControl()`：一键起飞和降落如何进行；
5. `firmware/control_altitude.ino` 的 `updateAltitudeHoldControl()`：怎样保持高度；
6. `firmware/control_position.ino` 的 `updatePositionControlSplit()`：怎样减少水平漂移；
7. `firmware/control_stabilization.ino` 的 `controlAttitude()`、
   `controlRates()` 和 `controlTorque()`：怎样把目标变成四个电机的输出。

## 飞控主循环

飞控程序以每秒运行 300 次为目标，也就是大约每 3.3 毫秒重复一轮。`firmware.ino` 中的 `loop()` 写出了这一轮的执行顺序：

[![一次 300 Hz 主循环](/media/figures/control-cycle.svg)](/media/figures/control-cycle.svg)

每轮先尝试读取传感器，根据有效读数计算飞机当前的姿态、高度和水平速度。接着，程序根据遥控器、手机或 ROS 给出的目标，算出四个电机分别需要多大的输出，再更新电机控制信号。

更新电机输出后，程序继续处理串口和网络消息，并按各自的频率读取电池电压、更新指示灯和记录日志。它们都出现在主循环里，但不代表每项工作都以 300 Hz 执行。

修改过的参数会在电机没有输出时保存。程序每秒检查一次是否需要保存，避免飞行过程中写入存储器造成停顿。每轮结束时，还会更新运行记录，供防卡死监测判断主循环是否仍在正常工作。

## 飞控文件职责 {#固件文件职责}

| 文件 | 负责内容 | 出现什么问题时先看 |
|---|---|---|
| `firmware.ino` | 启动各模块，按顺序运行主循环 | 启动或执行顺序不清楚 |
| `time.ino` | 安排循环时间，统计各步骤耗时 | 循环变慢或耗时忽高忽低 |
| `imu_backend.h`、`imu.ino` | 选择 IMU 驱动，读取数据，换算安装方向，滤波和校准 | IMU 读数或校准异常 |
| `flow.ino` | 读取 TF-0850 数据，检查光流和测距是否正常 | 测距或光流数据缺失 |
| `estimate.ino` | 姿态、高度和水平运动估计 | 传感器值正常但估计状态错误 |
| `control.ino` | 依次调用起降、定高、定点和姿态控制 | 想了解控制算法的整体顺序 |
| `control_modes.ino` | 处理模式选择、摇杆输入和遥控器接管 | 模式选择或人工接管异常 |
| `control_offboard.ino` | 接收外部程序的目标，确认指令持续到达、所需传感器正常后启用控制 | ROS 指令无法开始控制飞机 |
| `control_auto_flight.ino` | 逐步完成起飞、降落和起飞后的模式切换 | 一键起飞或降落异常 |
| `control_altitude.ino` | 高度目标、垂直 PID 修正、倾斜补偿 | 定高或垂直响应异常 |
| `control_position.ino` | 根据光流估计的位置和速度修正水平移动 | 定点漂移或无法启用 |
| `control_stabilization.ino` | 姿态环、角速度 PID、四电机混控 | 姿态模式抖动或修正方向错误 |
| `motors.ino` | 设置电机引脚，输出 PWM 控制信号 | 电机没有输出或编号不对 |
| `rc.ino` | 读取 SBUS 遥控器，校准摇杆，识别接管和急停操作 | 物理遥控器行为异常 |
| `mavlink.ino` | 收发指令和飞机状态，读取或修改参数，限制每次发送的数据量 | Android/ROS 命令、QGC 参数读取或状态异常 |
| `safety.ino` | 起飞前检查、失联处理、上锁和翻覆停机 | 起飞被拒绝或异常后没有正确停机 |
| `loop_watchdog.ino` | 监测主循环是否卡住，超时后停止电机并重启 | 程序卡住或触发防卡死重启 |
| `parameters.ino` | 注册参数，检查数值范围，读取和保存设置 | 参数被拒绝或重启后不正确 |
| `ota.ino` | 在地面接收升级固件，检查新固件启动情况，失败时退回旧固件 | 更新或回退失败 |
| `log.ino`、`cli.ino` | 飞行日志和串口检查 | 分析可复现问题 |

## 控制模式

### 1. STAB：姿态与角速度控制

姿态模式首先解决“让飞机按要求倾斜和转向”。横滚、俯仰摇杆决定希望倾斜多少；偏航摇杆决定转向速度；油门直接控制总推力，不自动保持高度。

[![姿态与角速度串级控制](/media/figures/attitude-control.svg)](/media/figures/attitude-control.svg)

例如，希望飞机向前倾斜，而它现在还是水平的：

1. `controlAttitude()` 比较目标姿态和当前姿态，算出应该以多快的速度转过去。
2. `controlRates()` 比较这个目标转动速度和陀螺仪测得的转动速度，用 PID 算出修正量。
3. `controlTorque()` 把总推力和修正量分配给四个电机。这一步叫“混控”：不同电机一增一减，飞机才会倾斜或转向。

PID 中，P 根据当前误差修正，I 补偿持续存在的偏差，D 根据误差变化抑制过快的响应。如果某个电机已经无法再增加或减小输出，程序会一起缩小各轴修正量，并撤回这一轮新增的角速度积分，避免误差越积越多。

### 2. ALT_HOLD：增加垂直控制

定高模式在稳定姿态的基础上，再负责“飞到多高、保持多高”。主要代码在 `control_altitude.ino`。

程序先给出一份接近悬停所需的基础推力，再根据高度差和升降速度增加或减小推力。油门在中间附近时保持目标高度，推高或拉低油门会让目标高度逐渐变化。自动起降时，则由起降程序逐步改变高度目标。

电池电压会影响悬停所需的推力，因此程序会用最近的有效电压读数调整基础推力，并限制调整幅度。如果电压读数超时，就不再使用这项补偿。它只调整悬停基础推力，不给所有 PID 修正量统一乘一个系数。飞机倾斜时，还会适当增加总推力，弥补向上的分量减少。

ToF 暂时没有有效读数时，程序会把高度标为无效、暂停累积高度误差，并逐渐减小之前的修正量，而不是继续根据旧高度加大油门。持续失去测距后的处理见下面的“自动起飞与降落”。

### 3. POS_HOLD：增加水平控制

定点模式在定高的基础上，还要减少前后、左右的漂移。主要代码在 `control_position.ino`：

[![水平位置控制](/media/figures/position-control.svg)](/media/figures/position-control.svg)

程序根据光流估计的水平移动情况，决定向哪个方向倾斜，抵消漂移。推摇杆时，摇杆表示希望移动的方向和速度；目标位置也随之移动，姿态控制仍负责让飞机保持平稳。

启用前会检查光流和测距是否正常、飞机是否已离地、倾斜是否过大，以及偏航摇杆是否正在要求明显转向。这些条件通过后才开始定点。靠近地面时会减弱部分水平修正，减少低高度光流噪声的影响。

几个与代码对应的限制：

- 摇杆、位置误差和 ROS 速度指令产生的目标速度会先合在一起，再用 `POS_STICK_V` 限制最终水平速度。
- 光流暂时不能用于定点时，`updateBoundedPositionFallback()` 仍允许有效的摇杆指令控制倾斜；没有指令时回到水平目标。倾角不超过定点控制的 `12°` 上限，也不会自动改用姿态模式更大的倾角范围。
- 程序会保留地面测得的光流零点偏差。如果没有测得可靠偏差、只能暂按零偏差处理，就把水平修正限制在 `3°` 内，并暂停速度误差的积分。

## 模式与执行器控制权 {#模式与执行器控制权}

可选用的三种模式有姿态控制 `STAB`、定高控制 `ALT_HOLD` 和定点控制 `POS_HOLD`。除此之外，自动起降以及 ROS 持续发送目标的 Offboard 控制会用到内部的 `AUTO` 状态。

几个来源不能随意覆盖彼此的指令。控制权归属遵循以下规则：

- **遥控器急停始终单独处理。** 自动起降或 Offboard 控制期间，仍会检查是否急停。
- **失联后的降落流程。** 已触发保护时，重新收到网络摇杆消息不会直接恢复正常控制。
- **接收机通电不等于接管。** 普通遥控器接管需要明确的操作，不会因为收到一帧数据就抢走手机控制。自动起降中改变遥控器模式开关，可以取消自动流程并接管。
- **Offboard 与普通摇杆控制分开。** 先退出 Offboard 或切换到普通飞行模式，再发送手机或 ROS 的摇杆指令。

## 自动起飞与降落

[![自动起飞与降落状态图](/media/figures/auto-flight-states.svg)](/media/figures/auto-flight-states.svg)

飞控把起降拆成连续的小步骤：

- 起飞时，程序逐渐提高目标高度；达到目标后，继续使用所选的定高或定点模式。正常起降期间，只要持续收到有效摇杆指令，仍可调整水平移动和转向。选择定点模式时，水平控制也会参与，帮助减少漂移。

- 降落时，程序先结束 Offboard 控制，再逐渐降低目标高度，接近地面后减小推力，确认接地后上锁。

如果控制端失联，程序会根据测距是否可用选择处理方式：

- **测距仍正常：** 根据高度控制下降，并持续检查是否接地。
- **测距也丢失：** 改为逐步减小推力，最多持续 `SF_DESCEND_TIME`（默认 5 秒）后停机。因为没有可靠测距，超时停机不会被当成“已经确认落地”。

后一种处理也用于自动起降中持续失去测距的情况。具体触发条件见 `control_auto_flight.ino` 和 `safety.ino`。

## Android 与 ROS 的命令路径

Android 和 ROS 2 都通过 MAVLink 消息与飞控通信。MAVLink 规定了模式、起降、控制目标和飞机状态等消息的格式：

[![Android / ROS 指令路径](/media/figures/command-path.svg)](/media/figures/command-path.svg)

手机和 ROS 负责告诉飞机“想做什么”；姿态控制和电机输出仍在飞控里完成。飞控收到指令后，也会检查传感器、当前状态和指令是否超时，再决定能否执行。

同一架飞机一次使用一个控制客户端。飞控根据有效的控制端心跳记录回复地址和源端口，解锁后固定使用这个地址。地面切换客户端时，先关闭原客户端，等待 3 秒再连接另一个。

QGC 只作为可选的地面参数工具：飞机上锁时，可通过标准参数消息查看和修改设置，不用于本项目的起飞或飞行控制。

## 参数与校准

NVS 是芯片内用于断电保存设置的存储区：启动时读取已保存的有效值；某项没有已有保存值，就使用源码中的默认值。

修改参数时会检查数值是否允许，解锁后不能通过普通参数接口修改。改过的数值由 `syncParameters()` 在电机无输出时保存，具体见[飞控主循环](#飞控主循环)。

## 修改与测试

第一次改代码，按下面做：

1. 复现一个问题并保存日志；
2. 按上表找到负责该功能的文件；
3. 一次只改一个行为或一组同类参数；
4. 添加一个能复现问题的自动化测试；
5. 运行测试并重新编译固件；
6. 刷入固件后，先拆桨检查传感器、电机顺序和急停，再进行低高度飞行测试。

涉及多个模块时，先分别测试，再进行联调。重构时保持现有接口和行为，方便对比修改前后的结果。

## 编译固件需要什么 {#固定固件工具链}

以下命令使用这些版本，方便复现相同的编译环境：

| 依赖 | 版本 |
|---|---|
| Arduino-ESP32 | `3.3.6` |
| FlixPeriph（IMU 与 SBUS） | `1.10.4` |
| MAVLink Arduino 库 | `2.0.25` |
| 板卡 | `esp32:esp32:XIAO_ESP32S3` |
| 选项 | `PSRAM=opi,PartitionScheme=default_8MB,FlashMode=dio` |

一次性安装：

```bash
arduino-cli core install esp32:esp32@3.3.6 \
  --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli lib install "FlixPeriph@1.10.4" "MAVLink@2.0.25"
```

在仓库根目录执行，编译使用 MPU6500/MPU9250 的固件。下面的目录用于临时编译，完成后把需要保留的固件放到项目 `output/`，再清理临时目录：

```bash
arduino-cli compile \
  --clean \
  --fqbn 'esp32:esp32:XIAO_ESP32S3:PSRAM=opi,PartitionScheme=default_8MB,FlashMode=dio' \
  --output-dir /private/tmp/open32drone-build \
  firmware
```

更换 IMU 型号时，通过编译选项选择驱动。后面的姿态和控制代码不变，但安装方向、读数和校准仍要在对应硬件上检查：

```bash
arduino-cli compile --clean \
  --fqbn 'esp32:esp32:XIAO_ESP32S3:PSRAM=opi,PartitionScheme=default_8MB,FlashMode=dio' \
  --build-property 'compiler.cpp.extra_flags=-DOPEN32DRONE_IMU_BACKEND=OPEN32DRONE_IMU_ICM20948' \
  --output-dir /private/tmp/open32drone-icm20948 firmware

arduino-cli compile --clean \
  --fqbn 'esp32:esp32:XIAO_ESP32S3:PSRAM=opi,PartitionScheme=default_8MB,FlashMode=dio' \
  --build-property 'compiler.cpp.extra_flags=-DOPEN32DRONE_IMU_BACKEND=OPEN32DRONE_IMU_MPU6050' \
  --output-dir /private/tmp/open32drone-mpu6050 firmware
```

不要让多个后端同时共用一个 Arduino 编译缓存；顺序执行并使用 `--clean`，避免复用由
另一个宏配置编译的对象。更换 IMU 后，先核对安装方向和读数，再完成校准与电机检查。

两个固件产物用途不同：

- `firmware.ino.merged.bin`：完整 USB 镜像，在 `0x0` 写入；
- `firmware.ino.bin`：只用于 A/B OTA 的应用镜像。

首次安装使用完整固件，通过 USB 写入 `0x0`；手机 OTA 使用应用固件。两个文件不能互换。

## 命令行刷写（可选） {#usb-刷写}

常规安装可直接使用[浏览器刷写工具](../guide/04-firmware-flight.md#flashing)。需要离线或命令行操作时，使用下面的方法。先从 [Release](https://github.com/npu-ius-lab/open32drone/releases/latest) 下载完整固件，将终端切换到下载目录。下面的文件名是示例，执行时请替换为实际下载的文件名。

安装 [Python 3.10 或以上](https://www.python.org/downloads/)。下面固定使用 esptool 5.1.0 和 pyserial 3.5，安装到独立虚拟环境；后者同时提供串口查看和校准终端。只做自己系统对应的一组步骤。

### Windows（PowerShell）

安装 Python 后重新打开 PowerShell。在下载文件夹的地址栏输入 `powershell` 并回车：

```powershell
py -3 --version
py -3 -m venv .venv
.\.venv\Scripts\python.exe -m pip install "esptool==5.1.0" "pyserial==3.5"
.\.venv\Scripts\python.exe -m esptool version
.\.venv\Scripts\python.exe -m serial.tools.list_ports -v
```

Python 版本须至少 3.10。记录设备管理器或上条命令显示的串口，例如 `COM5`。按下方“进入下载模式”操作后，把示例端口换成实际端口：

```powershell
.\.venv\Scripts\python.exe -m esptool --chip esp32s3 --port COM5 erase-flash
.\.venv\Scripts\python.exe -m esptool --chip esp32s3 --port COM5 --baud 460800 write-flash 0x0 .\Open32Drone-20260928-190250-full.bin
```

### macOS（终端）

安装 Python 后，在终端输入 `cd `（末尾有空格），把下载文件夹拖入终端并回车：

```bash
python3 --version
python3 -m venv .venv
.venv/bin/python -m pip install "esptool==5.1.0" "pyserial==3.5"
.venv/bin/python -m esptool version
.venv/bin/python -m serial.tools.list_ports -v
```

记录 USB 串口，例如 `/dev/cu.usbmodem1101`。进入下载模式后，用实际端口替换示例：

```bash
.venv/bin/python -m esptool --chip esp32s3 --port /dev/cu.usbmodem1101 erase-flash
.venv/bin/python -m esptool --chip esp32s3 --port /dev/cu.usbmodem1101 --baud 460800 write-flash 0x0 Open32Drone-20260928-190250-full.bin
```

### Ubuntu / Debian（终端）

安装 Python 的虚拟环境支持，然后进入下载文件夹执行后续命令：

```bash
sudo apt update
sudo apt install python3 python3-venv
python3 --version
python3 -m venv .venv
.venv/bin/python -m pip install "esptool==5.1.0" "pyserial==3.5"
.venv/bin/python -m esptool version
.venv/bin/python -m serial.tools.list_ports -v
```

记录串口，例如 `/dev/ttyACM0`。进入下载模式后，用实际端口替换示例：

```bash
.venv/bin/python -m esptool --chip esp32s3 --port /dev/ttyACM0 erase-flash
.venv/bin/python -m esptool --chip esp32s3 --port /dev/ttyACM0 --baud 460800 write-flash 0x0 Open32Drone-20260928-190250-full.bin
```

### 进入下载模式、刷写和重启

1. 用 USB 数据线连接 XIAO。按住 **BOOT**，按一下 **RESET**，再松开 **BOOT**。
2. 重新查看串口：下载模式的端口号可能与正常启动不同。
3. 关闭串口监视器，再执行对应系统的两条刷写命令。**erase-flash 会清除校准、参数和 Wi-Fi 设置**；此流程用于首次安装或完整恢复，不是保留设置的日常升级。
4. 等待命令成功结束，按一下 RESET 正常启动。完整镜像必须写入 `0x0`，不能用 app 文件替代。

### 打开串口

刷写后再次查询端口。在相同文件夹打开 115200 波特率串口：

```powershell
# Windows，将 COM5 换成正常启动后的端口
.\.venv\Scripts\python.exe -m serial.tools.miniterm COM5 115200 --eol LF
```

```bash
# macOS；Linux 将端口替换为 /dev/ttyACM0
.venv/bin/python -m serial.tools.miniterm /dev/cu.usbmodem1101 115200 --eol LF
```

退出串口按 `Ctrl+]`。打开后按一次 RESET，飞机水平静置，等待：

```text
Initializing complete
Gyro calibration complete
```

然后返回[起飞前准备](../guide/04-firmware-flight.md#preflight)。

### 遇到问题

| 现象 | 处理 |
| --- | --- |
| 没有串口 | 换确认可传数据的线、直连电脑 USB，重新进入 BOOT；先排除仅充电线 |
| Windows 未知 USB 设备 | 在设备管理器确认型号，按 [XIAO 官方说明](https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/)处理；不要盲目安装 CH340/CP210x 驱动 |
| 串口被占用 | 关闭串口终端、IDE 串口监视器，重新刷写 |
| Linux Permission denied | 执行 `sudo usermod -aG dialout "$USER"`，注销并重新登录；不要用 sudo 安装 Python 包 |
| 一直停在 Connecting | 重新进入 BOOT、查询端口并检查 USB 连接 |
| 写入中断 | 将 `--baud 460800` 改为 `--baud 115200`，重新刷写 |
| 找不到固件文件 | 确认终端在下载文件夹，文件名没有浏览器添加的重复下载后缀 |

工具依据：[Espressif esptool 安装说明](https://docs.espressif.com/projects/esptool/en/latest/esp32/installation.html)。刷写失败时不要继续首飞步骤。


## Android 构建

```bash
cd android
./gradlew --no-daemon testDebugUnitTest lintDebug assembleDebug
```

调试 APK：

```text
android/app/build/outputs/apk/debug/app-debug.apk
```

Android 当前为 `0.1.2`（`versionCode 3`），与同一源码版本的固件一起使用。

修改 App 后，重点检查：能否通过飞机热点连接、是否只处理所选飞机的数据、断线后能否重连，以及一键起降、起降中摇杆操作、遥控器接管和急停是否正常。图传在单独线程中处理，优先级低于控制通信；显示视频不参与飞行控制。

## ROS 2 构建

```bash
mkdir -p ~/osdrone_ws/src
cp -a ros2 ~/osdrone_ws/src/open32drone_driver
cd ~/osdrone_ws
rosdep install --from-paths src --ignore-src -r -y
colcon build --symlink-install
source install/setup.bash
```

ROS 包清单使用 `0.1.2`，应与同一源码版本的固件和 Android 客户端配套。

需要持续把仓库修改直接用于 ROS 工作空间时，新建工作空间后可用符号链接代替前面的
复制命令：

```bash
mkdir -p ~/osdrone_ws/src
ln -s /path/to/open32drone/ros2 ~/osdrone_ws/src/open32drone_driver
```

修改现有节点时，编辑 `ros2/open32drone_driver/` 中的对应文件；新增可执行节点时，再到 `setup.py` 登记入口，需要一起启动时修改 `launch/`。之后只重建当前包：

```bash
cd ~/osdrone_ws
colcon build --symlink-install --packages-select open32drone_driver
source install/setup.bash
```

按 ROS 指南第 3 节启动控制栈后，再在另一个终端拆桨运行
`ros2 run open32drone_driver bench_test --duration 5`。普通应用只使用已发布的
`cmd_vel`、里程计、命令话题和服务；不要在另一个节点里复制固件的解锁、起飞和降落
流程。接口和首次起降流程见 [ROS 2 控制](../guide/06-ros.md)。如果修改了 MAVLink 消息字段或解锁、起降等操作的处理方式，还要同步检查固件、Android 和相关测试。

## 在电脑上运行测试 {#主机验证}

在仓库根目录执行：

```bash
python3 -m compileall -q ros2
python3 -m unittest discover -s tests -p 'test_*.py' -v
git diff --check
```

Android 验证：

```bash
cd android
./gradlew --no-daemon testDebugUnitTest lintDebug assembleDebug
```

自动化测试覆盖：

- 硬件引脚、电机通道配置、模式、起飞前检查和失联条件；
- 陀螺仪、加速度计和遥控器校准，以及设置保存；
- TF-0850 数据解析、光流和测距的有效条件、IMU 安装方向和参数；
- 支持的 MAVLink、遥测、电压输入和 A/B OTA；
- 主循环调度和超时统计、不同 IMU 的编译选项、参数和飞机状态的发送安排；
- Android 命令、摇杆、路由、选中飞机隔离、图传优先级、重连和版本；
- ROS 控制计算、指令来源、话题、坐标转换和 OTA 上传检查；
- 仓库目录和中英文文档链接。

重新编译后，还需在飞机上检查传感器、电机输出和控制操作。

## 编译后的检查

1. 拆下桨叶，刷入新固件，确认可以正常启动。
2. 检查 IMU、ToF、电池电压和所用控制端的连接。
3. 确认电机编号、转向、上锁和急停正常。
4. 完成[起飞前准备](../guide/04-firmware-flight.md#preflight)，在空旷区域进行短时低高度起降。
5. 若出现异常，先保存日志并排查，再继续测试。

## OTA 更新机制

手机通过 HTTP `8080` 上传应用固件，同时提供文件的 SHA-256 校验值。飞控把新固件写入另一块应用分区，不覆盖当前正在运行的固件；这种保留新旧两份固件的方式叫 A/B OTA。

更新只允许在地面、上锁且电机没有输出时进行。自动起降、Offboard 控制或上一次更新还没完成启动检查时，也不能开始新的更新。

重启后，程序会检查参数存储、IMU、陀螺仪校准、主循环、TF-0850 和 Wi-Fi。它们持续正常一段时间后，新固件才被确认可用；超过等待时间仍未通过检查，则退回原来的固件。相关代码在 `ota.ino`。

## 提交修改前

- 运行相关测试，确认固件、Android 和 ROS 2 可以正常构建。
- 修改控制消息或接口时，同步检查固件与客户端的处理逻辑。
- 功能或操作步骤变化时，同步更新中英文文档。
- 在 PR 中简述修改内容、测试方法和结果。贡献流程见[参与项目](../project/contributing.zh-CN.md)。
