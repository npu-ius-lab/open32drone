"""One supervised hover or feedback-gated cross acceptance flight."""

import argparse
import json
import math
from pathlib import Path
import time

import rclpy
from geometry_msgs.msg import PoseStamped
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

from .names import DEFAULT_ROBOT_NAME, frame_prefix, robot_name
from .control_math import StableDurationGate, position_is_stable


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
        self.speed = None
        self.yaw = 0.0
        self.phase = "PREFLIGHT"
        self.events = []
        self.offboard = ""
        self.require_offboard = False
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
        self.goal_publisher = self.create_publisher(PoseStamped, "goal_pose", reliable)
        self.create_subscription(String, "offboard/status", self._offboard, reliable)
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
        q = message.pose.pose.orientation
        quaternion = (q.x, q.y, q.z, q.w)
        if (not all(math.isfinite(value) for value in (*values, *quaternion))
                or abs(sum(value*value for value in quaternion)-1.0) > 0.05):
            return
        self.position = values
        self.position_received_at = time.monotonic()
        self.yaw = math.atan2(2*(q.w*q.z+q.x*q.y), 1-2*(q.y*q.y+q.z*q.z))
        # Position differences stay in the same frame as the tested goals.
        # Do not mistake an odom child-frame twist for world vertical speed.
        previous = next((s for s in reversed(self.samples)
                         if self.position_received_at-s[0] >= 0.3), None)
        if previous is not None:
            dt = self.position_received_at-previous[0]
            self.speed = tuple((a-b)/dt for a, b in zip(values, previous[1:4]))
        self.samples.append((self.position_received_at, *values, self.phase, *quaternion))

    def _offboard(self, message):
        self.offboard = message.data

    def mark(self, phase, goal=None):
        self.phase = phase
        self.events.append({"phase": phase, "time": time.monotonic(), "goal": goal})
        print(phase, flush=True)

    def check_live(self):
        now = time.monotonic()
        if not self.state.connected or now-self.state_received_at > 1.0:
            raise RuntimeError("FCU state disconnected or stale")
        if now-self.position_received_at > 0.5:
            raise RuntimeError("local position became stale")
        if not self.state.armed:
            raise RuntimeError("aircraft disarmed unexpectedly")
        if self.require_offboard and ("phase=ACTIVE" not in self.offboard
                                      or self.state.mode not in ("AUTO", "CMODE(3)")):
            raise RuntimeError("Offboard ownership lost during position test")

    def stable_at(self, goal, tolerance):
        if self.speed is None:
            return False
        return position_is_stable(
            math.hypot(self.position[0]-goal[0], self.position[1]-goal[1]),
            abs(self.position[2]-goal[2]), math.hypot(*self.speed[:2]),
            self.speed[2], tolerance, 0.08)

    def wait_stable(self, goal, tolerance, publish=None):
        gate = StableDurationGate(1.0)
        deadline = time.monotonic()+20.0
        next_publish = 0.0
        while time.monotonic() < deadline:
            rclpy.spin_once(self, timeout_sec=0.05)
            self.check_live()
            now = time.monotonic()
            if publish is not None and now >= next_publish:
                publish()
                next_publish = now+0.5
            if gate.update(now, self.stable_at(goal, tolerance)):
                return
        raise RuntimeError(f"target not reached and settled: {goal}")

    def hover(self, goal, duration, tolerance):
        deadline = time.monotonic()+duration
        outside = StableDurationGate(1.0)
        while time.monotonic() < deadline:
            rclpy.spin_once(self, timeout_sec=0.05)
            self.check_live()
            if outside.update(time.monotonic(), not self.stable_at(goal, tolerance)):
                raise RuntimeError("hover position or speed left tolerance for 1.0 s")

    def landed(self):
        now = time.monotonic()
        return (self.state.connected and not self.state.armed
                and now-self.state_received_at < 1.0
                and now-self.extended_state_received_at < 1.0
                and self.extended_state.landed_state == ExtendedState.LANDED_STATE_ON_GROUND)

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
            and self.count_publishers("command/result") > 0
            and self.state_received_at > 0.0
            and time.monotonic() - self.state_received_at < 1.0
            and self.extended_state_received_at > 0.0
            and time.monotonic() - self.extended_state_received_at < 1.0
            and self.position_received_at > 0.0
            and time.monotonic() - self.position_received_at < 0.5,
            8.0,
            "driver discovery and telemetry",
        )
        if not self.state.connected:
            raise RuntimeError("FCU is not connected")
        if self.state.armed:
            raise RuntimeError("aircraft is already armed")
        # A bag recorder also subscribes to command. A subscriber count alone
        # does not prove the command handler is ready. Verify a read-only round
        # trip before issuing the single, non-retried takeoff command.
        self.command("status")

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
    parser.add_argument("--height-tolerance", type=float, default=0.10,
                        help="arrival/hover position tolerance in m (XY and Z)")
    parser.add_argument("--pattern", choices=("hover", "cross"), default="hover")
    parser.add_argument("--distance", type=float, default=0.4)
    parser.add_argument("--frame-prefix", default=None)
    parser.add_argument("--output", help="new JSON file for phase events and telemetry")
    return parser.parse_args(args)


def main(args=None):
    parsed = parse_args(args)
    if not math.isfinite(parsed.height) or not 0.20 <= parsed.height <= 5.80:
        raise SystemExit("--height must be within [0.20, 5.80] m")
    if not math.isfinite(parsed.hover) or not 2.0 <= parsed.hover <= 60.0:
        raise SystemExit("--hover must be within [2, 60] seconds")
    if not math.isfinite(parsed.height_tolerance) or not 0.05 <= parsed.height_tolerance <= 0.50:
        raise SystemExit("--height-tolerance must be within [0.05, 0.50] m")
    if not math.isfinite(parsed.distance) or not 0.1 <= parsed.distance <= 0.7:
        raise SystemExit("--distance must be within [0.1, 0.7] m")
    parsed.robot_name = robot_name(parsed.robot_name)
    goal_frame = f"{frame_prefix(parsed.frame_prefix, parsed.robot_name)}/odom"
    report_file = None
    if parsed.output:
        path = Path(parsed.output)
        path.parent.mkdir(parents=True, exist_ok=True)
        report_file = path.open("x", encoding="utf-8")  # Fail before arming if unwritable/existing.

    rclpy.init(args=[])
    node = FlightTest(parsed.robot_name)
    started_at = time.monotonic()
    wall_started_at = time.time()
    launch_height = 0.0
    test_error = None
    takeoff_attempted = False
    landing_attempted = False
    try:
        node.wait_live()
        launch_height = node.position[2]
        target_height = launch_height + parsed.height
        if target_height > 5.8:
            raise RuntimeError("absolute target height exceeds 5.8 m")
        home = (*node.position[:2], target_height)
        node.mark("TAKEOFF", home)
        takeoff_attempted = True
        node.command(f"takeoff {parsed.height:g}")
        node.spin_until(lambda: node.state.armed, 5.0, "armed state")
        node.wait_stable(home, parsed.height_tolerance)
        node.mark("INITIAL_HOVER", home)
        node.hover(home, parsed.hover, parsed.height_tolerance)
        if parsed.pattern == "cross":
            yaw = node.yaw
            node.mark("OFFBOARD_START")
            node.command("rc stop")
            node.command("offboard start")
            node.spin_until(lambda: "phase=ACTIVE" in node.offboard
                            and node.state.mode in ("AUTO", "CMODE(3)"), 6, "confirmed AUTO")
            node.require_offboard = True
            for name, forward, left in (("FORWARD", parsed.distance, 0), ("BACKWARD", 0, 0),
                                        ("LEFT", 0, parsed.distance), ("RIGHT", 0, 0)):
                goal = (home[0]+math.cos(yaw)*forward-math.sin(yaw)*left,
                        home[1]+math.sin(yaw)*forward+math.cos(yaw)*left, home[2])
                message = PoseStamped()
                message.header.frame_id = goal_frame
                message.pose.position.x, message.pose.position.y, message.pose.position.z = goal
                message.pose.orientation.w = 1.0
                def publish_goal():
                    message.header.stamp = node.get_clock().now().to_msg()
                    node.goal_publisher.publish(message)
                node.mark(name, goal)
                node.wait_stable(goal, parsed.height_tolerance, publish_goal)
                node.mark(name+"_SETTLED", goal)
                node.hover(goal, 1.0, parsed.height_tolerance)
            node.mark("FINAL_HOVER", home)
            node.hover(home, parsed.hover, parsed.height_tolerance)
        node.mark("LAND")
        landing_attempted = True
        node.command("land")
        node.spin_until(node.landed, 25.0, "landed and disarmed state")
    except (Exception, KeyboardInterrupt) as error:
        test_error = str(error)
        if takeoff_attempted and not landing_attempted and node.state.connected:
            print("RECOVERY: requesting land once")
            try:
                node.command("land", timeout=4.0)
                node.spin_until(node.landed, 25.0, "recovery landing")
            except Exception as recovery_error:
                test_error += f"; recovery failed: {recovery_error}"
    finally:
        node.mark("FINISH")
        report = {"error": test_error, "pattern": parsed.pattern, "events": node.events,
                  "clock": {"monotonic_start": started_at, "unix_start": wall_started_at},
                  "sample_fields": ["monotonic_s", "x", "y", "z", "phase", "qx", "qy", "qz", "qw"],
                  "final_armed": node.state.armed, "final_landed": node.landed(),
                  "summary": node.summary(parsed.height, launch_height, started_at)}
        print(json.dumps(report, indent=2))
        if report_file:
            json.dump({**report, "samples": node.samples}, report_file, indent=2)
            report_file.close()
        node.destroy_node()
        rclpy.try_shutdown()

    if test_error:
        print("FAIL: " + test_error)
        return 1
    print(f"PASS: {parsed.pattern} sequence reached and settled, landed and disarmed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
