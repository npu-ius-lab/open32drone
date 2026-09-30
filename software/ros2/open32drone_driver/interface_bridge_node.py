"""Reliable, conventional ROS interfaces for Open32Drone MAVROS telemetry."""

import rclpy
from diagnostic_msgs.msg import DiagnosticArray, DiagnosticStatus, KeyValue
from geometry_msgs.msg import PoseStamped, TransformStamped
from mavros_msgs.msg import RCIn, State
from nav_msgs.msg import Odometry
from rclpy.node import Node
from rclpy.qos import (
    DurabilityPolicy,
    HistoryPolicy,
    QoSProfile,
    ReliabilityPolicy,
    qos_profile_sensor_data,
)
from sensor_msgs.msg import BatteryState, Imu, Range
from std_msgs.msg import Bool, String, UInt16MultiArray
from tf2_ros import TransformBroadcaster


class InterfaceBridge(Node):
    """Republish MAVROS sensor QoS as reliable standard topics and TF."""

    def __init__(self):
        super().__init__("open32drone_interface_bridge")
        self.declare_parameter("source_prefix", "UAS1")
        self.declare_parameter("odom_frame_id", "odom")
        self.declare_parameter("base_frame_id", "base_link")
        self.declare_parameter("tof_frame_id", "tof_link")
        self.declare_parameter("state_timeout", 3.0)

        prefix = str(self.get_parameter("source_prefix").value).strip("/")
        self.odom_frame_id = str(self.get_parameter("odom_frame_id").value)
        self.base_frame_id = str(self.get_parameter("base_frame_id").value)
        self.tof_frame_id = str(self.get_parameter("tof_frame_id").value)
        self.last_state = State()
        self.last_state_at = 0.0
        self.last_imu_at = 0.0
        self.last_odom_at = 0.0
        self.last_range_at = 0.0
        self.last_battery_at = 0.0
        self.last_rc_at = 0.0

        reliable = QoSProfile(
            reliability=ReliabilityPolicy.RELIABLE,
            history=HistoryPolicy.KEEP_LAST,
            depth=10,
        )
        state_input_qos = QoSProfile(
            reliability=ReliabilityPolicy.RELIABLE,
            durability=DurabilityPolicy.TRANSIENT_LOCAL,
            history=HistoryPolicy.KEEP_LAST,
            depth=1,
        )
        latched = QoSProfile(
            reliability=ReliabilityPolicy.RELIABLE,
            durability=DurabilityPolicy.TRANSIENT_LOCAL,
            history=HistoryPolicy.KEEP_LAST,
            depth=1,
        )
        self.imu_publisher = self.create_publisher(Imu, "imu/data", reliable)
        self.imu_raw_publisher = self.create_publisher(Imu, "imu/data_raw", reliable)
        self.odom_publisher = self.create_publisher(Odometry, "odom", reliable)
        self.pose_publisher = self.create_publisher(PoseStamped, "pose", reliable)
        self.range_publisher = self.create_publisher(Range, "range/downward", reliable)
        self.battery_publisher = self.create_publisher(BatteryState, "battery", reliable)
        self.rc_publisher = self.create_publisher(RCIn, "rc/in", reliable)
        self.rc_channels_publisher = self.create_publisher(
            UInt16MultiArray, "rc/channels", reliable
        )
        self.state_publisher = self.create_publisher(State, "state", reliable)
        self.connected_publisher = self.create_publisher(Bool, "connected", reliable)
        self.status_publisher = self.create_publisher(
            String, "interface/status", reliable
        )
        self.diagnostics_publisher = self.create_publisher(
            DiagnosticArray, "diagnostics", latched
        )
        self.tf_broadcaster = TransformBroadcaster(self)

        self.create_subscription(
            State, f"{prefix}/state", self._state_callback, state_input_qos
        )
        self.create_subscription(
            Imu, f"{prefix}/imu/data", self._imu_callback, qos_profile_sensor_data
        )
        self.create_subscription(
            Imu, f"{prefix}/imu/data_raw", self._imu_raw_callback, qos_profile_sensor_data
        )
        self.create_subscription(
            Odometry,
            f"{prefix}/local_position/odom",
            self._odom_callback,
            qos_profile_sensor_data,
        )
        self.create_subscription(
            PoseStamped,
            f"{prefix}/local_position/pose",
            self._pose_callback,
            qos_profile_sensor_data,
        )
        self.create_subscription(
            RCIn, f"{prefix}/rc/in", self._rc_callback, qos_profile_sensor_data
        )
        self.create_subscription(
            Range,
            f"{prefix}/distance_sensor/tof",
            self._range_callback,
            qos_profile_sensor_data,
        )
        self.create_subscription(
            BatteryState,
            f"{prefix}/battery",
            self._battery_callback,
            qos_profile_sensor_data,
        )
        self.create_timer(0.5, self._publish_status)

    def _now(self):
        return self.get_clock().now().nanoseconds * 1e-9

    def _state_callback(self, message):
        self.last_state = message
        self.last_state_at = self._now()
        self.state_publisher.publish(message)

    def _imu_callback(self, message):
        message.header.frame_id = self.base_frame_id
        self.last_imu_at = self._now()
        self.imu_publisher.publish(message)

    def _imu_raw_callback(self, message):
        message.header.frame_id = self.base_frame_id
        self.imu_raw_publisher.publish(message)

    def _odom_callback(self, message):
        message.header.frame_id = self.odom_frame_id
        message.child_frame_id = self.base_frame_id
        self.last_odom_at = self._now()
        self.odom_publisher.publish(message)

        transform = TransformStamped()
        transform.header = message.header
        transform.child_frame_id = self.base_frame_id
        transform.transform.translation.x = message.pose.pose.position.x
        transform.transform.translation.y = message.pose.pose.position.y
        transform.transform.translation.z = message.pose.pose.position.z
        transform.transform.rotation = message.pose.pose.orientation
        self.tf_broadcaster.sendTransform(transform)

    def _pose_callback(self, message):
        message.header.frame_id = self.odom_frame_id
        self.pose_publisher.publish(message)

    def _rc_callback(self, message):
        self.last_rc_at = self._now()
        self.rc_publisher.publish(message)
        self.rc_channels_publisher.publish(
            UInt16MultiArray(data=[int(value) for value in message.channels])
        )

    def _range_callback(self, message):
        message.header.frame_id = self.tof_frame_id
        self.last_range_at = self._now()
        self.range_publisher.publish(message)

    def _battery_callback(self, message):
        self.last_battery_at = self._now()
        self.battery_publisher.publish(message)

    def _publish_connection_diagnostic(self, connected, state_age):
        robot_name = self.get_namespace().strip("/") or "open32drone"
        if not connected:
            level = DiagnosticStatus.STALE
            message = "FCU disconnected or heartbeat stale"
        else:
            level = DiagnosticStatus.OK
            message = "FCU connected"
        status = DiagnosticStatus(
            level=level,
            name=f"{robot_name} FCU connection",
            message=message,
            hardware_id=f"{robot_name}_fcu",
            values=[
                KeyValue(key="connected", value=str(connected).lower()),
                KeyValue(key="state_age_s", value=f"{state_age:.3f}"),
                KeyValue(key="source", value="MAVROS state"),
            ],
        )
        report = DiagnosticArray()
        report.header.stamp = self.get_clock().now().to_msg()
        report.status = [status]
        self.diagnostics_publisher.publish(report)

    def _age(self, timestamp):
        return self._now() - timestamp if timestamp else -1.0

    def _publish_status(self):
        state_age = self._age(self.last_state_at)
        timeout = float(self.get_parameter("state_timeout").value)
        connected = bool(
            self.last_state_at and state_age <= timeout and self.last_state.connected
        )
        if self.last_state_at:
            self.last_state.connected = connected
            self.last_state.header.stamp = self.get_clock().now().to_msg()
            self.state_publisher.publish(self.last_state)
        self.connected_publisher.publish(Bool(data=connected))
        status = String()
        status.data = (
            f"connected={connected} state_age={state_age:.2f}s "
            f"imu_age={self._age(self.last_imu_at):.2f}s "
            f"odom_age={self._age(self.last_odom_at):.2f}s "
            f"range_age={self._age(self.last_range_at):.2f}s "
            f"battery_age={self._age(self.last_battery_at):.2f}s "
            f"rc_age={self._age(self.last_rc_at):.2f}s"
        )
        self.status_publisher.publish(status)
        self._publish_connection_diagnostic(connected, state_age)


def main(args=None):
    rclpy.init(args=args)
    node = InterfaceBridge()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.try_shutdown()


if __name__ == "__main__":
    main()
