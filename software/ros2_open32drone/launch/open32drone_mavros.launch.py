# Open32Drone MAVROS 连接 launch（组件容器：Router + UAS）
# 连接 open32drone_v3 固件的 MAVLink（UDP 14550，飞控 AP 192.168.4.1）
# 用法：ros2 launch open32drone_driver open32drone_mavros.launch.py

from launch import LaunchDescription
from launch_ros.actions import ComposableNodeContainer
from launch_ros.descriptions import ComposableNode


def generate_launch_description():
    """Generate launch description for ESP32 MAVLink connection."""
    container = ComposableNodeContainer(
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
                    # 指向飞控的IP和端口
                    {"fcu_urls": ["udp://:14550@192.168.4.1:14550"]},
                    {"gcs_urls": ["udp://0.0.0.0:14551@"]},  # 使用不同端口避免冲突
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

                    # 根据飞控支持的MAVLink消息调整插件列表
                    {"plugin_allowlist": [
                        "sys_status",
                        "command",
                        "param",
                        "manual_control",
                        "imu",
                    ]},

                    # 系统ID配置 - 匹配飞控的SYSTEM_ID=1
                    {"system_id": 255},      # GCS通常用255
                    {"component_id": 240},   # MAVROS组件ID
                    {"target_system_id": 1}, # 飞控系统ID=1
                    {"target_component_id": 1}, # 飞控组件ID=1

                    # 连接参数优化
                    {"connection_timeout": 10.0},
                    {"startup_px4_usb_quirk": False},
                    {"heartbeat_interval": 1.0},
                    {"timeout_heartbeat": 5.0},
                    {"enable_autopilot_version_check": False},

                    # IMU插件特定配置
                    {"imu/frame_id": "base_link"},
                    {"imu/linear_acceleration_stdev": 0.0003},
                    {"imu/angular_velocity_stdev": 0.000349},
                    {"imu/orientation_stdev": 1.0},

                    # Manual Control插件配置
                    {"manual_control/send_interval": 0.05},  # 20Hz
                    {"manual_control/control_components": 15},  # 启用所有控制通道

                    # 系统状态配置
                    {"sys_status/disable_diag": False},
                    {"sys_status/diag_rate": 1.0},

                    # 命令插件配置
                    {"command/use_comp_id_system_control": False},

                    # QoS配置
                    {"imu/data/qos_history": "keep_last"},
                    {"imu/data/qos_depth": 10},
                    {"imu/data/qos_reliability": "best_effort"},
                    {"imu/data/qos_durability": "volatile"},

                    {"manual_control/send/qos_history": "keep_last"},
                    {"manual_control/send/qos_depth": 10},
                    {"manual_control/send/qos_reliability": "best_effort"},
                ],
                remappings=[
                    # 将IMU数据重映射到标准话题
                    ("/open32drone/UAS1/imu/data", "/imu/data"),
                    ("/open32drone/UAS1/imu/data_raw", "/imu/data_raw"),
                    # 将手动控制话题重映射到更易访问的位置
                    ("/open32drone/UAS1/manual_control/send", "/manual_control"),
                ],
                extra_arguments=[{"use_intra_process_comms": True}],
            ),
        ],
        output="screen",
    )

    return LaunchDescription([container])
