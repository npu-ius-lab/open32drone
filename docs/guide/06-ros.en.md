# 05 · ROS 2 control

Once the aircraft can take off and land reliably, connect it to ROS 2 to read IMU, range, battery and odometry data and send takeoff, landing, velocity and position commands. This chapter starts with connection checks and a keyboard flight, then uses the flight-test program before moving on to velocity and position targets.

## 5.1 Prepare the ROS computer

Ubuntu 24.04 with ROS 2 Jazzy is recommended. Install the Desktop version using the [official ROS 2 instructions](https://docs.ros.org/en/jazzy/Installation/Ubuntu-Install-Debs.html), then install MAVROS and the build tools:

```bash
sudo apt update
sudo apt install ros-jazzy-mavros ros-jazzy-mavros-extras \
  python3-colcon-common-extensions python3-rosdep
sudo ros2 run mavros install_geographiclib_datasets.sh
```

Copy the ROS package from the repository into a workspace:

```bash
source /opt/ros/jazzy/setup.bash
mkdir -p ~/osdrone_ws/src
cp -a /path/to/osrdrone/ros2 ~/osdrone_ws/src/open32drone_driver
cd ~/osdrone_ws
rosdep install --from-paths src --ignore-src -r -y
colcon build --symlink-install
source install/setup.bash
```

Run these commands in each new terminal:

```bash
source /opt/ros/jazzy/setup.bash
source ~/osdrone_ws/install/setup.bash
```

## 5.2 Connect to the aircraft

Start with the [router Wi-Fi setup](./04-firmware-flight.en.md#router) so the aircraft and ROS 2 computer share a LAN. Read the DHCP address with the aircraft's serial `wifi` command and reserve it in the router. In the terminal where you will launch the driver, enter the address shown by `wifi` and test it:

```bash
printf 'Enter the aircraft IP shown by wifi: '
read -r AIRCRAFT_IP
ping -c 3 "$AIRCRAFT_IP"
```

Launch the full driver in the same terminal, passing this address as `aircraft_ip`:

```bash
ros2 launch open32drone_driver open32drone.launch.py \
  aircraft_ip:="$AIRCRAFT_IP"
```

For initial setup, or when a router is unavailable, connect the computer directly to the `open32drone` hotspot. In AP mode the aircraft address is fixed at `192.168.4.1`, so use the default launch command:

```bash
ros2 launch open32drone_driver open32drone.launch.py
```

Both network modes use MAVLink UDP `14550`. Close the Android control connection before using ROS. Both clients may join the same router, but they must not control the same aircraft simultaneously.

Check status in a second terminal:

```bash
ros2 run open32drone_driver control status
ros2 topic echo /open32drone/connected --once
```

Once `connected` is `true`, check live sensor data:

```bash
ros2 topic hz /open32drone/imu/data
ros2 topic echo /open32drone/range/downward --once
ros2 topic echo /open32drone/battery --once
ros2 topic echo /open32drone/odom --once
```

## 5.3 From state feedback to control

The aircraft sends its state to the computer, and the computer sends control targets back. The keyboard program and the topic commands below use the same driver and remain subject to the flight controller's takeoff, landing and protection logic.

[![ROS 2 data and control flow](/media/figures/ros-dataflow.en.svg)](/media/figures/ros-dataflow.en.svg)

| Question | Topics to check | Message type and meaning |
|---|---|---|
| Is the link alive? | `/open32drone/connected`, `/open32drone/state` | `Bool` heartbeat status; armed state and mode in `mavros_msgs/State` |
| Where is the aircraft, and which way is it pointing? | `/open32drone/pose`, `/open32drone/odom` | `PoseStamped` pose; `Odometry` pose and velocity |
| Are range and voltage updating? | `/open32drone/range/downward`, `/open32drone/battery` | `Range` downward ToF; `BatteryState` voltage |
| Was the takeoff request accepted? | `/open32drone/command/result`, `/open32drone/flight/status` | Command acknowledgement and subsequent flight state are separate stages |
| Does the computer have motion control? | `/open32drone/offboard/status`, `/open32drone/rc/status` | Offboard phase and ROS/physical-transmitter authority |
| Where should the aircraft move next? | `/open32drone/cmd_vel`, `/open32drone/goal_pose` | `Twist` body-frame velocity; `PoseStamped` absolute position in the odometry frame |

Do not mix the two coordinate frames. In `cmd_vel`, `x` points forward along the nose, `y` to the aircraft's left and `z` upward. `goal_pose` uses `open32drone/odom`, whose origin belongs to the current odometry estimate. Section 5.8 illustrates the difference. The TF tree connects `open32drone/odom → open32drone/base_link → open32drone/tof_link`. A topic appearing in the list only means ROS has discovered the interface. Before takeoff, confirm that state, position and range keep updating.

## 5.4 Fly with the keyboard

Complete the first flight with a transmitter or Android and check that the topics above update continuously before trying ROS control. Place the aircraft in the center of the flight area, power it on and wait for gyro calibration.

The keyboard program lets you take off, move and land one step at a time. It is a separate node and **does not** start automatically with the launch file. Close Android, other ROS control nodes and any terminal continuously publishing `cmd_vel`. Keep the driver in the first terminal. Monitor `/open32drone/state` and `/open32drone/offboard/status` in a second terminal, then run this in a third interactive terminal:

```bash
source ~/osdrone_ws/install/setup.bash
ros2 run open32drone_driver keyboard --robot-name open32drone --height 0.65
```

After each key press, check the message and aircraft state before continuing:

1. Press `t` on the ground. The program checks connection, position and ToF data, stops the ROS RC stream and sends one takeoff request. After acknowledgement, wait for the aircraft to climb, settle and return to position hold. Continue only when the program reports that takeoff is complete and position hold is active.
2. Press `o`. The program requests Offboard and waits for `phase=ACTIVE`. Movement keys do not take effect during warmup or mode switching. The request is rejected if the physical transmitter has control, state data is stale or takeoff is incomplete.
3. The default `v` mode controls velocity. Tap `w` once and observe a short forward movement along the nose, followed by a stop; then try `s/a/d` in turn. `i/k` control climb/descent and `j/l` left/right yaw. Each press sends a velocity pulse of about `0.30 s`, with defaults of `0.20 m/s` horizontally, `0.15 m/s` vertically and `0.40 rad/s` in yaw. The terminal does not need to report key release: the program sends zero velocity, and the existing watchdog captures the current position.
4. Press `p` for position mode. Each press of `w/s/a/d/i/k` generates a `0.20 m` step from the **latest received position and heading**; `j/l` do not command yaw in this mode. The program sends an absolute target in `open32drone/odom`. Repeated presses do not accumulate unfinished targets into a distant waypoint.
5. Press Space to stop new motion and hold the current position. To finish, press `g` to land and wait for disarmed and landed feedback, then `q` to exit. `q` only releases Offboard and exits; **it is not a landing key**.

The program rejects key actions when aircraft-state checks fail. Read the reason and inspect the status topics before proceeding. Do not open another `ros2 topic pub -r` stream alongside it. `--horizontal`, `--vertical`, `--yaw-rate`, `--step` and `--pulse` adjust the motion settings, subject to driver and firmware limits. Keep the defaults for the first exercise.

## 5.5 Verify a complete takeoff and landing in a program

Next, let the program carry out a takeoff and landing. First confirm that the aircraft is landed and disarmed and that the keyboard program has exited. Someone must remain present to observe the test:

```bash
ros2 run open32drone_driver flight_test --height 0.65 --hover 5
```

The script checks that read-only commands receive replies, then requests takeoff once. **A takeoff acknowledgement only means the flight controller accepted the command.** The script waits for position and velocity to remain stable before timing the 5-second hover. It also confirms touchdown and disarm after landing. Sending a command, receiving an acknowledgement and completing the action are checked separately.

After a single takeoff and landing passes, run the following test to move forward, return, move left and return, checking arrival from feedback:

```bash
ros2 run open32drone_driver flight_test --pattern cross \
  --height 0.65 --distance 0.4 --hover 5 --output flight-cross.json
```

The aircraft must settle at each target before the next step. If it does not arrive, the test reports failure and requests landing; it does not assume arrival after a fixed `sleep`. By default, horizontal and height errors must stay within `0.10 m`, and horizontal and vertical speeds within `0.08 m/s`, for 1 second. Each target has a 20-second timeout. `--output` saves phases and trajectory without overwriting an existing file.

The physical transmitter's emergency stop remains an independent way to stop the motors. ROS also provides `ros2 run open32drone_driver control emergency-stop`. Emergency stop does not descend normally and is only for situations where a safe landing is no longer possible. Ordinary `disarm` requires confirmed ground state and cannot replace `land`; a command ACK cannot replace actual disarmed feedback.

Yaw rate follows the ROS convention: positive `--yaw-rate` is counterclockwise, negative is clockwise. Run `offboard stop` before switching from Offboard to stick control. Keep feet and moving objects out from under the aircraft during flight; arrange sensor-occlusion experiments separately.

## 5.6 Send velocity commands directly

Pressing `w` in section 5.4 briefly sends body-frame `x` velocity, then returns it to zero. `control velocity` lets you specify the speed and duration directly before trying messages from your own ROS node.

Start this command-line exercise only after the previous flight has landed and the keyboard program has exited. After `control takeoff` receives its acknowledgement, check that `/state` has entered `CMODE(5)` or `POS_HOLD` and that height is stable:

```bash
ros2 run open32drone_driver control status
ros2 run open32drone_driver control takeoff --height 0.65
# Watch /open32drone/state and /open32drone/pose to confirm position hold and height.
```

`control velocity` prepares Offboard, waits for activation, streams setpoints for the requested duration and sends zero velocity at the end. Start with a small forward command, confirm that the aircraft stops, then move left:

```bash
ros2 run open32drone_driver control velocity 0.15 0.00 0.00 --duration 1.5
ros2 run open32drone_driver control velocity 0.00 0.15 0.00 --duration 1.5
```

Use `--yaw-rate` for yaw. Negative values command backward, right, down or right yaw respectively. Move one axis at a time in the first exercise. ROS limits total horizontal speed to `0.70 m/s`, vertical speed to `0.35 m/s` and yaw rate to `1.0 rad/s`; the firmware applies its final limits as well.

Four commands can form a small square. After each one, check that the aircraft has settled again. `0.15 × 1.5 ≈ 0.225 m` is only the ideal integrated distance; acceleration, deceleration and position capture affect the actual path:

```bash
ros2 run open32drone_driver control velocity  0.15  0.00 0.00 --duration 1.5
ros2 run open32drone_driver control velocity  0.00  0.15 0.00 --duration 1.5
ros2 run open32drone_driver control velocity -0.15  0.00 0.00 --duration 1.5
ros2 run open32drone_driver control velocity  0.00 -0.15 0.00 --duration 1.5
ros2 run open32drone_driver control land
```

Your own node should publish `geometry_msgs/msg/Twist` to `/open32drone/cmd_vel` continuously at about 20 Hz. **A single message does not sustain motion.** If publishing stops for more than `0.50 s`, the Offboard watchdog captures the current position. Use only one source at a time: keyboard pulses, timed `control velocity` commands or a continuous `Twist` publisher. The next section sends velocity directly through a topic.

## 5.7 From keyboard input to ROS topics

The keyboard's `t`, `o`, `w` and `g` actions request takeoff, obtain motion control, move forward and land. Exit the keyboard program and use the same ROS topics directly in a **new experiment**:

| Keyboard action | Topic | How to send |
|---|---|---|
| `t` takeoff, `o` enter Offboard, `g` land | `/open32drone/command` | Send each step once and wait for the actual state change |
| `w` move forward along the nose | `/open32drone/cmd_vel` | Publish velocity continuously; return to zero when stopping |
| A position step in `p` mode | `/open32drone/goal_pose` | Calculate an absolute target from the current pose first; covered in the next section |

`/open32drone/command` uses `std_msgs/msg/String`. Read `/open32drone/command/result` for acknowledgements, then `/state`, `/pose` and `/offboard/status` to confirm completion. Keep the driver terminal open and inspect these topics in another terminal:

```bash
# Monitor terminal: run each echo separately; press Ctrl-C before the next one.
ros2 topic echo /open32drone/command/result
ros2 topic echo /open32drone/state
ros2 topic echo /open32drone/offboard/status
ros2 topic echo /open32drone/pose
```

```bash
# Command terminal: run one line at a time and check the monitor after each step.
ros2 topic pub --once /open32drone/command std_msgs/msg/String '{data: "rc stop"}'
ros2 topic pub --once /open32drone/command std_msgs/msg/String '{data: "takeoff 0.65"}'
# Wait for position hold and stable height; do not send motion during AUTO takeoff.
ros2 topic pub --once /open32drone/command std_msgs/msg/String '{data: "offboard start"}'
# Wait for phase=ACTIVE on /offboard/status.
```

This `Twist` commands `0.10 m/s` along the nose. It publishes continuously until `Ctrl-C`; afterward, check that the aircraft captures its current position:

```bash
ros2 topic pub -r 20 /open32drone/cmd_vel geometry_msgs/msg/Twist \
  '{linear: {x: 0.10, y: 0.0, z: 0.0}, angular: {z: 0.0}}'
```

To finish, release Offboard first. Wait for `phase=IDLE` and a return to position hold before requesting landing. Finally, check `armed=false` in `/state` and `landed_state=1` in `/flight/status`.

```bash
ros2 topic pub --once /open32drone/command std_msgs/msg/String '{data: "offboard stop"}'
# Wait for phase=IDLE on /offboard/status and CMODE(5) or POS_HOLD on /state.
ros2 topic pub --once /open32drone/command std_msgs/msg/String '{data: "land"}'
```

Your own node must also handle these wait conditions, timeouts and failures. Confirm that an action has completed before starting the next one.

## 5.8 Position control: calculate a target from current position and heading

A velocity topic specifies how fast to move along the nose; a position topic specifies a coordinate in odometry. “Move forward `0.20 m`” therefore does not mean `goal_pose.x = 0.20`. The diagram shows two headings at the same position. Only the horizontal plane is shown; height stays at `0.65 m`:

![A 0.20 m forward step produces different absolute targets when the aircraft heading changes in the odometry frame](/media/figures/ros-position-frames.en.svg)

The grey axes are fixed in the current `open32drone/odom`, while the blue arrow always points along the nose. On the left, the nose points along `+X`: moving `0.20 m` from `(1.00, -0.40)` gives `(1.20, -0.40)`. On the right, after a `90°` left turn, the same forward step gives `(1.00, -0.20)`. Real odometry origins and headings depend on the current estimate. These numbers are calculation examples and **must not be copied directly as real-aircraft targets**.

Before a position experiment, confirm that the previous flight has landed and exit its control program. Take off again as in section 5.7, wait for position hold and `phase=ACTIVE`, then read **the latest odometry and heading from this flight**:

```bash
ros2 topic echo /open32drone/odom --once
```

For current position `(x_now, y_now, z_now)`, yaw `ψ` and a desired body-forward/left displacement `(dx, dy)`, calculate the **absolute odometry target**:

```text
x_goal = x_now + cos(ψ)·dx − sin(ψ)·dy
y_goal = y_now + sin(ψ)·dx + cos(ψ)·dy
z_goal = z_now                 # Keep the current height for horizontal motion
```

The orientation in `/odom` is a quaternion; your node must convert it to yaw. To understand the directions, compare the target printed by keyboard `p` mode in section 5.4 with the diagram.

Keyboard `p` mode uses this calculation: each key press starts from the latest pose rather than adding to an unfinished target. Send your calculated `(x_goal, y_goal, z_goal)` with `control position` or publish it to `/open32drone/goal_pose`. The values below **apply only to the example on the left**; replace them with the target calculated from your latest observations:

```bash
ros2 run open32drone_driver control position 1.20 -0.40 0.65

# Equivalent topic command; choose this OR the command above, not both.
ros2 topic pub --once /open32drone/goal_pose geometry_msgs/msg/PoseStamped \
  '{header: {frame_id: "open32drone/odom"}, pose: {position: {x: 1.20, y: -0.40, z: 0.65}, orientation: {w: 1.0}}}'
```

`frame_id` must be `open32drone/odom` or the driver rejects the target. The current driver uses only the position fields of `goal_pose`. `orientation.w: 1.0` supplies a valid quaternion; it **does not turn the nose toward world-frame `+X`**. The driver limits each horizontal target coordinate to within `0.80 m` of the current position and approach speed to `0.15 m/s`. Observe current feedback before sending another target. Use `flight_test --pattern cross` from section 5.5 to verify arrival and settling, rather than relying on a fixed wait.

After the position experiment, release Offboard, land and confirm disarm as in section 5.7. See [Parameters and interfaces](../reference/firmware.md) for message formats and firmware conventions.

## 5.9 Inspect flight state with RViz and rosbag

Launch with RViz:

```bash
ros2 launch open32drone_driver open32drone.launch.py use_rviz:=true
```

Record a ROS flight:

```bash
ros2 bag record \
  /open32drone/imu/data \
  /open32drone/range/downward \
  /open32drone/odom \
  /open32drone/battery \
  /open32drone/flight/status \
  /open32drone/offboard/status
```

During playback, compare commands with the actual trajectory and check height and voltage changes. [The next chapter](./07-rl.en.md) trains a reinforcement-learning policy in simulation to choose actions from the current state.
