import math
import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "ros2"))

from open32drone_driver.control_math import (  # noqa: E402
    StableDurationGate,
    bounded_xy_velocity,
    limit_xy_velocity,
    position_is_stable,
)


class RosControlMathTests(unittest.TestCase):
    def test_position_velocity_stops_inside_deadband(self):
        self.assertEqual(
            bounded_xy_velocity(0.02, 0.0, 0.8, 0.15, 0.03),
            (0.0, 0.0),
        )

    def test_position_velocity_is_proportional_near_goal(self):
        vx, vy = bounded_xy_velocity(0.06, 0.08, 0.8, 0.15, 0.03)
        self.assertAlmostEqual(vx, 0.048)
        self.assertAlmostEqual(vy, 0.064)

    def test_position_velocity_is_vector_limited(self):
        vx, vy = bounded_xy_velocity(0.30, 0.40, 0.8, 0.15, 0.03)
        self.assertAlmostEqual(math.hypot(vx, vy), 0.15)
        self.assertAlmostEqual(vx, 0.09)
        self.assertAlmostEqual(vy, 0.12)

    def test_direct_velocity_is_vector_limited(self):
        vx, vy = limit_xy_velocity(0.70, 0.70, 0.70)
        self.assertAlmostEqual(math.hypot(vx, vy), 0.70)
        self.assertAlmostEqual(vx, math.sqrt(0.245))
        self.assertAlmostEqual(vy, math.sqrt(0.245))

    def test_position_stability_requires_position_and_speed(self):
        self.assertTrue(position_is_stable(0.04, 0.03, 0.05, 0.02, 0.07, 0.10))
        self.assertFalse(position_is_stable(0.08, 0.03, 0.05, 0.02, 0.07, 0.10))
        self.assertFalse(position_is_stable(0.04, 0.03, 0.11, 0.02, 0.07, 0.10))

    def test_non_finite_position_is_never_stable(self):
        self.assertFalse(
            position_is_stable(math.nan, 0.0, 0.0, 0.0, 0.07, 0.10)
        )

    def test_arrival_gate_requires_continuous_hold(self):
        gate = StableDurationGate(1.0)
        self.assertFalse(gate.update(10.0, True))
        self.assertFalse(gate.update(10.7, True))
        self.assertTrue(gate.update(11.0, True))

    def test_arrival_gate_resets_when_condition_breaks(self):
        gate = StableDurationGate(1.0)
        self.assertFalse(gate.update(10.0, True))
        self.assertFalse(gate.update(10.8, False))
        self.assertFalse(gate.update(11.2, True))
        self.assertTrue(gate.update(12.2, True))


if __name__ == "__main__":
    unittest.main()
