"""Launch one namespaced Open32Drone ROS 2 control stack."""

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.conditions import IfCondition
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.parameter_descriptions import ParameterValue
from launch_ros.substitutions import FindPackageShare


MAVROS_PARAMETERS = {
    "tgt_component": 1,
    "plugin_allowlist": [
        "sys_status",
        "command",
        "distance_sensor",
        "param",
        "manual_control",
        "rc_io",
        "imu",
        "extended_state",
        "local_position",
        "setpoint_raw",
        "setpoint_attitude",
    ],
    "imu/linear_acceleration_stdev": 0.0003,
    "imu/angular_velocity_stdev": 0.000349,
    "imu/orientation_stdev": 1.0,
    # The reliable interface bridge is the only TF authority. Its frame IDs
    # are prefixed per aircraft, so MAVROS must not publish a duplicate edge.
    "local_position/tf/send": False,
    "manual_control/send_interval": 0.05,
    "manual_control/control_components": 15,
    "sys_status/disable_diag": False,
    "sys_status/diag_rate": 1.0,
    "command/use_comp_id_system_control": False,
}


def generate_launch_description():
    package_share = FindPackageShare("open32drone_driver")
    plugin_config = PathJoinSubstitution(
        [package_share, "config", "mavros_plugins.yaml"]
    )
    aircraft_ip = LaunchConfiguration("aircraft_ip")
    robot_name = LaunchConfiguration("robot_name")
    frame_prefix = LaunchConfiguration("frame_prefix")
    mav_sys_id = LaunchConfiguration("mav_sys_id")
    local_udp_port = LaunchConfiguration("local_udp_port")
    fcu_url = LaunchConfiguration("fcu_url")
    use_rviz = LaunchConfiguration("use_rviz")
    mavros_namespace = PathJoinSubstitution([robot_name, "UAS1"])
    odom_frame = PathJoinSubstitution([frame_prefix, "odom"])
    base_frame = PathJoinSubstitution([frame_prefix, "base_link"])
    tof_frame = PathJoinSubstitution([frame_prefix, "tof_link"])
    mavros_parameters = dict(MAVROS_PARAMETERS)
    mavros_parameters["fcu_url"] = fcu_url
    mavros_parameters["tgt_system"] = ParameterValue(mav_sys_id, value_type=int)
    mavros_parameters["imu/frame_id"] = ParameterValue(base_frame, value_type=str)
    mavros_parameters["local_position/frame_id"] = ParameterValue(
        odom_frame, value_type=str
    )

    nodes = [
        Node(
            package="mavros",
            executable="mavros_node",
            namespace=mavros_namespace,
            name="mavros",
            output="screen",
            parameters=[mavros_parameters, plugin_config],
        ),
        Node(
            package="open32drone_driver",
            executable="interface_bridge",
            namespace=robot_name,
            name="interface_bridge",
            output="screen",
            parameters=[
                {
                    "source_prefix": "UAS1",
                    "odom_frame_id": ParameterValue(odom_frame, value_type=str),
                    "base_frame_id": ParameterValue(base_frame, value_type=str),
                    "tof_frame_id": ParameterValue(tof_frame, value_type=str),
                }
            ],
        ),
        Node(
            package="open32drone_driver",
            executable="flight_manager",
            namespace=robot_name,
            name="flight_manager",
            output="screen",
        ),
        Node(
            package="open32drone_driver",
            executable="offboard_control",
            namespace=robot_name,
            name="offboard_control",
            output="screen",
            parameters=[{"odom_frame_id": ParameterValue(odom_frame, value_type=str)}],
        ),
        Node(
            package="open32drone_driver",
            executable="rc_bridge",
            namespace=robot_name,
            name="rc_bridge",
            output="screen",
        ),
        Node(
            package="rviz2",
            executable="rviz2",
            namespace=robot_name,
            name="open32drone_rviz",
            output="screen",
            condition=IfCondition(use_rviz),
            arguments=[
                "-d",
                PathJoinSubstitution([package_share, "config", "open32drone.rviz"]),
                "-f",
                odom_frame,
            ],
        ),
    ]

    return LaunchDescription(
        [
            DeclareLaunchArgument(
                "robot_name",
                default_value="open32drone",
                description="Unique ROS namespace for this aircraft",
            ),
            DeclareLaunchArgument(
                "frame_prefix",
                default_value=robot_name,
                description="Unique TF frame prefix without a leading slash",
            ),
            DeclareLaunchArgument(
                "aircraft_ip",
                default_value="192.168.4.1",
                description="Aircraft IPv4 address in direct-AP or router-STA mode",
            ),
            DeclareLaunchArgument(
                "mav_sys_id",
                default_value="1",
                description="MAVLink system ID configured in this aircraft",
            ),
            DeclareLaunchArgument(
                "local_udp_port",
                default_value="14550",
                description="Unique MAVROS UDP bind port on this ROS host",
            ),
            DeclareLaunchArgument(
                "fcu_url",
                default_value=[
                    "udp://0.0.0.0:",
                    local_udp_port,
                    "@",
                    aircraft_ip,
                    ":14550",
                ],
                description="Advanced MAVROS UDP endpoint override",
            ),
            DeclareLaunchArgument("use_rviz", default_value="false"),
            *nodes,
        ]
    )
