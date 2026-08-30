import unittest

from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "ros2"))

from open32drone_driver.names import frame_prefix, robot_name
from open32drone_driver.system_control import _stop_targets


class RosNameTests(unittest.TestCase):
    def test_robot_name_accepts_one_safe_namespace_component(self):
        self.assertEqual(robot_name("/drone01/"), "drone01")
        self.assertEqual(robot_name("_lab2"), "_lab2")
        for value in ("", "1drone", "drone-01", "drone/01", "../drone"):
            with self.subTest(value=value):
                with self.assertRaises(ValueError):
                    robot_name(value)

    def test_frame_prefix_defaults_to_robot_and_allows_nested_names(self):
        self.assertEqual(frame_prefix(None, "drone01"), "drone01")
        self.assertEqual(frame_prefix("lab/drone01", "unused"), "lab/drone01")
        for value in ("/", "bad-prefix", "lab/2drone"):
            with self.subTest(value=value):
                with self.assertRaises(ValueError):
                    frame_prefix(value, "drone01")

    def test_stale_reused_pid_is_never_a_stop_target(self):
        self.assertEqual(
            _stop_targets(4100, {4100: (1, "python3 unrelated_service.py")}, "drone01"),
            set(),
        )
        managed_table = {
            4200: (
                1,
                "ros2 launch open32drone_driver open32drone.launch.py "
                "robot_name:=drone01",
            ),
            4201: (4200, "mavros_node --ros-args -r __ns:=/drone01/UAS1"),
        }
        self.assertEqual(_stop_targets(4200, managed_table, "drone01"), {4200, 4201})


if __name__ == "__main__":
    unittest.main()
