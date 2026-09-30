"""Explicit, watchdog-protected SBUS-style channel control over MANUAL_CONTROL."""

import math

import rclpy
from mavros_msgs.msg import ManualControl, OverrideRCIn, State, SysStatus
from rclpy.node import Node
from rclpy.qos import QoSProfile, ReliabilityPolicy, qos_profile_sensor_data
from std_msgs.msg import String
from std_srvs.srv import Trigger


def _clamp(value, lower, upper):
    return max(lower, min(upper, value))


class RCBridge(Node):
    """Convert configured raw channels to the firmware MANUAL_CONTROL contract."""

    def __init__(self):
        super().__init__("open32drone_rc_bridge")
        self.declare_parameter("publish_rate", 20.0)
        self.declare_parameter("command_timeout", 0.30)
        self.declare_parameter("channel_min", 240)
        self.declare_parameter("channel_center", 1023)
        self.declare_parameter("channel_max", 1807)
        self.declare_parameter("roll_channel", 0)
        self.declare_parameter("pitch_channel", 1)
        self.declare_parameter("throttle_channel", 2)
        self.declare_parameter("yaw_channel", 3)
        self.declare_parameter("manual_control_topic", "UAS1/manual_control/send")

        self.state = State()
        self.enabled = False
        self.last_channels = None
        self.last_command_at = 0.0
        self.physical_rc_priority = None
        self.last_sys_status_at = 0.0
        self.manual_publisher = self.create_publisher(
            ManualControl, self.get_parameter("manual_control_topic").value, 10
        )
        self.status_publisher = self.create_publisher(
            String, "rc/status", 10
        )
        state_qos = QoSProfile(
            reliability=ReliabilityPolicy.RELIABLE,
            depth=1,
        )
        self.create_subscription(
            State, "state", self._state_callback, state_qos
        )
        self.create_subscription(
            SysStatus,
            "UAS1/sys_status",
            self._sys_status_callback,
            qos_profile_sensor_data,
        )
        self.create_subscription(
            OverrideRCIn, "rc/override", self._channels_callback, 10
        )
        self.create_service(Trigger, "rc/start", self._start)
        self.create_service(Trigger, "rc/stop", self._stop)

        rate = float(self.get_parameter("publish_rate").value)
        if not math.isfinite(rate) or rate < 10.0 or rate > 50.0:
            raise ValueError("publish_rate must be within [10, 50] Hz")
        command_timeout = float(self.get_parameter("command_timeout").value)
        if not math.isfinite(command_timeout) or command_timeout < 0.10 or command_timeout > 1.0:
            raise ValueError("command_timeout must be within [0.10, 1.0] s")
        self.create_timer(1.0 / rate, self._publish_manual_control)
        self.create_timer(0.5, self._publish_status)

    def _now(self):
        return self.get_clock().now().nanoseconds * 1e-9

    def _state_callback(self, message):
        self.state = message
        if self.enabled and (not message.connected or not message.armed):
            self.enabled = False
            self.last_channels = None
            self.get_logger().warning("RC bridge stopped: FCU disconnected or disarmed")

    def _channels_callback(self, message):
        if not self.enabled:
            return
        indices = [
            int(self.get_parameter(name).value)
            for name in ("roll_channel", "pitch_channel", "throttle_channel", "yaw_channel")
        ]
        if any(index < 0 or index >= len(message.channels) for index in indices):
            self.get_logger().error("Configured RC channel index is out of range")
            return
        selected = [int(message.channels[index]) for index in indices]
        if any(value in (OverrideRCIn.CHAN_RELEASE, OverrideRCIn.CHAN_NOCHANGE) for value in selected):
            return
        self.last_channels = selected
        self.last_command_at = self._now()

    def _sys_status_callback(self, message):
        self.physical_rc_priority = bool(message.sensors_health & (1 << 16))
        self.last_sys_status_at = self._now()

    def _start(self, _request, response):
        if not self.state.connected or not self.state.armed:
            response.success = False
            response.message = "FCU must be connected and armed"
            return response
        sys_status_fresh = self.last_sys_status_at and (
            self._now() - self.last_sys_status_at <= 1.5
        )
        if sys_status_fresh and self.physical_rc_priority:
            response.success = False
            response.message = "physical SBUS has control priority"
            return response
        self.enabled = True
        self.last_channels = None
        self.last_command_at = 0.0
        response.success = True
        response.message = "RC bridge enabled; publish fresh channels continuously"
        return response

    def _stop(self, _request, response):
        self.enabled = False
        self.last_channels = None
        response.success = True
        response.message = "RC bridge stopped; firmware fallback owns recovery"
        return response

    def _centered(self, value):
        minimum = float(self.get_parameter("channel_min").value)
        center = float(self.get_parameter("channel_center").value)
        maximum = float(self.get_parameter("channel_max").value)
        span = maximum - center if value >= center else center - minimum
        if span <= 0.0:
            return 0.0
        return _clamp((value - center) / span, -1.0, 1.0)

    def _throttle(self, value):
        minimum = float(self.get_parameter("channel_min").value)
        maximum = float(self.get_parameter("channel_max").value)
        if maximum <= minimum:
            return 0.0
        return _clamp((value - minimum) / (maximum - minimum), 0.0, 1.0)

    def _publish_manual_control(self):
        if not self.enabled or self.last_channels is None:
            return
        age = self._now() - self.last_command_at
        command_timeout = float(self.get_parameter("command_timeout").value)
        if age > command_timeout:
            self.enabled = False
            self.last_channels = None
            self.get_logger().warning(
                "RC input timed out; stopped MANUAL_CONTROL stream for firmware failsafe"
            )
            return

        roll, pitch, throttle, yaw = self.last_channels
        message = ManualControl()
        message.header.stamp = self.get_clock().now().to_msg()
        # MAVROS 2.x send path copies these fields directly into the MAVLink
        # integer payload, so the transmit contract is -1000..1000/0..1000.
        message.x = self._centered(pitch) * 1000.0
        message.y = self._centered(roll) * 1000.0
        message.z = self._throttle(throttle) * 1000.0
        message.r = self._centered(yaw) * 1000.0
        self.manual_publisher.publish(message)

    def _publish_status(self):
        message = String()
        age = self._now() - self.last_command_at if self.last_command_at else -1.0
        sys_status_age = self._now() - self.last_sys_status_at if self.last_sys_status_at else -1.0
        physical_rc = "unknown"
        if self.physical_rc_priority is not None and 0.0 <= sys_status_age <= 1.5:
            physical_rc = str(self.physical_rc_priority).lower()
        command_timeout = float(self.get_parameter("command_timeout").value)
        command_fresh = self.enabled and self.last_channels is not None and (
            0.0 <= age <= command_timeout
        )
        message.data = (
            f"enabled={self.enabled} connected={self.state.connected} "
            f"armed={self.state.armed} command_age={age:.2f}s "
            f"command_fresh={str(command_fresh).lower()} "
            f"physical_rc_priority={physical_rc} sys_status_age={sys_status_age:.2f}s"
        )
        self.status_publisher.publish(message)


def main(args=None):
    rclpy.init(args=args)
    node = RCBridge()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.try_shutdown()


if __name__ == "__main__":
    main()
