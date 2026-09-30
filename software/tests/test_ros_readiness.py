"""Exercise preflight readiness without requiring ROS on the build host."""
import ast
from pathlib import Path
from types import SimpleNamespace
import unittest

ROOT = Path(__file__).resolve().parents[1]
tree = ast.parse((ROOT / "ros2/open32drone_driver/flight_test.py").read_text(encoding="utf-8"))
method = next(n for c in tree.body if isinstance(c, ast.ClassDef)
              and c.name == "FlightTest" for n in c.body
              if isinstance(n, ast.FunctionDef) and n.name == "wait_live")
scope = {"time": SimpleNamespace(monotonic=lambda: 100.0)}
exec(compile(ast.Module(body=[method], type_ignores=[]), "readiness", "exec"), scope)


class ReadinessTests(unittest.TestCase):
    def fake(self, reply_publishers=1, pose_age=0.1, status_ok=True):
        sent = []
        def spin(predicate, timeout, description):
            if not predicate():
                raise RuntimeError(description)
        def command(text):
            sent.append(text)
            if not status_ok:
                raise RuntimeError("status timeout")
        return SimpleNamespace(
            command_publisher=SimpleNamespace(get_subscription_count=lambda: 1),
            count_publishers=lambda topic: reply_publishers,
            state_received_at=99.9, extended_state_received_at=99.9,
            position_received_at=100-pose_age,
            state=SimpleNamespace(connected=True, armed=False),
            spin_until=spin, command=command), sent

    def test_recorder_alone_does_not_allow_takeoff(self):
        node, sent = self.fake(reply_publishers=0)
        with self.assertRaises(RuntimeError):
            scope["wait_live"](node)
        self.assertEqual(sent, [])

    def test_stale_pose_blocks_readiness(self):
        node, sent = self.fake(pose_age=1)
        with self.assertRaises(RuntimeError):
            scope["wait_live"](node)
        self.assertEqual(sent, [])

    def test_readonly_status_must_succeed(self):
        node, sent = self.fake(status_ok=False)
        with self.assertRaisesRegex(RuntimeError, "status timeout"):
            scope["wait_live"](node)
        self.assertEqual(sent, ["status"])

    def test_ready_only_sends_readonly_status(self):
        node, sent = self.fake()
        scope["wait_live"](node)
        self.assertEqual(sent, ["status"])
