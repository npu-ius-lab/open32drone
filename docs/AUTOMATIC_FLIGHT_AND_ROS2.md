# ROS 2 Companion Software and Automatic Flight

[English](AUTOMATIC_FLIGHT_AND_ROS2.md) · [简体中文](AUTOMATIC_FLIGHT_AND_ROS2.zh-CN.md)

The Open32Drone ROS 2 package provides telemetry, lifecycle commands, velocity and position targets, raw-RC testing, TF, RViz2, and acceptance tools above MAVROS.

ROS 2 does not depend on QGC. Android and ROS 2 may share a LAN, but only one may control an aircraft at a time. Explicit physical-SBUS input retains ordinary takeover priority, and the low-throttle/full-left-yaw emergency-disarm path remains independent of software ownership.

## Software Composition and Version Relationship

```text
open32drone_driver
├── MAVROS                    UDP 14550 flight-controller transport
├── interface_bridge_node     Reliable telemetry, diagnostics, and TF
├── flight_manager_node       Modes, takeoff, landing, and lifecycle
├── offboard_control_node     Velocity/position targets and watchdog
├── rc_bridge_node            Explicit raw-RC test stream
├── control_cli               Interactive commands
├── bench_test                Propeller-off read-only acceptance
└── flight_test               Takeoff-hover-land acceptance
```

## 1. Interface Summary

| Interface | Type | Meaning |
|---|---|---|
| `/open32drone/connected` | `std_msgs/Bool` | Live MAVLink connection |
| `/open32drone/state` | `mavros_msgs/State` | Mode, arm, and connection state |
| `/open32drone/imu/data` | `sensor_msgs/Imu` | Reliable attitude and IMU |
| `/open32drone/odom` | `nav_msgs/Odometry` | Onboard relative position and velocity |
| `/open32drone/range/downward` | `sensor_msgs/Range` | Downward TF-0850 range |
| `/open32drone/battery` | `sensor_msgs/BatteryState` | Measured voltage; current and percentage remain unknown |
| `/open32drone/rc/channels` | `std_msgs/UInt16MultiArray` | Physical SBUS channels |
| `/open32drone/cmd_vel` | `geometry_msgs/Twist` | Body velocity: x forward, y left, z up |
| `/open32drone/goal_pose` | `geometry_msgs/PoseStamped` | Absolute target in `odom` |
| `/open32drone/command` | `std_msgs/String` | One-shot lifecycle command |

Services include `/open32drone/arm`, `disarm`, `takeoff`, `land`, and `emergency_stop`. The firmware remains responsible for preflight checks, sensor gates, timeouts, link loss, and motor output; the ROS nodes are not a second flight controller.

## 2. Build and Start

```bash
mkdir -p ~/osdrone_ws/src
cp -a /path/to/osrdrone/ros2 ~/osdrone_ws/src/open32drone_driver
cd ~/osdrone_ws
rosdep install --from-paths src --ignore-src -r -y
colcon build --symlink-install
source install/setup.bash
```

For the direct aircraft AP:

```bash
ros2 launch open32drone_driver open32drone.launch.py
```

The default MAVROS URL is:

```text
udp://0.0.0.0:14550@192.168.4.1:14550
```

In router STA mode, pass the aircraft DHCP address:

```bash
ros2 launch open32drone_driver open32drone.launch.py aircraft_ip:=192.168.31.42
```

## 3. Connection and Sensor Preflight

Close Android and other MAVLink controllers, then inspect live data rather than only topic names:

```bash
ros2 run open32drone_driver control status
ros2 topic echo /open32drone/connected --once
ros2 topic hz /open32drone/imu/data
ros2 topic echo /open32drone/range/downward --once
ros2 run open32drone_driver bench_test --duration 5
```

`connected` must be `true`, and IMU and ToF must update continuously. Run the first installation, protocol change, or hardware-replacement bench check with propellers removed.

## 4. Modes, Takeoff, and Landing

ROS takeoff hands over to position hold by default. A separate arm command is unnecessary because the firmware performs preflight, arming, climb, and mode handover:

```bash
ros2 run open32drone_driver control status
ros2 run open32drone_driver control takeoff --height 0.65
ros2 topic echo /open32drone/odom
ros2 run open32drone_driver control land
```

Confirm `armed: false` after landing. Do not substitute `disarm` for landing in the air; it stops the motors immediately.

## 5. Velocity Control

The helper acquires Offboard ownership, continuously streams the target, and captures position at the end:

```bash
ros2 run open32drone_driver control velocity 0.25 0.00 0.00 --duration 1.5
ros2 run open32drone_driver control velocity 0.00 0.25 0.00 --duration 1.5
ros2 run open32drone_driver control velocity 0.00 0.00 0.20 --duration 1.0
```

Direct publication must remain continuous, normally at 20 Hz:

```bash
ros2 topic pub -r 20 /open32drone/cmd_vel geometry_msgs/msg/Twist \
  '{linear: {x: 0.20, y: 0.0, z: 0.0}, angular: {z: 0.0}}'
```

After 0.50 s without a fresh command, the watchdog captures the current position. ROS and firmware independently bound the horizontal velocity vector.

## 6. Position Control

```bash
ros2 topic echo /open32drone/odom --once
ros2 run open32drone_driver control position 0.30 0.00 0.65
```

The target is an absolute coordinate in the current `open32drone/odom` frame, not a displacement relative to the aircraft. The teaching interface limits new horizontal targets near the current position and limits approach speed; read odometry before sending a target.

## 7. Raw RC Channel Testing

This is a protocol and mapping test, not the preferred autonomy interface:

```bash
ros2 run open32drone_driver control rc \
  --roll 1023 --pitch 1023 --throttle 1100 --yaw 1023 --duration 1.0
```

Channels use the SBUS range `[240, 1807]`. Fresh explicit physical-SBUS input causes the ROS RC request to be rejected. At completion, the bridge stops MANUAL_CONTROL and returns ownership to position hold.

## 8. Text Commands and Services

```bash
ros2 topic pub --once /open32drone/command std_msgs/msg/String '{data: "status"}'
ros2 topic pub --once /open32drone/command std_msgs/msg/String '{data: "takeoff 0.65"}'
ros2 topic echo /open32drone/command/result
ros2 topic pub --once /open32drone/command std_msgs/msg/String '{data: "land"}'
```

Send lifecycle commands once and inspect the matching JSON result. Do not use high-rate repetition to hide packet loss or a firmware rejection.

## 9. Acceptance Scripts

Propeller-off bench acceptance:

```bash
ros2 run open32drone_driver bench_test --duration 5
```

Supervised flight acceptance:

```bash
ros2 run open32drone_driver flight_test --height 0.65 --hover 5
```

`flight_test` performs one relative-height takeoff, observes hover, and lands. It does not move laterally, modify parameters, or retry commands. Stale pose, sustained descent, excessive height error, failure to disarm, or missing landed confirmation fails the test.

## 10. Multi-Aircraft Operation, TF, and RViz2

Multi-aircraft operation requires router STA. Each aircraft needs a unique aircraft IP, `MAV_SYS_ID`, ROS `robot_name`/namespace, host-local UDP port, and TF prefix:

```bash
ros2 launch open32drone_driver open32drone.launch.py \
  robot_name:=drone01 frame_prefix:=drone01 \
  aircraft_ip:=192.168.31.101 mav_sys_id:=1 local_udp_port:=14551
```

Use different addresses, System IDs, names, and local ports for the second aircraft. Normal multi-aircraft isolation uses namespaces; a central controller that must discover all aircraft should use the same `ROS_DOMAIN_ID`.

```bash
ros2 launch open32drone_driver open32drone.launch.py use_rviz:=true
```

The default TF is `open32drone/odom -> open32drone/base_link`; multi-aircraft frames follow `frame_prefix`.

## 11. Network, Camera, and OTA Boundaries

The aircraft starts its AP by default or can join a router with the local serial command `sta <ssid> <password>`. If startup connection fails for 8 s, the firmware opens a recovery AP. MAVLink replies follow the latest valid UDP sender, so Android and ROS 2 must not control the same aircraft concurrently.

The current ROS package does not relay experimental HTTP MJPEG or publish `camera_info`. OpenCV may open `http://<aircraft-address>/stream` directly, but the firmware permits one viewer. Streaming does not enter the 300 Hz flight loop.

A/B OTA is allowed only while disarmed and landed with automatic flight and Offboard stopped. Upload `Open32Drone-minimal-app.bin`; never send the merged image to the OTA endpoint.

## 12. Emergency Stop and Troubleshooting

```bash
ros2 run open32drone_driver control emergency-stop
```

This command requests immediate disarm and is reserved for tip-over, entanglement, clear loss of control, or motors spinning rapidly after landing. It shares Wi-Fi/MAVLink with normal control and cannot replace the physical-SBUS emergency gesture or an electrically independent stop.

| Symptom | Check |
|---|---|
| `connected=false` | Aircraft IP, UDP 14550, local-port conflicts, and whether Android is still active |
| No `cmd_vel` response | Aircraft airborne, odometry fresh, and Offboard active |
| Position target returns to an old point | Send a new absolute coordinate in the current `odom` frame |
| RC request rejected | Stop physical-SBUS input and explicitly enable the RC bridge |
| No aircraft in RViz | Fixed frame and `odom -> base_link` TF |
| No HTTP video | Another viewer, aircraft address; ROS provides no camera topic |
