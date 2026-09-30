"""Small, ROS-independent helpers for bounded Offboard control."""

import math


class StableDurationGate:
    """Require one condition to remain true for a continuous duration."""

    def __init__(self, hold_time):
        if not math.isfinite(hold_time) or hold_time <= 0.0:
            raise ValueError("hold_time must be positive and finite")
        self.hold_time = hold_time
        self.stable_since = None

    def update(self, now, stable):
        if not math.isfinite(now):
            self.stable_since = None
            return False
        if not stable:
            self.stable_since = None
            return False
        if self.stable_since is None:
            self.stable_since = now
            return False
        return now - self.stable_since >= self.hold_time


def limit_xy_velocity(velocity_x, velocity_y, max_speed):
    """Limit one finite XY velocity vector without changing its direction."""
    values = (velocity_x, velocity_y, max_speed)
    if not all(math.isfinite(value) for value in values):
        raise ValueError("velocity command values must be finite")
    if max_speed <= 0.0:
        raise ValueError("velocity command limit must be positive")

    speed = math.hypot(velocity_x, velocity_y)
    if speed <= max_speed or speed <= 1e-9:
        return velocity_x, velocity_y
    scale = max_speed / speed
    return velocity_x * scale, velocity_y * scale


def bounded_xy_velocity(error_x, error_y, gain, max_speed, deadband):
    """Convert an ENU position error into a bounded ENU velocity command."""
    values = (error_x, error_y, gain, max_speed, deadband)
    if not all(math.isfinite(value) for value in values):
        raise ValueError("position controller values must be finite")
    if gain <= 0.0 or max_speed <= 0.0 or deadband < 0.0:
        raise ValueError("position controller limits must be positive")

    distance = math.hypot(error_x, error_y)
    if distance <= deadband:
        return 0.0, 0.0

    return limit_xy_velocity(error_x * gain, error_y * gain, max_speed)


def position_is_stable(
    horizontal_error,
    vertical_error,
    horizontal_speed,
    vertical_speed,
    position_tolerance,
    speed_tolerance,
):
    """Return true only when both position and motion are inside the gate."""
    values = (
        horizontal_error,
        vertical_error,
        horizontal_speed,
        vertical_speed,
        position_tolerance,
        speed_tolerance,
    )
    if not all(math.isfinite(value) for value in values):
        return False
    if position_tolerance <= 0.0 or speed_tolerance <= 0.0:
        return False
    return (
        horizontal_error <= position_tolerance
        and vertical_error <= position_tolerance
        and horizontal_speed <= speed_tolerance
        and abs(vertical_speed) <= speed_tolerance
    )
