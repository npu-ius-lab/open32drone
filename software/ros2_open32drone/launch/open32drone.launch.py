# Open32Drone 一体化 launch：图传节点 + MAVROS 飞控连接
# 用法：ros2 launch open32drone_driver open32drone.launch.py
# 参数：camera_url（默认 http://192.168.4.1/stream）

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import ComposableNodeContainer, Node
from launch_ros.descriptions import ComposableNode


def generate_launch_description():
    camera_url = LaunchConfiguration("camera_url", default="http://192.168.4.1/stream")

    camera_node = Node(
        package="open32drone_driver",
        executable="camera",
        name="open32drone_camera",
        output="screen",
        parameters=[{"url": camera_url}],
    )

    mavros_container = ComposableNodeContainer(
        name="open32drone",
        namespace="",
        package="rclcpp_components",
        executable="component_container_mt",
        composable_node_descriptions=[
            ComposableNode(
                package="mavros",
                plugin="mavros::router::Router",
                name="mavros_router",
                parameters=[
                    {"fcu_urls": ["udp://:14550@192.168.4.1:14550"]},
                    {"gcs_urls": ["udp://0.0.0.0:14551@"]},
                    {"uas_urls": ["/open32drone_uas"]},
                    {"fcu_protocol": "v2.0"},
                ],
                extra_arguments=[{"use_intra_process_comms": True}],
            ),
            ComposableNode(
                package="mavros",
                plugin="mavros::uas::UAS",
                name="UAS1",
                namespace="open32drone",
                parameters=[
                    {"uas_url": "/open32drone_uas"},
                    {"fcu_protocol": "v2.0"},
                    {"plugin_allowlist": [
                        "sys_status", "command", "param", "manual_control", "imu",
                    ]},
                    {"system_id": 255},
                    {"component_id": 240},
                    {"target_system_id": 1},
                    {"target_component_id": 1},
                    {"connection_timeout": 10.0},
                    {"startup_px4_usb_quirk": False},
                    {"heartbeat_interval": 1.0},
                    {"timeout_heartbeat": 5.0},
                    {"enable_autopilot_version_check": False},
                    {"imu/frame_id": "base_link"},
                    {"manual_control/send_interval": 0.05},
                    {"manual_control/control_components": 15},
                    {"sys_status/disable_diag": False},
                    {"sys_status/diag_rate": 1.0},
                    {"command/use_comp_id_system_control": False},
                ],
                remappings=[
                    ("/open32drone/UAS1/imu/data", "/imu/data"),
                    ("/open32drone/UAS1/imu/data_raw", "/imu/data_raw"),
                    ("/open32drone/UAS1/manual_control/send", "/manual_control"),
                ],
                extra_arguments=[{"use_intra_process_comms": True}],
            ),
        ],
        output="screen",
    )

    return LaunchDescription([
        DeclareLaunchArgument("camera_url", default_value="http://192.168.4.1/stream"),
        camera_node,
        mavros_container,
    ])
