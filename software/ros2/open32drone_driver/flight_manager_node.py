"""High-level, safety-oriented Open32Drone MAVROS command bridge."""

import json
import math

import rclpy
from mavros_msgs.msg import ExtendedState, State
from mavros_msgs.srv import CommandBool, CommandLong
from rclpy.callback_groups import MutuallyExclusiveCallbackGroup
from rclpy.node import Node
from rclpy.qos import QoSProfile, ReliabilityPolicy, qos_profile_sensor_data
from std_msgs.msg import String
from std_srvs.srv import Trigger

MAV_CMD_NAV_LAND = 21
MAV_CMD_NAV_TAKEOFF = 22
MAV_CMD_DO_SET_MODE = 176

MODE_NAMES = {
    "stab": 2,
    "stabilize": 2,
    "alt": 4,
    "alt_hold": 4,
    "altitude": 4,
    "pos": 5,
    "pos_hold": 5,
    "position": 5,
}


class FlightManager(Node):
    def __init__(self):
        super().__init__("open32drone_flight_manager")
        self.declare_parameter("takeoff_height", 0.65)
        self.declare_parameter("requested_mode", 5)
        self.declare_parameter("arming_service", "UAS1/cmd/arming")
        self.declare_parameter("command_service", "UAS1/cmd/command")
        self.state = State()
        self.extended_state = ExtendedState()
        # Service callbacks await MAVROS/bridge service clients. Keep the incoming
        # service and outgoing client entities in separate groups so the client
        # response can run while a flight command is suspended, while serializing
        # operator flight commands within their own group.
        self.service_callback_group = MutuallyExclusiveCallbackGroup()
        self.client_callback_group = MutuallyExclusiveCallbackGroup()
        self.arm_client = self.create_client(
            CommandBool,
            self.get_parameter("arming_service").value,
            callback_group=self.client_callback_group,
        )
        self.command_client = self.create_client(
            CommandLong,
            self.get_parameter("command_service").value,
            callback_group=self.client_callback_group,
        )
        self.trigger_clients = {
            "offboard_start": self.create_client(
                Trigger, "offboard/start", callback_group=self.client_callback_group
            ),
            "offboard_stop": self.create_client(
                Trigger, "offboard/stop", callback_group=self.client_callback_group
            ),
            "rc_start": self.create_client(
                Trigger, "rc/start", callback_group=self.client_callback_group
            ),
            "rc_stop": self.create_client(
                Trigger, "rc/stop", callback_group=self.client_callback_group
            ),
        }
        state_qos = QoSProfile(
            reliability=ReliabilityPolicy.RELIABLE,
            depth=1,
        )
        self.create_subscription(State, "state", self._state_callback, state_qos)
        self.create_subscription(
            ExtendedState,
            "UAS1/extended_state",
            self._extended_state_callback,
            qos_profile_sensor_data,
        )
        self.create_subscription(String, "command", self._topic_command, 10)
        self.status_publisher = self.create_publisher(String, "flight/status", 10)
        self.command_result_publisher = self.create_publisher(
            String, "command/result", 10
        )
        self.create_timer(0.5, self._publish_status)
        self.create_service(
            Trigger, "flight/arm", self._arm,
            callback_group=self.service_callback_group,
        )
        self.create_service(
            Trigger, "flight/disarm", self._disarm,
            callback_group=self.service_callback_group,
        )
        self.create_service(
            Trigger, "flight/takeoff", self._takeoff,
            callback_group=self.service_callback_group,
        )
        self.create_service(
            Trigger, "flight/land", self._land,
            callback_group=self.service_callback_group,
        )
        self.create_service(
            Trigger, "flight/set_mode", self._set_mode,
            callback_group=self.service_callback_group,
        )
        self.create_service(
            Trigger, "flight/emergency_stop", self._emergency_stop,
            callback_group=self.service_callback_group,
        )
        # Short aliases keep teleoperation scripts as simple as a mobile robot.
        self.create_service(
            Trigger, "arm", self._arm, callback_group=self.service_callback_group
        )
        self.create_service(
            Trigger, "disarm", self._disarm, callback_group=self.service_callback_group
        )
        self.create_service(
            Trigger, "takeoff", self._takeoff, callback_group=self.service_callback_group
        )
        self.create_service(
            Trigger, "land", self._land, callback_group=self.service_callback_group
        )
        self.create_service(
            Trigger, "emergency_stop", self._emergency_stop,
            callback_group=self.service_callback_group,
        )

    def _state_callback(self, message):
        self.state = message

    def _extended_state_callback(self, message):
        self.extended_state = message

    def _publish_status(self):
        message = String()
        message.data = (
            f"connected={self.state.connected} armed={self.state.armed} "
            f"mode={self.state.mode} landed_state={self.extended_state.landed_state}"
        )
        self.status_publisher.publish(message)

    def _publish_command_result(self, command, success, message):
        result = String()
        result.data = json.dumps(
            {"command": command, "success": bool(success), "message": str(message)},
            ensure_ascii=False,
            separators=(",", ":"),
        )
        self.command_result_publisher.publish(result)

    @staticmethod
    def _normalize_action(tokens):
        action = tokens[0].lower().replace("-", "_")
        if action in ("offboard", "rc") and len(tokens) >= 2:
            action = f"{action}_{tokens[1].lower().replace('-', '_')}"
        if action in ("emergency", "estop"):
            action = "emergency_stop"
        return action

    def _topic_command(self, message):
        command = message.data.strip()
        tokens = command.split()
        if not tokens:
            self._publish_command_result(command, False, "empty command")
            return
        action = self._normalize_action(tokens)

        if action == "status":
            if len(tokens) != 1:
                self._publish_command_result(command, False, "usage: status")
                return
            self._publish_command_result(
                command,
                True,
                f"connected={self.state.connected} armed={self.state.armed} "
                f"mode={self.state.mode} landed_state={self.extended_state.landed_state}",
            )
            return
        if action in ("arm", "disarm", "emergency_stop"):
            if len(tokens) != 1:
                self._publish_command_result(command, False, f"usage: {tokens[0]}")
                return
            self._topic_set_armed(command, action == "arm")
            return
        if action == "takeoff":
            if len(tokens) > 2:
                self._publish_command_result(command, False, "usage: takeoff [height_m]")
                return
            try:
                height = (
                    float(tokens[1])
                    if len(tokens) == 2
                    else float(self.get_parameter("takeoff_height").value)
                )
            except ValueError:
                self._publish_command_result(command, False, "takeoff height must be numeric")
                return
            if not math.isfinite(height) or height < 0.2 or height > 5.8:
                self._publish_command_result(command, False, "takeoff height must be within [0.2, 5.8] m")
                return
            self._topic_send_command(command, MAV_CMD_NAV_TAKEOFF, {7: height})
            return
        if action == "land":
            if len(tokens) != 1:
                self._publish_command_result(command, False, "usage: land")
                return
            self._topic_land(command)
            return
        if action == "mode":
            if len(tokens) != 2:
                self._publish_command_result(command, False, "usage: mode <stabilize|altitude|position>")
                return
            requested = tokens[1].lower().replace("-", "_")
            mode = MODE_NAMES.get(requested)
            if mode not in (2, 4, 5):
                self._publish_command_result(command, False, f"unsupported mode: {tokens[1]}")
                return
            self._topic_send_command(command, MAV_CMD_DO_SET_MODE, {2: mode})
            return
        if action in self.trigger_clients:
            if len(tokens) != 2:
                self._publish_command_result(command, False, f"usage: {tokens[0]} <start|stop>")
                return
            self._topic_trigger(command, self.trigger_clients[action])
            return
        self._publish_command_result(
            command,
            False,
            "supported: status, arm, disarm, emergency_stop, takeoff [m], land, "
            "mode <name>, offboard start|stop, rc start|stop",
        )

    def _topic_set_armed(self, command, value):
        if not self.arm_client.service_is_ready():
            self._publish_command_result(command, False, f"service unavailable: {self.arm_client.srv_name}")
            return
        request = CommandBool.Request()
        request.value = value
        future = self.arm_client.call_async(request)
        future.add_done_callback(lambda result: self._topic_mavros_result(command, result))

    def _topic_send_command(self, command_text, command, params=None):
        if not self.command_client.service_is_ready():
            self._publish_command_result(
                command_text, False, f"service unavailable: {self.command_client.srv_name}"
            )
            return
        request = CommandLong.Request()
        request.broadcast = False
        request.command = command
        request.confirmation = 0
        params = params or {}
        for index in range(1, 8):
            setattr(request, f"param{index}", float(params.get(index, 0.0)))
        future = self.command_client.call_async(request)
        future.add_done_callback(lambda result: self._topic_mavros_result(command_text, result))

    def _topic_land(self, command):
        # Stop the ROS manual stream before requesting an automatic landing.
        # Firmware independently releases an active Offboard owner on LAND.
        client = self.trigger_clients["rc_stop"]
        if not client.service_is_ready():
            self._topic_send_command(command, MAV_CMD_NAV_LAND)
            return
        future = client.call_async(Trigger.Request())
        future.add_done_callback(
            lambda result: self._topic_land_after_rc_stop(command, result)
        )

    def _topic_land_after_rc_stop(self, command, future):
        try:
            future.result()
        except Exception as error:  # ROS service transport failure
            self.get_logger().warning(f"RC stop before landing failed: {error}")
        self._topic_send_command(command, MAV_CMD_NAV_LAND)

    def _topic_mavros_result(self, command, future):
        try:
            result = future.result()
            success = bool(result.success)
            message = f"FCU result={result.result}"
        except Exception as error:  # ROS service transport failure
            success = False
            message = str(error)
        self._publish_command_result(command, success, message)

    def _topic_trigger(self, command, client):
        if not client.service_is_ready():
            self._publish_command_result(command, False, f"service unavailable: {client.srv_name}")
            return
        future = client.call_async(Trigger.Request())
        future.add_done_callback(lambda result: self._topic_trigger_result(command, result))

    def _topic_trigger_result(self, command, future):
        try:
            result = future.result()
            success = bool(result.success)
            message = result.message
        except Exception as error:  # ROS service transport failure
            success = False
            message = str(error)
        self._publish_command_result(command, success, message)

    @staticmethod
    def _unavailable(response, name):
        response.success = False
        response.message = f"MAVROS service unavailable: {name}"
        return response

    async def _set_armed(self, value, response):
        if not self.arm_client.service_is_ready():
            return self._unavailable(response, self.arm_client.srv_name)
        request = CommandBool.Request()
        request.value = value
        result = await self.arm_client.call_async(request)
        response.success = bool(result.success)
        response.message = f"FCU result={result.result}"
        return response

    async def _arm(self, _request, response):
        return await self._set_armed(True, response)

    async def _disarm(self, _request, response):
        return await self._set_armed(False, response)

    async def _emergency_stop(self, _request, response):
        return await self._set_armed(False, response)

    async def _send_command(self, command, response, params=None):
        if not self.command_client.service_is_ready():
            return self._unavailable(response, self.command_client.srv_name)
        request = CommandLong.Request()
        request.broadcast = False
        request.command = command
        request.confirmation = 0
        params = params or {}
        for index in range(1, 8):
            setattr(request, f"param{index}", float(params.get(index, 0.0)))
        result = await self.command_client.call_async(request)
        response.success = bool(result.success)
        response.message = f"FCU result={result.result}"
        return response

    async def _takeoff(self, _request, response):
        height = float(self.get_parameter("takeoff_height").value)
        if not math.isfinite(height) or height < 0.2 or height > 5.8:
            response.success = False
            response.message = "takeoff_height must be finite and within [0.2, 5.8] m"
            return response
        return await self._send_command(MAV_CMD_NAV_TAKEOFF, response, {7: height})

    async def _land(self, _request, response):
        # Keep the service API identical to the text-command API. A live ROS RC
        # stream can otherwise cancel automatic landing as soon as its throttle
        # is above the cancellation threshold.
        client = self.trigger_clients["rc_stop"]
        if client.service_is_ready():
            try:
                await client.call_async(Trigger.Request())
            except Exception as error:  # ROS service transport failure
                self.get_logger().warning(f"RC stop before landing failed: {error}")
        return await self._send_command(MAV_CMD_NAV_LAND, response)

    async def _set_mode(self, _request, response):
        mode = int(self.get_parameter("requested_mode").value)
        if mode not in (2, 4, 5):
            response.success = False
            response.message = "requested_mode must be 2 (STAB), 4 (ALT), or 5 (POS)"
            return response
        return await self._send_command(MAV_CMD_DO_SET_MODE, response, {2: mode})


def main(args=None):
    rclpy.init(args=args)
    node = FlightManager()
    executor = rclpy.executors.MultiThreadedExecutor(num_threads=2)
    executor.add_node(node)
    try:
        executor.spin()
    except KeyboardInterrupt:
        pass
    finally:
        executor.shutdown()
        node.destroy_node()
        rclpy.try_shutdown()


if __name__ == "__main__":
    main()
