# 06 · ROS 2 控制

Open32Drone ROS 2 包在 MAVROS 之上提供一套接近无人车的精简接口，适合教学、实验和
后续集群开发。它不依赖 QGC；同一台 ROS 主机上的多个 MAVROS 进程不能绑定同一个
UDP 端口。Android 和 ROS 可以位于同一局域网，但同一时刻只能有一个控制端占用同一架飞机。

ROS 包清单版本是 `0.1.0`。Open32Drone 固件、Android 和
ROS 2 应从同一个源码版本构建。

**命令应答与运动状态：** 命令成功表示收到飞控真实应答；速度控制仍须等 AUTO 模式确认后才开始。
`fresh local position is required` 表示 ROS 位置消息缺失或超过 0.5 秒未更新，
不等于机载 ToF 坏了。预热被拒会在激活期限内重试；若仍失败，不要反复发送移动命令，
先降落，再保留启动日志以及飞机命名空间下的 `offboard/status`、
`UAS1/local_position/pose`、`UAS1/setpoint_raw/local` 数据。不要调大保护超时或修改 PID
来掩盖指令流缺失。

制作离线仿真模型时，先看 [URDF / USD 模型导出](07-rl.md)。
下面的连接和控制步骤面向真机；模型指南尚未给当前 ROS 包增加仿真后端。

第一次使用 ROS 只做下面五件事：

1. 普通遥控或 Android 首飞已经通过，并关闭 Android 控制端；
2. ROS 电脑直连飞机热点，或与 STA 模式飞机连接同一个路由器；
3. 按第 2 节安装，在一个终端按第 3 节启动；
4. 在另一个终端确认 `/open32drone/connected` 为 `true`；
5. 第 4–5 节先作为参考，直接跳到第 6 节，只执行一次“起飞 → 悬停 → 降落”。

单机流程通过以前不用阅读第 5 节多机配置，也不要同时打开 Android、ROS 和其他
MAVLink 控制端。

## 1. 环境要求

- 已安装 ROS 2、`colcon` 和 MAVROS；
- 主机直连飞机 AP，或者与已经配置 STA 的飞机连接同一个可信路由器；
- ROS 主机可以访问所选的飞机 IPv4 地址；
- 关闭 Android 和其他 MAVLink 客户端；
- 安装和台架检查期间必须拆桨。

启动 ROS 前先确认网络：

```bash
ping -c 3 192.168.4.1  # 路由器模式替换为飞机的 STA 地址
```

## 2. 安装

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

### 最小 ROS 开发闭环

自己的 ROS 程序只使用本包提供的 `/open32drone/cmd_vel`、`/open32drone/odom`、命令
话题或服务，不要绕过驱动直接重复实现 MAVLink 起飞/降落状态机。按上面的复制方式
安装时，后续直接修改 `~/osdrone_ws/src/open32drone_driver/` 中的源码，然后执行：

```bash
cd ~/osdrone_ws
colcon build --symlink-install --packages-select open32drone_driver
source install/setup.bash
```

新增 Python 节点放入工作空间源码的 `open32drone_driver/` Python 包，并在 `setup.py`
注册入口；新增启动参数同步修改 `launch/`。第 3 节启动成功后，在第二个终端拆桨运行
`ros2 run open32drone_driver bench_test --duration 5`，再用第 6 节的单次起降验证。只有
共享 MAVLink 协议发生变化时，才需要同步修改固件、Android 和协议契约测试。更完整
的构建与评审规则见[开发指南](../reference/source-build.zh-CN.md)。

## 3. 启动并证明链路真实连接

`connected=true` 只证明心跳连接；起飞前还要确认 IMU、odom 和测距持续更新。
自定义启动文件时，不要给 `mavros_node` 添加全局 `name="mavros"`：这会同时重命名
内部插件，破坏话题路径和插件配置。官方启动文件已将 ToF 输出映射至
`UAS1/distance_sensor/tof`，供接口桥发布 `range/downward`。

起飞命令先验证只读 `status` 请求能完整往返；若超时，先排查命令链路，不连续重发起飞。
录制 ROS bag 时，必须先正常停止录制，再读取 SQLite 数据库；飞行中查看实时话题，
不要直接查询正在写入的数据库。

直连 AP 模式下启动完整控制栈：

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

高级 MAVROS 路由仍可直接覆盖 `fcu_url:=...`。建议在路由器中保留飞机 DHCP
地址，让每架飞机的地址可预测。Android 和 ROS 可以位于同一局域网，但同一时刻
只能有一个 MAVLink 控制端。

在第二个终端检查实时数据，不能只看话题名字：

```bash
source ~/osdrone_ws/install/setup.bash
ros2 run open32drone_driver control status
ros2 topic echo /open32drone/connected --once
ros2 topic hz /open32drone/imu/data
ros2 topic echo /open32drone/range/downward --once
```

`/open32drone/connected` 必须为 `true`，IMU 必须持续更新，向下距离必须有新鲜
TF-0850 数据包。话题出现在列表中并不能证明 MAVLink 已连接。

## 4. 对外接口

### 遥测

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

桥接节点把关键传感器重新发布为 Reliable QoS，RViz 和 `rqt` 不需要再适配 MAVROS
的 sensor-data QoS。

### 控制

| 接口 | 含义 |
|---|---|
| `/open32drone/command` | 单次生命周期文本命令 |
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

## 5. 高级：同一局域网控制多架飞机

多机必须使用路由器 STA 模式。每架飞机直连 AP 时默认都是同一个
`192.168.4.1`，无法在同一局域网里区分多架飞机。对外话题、服务、MAVROS 内部接口和
TF 由以下配置隔离：

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

第二架必须使用不同值。建议在路由器里为每架飞机保留固定 DHCP 地址，不能用可能变化的
临时地址识别飞机。

中央 ROS 程序需要同时发现两架飞机时，两套进程使用相同的 `ROS_DOMAIN_ID`。下面示例
让两套 MAVROS 运行在同一主机，所以本地 UDP 端口必须不同：

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

`ros2 topic list` 显示的是 DDS 发现结果，不代表获得了控制权。中央程序正是依靠同时
看到它们来做集群；命名空间保证命令不会串到另一架飞机。发布到
`/drone01/cmd_vel` 的命令不会进入 `/drone02/cmd_vel`。辅助命令必须显式选择目标：

```bash
ros2 run open32drone_driver control --robot-name drone01 status
ros2 run open32drone_driver control --robot-name drone02 takeoff --height 0.65
ros2 run open32drone_driver bench_test --robot-name drone01 --duration 5
```

只有两个实验完全不应互相发现时才使用不同 `ROS_DOMAIN_ID`。如果没有专门的 DDS/域桥，
不同域不适合单个中央集群控制器。多机常规隔离依靠命名空间，Domain ID 是额外的实验室边界。

如果每架飞机各用一台独立伴随计算机，它们可以都使用本机 UDP 端口 `14550`，因为不同
主机上的套接字不会冲突；飞机 IP、固件 `MAV_SYS_ID`、`robot_name` 和 TF 前缀仍必须
对应正确的飞机。以后增加 ROS 图像节点时也必须放在该飞机命名空间下，例如
`/drone01/camera/image_raw`。

后台进程管理也按飞机隔离：

```bash
ros2 run open32drone_driver system start \
  --robot-name drone01 --aircraft-ip 192.168.31.101 \
  --mav-sys-id 1 --local-udp-port 14551
ros2 run open32drone_driver system status --robot-name drone01
ros2 run open32drone_driver system stop --robot-name drone01
```

报告或停止已保存 PID 前，工具会核对当前进程命令是否仍属于该飞机命名空间。主机
重启后若旧 PID 被其他进程复用，工具会忽略旧记录，不会向无关进程发送信号。

## 6. 正常飞行流程

ROS 自动起飞默认进入定点。无需单独发送 `arm`：`takeoff` 由固件统一完成预检、
解锁、爬升和定点交接；结果不受命令前飞控显示的待机模式影响。

### 最小起飞—悬停—降落

飞机放在空旷安全区并有人监护：

```bash
ros2 run open32drone_driver control status
ros2 run open32drone_driver control takeoff --height 0.65
ros2 topic echo /open32drone/odom
ros2 run open32drone_driver control land
```

收到起飞成功结果后再发送移动命令。降落后确认 `armed: false` 和落地状态。

### 速度控制

辅助命令会进入 Offboard、持续发送设定值、结束命令并要求飞机捕获当前位置：

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

ROS 节点把水平速度向量总值限制为 `0.70 m/s`，与固件默认 `POS_STICK_V` 一致；固件端
仍会再次执行最终限幅。垂直速度保持 `0.35 m/s`，偏航角速度保持 `1.0 rad/s`。
超过 `0.50 s` 没有新命令时，看门狗捕获当前位置。

直接发布 `/open32drone/cmd_vel` 时必须连续发送，通常使用 20 Hz：

```bash
ros2 topic pub -r 20 /open32drone/cmd_vel geometry_msgs/msg/Twist \
  '{linear: {x: 0.20, y: 0.0, z: 0.0}, angular: {z: 0.0}}'
```

按 `Ctrl-C` 停止。不能只发一帧速度后期待它一直生效。

### 位置控制

```bash
ros2 run open32drone_driver control position 0.30 0.00 0.65
```

这里是当前 `open32drone/odom` 坐标系中的绝对坐标，不是相对飞机的位移，发送前先看
`/open32drone/odom`。
为保证教学测试安全，新水平目标被限制在当前位置 `0.80 m` 内，接近速度不超过
`0.15 m/s`。

## 7. 文本命令与服务

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

生命周期命令只发送一次，并等待对应结果。驱动不会靠重复起飞或降落来掩盖丢包或
固件拒绝。

## 8. 原始 RC 通道测试

这是协议测试接口，不是推荐的自主控制接口：

```bash
ros2 run open32drone_driver control rc \
  --roll 1023 --pitch 1023 --throttle 1100 --yaw 1023 --duration 1.0
```

数值使用 `[240, 1807]` 的 SBUS 范围。桥接节点换算为 MAVLink `MANUAL_CONTROL`，
要求数据持续新鲜；命令结束后显式停止数据流并回到定点。新鲜的物理 SBUS 操作优先，
此时 ROS RC 启动会被拒绝。物理遥控器急停可以作为有人监护时的独立安全手段，但不应
成为正常 ROS 流程的一部分。

查看物理遥控通道：

```bash
ros2 topic echo /open32drone/rc/in
ros2 topic echo /open32drone/rc/channels
```

## 9. RViz 与 TF

```bash
ros2 launch open32drone_driver open32drone.launch.py use_rviz:=true
```

默认固定坐标系是 `open32drone/odom`，桥接节点发布
`open32drone/odom -> open32drone/base_link`。多机启动时使用各自 `frame_prefix`，例如
`drone01/odom -> drone01/base_link`。默认 RViz 配置显示里程计、
位姿、TF 和向下距离。当前 ROS 包不转发实验性的 HTTP MJPEG，也不提供
`camera_info`。OpenCV 可以直接打开 `http://<飞机地址>/stream`，但固件只允许一个
图传观看端；ROS 拥有 MAVLink 控制时必须关闭 Android 控制端。

## 10. 验收脚本

正常验收时，飞机下方不要有人脚或移动物体，否则会改变 ToF 距离和光流观测。
刻意遮挡传感器的测试应单独记录，不用这类片段调整正常悬停参数。

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

默认 `--pattern hover` 只起飞、悬停、降落，不横移。开始悬停前，水平/高度误差必须
在 0.10 m 内，水平和垂直速度均不超过 0.08 m/s，并连续保持 1 秒。
随后才计入 `--hover` 时间；不是刚进入高度范围就宣布悬停。

位置到点测试（有人监护，留出移动空间）：

```bash
ros2 run open32drone_driver flight_test --pattern cross --height 0.65 --distance 0.4 --hover 5 --output flight-cross.json
```

流程：稳定起飞 → 悬停 → 前方 0.4 m → 回原点 → 左侧 0.4 m → 回原点 → 悬停 → 降落。
机头方向在初始悬停结束时固定为本轮参考，目标点使用固定里程计坐标，不跟着漂移移动。
每个点都要求“到位且停稳连续 1 秒”，再观察 1 秒才进入下一点；20 秒仍不到位就失败并
请求降落，不改目标、不调参数、不强行继续。`--height-tolerance` 是测试的 XY/Z 验收
误差（默认 0.10 m），不是飞控参数。`--distance` 范围 0.1–0.7 m；这不改变日常控制限制。
`--output` 保存阶段时间和位置原始样本；指定文件已存在时，起飞前直接报错，避免覆盖证据。

需要检查连续速度控制时，使用定时速度命令；它与位置到点测试不是同一项：

```bash
ros2 run open32drone_driver control velocity 0.15 0.00 0.00 --duration 10
```

该命令只在飞机已起飞后执行，按约 20 Hz 发速度，10 秒后发零速度；不保证刚好移动
1.5 m，也不保证已经刹停。Offboard 状态过期、失去激活状态或上锁会报失败，而不是仅凭
计时结束返回成功。ROS 收到 AUTO 的命令 ACK 后，必须再收到实际 AUTO 模式反馈才显示
ACTIVE。测试不重试起飞；失败后的降落请求也不会循环重发。上述数据只验证机载里程计
估计，不能替代外部定位精度测量。

## 11. 无响应时

1. 确认 `ping <飞机地址>` 成功，并核对启动参数 `aircraft_ip`；
2. ROS 需要接管该飞机时关闭 Android，并确认没有其他进程占用本次启动的
   `local_udp_port`；
3. 看 `control status`，不要把话题列表当作连接证据；
4. 查看 `/open32drone/command/result` 和 MAVROS `statustext` 中的预检拒绝原因；
5. ROS 需要控制时停止物理 SBUS 操作；
6. 修改固件参数前先阅读[故障排查](05-tuning.md)。
