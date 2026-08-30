"""One supervised takeoff, hover, and landing acceptance flight."""

import argparse
import json
import math
import time

import rclpy
from mavros_msgs.msg import ExtendedState, State
from nav_msgs.msg import Odometry
from rclpy.node import Node
from rclpy.qos import (
    HistoryPolicy,
    QoSProfile,
    ReliabilityPolicy,
    qos_profile_sensor_data,
)
from std_msgs.msg import String

from .names import DEFAULT_ROBOT_NAME, robot_name


class FlightTest(Node):
    def __init__(self, namespace=DEFAULT_ROBOT_NAME):
        super().__init__(
            "open32drone_flight_test", namespace=robot_name(namespace)
        )
        reliable = QoSProfile(
            reliability=ReliabilityPolicy.RELIABLE,
            history=HistoryPolicy.KEEP_LAST,
            depth=10,
        )
        self.state = State()
        self.state_received_at = 0.0
        self.extended_state = ExtendedState()
        self.extended_state_received_at = 0.0
        self.position = None
        self.position_received_at = 0.0
        self.command_results = {}
        self.samples = []
        self.command_publisher = self.create_publisher(
            String, "command", reliable
        )
        self.create_subscription(State, "state", self._state, reliable)
        self.create_subscription(
            ExtendedState,
            "UAS1/extended_state",
            self._extended_state,
            qos_profile_sensor_data,
        )
        self.create_subscription(Odometry, "odom", self._odom, reliable)
        self.create_subscription(
            String, "command/result", self._result, reliable
        )

    def _state(self, message):
        self.state = message
        self.state_received_at = time.monotonic()

    def _extended_state(self, message):
        self.extended_state = message
        self.extended_state_received_at = time.monotonic()

    def _odom(self, message):
        point = message.pose.pose.position
        values = (point.x, point.y, point.z)
        if not all(math.isfinite(value) for value in values):
            return
        self.position = values
        self.position_received_at = time.monotonic()
        self.samples.append((time.monotonic(), *values))

    def _result(self, message):
        try:
            result = json.loads(message.data)
        except json.JSONDecodeError:
            return
        command = result.get("command")
        if command:
            self.command_results[command] = result

    def spin_until(self, predicate, timeout, description):
        deadline = time.monotonic() + timeout
        while rclpy.ok() and time.monotonic() < deadline:
            rclpy.spin_once(self, timeout_sec=0.05)
            if predicate():
                return
        raise RuntimeError(f"timeout waiting for {description}")

    def wait_live(self):
        self.spin_until(
            lambda: self.command_publisher.get_subscription_count() > 0
            and self.state_received_at > 0.0
            and self.extended_state_received_at > 0.0
            and self.position_received_at > 0.0,
            8.0,
            "driver discovery and telemetry",
        )
        if not self.state.connected:
            raise RuntimeError("FCU is not connected")
        if self.state.armed:
            raise RuntimeError("aircraft is already armed")

    def command(self, text, timeout=5.0):
        self.command_results.pop(text, None)
        self.command_publisher.publish(String(data=text))
        self.spin_until(lambda: text in self.command_results, timeout, f"ACK for {text}")
        result = self.command_results[text]
        if not result.get("success"):
            raise RuntimeError(f"{text} rejected: {result.get('message', 'unknown')}")

    def summary(self, target_height, launch_height, started_at):
        airborne = [sample for sample in self.samples if sample[0] >= started_at]
        if not airborne:
            return {
                "relative_target_height_m": target_height,
                "launch_height_m": launch_height,
                "samples": 0,
            }
        xs = [sample[1] for sample in airborne]
        ys = [sample[2] for sample in airborne]
        zs = [sample[3] for sample in airborne]
        return {
            "relative_target_height_m": target_height,
            "launch_height_m": round(launch_height, 3),
            "absolute_target_height_m": round(launch_height + target_height, 3),
            "samples": len(airborne),
            "peak_height_m": round(max(zs), 3),
            "final_height_m": round(zs[-1], 3),
            "xy_span_m": round(math.hypot(max(xs) - min(xs), max(ys) - min(ys)), 3),
            "duration_s": round(airborne[-1][0] - airborne[0][0], 2),
        }


def parse_args(args=None):
    parser = argparse.ArgumentParser(
        description="Supervised Open32Drone takeoff-hover-land test"
    )
    parser.add_argument("--robot-name", default=DEFAULT_ROBOT_NAME)
    parser.add_argument("--height", type=float, default=0.65)
    parser.add_argument("--hover", type=float, default=5.0)
    parser.add_argument("--height-tolerance", type=float, default=0.18)
    return parser.parse_args(args)


def main(args=None):
    parsed = parse_args(args)
    if not math.isfinite(parsed.height) or not 0.20 <= parsed.height <= 5.80:
        raise SystemExit("--height must be within [0.20, 5.80] m")
    if not math.isfinite(parsed.hover) or not 2.0 <= parsed.hover <= 60.0:
        raise SystemExit("--hover must be within [2, 60] seconds")
    if not math.isfinite(parsed.height_tolerance) or not 0.05 <= parsed.height_tolerance <= 0.50:
        raise SystemExit("--height-tolerance must be within [0.05, 0.50] m")

    rclpy.init(args=[])
    node = FlightTest(parsed.robot_name)
    started_at = time.monotonic()
    launch_height = 0.0
    test_error = None
    try:
        node.wait_live()
        launch_height = node.position[2]
        target_height = launch_height + parsed.height
        print(f"TAKEOFF: target={parsed.height:.2f} m")
        node.command(f"takeoff {parsed.height:g}")
        node.spin_until(lambda: node.state.armed, 5.0, "armed state")
        node.spin_until(
            lambda: node.position is not None
            and abs(node.position[2] - target_height) <= parsed.height_tolerance,
            15.0,
            "target height",
        )
        print(f"HOVER: {parsed.hover:.1f} s")
        hover_deadline = time.monotonic() + parsed.hover
        altitude_outside_since = None
        while rclpy.ok() and time.monotonic() < hover_deadline:
            rclpy.spin_once(node, timeout_sec=0.05)
            now = time.monotonic()
            if not node.state.connected:
                raise RuntimeError("FCU disconnected during hover")
            if not node.state.armed:
                raise RuntimeError("aircraft disarmed during hover")
            if node.position is None or now - node.position_received_at > 1.0:
                raise RuntimeError("local position became stale during hover")
            if abs(node.position[2] - target_height) > parsed.height_tolerance:
                if altitude_outside_since is None:
                    altitude_outside_since = now
                elif now - altitude_outside_since >= 1.0:
                    raise RuntimeError(
                        "hover altitude left tolerance for 1.0 s "
                        f"(z={node.position[2]:.2f}, target={target_height:.2f})"
                    )
            else:
                altitude_outside_since = None
        print("LAND")
        node.command("land")
        node.spin_until(
            lambda: not node.state.armed
            and node.extended_state.landed_state
            == ExtendedState.LANDED_STATE_ON_GROUND,
            25.0,
            "landed and disarmed state",
        )
    except (RuntimeError, KeyboardInterrupt) as error:
        test_error = str(error)
        if node.state.connected and node.state.armed:
            print("RECOVERY: requesting land once")
            try:
                node.command("land", timeout=4.0)
                node.spin_until(lambda: not node.state.armed, 20.0, "recovery landing")
            except RuntimeError as recovery_error:
                test_error += f"; recovery failed: {recovery_error}"
    finally:
        print(json.dumps(node.summary(parsed.height, launch_height, started_at), indent=2))
        node.destroy_node()
        rclpy.try_shutdown()

    if test_error:
        print("FAIL: " + test_error)
        return 1
    print("PASS: takeoff, target-height capture, hover, landing, and disarm")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
