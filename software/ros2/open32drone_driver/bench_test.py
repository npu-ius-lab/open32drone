"""Short, propeller-off telemetry acceptance test."""

import argparse
import json
import time

import rclpy
from mavros_msgs.msg import RCIn, State
from nav_msgs.msg import Odometry
from rclpy.node import Node
from rclpy.qos import HistoryPolicy, QoSProfile, ReliabilityPolicy
from sensor_msgs.msg import BatteryState, Imu, Range

from .names import DEFAULT_ROBOT_NAME, robot_name


class BenchTest(Node):
    def __init__(self, namespace=DEFAULT_ROBOT_NAME):
        super().__init__(
            "open32drone_bench_test", namespace=robot_name(namespace)
        )
        reliable = QoSProfile(
            reliability=ReliabilityPolicy.RELIABLE,
            history=HistoryPolicy.KEEP_LAST,
            depth=10,
        )
        self.started = time.monotonic()
        self.state = State()
        self.counts = {name: 0 for name in ("state", "imu", "odom", "range", "battery", "rc")}
        self.create_subscription(State, "state", self._state, reliable)
        self.create_subscription(Imu, "imu/data", self._count("imu"), reliable)
        self.create_subscription(Odometry, "odom", self._count("odom"), reliable)
        self.create_subscription(Range, "range/downward", self._count("range"), reliable)
        self.create_subscription(BatteryState, "battery", self._count("battery"), reliable)
        self.create_subscription(RCIn, "rc/in", self._count("rc"), reliable)

    def _state(self, message):
        self.state = message
        self.counts["state"] += 1

    def _count(self, name):
        def callback(_message):
            self.counts[name] += 1

        return callback

    def report(self, duration):
        elapsed = max(time.monotonic() - self.started, 0.001)
        return {
            "duration_s": round(elapsed, 2),
            "connected": bool(self.state.connected),
            "armed": bool(self.state.armed),
            "mode": self.state.mode,
            "rates_hz": {
                name: round(count / elapsed, 2) for name, count in self.counts.items()
            },
            "requested_duration_s": duration,
        }


def parse_args(args=None):
    parser = argparse.ArgumentParser(description="Open32Drone propeller-off bench test")
    parser.add_argument("--robot-name", default=DEFAULT_ROBOT_NAME)
    parser.add_argument("--duration", type=float, default=5.0)
    parser.add_argument("--require-battery", action="store_true")
    parser.add_argument("--require-rc", action="store_true")
    return parser.parse_args(args)


def main(args=None):
    parsed = parse_args(args)
    if parsed.duration < 2.0 or parsed.duration > 60.0:
        raise SystemExit("--duration must be within [2, 60] seconds")
    rclpy.init(args=[])
    node = BenchTest(parsed.robot_name)
    try:
        deadline = time.monotonic() + parsed.duration
        while rclpy.ok() and time.monotonic() < deadline:
            rclpy.spin_once(node, timeout_sec=0.1)
        report = node.report(parsed.duration)
        print(json.dumps(report, ensure_ascii=False, indent=2))
        rates = report["rates_hz"]
        failures = []
        if not report["connected"]:
            failures.append("FCU not connected")
        for name in ("state", "imu", "odom", "range"):
            if rates[name] < 1.0:
                failures.append(f"{name} rate below 1 Hz")
        if parsed.require_battery and rates["battery"] < 0.5:
            failures.append("battery telemetry missing")
        if parsed.require_rc and rates["rc"] < 1.0:
            failures.append("RC telemetry missing")
        if failures:
            print("FAIL: " + "; ".join(failures))
            return 1
        print("PASS: link, IMU, odometry and downward range are live")
        return 0
    finally:
        node.destroy_node()
        rclpy.try_shutdown()


if __name__ == "__main__":
    raise SystemExit(main())
