"""Small command-line surface for Open32Drone lifecycle and motion control."""

import argparse
import json
import math
import sys
import time

import rclpy
from geometry_msgs.msg import PoseStamped, Twist
from mavros_msgs.msg import OverrideRCIn
from rclpy.node import Node
from rclpy.qos import HistoryPolicy, QoSProfile, ReliabilityPolicy
from std_msgs.msg import String

from .names import DEFAULT_ROBOT_NAME, frame_prefix, robot_name


def _finite(values):
    return all(math.isfinite(value) for value in values)


class ControlCLI(Node):
    def __init__(self, namespace=DEFAULT_ROBOT_NAME, tf_prefix=None):
        namespace = robot_name(namespace)
        super().__init__("open32drone_control_cli", namespace=namespace)
        self.odom_frame_id = f"{frame_prefix(tf_prefix, namespace)}/odom"
        reliable = QoSProfile(
            reliability=ReliabilityPolicy.RELIABLE,
            history=HistoryPolicy.KEEP_LAST,
            depth=10,
        )
        self.command_result = None
        self.status = {}
        self.command_publisher = self.create_publisher(
            String, "command", reliable
        )
        self.velocity_publisher = self.create_publisher(Twist, "cmd_vel", reliable)
        self.position_publisher = self.create_publisher(PoseStamped, "goal_pose", reliable)
        self.rc_publisher = self.create_publisher(
            OverrideRCIn, "rc/override", reliable
        )
        self.command_result_subscription = self.create_subscription(
            String, "command/result", self._command_result, reliable
        )
        for name, topic in {
            "flight": "flight/status",
            "offboard": "offboard/status",
            "rc": "rc/status",
            "interface": "interface/status",
        }.items():
            self.create_subscription(
                String, topic, lambda message, key=name: self._status(key, message), reliable
            )

    def _command_result(self, message):
        try:
            self.command_result = json.loads(message.data)
        except json.JSONDecodeError:
            self.command_result = {
                "command": "",
                "success": False,
                "message": f"invalid result: {message.data}",
            }

    def _status(self, name, message):
        self.status[name] = message.data

    def command(self, text, timeout=10.0):
        self.command_result = None
        message = String(data=text)
        self.wait_for(
            lambda: self.command_publisher.get_subscription_count() > 0
            and self.count_publishers("command/result") > 0,
            min(timeout, 3.0),
            "command bridge discovery",
        )
        # Lifecycle commands are deliberately published exactly once. Repeating
        # arm/takeoff/land to compensate for discovery would be unsafe.
        self.command_publisher.publish(message)
        deadline = time.monotonic() + timeout
        while rclpy.ok() and time.monotonic() < deadline:
            rclpy.spin_once(self, timeout_sec=0.05)
            result = self.command_result
            if result is not None and result.get("command") == text:
                print(json.dumps(result, ensure_ascii=False))
                return 0 if result.get("success") else 1
        print(f"command timeout: {text}", file=sys.stderr)
        return 1

    def wait_for(self, predicate, timeout, description):
        deadline = time.monotonic() + timeout
        while rclpy.ok() and time.monotonic() < deadline:
            rclpy.spin_once(self, timeout_sec=0.05)
            if predicate():
                return
        raise RuntimeError(f"timeout waiting for {description}")

    def prepare_offboard(self):
        if self.command("rc stop") != 0:
            return False
        if self.command("offboard start") != 0:
            return False
        self.wait_for(
            lambda: "phase=ACTIVE" in self.status.get("offboard", ""),
            6.0,
            "Offboard ACTIVE",
        )
        return True

    def prepare_rc(self):
        if self.command("offboard stop") != 0:
            return False
        self.wait_for(
            lambda: "phase=IDLE" in self.status.get("offboard", ""),
            6.0,
            "Offboard IDLE",
        )
        return self.command("rc start") == 0

    def publish_repeated(self, publisher, message, duration, stamp_header=False):
        deadline = time.monotonic() + duration
        while rclpy.ok() and time.monotonic() < deadline:
            if stamp_header:
                message.header.stamp = self.get_clock().now().to_msg()
            publisher.publish(message)
            rclpy.spin_once(self, timeout_sec=0.05)

    def velocity(self, x, y, z, yaw_rate, duration):
        if not _finite((x, y, z, yaw_rate, duration)) or duration <= 0.0:
            raise ValueError("velocity values must be finite and duration must be positive")
        message = Twist()
        message.linear.x = x
        message.linear.y = y
        message.linear.z = z
        message.angular.z = yaw_rate
        self.publish_repeated(self.velocity_publisher, message, duration)
        self.publish_repeated(self.velocity_publisher, Twist(), 0.5)

    def position(self, x, y, z):
        if not _finite((x, y, z)):
            raise ValueError("position values must be finite")
        message = PoseStamped()
        message.header.frame_id = self.odom_frame_id
        message.pose.position.x = x
        message.pose.position.y = y
        message.pose.position.z = z
        message.pose.orientation.w = 1.0
        self.publish_repeated(self.position_publisher, message, 1.0, stamp_header=True)

    def rc(self, roll, pitch, throttle, yaw, duration, center_after):
        channels = (roll, pitch, throttle, yaw)
        if any(value < 240 or value > 1807 for value in channels):
            raise ValueError("RC channels must be within [240, 1807]")
        if not math.isfinite(duration) or duration <= 0.0:
            raise ValueError("duration must be positive")
        message = OverrideRCIn()
        message.channels = [OverrideRCIn.CHAN_NOCHANGE] * 18
        message.channels[0:4] = channels
        self.publish_repeated(self.rc_publisher, message, duration)
        if center_after:
            centered = OverrideRCIn()
            centered.channels = [OverrideRCIn.CHAN_NOCHANGE] * 18
            # Center attitude/yaw only. A non-centering throttle must retain the
            # commanded value instead of receiving an unintended half-throttle step.
            centered.channels[0:4] = (1023, 1023, throttle, 1023)
            self.publish_repeated(self.rc_publisher, centered, 0.5)


def parse_args(args=None):
    parser = argparse.ArgumentParser(
        description="Open32Drone simplified lifecycle and motion control"
    )
    parser.add_argument("--robot-name", default=DEFAULT_ROBOT_NAME)
    parser.add_argument(
        "--frame-prefix",
        default=None,
        help="TF prefix (defaults to --robot-name)",
    )
    sub = parser.add_subparsers(dest="action", required=True)
    for action in ("status", "arm", "disarm", "land", "emergency-stop"):
        sub.add_parser(action)
    takeoff = sub.add_parser("takeoff")
    takeoff.add_argument("--height", type=float, default=0.6)
    mode = sub.add_parser("mode")
    mode.add_argument("name", choices=("stabilize", "altitude", "position"))
    offboard = sub.add_parser("offboard")
    offboard.add_argument("state", choices=("start", "stop"))
    rc_stream = sub.add_parser("rc-stream")
    rc_stream.add_argument("state", choices=("start", "stop"))
    velocity = sub.add_parser("velocity")
    velocity.add_argument("x", type=float, help="forward m/s")
    velocity.add_argument("y", type=float, help="left m/s")
    velocity.add_argument("z", type=float, help="up m/s")
    velocity.add_argument("--yaw-rate", type=float, default=0.0)
    velocity.add_argument("--duration", type=float, default=1.0)
    position = sub.add_parser("position")
    position.add_argument("x", type=float)
    position.add_argument("y", type=float)
    position.add_argument("z", type=float)
    rc = sub.add_parser("rc")
    rc.add_argument("--roll", type=int, default=1023)
    rc.add_argument("--pitch", type=int, default=1023)
    rc.add_argument("--throttle", type=int, default=1023)
    rc.add_argument("--yaw", type=int, default=1023)
    rc.add_argument("--duration", type=float, default=1.0)
    rc.add_argument("--no-center-after", action="store_true")
    return parser.parse_args(args)


def main(args=None):
    parsed = parse_args(args)
    rclpy.init(args=[])
    node = ControlCLI(parsed.robot_name, parsed.frame_prefix)
    result = 0
    try:
        if parsed.action == "status":
            status_names = ("interface", "flight", "offboard", "rc")

            def publishers_ready():
                return all(name in node.status for name in status_names)

            def live_state_ready():
                return (
                    publishers_ready()
                    and "state_age=-1.00s" not in node.status["interface"]
                )
            try:
                node.wait_for(live_state_ready, 3.0, "live status")
            except RuntimeError:
                # A disconnected FCU legitimately has no live state. Still show
                # the four driver statuses, but never print transient unavailable
                # entries merely because DDS discovery was still in progress.
                if not publishers_ready():
                    raise
            for name in status_names:
                print(f"{name}: {node.status.get(name, 'unavailable')}")
        elif parsed.action in ("arm", "disarm", "land"):
            result = node.command(parsed.action)
        elif parsed.action == "emergency-stop":
            result = node.command("emergency_stop")
        elif parsed.action == "takeoff":
            result = node.command(f"takeoff {parsed.height:g}")
        elif parsed.action == "mode":
            result = node.command(f"mode {parsed.name}")
        elif parsed.action == "offboard":
            result = node.command(f"offboard {parsed.state}")
        elif parsed.action == "rc-stream":
            result = node.command(f"rc {parsed.state}")
        elif parsed.action == "velocity":
            if node.prepare_offboard():
                node.velocity(parsed.x, parsed.y, parsed.z, parsed.yaw_rate, parsed.duration)
            else:
                result = 1
        elif parsed.action == "position":
            if node.prepare_offboard():
                node.position(parsed.x, parsed.y, parsed.z)
            else:
                result = 1
        elif parsed.action == "rc":
            if node.prepare_rc():
                node.rc(
                    parsed.roll,
                    parsed.pitch,
                    parsed.throttle,
                    parsed.yaw,
                    parsed.duration,
                    not parsed.no_center_after,
                )
                # A finite RC command is a robot-style motion primitive, not a
                # request to simulate link loss. End its stream explicitly and
                # hand the aircraft back to Position Hold before the firmware's
                # 500 ms MANUAL_CONTROL timeout can start a failsafe descent.
                if node.command("rc stop") != 0:
                    result = 1
                elif node.command("mode position") != 0:
                    result = 1
            else:
                result = 1
    except (RuntimeError, ValueError, KeyboardInterrupt) as error:
        print(str(error), file=sys.stderr)
        result = 1
    finally:
        node.destroy_node()
        rclpy.try_shutdown()
    return result


if __name__ == "__main__":
    raise SystemExit(main())
