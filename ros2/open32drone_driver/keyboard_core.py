"""Pure keyboard motion math; no terminal or ROS dependency."""

import math


BODY_MOTION = {
    "w": (1.0, 0.0, 0.0, 0.0),
    "s": (-1.0, 0.0, 0.0, 0.0),
    "a": (0.0, 1.0, 0.0, 0.0),
    "d": (0.0, -1.0, 0.0, 0.0),
    "i": (0.0, 0.0, 1.0, 0.0),
    "k": (0.0, 0.0, -1.0, 0.0),
    "j": (0.0, 0.0, 0.0, 1.0),
    "l": (0.0, 0.0, 0.0, -1.0),
}


def yaw_from_quaternion(orientation):
    return math.atan2(
        2.0 * (orientation.w * orientation.z + orientation.x * orientation.y),
        1.0 - 2.0 * (orientation.y * orientation.y + orientation.z * orientation.z),
    )


def position_step(position, yaw, key, step, min_altitude=0.05, max_altitude=5.80):
    """Return an absolute ENU goal measured from the *observed* current pose."""
    forward, left, up, turn = BODY_MOTION[key]
    if turn:
        raise ValueError("yaw keys are only supported in velocity mode")
    return (
        position.x + step * (math.cos(yaw) * forward - math.sin(yaw) * left),
        position.y + step * (math.sin(yaw) * forward + math.cos(yaw) * left),
        max(min_altitude, min(max_altitude, position.z + step * up)),
    )


def velocity_for_key(key, horizontal, vertical, yaw_rate):
    forward, left, up, turn = BODY_MOTION[key]
    return (forward * horizontal, left * horizontal, up * vertical, turn * yaw_rate)
