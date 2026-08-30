"""Safe MAVROS local-position/velocity control for Open32Drone."""

import math

import rclpy
from geometry_msgs.msg import PoseStamped, Twist, TwistStamped
from mavros_msgs.msg import PositionTarget, State
from mavros_msgs.srv import CommandLong
from rclpy.node import Node
from rclpy.qos import QoSProfile, ReliabilityPolicy, qos_profile_sensor_data
from std_msgs.msg import String
from std_srvs.srv import Trigger

from .control_math import bounded_xy_velocity, limit_xy_velocity

MAV_CMD_DO_SET_MODE = 176
MODE_AUTO = 3
MODE_POSITION_HOLD = 5


def _finite(values):
    return all(math.isfinite(value) for value in values)


def _clamp(value, lower, upper):
    return max(lower, min(upper, value))


class OffboardControl(Node):
    """Pre-stream setpoints, enter AUTO, and keep a bounded watchdog hold."""

    def __init__(self):
        super().__init__("open32drone_offboard_control")
        self.declare_parameter("publish_rate", 20.0)
        self.declare_parameter("warmup_time", 0.60)
        self.declare_parameter("command_timeout", 0.50)
        self.declare_parameter("pose_timeout", 0.50)
        self.declare_parameter("activation_timeout", 3.0)
        self.declare_parameter("auto_start_on_command", True)
        self.declare_parameter("max_horizontal_offset", 0.80)
        # Match the firmware POS_STICK_V default. The firmware repeats the same
        # vector bound and remains authoritative if either side is reconfigured.
        self.declare_parameter("max_horizontal_speed", 0.70)
        self.declare_parameter("max_position_speed", 0.15)
        self.declare_parameter("position_gain", 0.80)
        self.declare_parameter("position_deadband", 0.03)
        self.declare_parameter("max_vertical_speed", 0.35)
        self.declare_parameter("max_yaw_rate", 1.0)
        self.declare_parameter("min_altitude", 0.05)
        self.declare_parameter("max_altitude", 5.80)
        self.declare_parameter("odom_frame_id", "odom")
        self.declare_parameter("state_topic", "state")
        self.declare_parameter("pose_topic", "UAS1/local_position/pose")
        self.declare_parameter("setpoint_topic", "UAS1/setpoint_raw/local")
        self.declare_parameter("command_service", "UAS1/cmd/command")

        rate = float(self.get_parameter("publish_rate").value)
        if not math.isfinite(rate) or rate < 10.0 or rate > 50.0:
            raise ValueError("publish_rate must be within [10, 50] Hz")
        position_gain = float(self.get_parameter("position_gain").value)
        position_speed = float(self.get_parameter("max_position_speed").value)
        position_deadband = float(self.get_parameter("position_deadband").value)
        if (
            not _finite((position_gain, position_speed, position_deadband))
            or position_gain <= 0.0
            or position_speed <= 0.0
            or position_deadband < 0.0
        ):
            raise ValueError("position gain/speed must be positive and deadband non-negative")

        self.state = State()
        self.pose = None
        self.pose_received_at = 0.0
        self.phase = "IDLE"
        self.target_kind = "POSITION"
        self.position_goal = [0.0, 0.0, 0.0]
        self.velocity_target = [0.0, 0.0, 0.0]
        self.velocity_yaw_rate = 0.0
        self.command_received_at = 0.0
        self.operator_command_active = False
        self.warmup_started_at = 0.0
        self.activation_deadline = 0.0
        self.next_auto_request_at = 0.0
        self.mode_request_pending = False

        self.setpoint_publisher = self.create_publisher(
            PositionTarget, self.get_parameter("setpoint_topic").value, qos_profile_sensor_data
        )
        self.status_publisher = self.create_publisher(String, "offboard/status", 10)
        state_qos = QoSProfile(
            reliability=ReliabilityPolicy.RELIABLE,
            depth=1,
        )
        self.create_subscription(
            State, self.get_parameter("state_topic").value, self._state_callback, state_qos
        )
        self.create_subscription(
            PoseStamped,
            self.get_parameter("pose_topic").value,
            self._pose_callback,
            qos_profile_sensor_data,
        )
        self.create_subscription(
            PoseStamped,
            "offboard/position_target",
            self._position_callback,
            10,
        )
        self.create_subscription(
            TwistStamped,
            "offboard/velocity_target",
            self._velocity_callback,
            10,
        )
        self.create_subscription(Twist, "cmd_vel", self._cmd_vel_callback, 10)
        self.create_subscription(PoseStamped, "goal_pose", self._position_callback, 10)
        self.command_client = self.create_client(
            CommandLong, self.get_parameter("command_service").value
        )
        self.create_service(Trigger, "offboard/start", self._start)
        self.create_service(Trigger, "offboard/stop", self._stop)
        self.create_timer(1.0 / rate, self._control_tick)
        self.create_timer(0.2, self._publish_status)

    def _now(self):
        return self.get_clock().now().nanoseconds * 1e-9

    def _state_callback(self, message):
        self.state = message
        if self.phase != "IDLE" and (not message.connected or not message.armed):
            self.get_logger().warning("Offboard stopped because FCU disconnected or disarmed")
            self.phase = "IDLE"
            self.mode_request_pending = False

    def _pose_callback(self, message):
        values = (message.pose.position.x, message.pose.position.y, message.pose.position.z)
        if not _finite(values):
            return
        self.pose = message
        self.pose_received_at = self._now()

    def _position_callback(self, message):
        values = (message.pose.position.x, message.pose.position.y, message.pose.position.z)
        if not _finite(values) or not self._pose_fresh():
            return
        if self.phase != "ACTIVE":
            self._auto_start("position command")
            if self.phase == "IDLE":
                return
        current = self._current_position()
        offset = float(self.get_parameter("max_horizontal_offset").value)
        minimum = float(self.get_parameter("min_altitude").value)
        maximum = float(self.get_parameter("max_altitude").value)
        self.position_goal = [
            _clamp(values[0], current[0] - offset, current[0] + offset),
            _clamp(values[1], current[1] - offset, current[1] + offset),
            _clamp(values[2], minimum, maximum),
        ]
        self.target_kind = "POSITION"
        self.command_received_at = self._now()
        self.operator_command_active = True

    def _velocity_callback(self, message):
        values = (message.twist.linear.x, message.twist.linear.y, message.twist.linear.z)
        self._accept_velocity(values, message.twist.angular.z)

    def _cmd_vel_callback(self, message):
        values = (message.linear.x, message.linear.y, message.linear.z, message.angular.z)
        if not _finite(values) or not self._pose_fresh():
            return
        if self.phase != "ACTIVE":
            if any(abs(value) > 1e-6 for value in values):
                self._auto_start("cmd_vel")
            if self.phase not in ("PREPARING", "WARMUP"):
                return
        orientation = self.pose.pose.orientation
        yaw = math.atan2(
            2.0 * (orientation.w * orientation.z + orientation.x * orientation.y),
            1.0 - 2.0 * (orientation.y * orientation.y + orientation.z * orientation.z),
        )
        cos_yaw = math.cos(yaw)
        sin_yaw = math.sin(yaw)
        local_velocity = (
            cos_yaw * message.linear.x - sin_yaw * message.linear.y,
            sin_yaw * message.linear.x + cos_yaw * message.linear.y,
            message.linear.z,
        )
        self._accept_velocity(local_velocity, message.angular.z)

    def _accept_velocity(self, values, yaw_rate):
        if not _finite((*values, yaw_rate)):
            return
        if self.phase != "ACTIVE":
            if any(abs(value) > 1e-6 for value in (*values, yaw_rate)):
                self._auto_start("velocity command")
            if self.phase not in ("PREPARING", "WARMUP"):
                return
        horizontal = float(self.get_parameter("max_horizontal_speed").value)
        vertical = float(self.get_parameter("max_vertical_speed").value)
        max_yaw_rate = float(self.get_parameter("max_yaw_rate").value)
        velocity_x, velocity_y = limit_xy_velocity(values[0], values[1], horizontal)
        self.velocity_target = [
            velocity_x,
            velocity_y,
            _clamp(values[2], -vertical, vertical),
        ]
        # ROS uses positive angular.z for counter-clockwise yaw in ENU.  The
        # firmware consumes MAVLink LOCAL_NED and performs its own NED-to-body
        # sign conversion, so the raw MAVROS yaw-rate field must be reversed
        # here to preserve the ROS convention observed at the aircraft.
        self.velocity_yaw_rate = _clamp(-yaw_rate, -max_yaw_rate, max_yaw_rate)
        self.target_kind = "VELOCITY"
        self.command_received_at = self._now()
        self.operator_command_active = True

    def _pose_fresh(self):
        timeout = float(self.get_parameter("pose_timeout").value)
        return self.pose is not None and self._now() - self.pose_received_at <= timeout

    def _current_position(self):
        return [
            float(self.pose.pose.position.x),
            float(self.pose.pose.position.y),
            float(self.pose.pose.position.z),
        ]

    def _capture_hold(self, reason):
        if not self._pose_fresh():
            return False
        self.position_goal = self._current_position()
        self.velocity_target = [0.0, 0.0, 0.0]
        self.velocity_yaw_rate = 0.0
        self.target_kind = "POSITION"
        self.command_received_at = self._now()
        self.operator_command_active = False
        self.get_logger().warning(f"Offboard watchdog holding current position: {reason}")
        return True

    def _begin_start(self):
        if self.phase in ("PREPARING", "WARMUP", "ACTIVE"):
            return True, f"offboard already {self.phase.lower()}"
        if self.phase != "IDLE":
            return False, f"offboard is {self.phase.lower()}"
        if not self.state.connected or not self.state.armed:
            return False, "FCU must be connected and armed"
        if not self._pose_fresh():
            return False, "fresh local position is required"
        if not self.command_client.service_is_ready():
            return False, f"MAVROS service unavailable: {self.command_client.srv_name}"

        self.position_goal = self._current_position()
        self.velocity_target = [0.0, 0.0, 0.0]
        self.velocity_yaw_rate = 0.0
        self.target_kind = "POSITION"
        self.command_received_at = self._now()
        self.operator_command_active = False
        self.phase = "PREPARING"
        self._request_mode(MODE_POSITION_HOLD, "prepare")
        return True, "Position hold requested; offboard warmup will start after acknowledgement"

    def _auto_start(self, source):
        if self.phase != "IDLE" or not bool(
            self.get_parameter("auto_start_on_command").value
        ):
            return
        success, message = self._begin_start()
        if success:
            self.get_logger().info(f"Auto-started Offboard from {source}")
        else:
            self.get_logger().warning(f"Ignored {source}: {message}")

    def _start(self, _request, response):
        response.success, response.message = self._begin_start()
        return response

    def _stop(self, _request, response):
        if self.phase == "IDLE":
            response.success = True
            response.message = "offboard already stopped"
            return response
        if self.mode_request_pending or not self.command_client.service_is_ready():
            response.success = False
            response.message = "mode command is busy or unavailable"
            return response
        self.phase = "STOPPING"
        self._request_mode(MODE_POSITION_HOLD, "stop")
        response.success = True
        response.message = "Position hold requested; setpoint stream remains active until acknowledgement"
        return response

    def _request_mode(self, mode, purpose):
        request = CommandLong.Request()
        request.broadcast = False
        request.command = MAV_CMD_DO_SET_MODE
        request.confirmation = 0
        request.param2 = float(mode)
        self.mode_request_pending = True
        future = self.command_client.call_async(request)
        future.add_done_callback(lambda result: self._mode_response(result, purpose))

    def _mode_response(self, future, purpose):
        self.mode_request_pending = False
        try:
            result = future.result()
            accepted = bool(result.success)
            result_code = result.result
        except Exception as error:  # ROS service transport failure
            accepted = False
            result_code = str(error)

        now = self._now()
        if purpose == "prepare":
            if not accepted:
                self.phase = "IDLE"
                self.get_logger().error(f"Position preparation rejected: {result_code}")
                return
            self.phase = "WARMUP"
            self.warmup_started_at = now
            self.activation_deadline = now + float(self.get_parameter("activation_timeout").value)
            self.next_auto_request_at = now + float(self.get_parameter("warmup_time").value)
            return

        if purpose == "activate":
            if accepted:
                self.phase = "ACTIVE"
                self.get_logger().info("Offboard AUTO accepted; setpoint stream is active")
            elif now < self.activation_deadline:
                self.phase = "WARMUP"
                self.next_auto_request_at = now + 0.25
                self.get_logger().warning(f"AUTO not ready, continuing warmup: {result_code}")
            else:
                self.phase = "IDLE"
                self.get_logger().error(f"Offboard activation timed out: {result_code}")
            return

        if purpose in ("stop", "watchdog"):
            if accepted:
                self.phase = "IDLE"
                self.get_logger().info("Offboard released to Position Hold")
            else:
                # Stopping the stream invokes the firmware's 300 ms fallback.
                self.phase = "IDLE"
                self.get_logger().error(f"Position fallback rejected; firmware watchdog owns recovery: {result_code}")

    def _make_setpoint(self):
        message = PositionTarget()
        message.header.stamp = self.get_clock().now().to_msg()
        message.header.frame_id = str(self.get_parameter("odom_frame_id").value)
        message.coordinate_frame = PositionTarget.FRAME_LOCAL_NED
        ignore_acceleration = (
            PositionTarget.IGNORE_AFX | PositionTarget.IGNORE_AFY | PositionTarget.IGNORE_AFZ
        )
        ignore_heading = PositionTarget.IGNORE_YAW | PositionTarget.IGNORE_YAW_RATE

        if self.target_kind == "VELOCITY":
            message.type_mask = (
                PositionTarget.IGNORE_PX
                | PositionTarget.IGNORE_PY
                | PositionTarget.IGNORE_PZ
                | ignore_acceleration
                | PositionTarget.IGNORE_YAW
            )
            message.velocity.x, message.velocity.y, message.velocity.z = self.velocity_target
            message.yaw_rate = self.velocity_yaw_rate
        else:
            message.type_mask = (
                PositionTarget.IGNORE_PX
                | PositionTarget.IGNORE_PY
                | PositionTarget.IGNORE_VZ
                | ignore_acceleration
                | ignore_heading
            )
            # Use one horizontal controller only. ROS closes the absolute ENU
            # position loop and sends bounded ENU velocity; the firmware keeps
            # its existing velocity/attitude loop and altitude hold. Combining
            # PX/PY and VX/VY made two horizontal loops act on the same error.
            current = self._current_position()
            velocity_x, velocity_y = bounded_xy_velocity(
                self.position_goal[0] - current[0],
                self.position_goal[1] - current[1],
                float(self.get_parameter("position_gain").value),
                float(self.get_parameter("max_position_speed").value),
                float(self.get_parameter("position_deadband").value),
            )
            message.velocity.x = velocity_x
            message.velocity.y = velocity_y
            message.position.z = self.position_goal[2]
        return message

    def _control_tick(self):
        now = self._now()
        if self.phase in ("IDLE", "PREPARING"):
            return
        if not self.state.connected or not self.state.armed:
            self.phase = "IDLE"
            return
        if not self._pose_fresh():
            self.get_logger().error("Local position timed out; stopping stream for firmware fallback")
            self.phase = "IDLE"
            return

        # Until an operator command arrives, refresh the warmup target from the
        # latest XY feedback so switching to AUTO cannot pull toward an old point.
        # Preserve the altitude captured at start: following a takeoff overshoot
        # during warmup would otherwise turn that transient into the new target.
        if self.phase == "WARMUP" and not self.operator_command_active:
            current = self._current_position()
            self.position_goal[0:2] = current[0:2]

        command_timeout = float(self.get_parameter("command_timeout").value)
        if (
            self.operator_command_active
            and self.target_kind == "VELOCITY"
            and now - self.command_received_at > command_timeout
        ):
            self._capture_hold("command input timed out")

        self.setpoint_publisher.publish(self._make_setpoint())

        if (
            self.phase == "WARMUP"
            and not self.mode_request_pending
            and now >= self.next_auto_request_at
        ):
            if now >= self.activation_deadline:
                self.phase = "IDLE"
                self.get_logger().error("Offboard activation deadline expired")
            elif self.command_client.service_is_ready():
                self._request_mode(MODE_AUTO, "activate")

    def _publish_status(self):
        message = String()
        age = self._now() - self.command_received_at if self.command_received_at else -1.0
        message.data = (
            f"phase={self.phase} target={self.target_kind} connected={self.state.connected} "
            f"armed={self.state.armed} command_age={age:.2f}s pose_fresh={self._pose_fresh()} "
            f"goal=({self.position_goal[0]:.2f},{self.position_goal[1]:.2f},"
            f"{self.position_goal[2]:.2f})"
        )
        self.status_publisher.publish(message)


def main(args=None):
    rclpy.init(args=args)
    node = OffboardControl()
    executor = rclpy.executors.MultiThreadedExecutor(num_threads=2)
    executor.add_node(node)
    try:
        executor.spin()
    except KeyboardInterrupt:
        pass
    finally:
        executor.shutdown()
        node.destroy_node()
        rclpy.try_shutdown()


if __name__ == "__main__":
    main()
