"""Keyboard geometry and safety-gate regression checks without ROS hardware."""

import ast
import math
from pathlib import Path
import sys
from types import SimpleNamespace as NS
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "ros2"))
from open32drone_driver.keyboard_core import position_step, velocity_for_key


class KeyboardMathTests(unittest.TestCase):
    def test_forward_step_follows_observed_yaw_without_accumulating_old_goal(self):
        origin = NS(x=1.0, y=2.0, z=0.65)
        goal = position_step(origin, math.pi / 2, "w", .2)
        self.assertAlmostEqual(goal[0], 1.0)
        self.assertAlmostEqual(goal[1], 2.2)
        self.assertEqual(position_step(origin, math.pi / 2, "w", .2), goal)

    def test_left_and_vertical_bounds(self):
        origin = NS(x=0.0, y=0.0, z=5.75)
        self.assertAlmostEqual(position_step(origin, 0, "a", .2)[1], .2)
        self.assertEqual(position_step(origin, 0, "i", .2)[2], 5.8)

    def test_velocity_keys_are_flu(self):
        self.assertEqual(velocity_for_key("w", .2, .15, .4), (.2, 0.0, 0.0, 0.0))
        self.assertEqual(velocity_for_key("a", .2, .15, .4), (0.0, .2, 0.0, 0.0))
        self.assertEqual(velocity_for_key("j", .2, .15, .4), (0.0, 0.0, 0.0, .4))


class OffboardFrameTests(unittest.TestCase):
    def test_wrong_frame_cannot_start_or_change_goal(self):
        tree = ast.parse((ROOT / "ros2/open32drone_driver/offboard_control_node.py").read_text())
        original = next(n for n in tree.body if isinstance(n, ast.ClassDef)
                        and n.name == "OffboardControl")
        method = next(n for n in original.body if isinstance(n, ast.FunctionDef)
                      and n.name == "_position_callback")
        cls = ast.ClassDef(name="Probe", bases=[], keywords=[], decorator_list=[], body=[method])
        namespace = {"_finite": lambda values: True}
        exec(compile(ast.fix_missing_locations(ast.Module(body=[cls], type_ignores=[])),
                     "offboard_control_node.py", "exec"), namespace)
        node = namespace["Probe"]()
        node.get_parameter = lambda _: NS(value="open32drone/odom")
        node._pose_fresh = lambda: True
        node.phase = "IDLE"
        node._auto_start = lambda _: self.fail("wrong frame started Offboard")
        node.position_goal = [0, 0, .65]
        message = NS(header=NS(frame_id="map"),
                     pose=NS(position=NS(x=1.0, y=0.0, z=.65)))
        node._position_callback(message)
        self.assertEqual(node.position_goal, [0, 0, .65])


if __name__ == "__main__":
    unittest.main()
