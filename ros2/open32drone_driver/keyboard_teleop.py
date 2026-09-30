"""One-key-at-a-time teaching control over the existing ROS flight interface."""

import argparse
import math
import os
import select
import sys
import termios
import time
import tty

import rclpy
from geometry_msgs.msg import PoseStamped, Twist
from mavros_msgs.msg import State
from rclpy.qos import HistoryPolicy, QoSProfile, ReliabilityPolicy
from sensor_msgs.msg import Range

from .control_cli import ControlCLI
from .keyboard_core import BODY_MOTION, position_step, velocity_for_key, yaw_from_quaternion
from .names import DEFAULT_ROBOT_NAME


class KeyboardTeleop(ControlCLI):
    def __init__(self, args):
        super().__init__(args.robot_name, args.frame_prefix, "open32drone_keyboard")
        self.args = args
        self.state = None
        self.pose = None
        self.range = None
        self.received_at = {}
        self.phase = "GROUND"
        self.motion_mode = "VELOCITY"
        self.pulse_until = 0.0
        self.pulse_values = (0.0, 0.0, 0.0, 0.0)
        self.takeoff_target_z = 0.0
        self.phase_deadline = 0.0
        self.ready_since = 0.0
        self.last_lifecycle_key = None
        reliable = QoSProfile(reliability=ReliabilityPolicy.RELIABLE,
                              history=HistoryPolicy.KEEP_LAST, depth=10)
        self.create_subscription(State, "state", lambda msg: self._record("state", msg), reliable)
        self.create_subscription(PoseStamped, "pose", lambda msg: self._record("pose", msg), reliable)
        self.create_subscription(Range, "range/downward", lambda msg: self._record("range", msg), reliable)

    def _record(self, name, message):
        setattr(self, name, message)
        self.received_at[name] = time.monotonic()

    def _fresh(self, name, age):
        return time.monotonic() - self.received_at.get(name, 0.0) <= age

    def _status_fresh(self, name, age=1.5):
        return self._fresh_status(name, age) and bool(self.status.get(name))

    def _fresh_status(self, name, age):
        return time.monotonic() - self.status_received_at.get(name, 0.0) <= age

    def _owner_clear(self, quiet=False):
        if not self._status_fresh("rc"):
            if not quiet:
                print("拒绝操作：RC 状态过期")
            return False
        rc = self.status["rc"]
        if "physical_rc_priority=true" in rc or "physical_rc_priority=unknown" in rc:
            if not quiet:
                print("拒绝操作：物理遥控器优先或优先状态未知")
            return False
        if "enabled=True" in rc or "command_fresh=true" in rc:
            if not quiet:
                print("拒绝操作：ROS RC 流仍在发送")
            return False
        return True

    def _state_ready(self, quiet=False):
        pose_values = ()
        if self.pose is not None:
            position = self.pose.pose.position
            orientation = self.pose.pose.orientation
            pose_values = (position.x, position.y, position.z,
                           orientation.x, orientation.y, orientation.z, orientation.w)
        if (not self._fresh("state", 1.5) or not self.state.connected
                or not self._fresh("pose", 0.5)
                or self.pose.header.frame_id != self.odom_frame_id
                or not all(math.isfinite(value) for value in pose_values)):
            if not quiet:
                print("拒绝操作：连接或位置反馈过期")
            return False
        return True

    def _motion_ready(self):
        return (self.phase == "ACTIVE" and self._state_ready(quiet=True)
                and self.state.armed and self.state.mode in ("AUTO", "CMODE(3)")
                and self._status_fresh("offboard", 0.7)
                and "phase=ACTIVE" in self.status["offboard"]
                and self._owner_clear(quiet=True))

    def _stop_pulse(self):
        if self.pulse_until:
            self.velocity_publisher.publish(Twist())
        self.pulse_until = 0.0

    def _send_velocity(self, values):
        message = Twist()
        message.linear.x, message.linear.y, message.linear.z, message.angular.z = values
        self.velocity_publisher.publish(message)

    def _takeoff(self):
        if self.phase != "GROUND" or not self._state_ready() or self.state.armed:
            print("拒绝起飞：必须已连接、在地面且未解锁")
            return
        if not self._fresh("range", 0.7) or not math.isfinite(self.range.range):
            print("拒绝起飞：向下 ToF 数据过期")
            return
        if not self._status_fresh("rc") or "physical_rc_priority=false" not in self.status["rc"]:
            print("拒绝起飞：物理 RC 控制权未知或已占用")
            return
        if self.command("rc stop") != 0:
            return
        try:
            self.wait_for(lambda: self._owner_clear(quiet=True), 2.0, "ROS RC stop")
        except RuntimeError as error:
            print(error)
            return
        self.takeoff_target_z = self.pose.pose.position.z + self.args.height
        if self.command(f"takeoff {self.args.height:g}") == 0:
            self.phase = "TAKING_OFF"
            self.phase_deadline = time.monotonic() + 30.0
            self.ready_since = 0.0
            print("已收到起飞应答；等待定点模式和高度稳定，期间移动键无效。")
        else:
            self.phase = "UNKNOWN"
            print("起飞结果不确定；检查现场与状态，可按 g 请求降落。")

    def _start_offboard(self):
        if self.phase != "READY" or not self._state_ready() or not self.state.armed:
            print("起飞尚未确认完成，不能启用 Offboard")
            return
        if self.state.mode not in ("POS_HOLD", "CMODE(5)") or not self._owner_clear():
            print("定点模式或控制权条件不满足")
            return
        if self.command("offboard start") == 0:
            self.phase = "PREPARING"
            self.phase_deadline = time.monotonic() + 10.0
            print("等待 Offboard ACTIVE；预热期间不执行移动键。")

    def _land(self):
        if self.phase in ("GROUND", "LANDING"):
            print("当前已在地面或正在降落")
            return
        self._stop_pulse()
        if self.phase in ("ACTIVE", "PREPARING", "UNKNOWN"):
            if self.command("offboard stop") == 0:
                try:
                    self.wait_for(lambda: self._status_fresh("offboard", 0.7)
                                  and "phase=IDLE" in self.status["offboard"],
                                  6.0, "Offboard release before landing")
                except RuntimeError:
                    print("Offboard 释放未确认；继续尝试降落，请监护现场。")
            else:
                print("Offboard 释放被拒；继续尝试降落，请监护现场。")
        if self.command("rc stop") != 0:
            print("ROS RC 停流失败，暂不发送降落命令")
            return
        if self.command("land") == 0:
            self.phase = "LANDING"
            self.phase_deadline = time.monotonic() + 45.0
            print("已收到降落应答；等待上锁与落地反馈。")
        else:
            self.phase = "UNKNOWN"
            print("降落应答失败；检查飞机状态并保留物理急停手段。")

    def _quit(self):
        self._stop_pulse()
        if self.phase == "ACTIVE":
            if self.command("offboard stop") != 0:
                print("Offboard 释放失败，程序保持运行；可按 g 降落。")
                return False
            try:
                self.wait_for(lambda: self._status_fresh("offboard", 0.7)
                              and "phase=IDLE" in self.status["offboard"]
                              and self._fresh("state", 1.5)
                              and self.state.mode in ("POS_HOLD", "CMODE(5)"),
                              6.0, "Position Hold handoff")
            except RuntimeError as error:
                print(f"{error}；程序保持运行，可按 g 降落。")
                return False
            self.phase = "READY"
        if self.state is not None and self.state.armed and self.phase != "READY":
            print("飞行状态尚未安全交接；先按 g 降落或确认状态。")
            return False
        return True

    def _motion(self, key):
        if not self._motion_ready():
            print("移动被拒：需要新鲜状态、Offboard ACTIVE 和无 RC 争用")
            self._stop_pulse()
            return
        if self.motion_mode == "POSITION":
            if key in ("j", "l"):
                print("位置模式不执行偏航；按 v 切回速度模式")
                return
            pose = self.pose.pose
            x, y, z = position_step(pose.position, yaw_from_quaternion(pose.orientation),
                                    key, self.args.step)
            goal = PoseStamped()
            goal.header.frame_id = self.odom_frame_id
            goal.header.stamp = self.get_clock().now().to_msg()
            goal.pose.position.x, goal.pose.position.y, goal.pose.position.z = x, y, z
            goal.pose.orientation.w = 1.0
            self.position_publisher.publish(goal)
            print(f"绝对目标 {self.odom_frame_id}: ({x:.2f}, {y:.2f}, {z:.2f})")
        else:
            self.pulse_values = velocity_for_key(key, self.args.horizontal,
                                                  self.args.vertical, self.args.yaw_rate)
            self.pulse_until = time.monotonic() + self.args.pulse
            self._send_velocity(self.pulse_values)

    def handle_key(self, key):
        if key in ("\x03", "\x04"):
            key = "q"
        key = key.lower()
        if key not in ("t", "o", "g", "q"):
            self.last_lifecycle_key = None
        elif self.last_lifecycle_key == key:
            return False
        else:
            self.last_lifecycle_key = key
        if key == "t":
            self._takeoff()
        elif key == "o":
            self._start_offboard()
        elif key == "g":
            self._land()
        elif key == "q":
            return self._quit()
        elif key in ("v", "p"):
            self._stop_pulse()
            self.motion_mode = "VELOCITY" if key == "v" else "POSITION"
            print(f"控制模式：{self.motion_mode}")
        elif key in BODY_MOTION:
            self._motion(key)
        elif key == " ":
            self._stop_pulse()
            if self.phase == "ACTIVE" and self.motion_mode == "POSITION" and self._motion_ready():
                pose = self.pose.pose
                goal = PoseStamped()
                goal.header.frame_id = self.odom_frame_id
                goal.header.stamp = self.get_clock().now().to_msg()
                goal.pose.position = pose.position
                goal.pose.orientation.w = 1.0
                self.position_publisher.publish(goal)
            print("停止新运动，保持当前位置")
        return False

    def tick(self):
        now = time.monotonic()
        if self.phase == "TAKING_OFF":
            settled = (self._fresh("state", 1.5) and self._fresh("pose", 0.5)
                       and self.state.armed and self.state.mode in ("POS_HOLD", "CMODE(5)")
                       and self.pose.pose.position.z >= self.takeoff_target_z - 0.15)
            if settled:
                if not self.ready_since:
                    self.ready_since = now
                elif now - self.ready_since >= 0.5:
                    self.phase = "READY"
                    print("起飞完成并进入定点；按 o 启用 Offboard。")
            else:
                self.ready_since = 0.0
            if now >= self.phase_deadline and self.phase == "TAKING_OFF":
                self.phase = "UNKNOWN"
                print("起飞完成确认超时；禁止移动，检查状态或按 g 降落。")
        elif self.phase == "PREPARING":
            if (self._status_fresh("offboard", 0.7)
                    and "phase=ACTIVE" in self.status["offboard"]
                    and self._fresh("state", 1.5) and self.state.armed
                    and self.state.mode in ("AUTO", "CMODE(3)")):
                self.phase = "ACTIVE"
                print("Offboard ACTIVE；可使用移动键。")
            elif now >= self.phase_deadline:
                self.phase = "UNKNOWN"
                print("Offboard 激活超时；禁止移动，检查状态或按 g 降落。")
        elif self.phase == "LANDING":
            if (self._fresh("state", 1.5) and not self.state.armed
                    and self._status_fresh("flight")
                    and "landed_state=1" in self.status["flight"]):
                self.phase = "GROUND"
                print("已上锁并确认落地。")
            elif now >= self.phase_deadline:
                self.phase = "UNKNOWN"
                print("降落完成确认超时；检查飞机状态。")
        if self.pulse_until:
            if now >= self.pulse_until or not self._motion_ready():
                self._stop_pulse()
            else:
                self._send_velocity(self.pulse_values)
        if self.phase == "ACTIVE" and not self._motion_ready():
            self._stop_pulse()
            self.phase = "UNKNOWN"
            print("Offboard 状态或控制权丢失；已停止新运动，检查状态或按 g 降落。")


def parse_args(argv=None):
    parser = argparse.ArgumentParser(description="Open32Drone supervised keyboard control")
    parser.add_argument("--robot-name", default=DEFAULT_ROBOT_NAME)
    parser.add_argument("--frame-prefix", default=None)
    parser.add_argument("--height", type=float, default=0.65)
    parser.add_argument("--horizontal", type=float, default=0.20)
    parser.add_argument("--vertical", type=float, default=0.15)
    parser.add_argument("--yaw-rate", type=float, default=0.40)
    parser.add_argument("--step", type=float, default=0.20)
    parser.add_argument("--pulse", type=float, default=0.30)
    args = parser.parse_args(argv)
    for name, minimum, maximum in (("height", 0.2, 5.8), ("horizontal", 0.01, 0.7),
                                   ("vertical", 0.01, 0.35), ("yaw_rate", 0.01, 1.0),
                                   ("step", 0.01, 0.8), ("pulse", 0.05, 0.45)):
        value = getattr(args, name)
        if not math.isfinite(value) or not minimum <= value <= maximum:
            parser.error(f"--{name.replace('_', '-')} must be within [{minimum}, {maximum}]")
    return args


def main(argv=None):
    args = parse_args(argv)
    if not sys.stdin.isatty():
        print("keyboard requires an interactive terminal", file=sys.stderr)
        return 2
    rclpy.init(args=[])
    node = KeyboardTeleop(args)
    old_term = termios.tcgetattr(sys.stdin.fileno())
    try:
        tty.setcbreak(sys.stdin.fileno())
        print("t 起飞 | o Offboard | v 速度 / p 位置 | WASD 前后左右 | i/k 升降 | j/l 偏航")
        print("空格 停止 | g 降落 | q 释放 Offboard 后退出；物理遥控器急停始终独立。")
        while rclpy.ok():
            try:
                rclpy.spin_once(node, timeout_sec=0.0)
                node.tick()
                ready, _, _ = select.select([sys.stdin], [], [], 0.05)
                if ready:
                    chars = os.read(sys.stdin.fileno(), 64).decode("utf-8", "ignore")
                    for key in chars:
                        if node.handle_key(key):
                            return 0
            except KeyboardInterrupt:
                if node._quit():
                    return 0
    finally:
        termios.tcsetattr(sys.stdin.fileno(), termios.TCSADRAIN, old_term)
        node._stop_pulse()
        node.destroy_node()
        rclpy.try_shutdown()
    return 0


if __name__ == "__main__":
    sys.exit(main())
