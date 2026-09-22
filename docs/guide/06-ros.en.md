# 06 · ROS 2 control

The Open32Drone ROS 2 package exposes a small robot-style interface on top of
MAVROS. It is intended for teaching, experiments, and later multi-aircraft
coordination. It does not require QGC. MAVROS instances on one host use unique
local UDP bind ports. Android and ROS may be on the same LAN, but only one
controller may own one aircraft at a time.

The ROS package manifest uses version `0.1.0`.
Open32Drone firmware, Android, and ROS 2 should be built from the same
source revision.

**Acknowledgement and motion state:** command success means a real FCU
acknowledgement. Offboard moves only after AUTO mode is confirmed.
`fresh local position is required` means ROS position feedback is absent or older
than 0.5 s, not necessarily that the onboard ToF has failed. A warmup rejection
retries within the activation deadline; if activation still fails, do not keep
issuing motion commands: land and capture the launch log plus `offboard/status`,
`UAS1/local_position/pose` and `UAS1/setpoint_raw/local` under your aircraft namespace.
Do not increase watchdog timeouts or tune PID to hide a missing command stream.

To prepare an offline simulation model, use
[URDF / USD model export](07-rl.en.md). The connection and control
steps below operate a real aircraft; the model guide does not add a simulator
backend to this ROS package.

For the first ROS session, do only these five things:

1. Pass an ordinary SBUS or Android first flight, then close Android.
2. Connect the ROS computer to the aircraft AP, or to the same router as an
   aircraft configured for STA.
3. Install with section 2 and launch from one terminal with section 3.
4. Confirm `/open32drone/connected` is `true` from another terminal.
5. Treat sections 4–5 as reference for now; skip to section 6 and run one
   takeoff, hover, and landing.

Do not read the section 5 multi-aircraft setup until the single-aircraft path
passes. Do not run Android, ROS, and another MAVLink controller together.

## 1. Requirements

- ROS 2 with `colcon` and MAVROS installed;
- the host connected either to the aircraft AP or to the same trusted router as
  an aircraft configured for STA;
- the selected aircraft IPv4 address reachable from the ROS host;
- Android and other MAVLink clients closed;
- propellers removed for installation and bench checks.

Verify the network before starting ROS:

```bash
ping -c 3 192.168.4.1  # replace with the STA address when using a router
```

## 2. Install

Use either the repository `software/ros2/` directory or the matching ROS 2 source
archive from the same build set:

```bash
mkdir -p ~/osdrone_ws/src
cp -a /path/to/open32drone/software/ros2 ~/osdrone_ws/src/open32drone_driver
cd ~/osdrone_ws
rosdep install --from-paths src --ignore-src -r -y
colcon build --symlink-install
source install/setup.bash
```

Source the workspace in every new terminal:

```bash
source ~/osdrone_ws/install/setup.bash
```

### Minimal ROS development loop

Application nodes should use this package's `/open32drone/cmd_vel`, odometry,
command topic, or services. Do not bypass the driver by duplicating the MAVLink
takeoff/landing state machine. With the copy-based install above, edit the
source under `~/osdrone_ws/src/open32drone_driver/`, then run:

```bash
cd ~/osdrone_ws
colcon build --symlink-install --packages-select open32drone_driver
source install/setup.bash
```

Put a new Python node in the workspace source's `open32drone_driver/` Python
package and register its entry point in `setup.py`; put new launch arguments in
`launch/`. After section 3 is running, use a second terminal to run
`ros2 run open32drone_driver bench_test --duration 5` without propellers, then
perform the single takeoff/landing check in section 6. Only a shared MAVLink
protocol change requires matching firmware, Android, and protocol contract
updates. See [Development](../reference/source-build.md) for the complete build and review
rules.

## 3. Launch and prove the link

`connected=true` proves heartbeat reception only; check that IMU, odometry and
range continue updating before takeoff. Do not add a global `name="mavros"` to
`mavros_node` in a custom launch: it also renames internal plugins, breaking
topic paths and plugin configuration. The supplied launch maps ToF output to
`UAS1/distance_sensor/tof` for the bridge's `range/downward` interface.

Takeoff commands first check a read-only `status` round trip. If it times out,
resolve the command path before retrying takeoff. When recording a ROS bag,
stop the recorder cleanly before reading its SQLite database; use live ROS
subscriptions for in-flight monitoring instead of querying the active database.

Start the complete control stack in direct-AP mode:

```bash
ros2 launch open32drone_driver open32drone.launch.py
```

The default MAVROS endpoint is:

```text
udp://0.0.0.0:14550@192.168.4.1:14550
```

For router STA mode, use the DHCP address printed by the firmware `wifi`
command:

```bash
ros2 launch open32drone_driver open32drone.launch.py \
  aircraft_ip:=192.168.31.42
```

The lower-level `fcu_url:=...` override remains available for advanced MAVROS
routing. Prefer a router DHCP reservation so each aircraft keeps a predictable
address. Android and ROS can be on the same LAN, but only one may own MAVLink
control at a time.

In a second terminal, wait for a live FCU rather than relying on the topic list:

```bash
source ~/osdrone_ws/install/setup.bash
ros2 run open32drone_driver control status
ros2 topic echo /open32drone/connected --once
ros2 topic hz /open32drone/imu/data
ros2 topic echo /open32drone/range/downward --once
```

`/open32drone/connected` must be `true`, IMU data must update, and the downward
range topic must contain fresh TF-0850 packets before a flight test. Merely
seeing topic names does not prove that MAVLink is connected.

## 4. Public interface

### Telemetry

| Topic | Type | Meaning |
|---|---|---|
| `/open32drone/connected` | `std_msgs/Bool` | live heartbeat state |
| `/open32drone/state` | `mavros_msgs/State` | connected, armed, and mode |
| `/open32drone/imu/data` | `sensor_msgs/Imu` | attitude and filtered IMU data |
| `/open32drone/imu/data_raw` | `sensor_msgs/Imu` | raw MAVROS IMU surface |
| `/open32drone/odom` | `nav_msgs/Odometry` | local position and velocity |
| `/open32drone/pose` | `geometry_msgs/PoseStamped` | local pose |
| `/open32drone/range/downward` | `sensor_msgs/Range` | downward TF-0850 range |
| `/open32drone/battery` | `sensor_msgs/BatteryState` | measured voltage; current and percentage remain unknown; assisted-thrust compensation is firmware-owned |
| `/open32drone/rc/in` | `mavros_msgs/RCIn` | received physical SBUS channels |
| `/open32drone/rc/channels` | `std_msgs/UInt16MultiArray` | the same RC values as a simple array |
| `/open32drone/diagnostics` | `diagnostic_msgs/DiagnosticArray` | connection diagnostic |
| `/tf` | TF | `open32drone/odom -> open32drone/base_link` |

The bridge republishes the important sensor topics with reliable QoS so that
RViz and `rqt` can subscribe without MAVROS sensor-QoS warnings.

### Commands

| Interface | Meaning |
|---|---|
| `/open32drone/command` | one-shot lifecycle text command |
| `/open32drone/command/result` | JSON result for the exact command |
| `/open32drone/cmd_vel` | body-frame velocity: `x` forward, `y` left, `z` up |
| `/open32drone/goal_pose` | absolute position target in `open32drone/odom` |
| `/open32drone/rc/override` | SBUS-style raw-channel test input |

Short services are also available:

```text
/open32drone/arm  /open32drone/disarm  /open32drone/takeoff
/open32drone/land  /open32drone/emergency_stop
```

The `/open32drone/takeoff` service uses the `flight_manager.takeoff_height` parameter. Use
the CLI or text topic when an explicit height is required.

## 5. Advanced: multiple aircraft on one LAN

Multi-aircraft operation requires router STA mode. The default direct-AP address
is the same on every aircraft and cannot identify several aircraft on one LAN.
Public topics, services, MAVROS internals, and TF frames are isolated by the
following values:

| Setting | Aircraft 1 | Aircraft 2 | Why |
|---|---:|---:|---|
| aircraft STA address | `192.168.31.101` | `192.168.31.102` | selects the physical aircraft |
| firmware `MAV_SYS_ID` | `1` | `2` | identifies the MAVLink system |
| `robot_name` / TF prefix | `drone01` | `drone02` | isolates ROS names and frames |
| ROS-host UDP bind port | `14551` | `14552` | prevents conflicts when both MAVROS instances run on one host |

Set the system ID once on each aircraft through its local serial CLI, then
reboot and verify it with `p MAV_SYS_ID`:

```text
p MAV_SYS_ID 1
```

Use a different value on the second aircraft. Reserve each STA address in the
router; do not identify an aircraft by a DHCP address that may move.

Launch both stacks in the same `ROS_DOMAIN_ID` when a central program should
discover both aircraft. This example runs both MAVROS instances on one host, so
their local UDP bind ports are different:

```bash
export ROS_DOMAIN_ID=32

# Terminal 1
ros2 launch open32drone_driver open32drone.launch.py \
  robot_name:=drone01 frame_prefix:=drone01 \
  aircraft_ip:=192.168.31.101 mav_sys_id:=1 local_udp_port:=14551

# Terminal 2
ros2 launch open32drone_driver open32drone.launch.py \
  robot_name:=drone02 frame_prefix:=drone02 \
  aircraft_ip:=192.168.31.102 mav_sys_id:=2 local_udp_port:=14552
```

Export the same Domain ID in every terminal and central-control process that
should see this swarm.

The graph intentionally shows both sets of topics on every ROS computer in that
domain:

```text
/drone01/state       /drone02/state
/drone01/cmd_vel     /drone02/cmd_vel
/drone01/odom        /drone02/odom
```

`ros2 topic list` reports DDS discovery, not control ownership. Seeing both is
how one ROS computer can coordinate a swarm; the namespaces prevent commands
from crossing. A publisher on `/drone01/cmd_vel` cannot reach
`/drone02/cmd_vel`. Select one explicitly in helper commands:

```bash
ros2 run open32drone_driver control --robot-name drone01 status
ros2 run open32drone_driver control --robot-name drone02 takeoff --height 0.65
ros2 run open32drone_driver bench_test --robot-name drone01 --duration 5
```

Use a different `ROS_DOMAIN_ID` only when two experiments must not discover
each other at all. Separate domain IDs are not suitable for one central swarm
controller unless it runs a deliberate DDS/domain bridge. Namespaces are the
normal multi-aircraft mechanism; domain IDs are an additional lab boundary.

When every aircraft has a separate companion computer, those computers may
reuse local UDP port `14550` because sockets on different hosts do not conflict.
The aircraft IP address, firmware `MAV_SYS_ID`, `robot_name`, and TF prefix must
still identify the correct aircraft. A future ROS camera publisher must also
remain under the selected namespace, for example `/drone01/camera/image_raw`.

The background process helper is also per aircraft:

```bash
ros2 run open32drone_driver system start \
  --robot-name drone01 --aircraft-ip 192.168.31.101 \
  --mav-sys-id 1 --local-udp-port 14551
ros2 run open32drone_driver system status --robot-name drone01
ros2 run open32drone_driver system stop --robot-name drone01
```

Before reporting or stopping a saved PID, the helper verifies that the live
process command still belongs to that aircraft namespace. A stale PID file
after a host reboot is ignored rather than signalling an unrelated process.

## 6. Normal flight workflow

Use Position Hold for automatic ROS takeoff. A separate `arm` command is not
required: `takeoff` performs the firmware pre-arm checks, arms, climbs to the
relative target, and hands over to Position Hold. This result is independent
of the FCU standby mode reported before the request.

### Minimal takeoff-hover-land

With the aircraft in a clear supervised area:

```bash
ros2 run open32drone_driver control status
ros2 run open32drone_driver control takeoff --height 0.65
ros2 topic echo /open32drone/odom
ros2 run open32drone_driver control land
```

Wait for the takeoff result before sending motion. After landing, confirm
`armed: false` and on-ground state.

### Velocity control

The helper enters Offboard, streams setpoints, stops the command, and asks the
aircraft to hold its current position:

```bash
# Forward, back, left, right; 0.25 m/s for 1.5 s each.
ros2 run open32drone_driver control velocity  0.25  0.00 0.00 --duration 1.5
ros2 run open32drone_driver control velocity -0.25  0.00 0.00 --duration 1.5
ros2 run open32drone_driver control velocity  0.00  0.25 0.00 --duration 1.5
ros2 run open32drone_driver control velocity  0.00 -0.25 0.00 --duration 1.5

# Up, down, then rotate.
ros2 run open32drone_driver control velocity 0 0  0.20 --duration 1.0
ros2 run open32drone_driver control velocity 0 0 -0.20 --duration 1.0
ros2 run open32drone_driver control velocity 0 0 0 --yaw-rate 0.50 --duration 2.0
```

The ROS node bounds the total horizontal velocity vector to `0.70 m/s`, matching
the firmware `POS_STICK_V` default; the firmware applies the same final bound
again. Vertical speed remains limited to `0.35 m/s` and yaw rate to
`1.0 rad/s`. A `0.50 s` command watchdog captures the current position if
setpoints stop.

Direct `/open32drone/cmd_vel` publishers must run continuously, normally at 20 Hz:

```bash
ros2 topic pub -r 20 /open32drone/cmd_vel geometry_msgs/msg/Twist \
  '{linear: {x: 0.20, y: 0.0, z: 0.0}, angular: {z: 0.0}}'
```

Press `Ctrl-C` to stop. Do not publish a single velocity sample and assume it
will remain active.

### Position control

```bash
ros2 run open32drone_driver control position 0.30 0.00 0.65
```

The coordinates are absolute in the current `open32drone/odom` frame, not relative to the
aircraft. Inspect `/open32drone/odom` first. For safety, the node limits a new horizontal
goal to `0.80 m` from the current position and moves toward it at no more than
`0.15 m/s`.

## 7. Direct lifecycle topic and services

Text commands are useful in teaching scripts:

```bash
ros2 topic pub --once /open32drone/command std_msgs/msg/String \
  '{data: "takeoff 0.65"}'
ros2 topic echo /open32drone/command/result
ros2 topic pub --once /open32drone/command std_msgs/msg/String '{data: "land"}'
```

Supported text commands are:

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

Equivalent short service examples:

```bash
ros2 service call /open32drone/takeoff std_srvs/srv/Trigger '{}'
ros2 service call /open32drone/land std_srvs/srv/Trigger '{}'
ros2 service call /open32drone/emergency_stop std_srvs/srv/Trigger '{}'
```

Lifecycle commands are sent exactly once and wait for the corresponding
result. The driver does not repeat a failed takeoff or landing to hide a packet
loss or firmware rejection.

## 8. Raw RC-style test path

This is a protocol test path, not the recommended autonomous API:

```bash
ros2 run open32drone_driver control rc \
  --roll 1023 --pitch 1023 --throttle 1100 --yaw 1023 --duration 1.0
```

Values use the configured SBUS range `[240, 1807]`. The bridge converts them to
MAVLink `MANUAL_CONTROL`, requires fresh data, then explicitly stops the stream
and returns to Position Hold. A fresh physical SBUS pilot has priority and
causes ROS RC startup to be rejected. Use a physical RC emergency-disarm
gesture as an independent supervised stop path, not as part of normal ROS
control.

Observe physical channels with:

```bash
ros2 topic echo /open32drone/rc/in
ros2 topic echo /open32drone/rc/channels
```

## 9. RViz and TF

```bash
ros2 launch open32drone_driver open32drone.launch.py use_rviz:=true
```

The default fixed frame is `open32drone/odom`; the bridge publishes
`open32drone/odom -> open32drone/base_link`. A multi-aircraft launch uses its
`frame_prefix` instead, for example `drone01/odom -> drone01/base_link`. The
default RViz configuration displays odometry, pose, TF, and downward range.
The current ROS package does not republish the experimental HTTP MJPEG stream
or provide `camera_info`. An OpenCV process may open
`http://<aircraft-ip>/stream` directly, but the firmware accepts only one video
viewer and the Android controller must be closed while ROS owns MAVLink.

## 10. Acceptance tests

Keep the floor below the aircraft clear of feet and moving objects: these can
change ToF distance and optical-flow observations. Record intentional obstruction
tests separately; do not use those segments to tune ordinary hover behavior.

Propellers removed:

```bash
ros2 run open32drone_driver bench_test --duration 5
```

Add `--require-rc` only when a calibrated physical receiver is intentionally
part of the test. Add `--require-battery` only when voltage sensing hardware is
configured.

Guarded flight:

```bash
ros2 run open32drone_driver flight_test --height 0.65 --hover 5
```

The default `--pattern hover` does not move laterally. Before hover timing starts,
XY and height error must be within 0.10 m and both horizontal and vertical speed
must be at most 0.08 m/s continuously for one second. Merely crossing the height
band is not arrival.

For a supervised position-arrival test with clear movement space:

```bash
ros2 run open32drone_driver flight_test --pattern cross --height 0.65 --distance 0.4 --hover 5 --output flight-cross.json
```

Sequence: settled takeoff, hover, 0.4 m forward, home, 0.4 m left, home, hover,
land. Heading is captured after initial hover; waypoints remain fixed in odometry
coordinates instead of moving with drift. Each point must be reached and stopped
for one continuous second, followed by one second of observation. A 20-second
arrival timeout fails the test and requests landing; it never moves the goal,
tunes parameters or skips to the next point. `--height-tolerance` is the test's
XY/Z acceptance tolerance (default 0.10 m), not a firmware parameter. Test distance
is 0.1–0.7 m; this does not change ordinary control limits. `--output` stores phase
timestamps and position samples, and refuses an existing file before takeoff.

Keep the original timed-velocity reproduction separate:

```bash
ros2 run open32drone_driver control velocity 0.15 0.00 0.00 --duration 10
```

Run it only after takeoff. It sends velocity at about 20 Hz, then zero velocity;
it does not guarantee 1.5 m travel or a completed stop. Stale/inactive Offboard
status or disarm now fails the command instead of silently succeeding after the
timer. AUTO command acknowledgement must be followed by actual AUTO mode feedback
before Offboard becomes ACTIVE. Tests never retry takeoff or loop landing requests.
Position results use onboard odometry, not independent external position truth.

## 11. If it does not respond

1. Confirm `ping <aircraft-ip>` works and that the launch `aircraft_ip` matches.
2. Close Android when ROS should own this aircraft; ensure no other process uses
   this launch's `local_udp_port`.
3. Check `control status`; do not use the topic list as connection proof.
4. Read `/open32drone/command/result` and MAVROS `statustext` for the rejected
   pre-arm gate.
5. Stop physical SBUS input if ROS should own control.
6. Use [Troubleshooting](05-tuning.en.md) before changing firmware
   parameters.
