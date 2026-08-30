# ROS 2 配套软件与自动飞行

[English](AUTOMATIC_FLIGHT_AND_ROS2.md) · [简体中文](AUTOMATIC_FLIGHT_AND_ROS2.zh-CN.md)

Open32Drone 的 ROS 2 包在 MAVROS 之上提供遥测、生命周期命令、速度/位置目标、原始 RC 测试、TF、RViz2 和验收工具。

ROS 2 不依赖 QGC。Android 和 ROS 2 可以连接同一局域网，但同一架飞机同一时刻只能由其中一个控制。物理 SBUS 的明确飞手动作具有普通接管优先级，最低油门加偏航左满的紧急上锁路径独立保留。

## 软件组成与版本关系

```text
open32drone_driver
├── MAVROS                    UDP 14550 与飞控通信
├── interface_bridge_node     Reliable 遥测、诊断和 TF
├── flight_manager_node       模式、起飞、降落和生命周期
├── offboard_control_node     速度/位置目标与看门狗
├── rc_bridge_node            显式启用的原始 RC 测试
├── control_cli               交互命令
├── bench_test                拆桨只读验收
└── flight_test               起飞—悬停—降落验收
```

## 1. 控制接口总览

| 接口 | 类型 | 含义 |
|---|---|---|
| `/open32drone/connected` | `std_msgs/Bool` | 实时 MAVLink 连接 |
| `/open32drone/state` | `mavros_msgs/State` | 模式、解锁与连接状态 |
| `/open32drone/imu/data` | `sensor_msgs/Imu` | Reliable 姿态与 IMU |
| `/open32drone/odom` | `nav_msgs/Odometry` | 机载相对位置与速度 |
| `/open32drone/range/downward` | `sensor_msgs/Range` | TF-0850 向下距离 |
| `/open32drone/battery` | `sensor_msgs/BatteryState` | 实测电压；电流与剩余比例未知 |
| `/open32drone/rc/channels` | `std_msgs/UInt16MultiArray` | 物理 SBUS 通道 |
| `/open32drone/cmd_vel` | `geometry_msgs/Twist` | 机体系速度，x 前、y 左、z 上 |
| `/open32drone/goal_pose` | `geometry_msgs/PoseStamped` | `odom` 中的绝对位置目标 |
| `/open32drone/command` | `std_msgs/String` | 单次生命周期命令 |

服务包括 `/open32drone/arm`、`disarm`、`takeoff`、`land` 和 `emergency_stop`。飞控端仍负责预检、传感器门控、超时、失联和电机输出；ROS 节点不是第二套飞控。

## 2. 构建和启动

```bash
mkdir -p ~/osdrone_ws/src
cp -a /path/to/osrdrone/ros2 ~/osdrone_ws/src/open32drone_driver
cd ~/osdrone_ws
rosdep install --from-paths src --ignore-src -r -y
colcon build --symlink-install
source install/setup.bash
```

飞机直连 AP 时：

```bash
ros2 launch open32drone_driver open32drone.launch.py
```

默认 MAVROS 地址为：

```text
udp://0.0.0.0:14550@192.168.4.1:14550
```

路由器 STA 模式需要传入飞机的 DHCP 地址：

```bash
ros2 launch open32drone_driver open32drone.launch.py aircraft_ip:=192.168.31.42
```

## 3. 飞行前连接与传感器检查

先关闭 Android 和其他 MAVLink 控制端，再检查真实数据，而不是只看话题名称：

```bash
ros2 run open32drone_driver control status
ros2 topic echo /open32drone/connected --once
ros2 topic hz /open32drone/imu/data
ros2 topic echo /open32drone/range/downward --once
ros2 run open32drone_driver bench_test --duration 5
```

`connected` 必须为 `true`，IMU 与 ToF 必须持续更新。第一次安装、改动协议或更换硬件后，台架检查必须拆桨。

## 4. 模式、起飞和降落

ROS 自动起飞默认交接到定点控制；无需先单独解锁，固件统一完成预检、解锁、爬升和模式交接：

```bash
ros2 run open32drone_driver control status
ros2 run open32drone_driver control takeoff --height 0.65
ros2 topic echo /open32drone/odom
ros2 run open32drone_driver control land
```

降落后必须确认 `armed: false`。空中不要用 `disarm` 代替降落；它会直接停桨。

## 5. 速度控制

辅助命令会取得 Offboard 控制权、持续发送设定值，并在结束时捕获当前位置：

```bash
ros2 run open32drone_driver control velocity 0.25 0.00 0.00 --duration 1.5
ros2 run open32drone_driver control velocity 0.00 0.25 0.00 --duration 1.5
ros2 run open32drone_driver control velocity 0.00 0.00 0.20 --duration 1.0
```

直接发布时需要连续刷新，通常使用 20 Hz：

```bash
ros2 topic pub -r 20 /open32drone/cmd_vel geometry_msgs/msg/Twist \
  '{linear: {x: 0.20, y: 0.0, z: 0.0}, angular: {z: 0.0}}'
```

超过 0.50 s 没有新命令时，看门狗捕获当前位置。水平速度向量由 ROS 节点和固件分别执行有界检查。

## 6. 位置控制

```bash
ros2 topic echo /open32drone/odom --once
ros2 run open32drone_driver control position 0.30 0.00 0.65
```

目标是当前 `open32drone/odom` 坐标系中的绝对位置，不是相对飞机的位移。教学接口把新水平目标限制在当前位置附近，并限制接近速度；发送前必须先读取当前里程计。

## 7. 原始 RC 通道测试

这是协议和映射测试接口，不是推荐的自主控制入口：

```bash
ros2 run open32drone_driver control rc \
  --roll 1023 --pitch 1023 --throttle 1100 --yaw 1023 --duration 1.0
```

通道使用 `[240, 1807]` 的 SBUS 范围。物理 SBUS 有新鲜明确输入时，ROS RC 请求会被拒绝。命令结束后桥接器停止 MANUAL_CONTROL 并交还定点。

## 8. 文本命令与服务

```bash
ros2 topic pub --once /open32drone/command std_msgs/msg/String '{data: "status"}'
ros2 topic pub --once /open32drone/command std_msgs/msg/String '{data: "takeoff 0.65"}'
ros2 topic echo /open32drone/command/result
ros2 topic pub --once /open32drone/command std_msgs/msg/String '{data: "land"}'
```

生命周期命令只发送一次，并检查对应 JSON 结果。不要用高频重复发布掩盖丢包或固件拒绝。

## 9. 验收脚本

拆桨台架：

```bash
ros2 run open32drone_driver bench_test --duration 5
```

有人监护的飞行验收：

```bash
ros2 run open32drone_driver flight_test --height 0.65 --hover 5
```

`flight_test` 只执行一次相对高度起飞、悬停观察和降落，不做横向移动、不修改参数、也不重试命令。位姿过期、持续下降、高度超差、未上锁或未确认落地都会使验收失败。

## 10. 多机、TF 与 RViz2

多机必须使用路由器 STA。每架飞机需要唯一的飞机 IP、`MAV_SYS_ID`、ROS `robot_name`/命名空间、主机 UDP 本地端口和 TF 前缀：

```bash
ros2 launch open32drone_driver open32drone.launch.py \
  robot_name:=drone01 frame_prefix:=drone01 \
  aircraft_ip:=192.168.31.101 mav_sys_id:=1 local_udp_port:=14551
```

第二架使用不同的地址、System ID、名称和本地端口。多机常规隔离依靠命名空间；需要中央控制器同时发现多架飞机时，应使用相同的 `ROS_DOMAIN_ID`。

```bash
ros2 launch open32drone_driver open32drone.launch.py use_rviz:=true
```

默认 TF 为 `open32drone/odom -> open32drone/base_link`；多机时前缀随 `frame_prefix` 改变。

## 11. 网络、相机和 OTA 边界

飞机默认建立 AP，也可由本地串口 `sta <ssid> <password>` 加入路由器；启动连接 8 s 失败后会开放恢复 AP。每架飞机的 MAVLink 回复会指向最近一个有效 UDP 发送端，因此 Android 与 ROS 2 不能同时控制同一架飞机。

当前 ROS 包不转发实验性 HTTP MJPEG，也不发布 `camera_info`。OpenCV 可直接打开 `http://<飞机地址>/stream`，但固件只允许一个观看端。图传不进入 300 Hz 飞控循环。

A/B OTA 只允许在上锁、落地且自动飞行和 Offboard 已停止时使用 `Open32Drone-minimal-app.bin`；不要把 merged 镜像上传到 OTA 接口。

## 12. 急停和故障处理

```bash
ros2 run open32drone_driver control emergency-stop
```

该命令会直接请求上锁，只用于翻覆、缠绕、明显失控或落地后电机仍高速旋转。它与普通控制共享 Wi-Fi/MAVLink，不能替代物理 SBUS 紧急上锁或电气独立停桨。

| 现象 | 检查 |
|---|---|
| `connected=false` | 飞机 IP、UDP 14550、本机端口占用、是否仍有 Android 控制端 |
| `cmd_vel` 无响应 | 飞机是否已起飞、里程计是否新鲜、Offboard 是否激活 |
| 位置目标拉回旧点 | 目标必须是当前 `odom` 中的新绝对坐标 |
| RC 请求被拒绝 | 停止物理 SBUS 操作并确认 RC 桥已显式启用 |
| RViz 无机体 | 固定坐标系和 `odom -> base_link` TF 是否一致 |
| HTTP 图传无画面 | 是否已有另一个观看端、飞机地址是否正确；ROS 不提供相机话题 |
