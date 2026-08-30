"""Small shared naming rules for one or more Open32Drone ROS graphs."""

import re


DEFAULT_ROBOT_NAME = "open32drone"
_ROS_NAME = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")


def robot_name(value):
    """Return one safe, relative ROS namespace component."""
    name = str(value).strip().strip("/")
    if not _ROS_NAME.fullmatch(name):
        raise ValueError(
            "robot name must contain only letters, digits and underscores "
            "and may not start with a digit"
        )
    return name


def frame_prefix(value, default_robot_name):
    """Return a slash-separated TF prefix with no leading/trailing slash."""
    prefix = str(value or default_robot_name).strip().strip("/")
    if not prefix or any(not _ROS_NAME.fullmatch(part) for part in prefix.split("/")):
        raise ValueError(
            "frame prefix must contain slash-separated ROS name components"
        )
    return prefix
