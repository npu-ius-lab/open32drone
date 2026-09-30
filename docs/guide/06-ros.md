# 05 · ROS 2 控制

飞机能稳定起降后，就可以接入 ROS 2，用程序读取 IMU、距离、电池和里程计数据，发送起飞、降落、速度和位置命令。这一章先检查连接，再用键盘飞一次；之后运行起降测试程序，学习直接发送速度和位置目标。

## 5.1 准备 ROS 电脑

推荐使用 Ubuntu 24.04 与 ROS 2 Jazzy。先按 [ROS 2 官方安装说明](https://docs.ros.org/en/jazzy/Installation/Ubuntu-Install-Debs.html)安装 Desktop 版本，再安装 MAVROS 和构建工具：

```bash
sudo apt update
sudo apt install ros-jazzy-mavros ros-jazzy-mavros-extras \
  python3-colcon-common-extensions python3-rosdep
sudo ros2 run mavros install_geographiclib_datasets.sh
```

把仓库中的 ROS 包放进工作空间：

```bash
source /opt/ros/jazzy/setup.bash
mkdir -p ~/osdrone_ws/src
cp -a /path/to/osrdrone/ros2 ~/osdrone_ws/src/open32drone_driver
cd ~/osdrone_ws
rosdep install --from-paths src --ignore-src -r -y
colcon build --symlink-install
source install/setup.bash
```

以后每次打开终端先执行：

```bash
source /opt/ros/jazzy/setup.bash
source ~/osdrone_ws/install/setup.bash
```

## 5.2 连接飞机

推荐先按[接入路由器 Wi-Fi](./04-firmware-flight.md#router)完成 STA 配置，
让飞机和 ROS 2 电脑处于同一个局域网。使用飞机串口的 `wifi` 命令读取 DHCP 地址，并在
路由器中设置 DHCP 地址保留。在准备启动驱动的终端输入 `wifi` 实际显示的飞机地址，先测试：

```bash
printf '请输入 wifi 命令显示的飞机 IP：'
read -r AIRCRAFT_IP
ping -c 3 "$AIRCRAFT_IP"
```

在同一个终端启动完整驱动，并把该地址传给 `aircraft_ip`：

```bash
ros2 launch open32drone_driver open32drone.launch.py \
  aircraft_ip:="$AIRCRAFT_IP"
```

首次配置或路由器不可用时，也可以让电脑直接连接飞机热点 `open32drone`。直连 AP 的飞机
地址固定为 `192.168.4.1`，此时使用默认启动命令：

```bash
ros2 launch open32drone_driver open32drone.launch.py
```

两种网络方式都使用 MAVLink UDP `14550`。开始 ROS 控制前关闭 Android 控制连接；两者
可以连接同一个路由器，但不能同时控制同一架飞机。

第二个终端查看状态：

```bash
ros2 run open32drone_driver control status
ros2 topic echo /open32drone/connected --once
```

`connected` 为 `true` 后，再看实时传感器：

```bash
ros2 topic hz /open32drone/imu/data
ros2 topic echo /open32drone/range/downward --once
ros2 topic echo /open32drone/battery --once
ros2 topic echo /open32drone/odom --once
```

## 5.3 从状态走到控制

连接后，飞机把状态发给电脑，电脑把控制目标发给飞机。键盘程序和后面的 ROS 话题命令使用同一套驱动，都要经过飞控的起降检查和保护逻辑。

[![ROS 2 数据与控制方向](/media/figures/ros-dataflow.svg)](/media/figures/ros-dataflow.svg)

| 要回答的问题 | 先看哪个话题 | 消息类型与含义 |
| --- | --- | --- |
| 链路还在吗？ | `/open32drone/connected`、`/open32drone/state` | `Bool` 心跳；`mavros_msgs/State` 的解锁和模式 |
| 飞机在哪、朝哪？ | `/open32drone/pose`、`/open32drone/odom` | `PoseStamped` 位姿；`Odometry` 位姿和速度 |
| 测距和电压是否更新？ | `/open32drone/range/downward`、`/open32drone/battery` | `Range` 向下 ToF；`BatteryState` 电压 |
| 起飞请求有没有被接受？ | `/open32drone/command/result`、`/open32drone/flight/status` | 命令应答与后续飞行状态，是两个阶段 |
| 电脑取得移动控制权了吗？ | `/open32drone/offboard/status`、`/open32drone/rc/status` | Offboard 阶段与 ROS/物理遥控器控制权 |
| 下一步往哪里动？ | `/open32drone/cmd_vel`、`/open32drone/goal_pose` | `Twist` 机体系速度；`PoseStamped` 里程计系绝对位置 |

这里有两个坐标系，不能混用。`cmd_vel` 的 `x` 指机头前方、`y` 指机体左侧、`z` 指上方；`goal_pose` 的坐标属于 `open32drone/odom`，原点由本次里程计决定。第 5.8 节会用图解释这一区别。TF 树把 `open32drone/odom → open32drone/base_link → open32drone/tof_link` 连起来。话题能在列表中出现，只能说明 ROS 发现了接口；起飞前仍要看到状态、位置和测距持续更新。

## 5.4 用键盘控制无人机

先用遥控器或 Android 完成首飞，并确认前面的话题持续更新，再尝试 ROS 控制。把飞机放在飞行区中央，上电，等陀螺校准完成。

键盘程序由你逐步操作起飞、移动和降落。它是独立节点，**不会** 随启动文件自动运行。先关闭 Android、其他 ROS 控制节点，以及持续发布 `cmd_vel` 的终端，保留第一个终端中的驱动。在第二个终端观察 `/open32drone/state` 和 `/open32drone/offboard/status`，第三个交互终端运行：

```bash
source ~/osdrone_ws/install/setup.bash
ros2 run open32drone_driver keyboard --robot-name open32drone --height 0.65
```

每按一步，先看提示和飞机状态，确认完成后再按下一个键：

1. 地面按 `t`。程序检查连接、位置与 ToF 数据，停止 ROS RC 流，只发送一次起飞。收到应答后，仍要等飞机爬升、稳定并回到定点；看到“起飞完成并进入定点”才继续。
2. 按 `o`。程序请求 Offboard 并等待 `phase=ACTIVE`；预热和模式切换期间，移动键不会提前生效。如果物理遥控器正在占用控制权、状态过期或起飞未完成，程序会拒绝。
3. 默认是 `v` 速度模式。轻按一次 `w`，观察机头方向的一小段前进和随后的停住；再依次试 `s/a/d`。`i/k` 是升降，`j/l` 是左/右偏航。每按一次只发送约 `0.30 s` 的速度脉冲，默认水平 `0.20 m/s`、垂直 `0.15 m/s`、偏航 `0.40 rad/s`；松键不需要终端报告，程序会发零速度并由现有看门狗捕获当前位置。
4. 按 `p` 切到位置模式。此时 `w/s/a/d/i/k` 每次以 **刚收到的当前位置和机头朝向** 生成一个 `0.20 m` 步进，`j/l` 不执行偏航。程序给出的是 `open32drone/odom` 中的绝对目标；反复按键不会把未到达的旧目标累加成远处航点。
5. 按空格停止新运动并保持当前位置。结束实验按 `g` 降落，看到上锁和落地反馈；再按 `q` 退出。`q` 只释放 Offboard、退出程序，**不是降落键**。

程序会检查飞机状态，条件不满足时会拒绝按键操作。遇到拒绝提示，先查看提示内容和上述状态话题，不要再开一个 `ros2 topic pub -r` 同时发送指令。`--horizontal`、`--vertical`、`--yaw-rate`、`--step` 和 `--pulse` 可以调整动作参数，实际值仍受驱动和飞控限幅；第一次练习保持默认值即可。

## 5.5 让程序验证完整起降

接下来让程序完成一次起降。先确认飞机已经落地、上锁，并退出键盘程序，再运行下面的命令。实验过程中仍需要有人在现场观察：

```bash
ros2 run open32drone_driver flight_test --height 0.65 --hover 5
```

脚本先检查只读命令能否收到回复，再请求一次起飞。**起飞应答只表示飞控接受了命令**，还要等位置和速度持续稳定，程序才开始计时悬停 5 秒。降落后也要继续确认接地和上锁，因此命令发出、收到应答和动作完成是分别检查的。

单次起降通过后，可以运行下面的测试，让飞机依次前进、回位、左移、回位，并根据反馈检查是否到达目标：

```bash
ros2 run open32drone_driver flight_test --pattern cross \
  --height 0.65 --distance 0.4 --hover 5 --output flight-cross.json
```

每到一个目标都要停稳，才会进入下一步。未能到位时，测试会报告失败并请求降落，不会只等一个固定的 `sleep` 时长就判定到达。默认要求水平和高度误差在 `0.10 m` 内、水平和垂直速度不超过 `0.08 m/s`，连续保持 1 秒；单点等待上限为 20 秒。`--output` 保存阶段和轨迹，不覆盖已有文件。

需要立即停桨时，物理遥控器急停仍是独立手段；ROS 也提供 `ros2 run open32drone_driver control emergency-stop`。急停不会执行正常下降，只用于已经无法安全降落的情况。普通 `disarm` 只允许在确认地面状态后执行，不能代替 `land`；命令 ACK 也不能代替实际的未武装反馈。

偏航角速度按 ROS 习惯：`--yaw-rate` 正值表示逆时针，负值表示顺时针。Offboard 控制结束后，先执行 `offboard stop`，再切换到摇杆控制。飞行时保持飞机下方没有人脚或移动物体，遮挡传感器的实验另行安排。

## 5.6 直接发送速度指令

第 5.4 节按 `w` 时，键盘程序会短暂发送机体系 `x` 方向的速度，然后归零。用 `control velocity` 可以直接指定速度和持续时间，之后再尝试从自己的 ROS 节点发布消息。

先确认上一轮实验已经落地、键盘程序已经退出，再开始这次命令行实验。`control takeoff` 收到起飞应答后，还要观察 `/state` 是否已进入 `CMODE(5)` 或 `POS_HOLD`，并确认高度稳定：

```bash
ros2 run open32drone_driver control status
ros2 run open32drone_driver control takeoff --height 0.65
# 观察 /open32drone/state 与 /open32drone/pose，确认定点和高度。
```

`control velocity` 会准备 Offboard、等待激活、按给定时长连续发送设定值，结束时发零速度。先执行一条较小的前进命令，确认停住，再执行左移：

```bash
ros2 run open32drone_driver control velocity 0.15 0.00 0.00 --duration 1.5
ros2 run open32drone_driver control velocity 0.00 0.15 0.00 --duration 1.5
```

偏航使用 `--yaw-rate`；负数分别表示后、右、下或右转。第一次练习每次只动一个轴。水平速度总值被 ROS 限制在 `0.70 m/s`，垂直速度不超过 `0.35 m/s`，偏航角速度不超过 `1.0 rad/s`，飞控还会做最终限幅。

四条命令可以组成一个小方形。每条之后先观察飞机是否重新停稳；`0.15 × 1.5 ≈ 0.225 m` 只是理想积分距离，实际轨迹会受加减速与位置捕获影响：

```bash
ros2 run open32drone_driver control velocity  0.15  0.00 0.00 --duration 1.5
ros2 run open32drone_driver control velocity  0.00  0.15 0.00 --duration 1.5
ros2 run open32drone_driver control velocity -0.15  0.00 0.00 --duration 1.5
ros2 run open32drone_driver control velocity  0.00 -0.15 0.00 --duration 1.5
ros2 run open32drone_driver control land
```

自己的 ROS 节点向 `/open32drone/cmd_vel` 发布速度时，使用 `geometry_msgs/msg/Twist` 消息，按约 20 Hz 连续发送；**只发一帧不会持续移动**。停止发布超过 `0.50 s`，Offboard 看门狗会捕获当前位置。键盘短脉冲、`control velocity` 定时命令和连续发布的 `Twist`，每次只能选一种来控制同一架飞机。下一节练习直接通过话题发送速度指令。

## 5.7 从键盘到 ROS 话题控制

前面按键盘的 `t`、`o`、`w` 和 `g`，分别完成起飞、取得移动控制权、向前移动和降落。现在退出键盘程序，在 **新的一次实验** 中直接使用相同的 ROS 话题：

| 键盘动作 | 对应话题 | 发送方式 |
| --- | --- | --- |
| `t` 起飞、`o` 进入 Offboard、`g` 降落 | `/open32drone/command` | 每步发送一次，等待实际状态变化 |
| `w` 向机头前方移动 | `/open32drone/cmd_vel` | 连续发布速度，停止后归零 |
| `p` 模式给出一个位置步进 | `/open32drone/goal_pose` | 先从当前位姿计算绝对目标；下一节再讲 |

`/open32drone/command` 使用 `std_msgs/msg/String`。观察 `/open32drone/command/result` 得到命令应答，再观察 `/state`、`/pose` 和 `/offboard/status` 确认动作完成。保留驱动终端，在另一个终端逐项观察：

```bash
# 观察终端：以下 echo 每次单独运行，Ctrl-C 后看下一项。
ros2 topic echo /open32drone/command/result
ros2 topic echo /open32drone/state
ros2 topic echo /open32drone/offboard/status
ros2 topic echo /open32drone/pose
```

```bash
# 发送终端：一条一条执行，每步核对观察终端。
ros2 topic pub --once /open32drone/command std_msgs/msg/String '{data: "rc stop"}'
ros2 topic pub --once /open32drone/command std_msgs/msg/String '{data: "takeoff 0.65"}'
# 等定点模式和高度稳定；不要在自动起飞的 AUTO 阶段抢先发移动指令。
ros2 topic pub --once /open32drone/command std_msgs/msg/String '{data: "offboard start"}'
# 等 /offboard/status 为 phase=ACTIVE。
```

下面用 `Twist` 发送沿机头方向 `0.10 m/s` 的速度。命令会持续发布消息，按 `Ctrl-C` 结束；结束后观察飞机是否捕获当前位置：

```bash
ros2 topic pub -r 20 /open32drone/cmd_vel geometry_msgs/msg/Twist \
  '{linear: {x: 0.10, y: 0.0, z: 0.0}, angular: {z: 0.0}}'
```

结束时先释放 Offboard，看到 `phase=IDLE` 且飞控回到定点，再请求降落；最终检查 `/state` 中 `armed=false` 和 `/flight/status` 中 `landed_state=1`。

```bash
ros2 topic pub --once /open32drone/command std_msgs/msg/String '{data: "offboard stop"}'
# 等 /offboard/status 为 phase=IDLE，/state 为 CMODE(5) 或 POS_HOLD。
ros2 topic pub --once /open32drone/command std_msgs/msg/String '{data: "land"}'
```

写自己的节点时，也要处理这些等待条件、超时和失败情况。确认飞机完成动作后，再进入下一步。

## 5.8 位置控制：从当前位置和朝向算目标

速度话题说的是“沿机头方向移动多快”，位置话题说的是“飞到里程计中的哪个坐标”。所以不能把“向前 `0.20 m`”直接写成 `goal_pose.x = 0.20`。先看同一位置的两种机头朝向；图中只画水平面，高度都保持 `0.65 m`：

![里程计坐标系中，机头朝向改变后，同样向前 0.20 米对应不同的绝对位置目标](/media/figures/ros-position-frames.svg)

图中灰色坐标轴固定在本次 `open32drone/odom`，蓝色箭头始终指向机头前方。左图机头朝 `+X`，从 `(1.00, -0.40)` 前进 `0.20 m` 得到 `(1.20, -0.40)`；右图机头左转 `90°` 后，同样前进 `0.20 m` 得到 `(1.00, -0.20)`。真实飞行的里程计原点和朝向由当次估计决定，图中数字只是计算示例，**不能直接复制为真机目标**。

做位置实验前，确认上一次已经落地，并退出控制程序。按第 5.7 节重新起飞，等待飞机进入定点且 `phase=ACTIVE`，然后读取**这次飞行最新的里程计和朝向**：

```bash
ros2 topic echo /open32drone/odom --once
```

设当前位置为 `(x_now, y_now, z_now)`、偏航角为 `ψ`，想沿机体前/左移动 `(dx, dy)`，把它转换成里程计中的**绝对目标**：

```text
x_goal = x_now + cos(ψ)·dx − sin(ψ)·dy
y_goal = y_now + sin(ψ)·dx + cos(ψ)·dy
z_goal = z_now                 # 水平移动时保持当前高度
```

`/odom` 的朝向字段是四元数；自己的节点需要从中换算偏航角。初次理解方向时，可回看第 5.4 节键盘 `p` 模式给出的目标，再与图中的方向关系核对。

键盘 `p` 模式也是这样计算的：每次按键都从最新位姿算出一个目标，不会在上一次尚未到达的目标上继续累加。把自己算出的 `(x_goal, y_goal, z_goal)` 用 `control position` 发送，或发布到 `/open32drone/goal_pose`。下面的数值**仅对应左图的计算示例**，实际实验必须替换为刚观测并计算出的目标：

```bash
ros2 run open32drone_driver control position 1.20 -0.40 0.65

# 等价的话题形式；与上一条二选一，不要同时发送。
ros2 topic pub --once /open32drone/goal_pose geometry_msgs/msg/PoseStamped \
  '{header: {frame_id: "open32drone/odom"}, pose: {position: {x: 1.20, y: -0.40, z: 0.65}, orientation: {w: 1.0}}}'
```

`frame_id` 必须是 `open32drone/odom`，否则驱动会拒绝。当前驱动只使用 `goal_pose` 的位置字段；示例里的 `orientation.w: 1.0` 是有效四元数的写法，**不会把机头转到世界系 `+X`**。驱动将目标的每个水平坐标限制在距当前位置 `0.80 m` 内，并将接近速度限制在 `0.15 m/s`。下一个目标应在观察当前反馈后再给；要证明“到达且停稳”，使用第 5.5 节的 `flight_test --pattern cross`，不要只靠固定等待时间。

位置实验结束后，按第 5.7 节释放 Offboard、降落并确认上锁。需要查阅消息格式和飞控约定时，见[参数与接口](../reference/firmware.zh-CN.md)。

## 5.9 用 RViz 和 rosbag 检查飞行状态

启动时打开 RViz：

```bash
ros2 launch open32drone_driver open32drone.launch.py use_rviz:=true
```

记录一次 ROS 飞行：

```bash
ros2 bag record \
  /open32drone/imu/data \
  /open32drone/range/downward \
  /open32drone/odom \
  /open32drone/battery \
  /open32drone/flight/status \
  /open32drone/offboard/status
```

回放时对照指令和实际轨迹，检查高度、电压的变化。[下一章的强化学习](./07-rl.md)会在仿真中训练策略，让它根据当前状态选择下一步动作。
