"""Deterministic execution of flight/Offboard logic, without ROS or motors."""
import ast
import math
from pathlib import Path
import sys
from types import SimpleNamespace as NS
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "ros2"))
from open32drone_driver.control_math import StableDurationGate, position_is_stable


def methods(file, class_name, selected, scope):
    tree = ast.parse((ROOT / "ros2/open32drone_driver" / file).read_text(encoding="utf-8"))
    original = next(n for n in tree.body if isinstance(n, ast.ClassDef) and n.name == class_name)
    cls = ast.ClassDef(name=class_name, bases=[], keywords=[], decorator_list=[],
                       body=[n for n in original.body if isinstance(n, ast.FunctionDef)
                             and n.name in selected])
    code = ast.fix_missing_locations(ast.Module(body=[cls], type_ignores=[]))
    exec(compile(code, file, "exec"), scope)
    return scope[class_name]


class FlightFlowTests(unittest.TestCase):
    def setUp(self):
        self.now = 100.0
        self.update = lambda n: None
        def spin(node, timeout_sec):
            self.now += max(0.001, timeout_sec)
            node.state_received_at = self.now
            node.position_received_at = self.now
            self.update(node)
        cls = methods("flight_test.py", "FlightTest",
                      {"wait_stable", "hover", "check_live", "stable_at", "landed"},
                      {"time": NS(monotonic=lambda: self.now), "math": math,
                       "rclpy": NS(spin_once=spin), "StableDurationGate": StableDurationGate,
                       "position_is_stable": position_is_stable,
                       "ExtendedState": NS(LANDED_STATE_ON_GROUND=1)})
        self.node = cls()
        self.node.state = NS(connected=True, armed=True, mode="CMODE(5)")
        self.node.position = (0.0, 0.0, 0.65)
        self.node.speed = (0.0, 0.0, 0.0)
        self.node.require_offboard = False
        self.node.state_received_at = self.node.position_received_at = self.now

    def test_flying_through_goal_is_not_arrival(self):
        self.node.speed = (0.3, 0.0, 0.0)
        with self.assertRaisesRegex(RuntimeError, "not reached and settled"):
            self.node.wait_stable((0, 0, 0.65), 0.1)

    def test_waits_for_continuous_stop_not_just_position(self):
        self.update = lambda n: setattr(n, "speed", (0.3 if self.now < 100.5 else 0, 0, 0))
        self.node.wait_stable((0, 0, 0.65), 0.1)
        self.assertGreaterEqual(self.now, 101.5)

    def test_stale_feedback_aborts_before_next_goal(self):
        self.update = lambda n: setattr(n, "position_received_at", self.now-1)
        with self.assertRaisesRegex(RuntimeError, "stale"):
            self.node.wait_stable((0, 0, 0.65), 0.1)

    def test_hover_drift_is_failure(self):
        self.node.position = (0.3, 0.0, 0.65)
        with self.assertRaisesRegex(RuntimeError, "left tolerance"):
            self.node.hover((0, 0, 0.65), 5, 0.1)

    def test_lost_offboard_aborts(self):
        self.node.require_offboard = True
        self.node.offboard = "phase=IDLE"
        with self.assertRaisesRegex(RuntimeError, "ownership lost"):
            self.node.check_live()

    def test_landing_needs_fresh_state_and_ground_confirmation(self):
        self.node.state.armed = False
        self.node.extended_state = NS(landed_state=1)
        self.node.extended_state_received_at = self.now-2
        self.assertFalse(self.node.landed())
        self.node.extended_state_received_at = self.now
        self.assertTrue(self.node.landed())


class OffboardFlowTests(unittest.TestCase):
    def setUp(self):
        cls = methods("offboard_control_node.py", "OffboardControl",
                      {"_mode_response", "_control_tick", "_state_callback", "_request_mode",
                       "_begin_start"},
                      {"MODE_POSITION_HOLD": 5, "MODE_AUTO": 3,
                       "MAV_CMD_DO_SET_MODE": 176, "CommandLong": NS(Request=NS)})
        self.node = cls()
        n = self.node
        n.phase = "WARMUP"
        n.state = NS(connected=True, armed=True, mode="CMODE(5)")
        n._now = lambda: 100.0
        n.get_parameter = lambda key: NS(value={"activation_timeout": 3, "command_timeout": .5,
                                              "warmup_time": .6}[key])
        n.get_logger = lambda: NS(info=lambda s: None, warning=lambda s: None, error=lambda s: None)
        n._pose_fresh = lambda: True
        n.operator_command_active = False
        n._make_setpoint = lambda: "setpoint"
        self.sent = []
        n.setpoint_publisher = NS(publish=self.sent.append)
        self.modes = []
        self.request_mode = n._request_mode
        n._request_mode = lambda mode, purpose: self.modes.append((mode, purpose))

    def test_mode_command_requires_real_ack_for_generic_autopilot(self):
        requests = []
        self.node.command_client = NS(call_async=lambda r: (
            requests.append(r) or NS(add_done_callback=lambda cb: None)))
        self.request_mode(3, "activate")
        self.assertEqual(requests[0].confirmation, 1)
        self.assertFalse(requests[0].broadcast)

    def test_prepare_ack_starts_warmup_clock(self):
        self.node.phase = "PREPARING"
        self.node._mode_response(NS(result=lambda: NS(success=True, result=0)), "prepare")
        self.assertEqual(self.node.phase, "WARMUP")
        self.assertAlmostEqual(self.node.next_auto_request_at, 100.6)

    def test_denied_prepare_never_starts_stream(self):
        self.node.phase = "PREPARING"
        self.node._mode_response(NS(result=lambda: NS(success=False, result=2)), "prepare")
        self.node._control_tick()
        self.assertEqual(self.node.phase, "IDLE")
        self.assertEqual(self.sent, [])

    def test_denied_auto_continues_bounded_warmup(self):
        self.node.activation_deadline = 103.0
        self.node._mode_response(NS(result=lambda: NS(success=False, result=2)), "activate")
        self.assertEqual(self.node.phase, "WARMUP")
        self.assertEqual(self.node.next_auto_request_at, 100.25)
        self.node._current_position = lambda: [0, 0, .65]
        self.node.position_goal = [0, 0, .65]
        self.node._control_tick()
        self.assertEqual(self.sent, ["setpoint"])

    def test_ack_after_pose_timeout_does_not_restart_stream(self):
        self.node.phase = "IDLE"
        self.ack()
        self.assertEqual(self.node.phase, "IDLE")

    def test_restart_waits_for_previous_transaction(self):
        self.node.phase = "IDLE"
        self.node.mode_request_pending = True
        success, _ = self.node._begin_start()
        self.assertFalse(success)
        self.assertEqual(self.modes, [])

    def test_disconnect_does_not_forget_pending_transaction(self):
        self.node.mode_request_pending = True
        self.node._state_callback(NS(connected=False, armed=False, mode=""))
        self.assertEqual(self.node.phase, "IDLE")
        self.assertTrue(self.node.mode_request_pending)
        self.ack()
        self.assertFalse(self.node.mode_request_pending)
        self.assertEqual(self.node.phase, "IDLE")

    def test_rejected_activation_cannot_retry_forever(self):
        self.node.activation_deadline = 99.0
        self.node._mode_response(NS(result=lambda: NS(success=False, result=2)), "activate")
        self.assertEqual(self.node.phase, "IDLE")

    def test_all_lifecycle_command_long_paths_require_ack(self):
        tree = ast.parse((ROOT / "ros2/open32drone_driver/flight_manager_node.py").read_text(encoding="utf-8"))
        assignments = [n for n in ast.walk(tree) if isinstance(n, ast.Assign)
                       and any(isinstance(t, ast.Attribute) and t.attr == "confirmation"
                               for t in n.targets)]
        self.assertEqual(len(assignments), 2)
        self.assertTrue(all(isinstance(n.value, ast.Constant) and n.value.value == 1
                            for n in assignments))

    def ack(self):
        self.node._mode_response(NS(result=lambda: NS(success=True, result=0)), "activate")

    def test_ack_does_not_mean_active(self):
        self.ack()
        self.assertEqual(self.node.phase, "CONFIRMING")
        self.node._control_tick()
        self.assertEqual(self.node.phase, "CONFIRMING")
        self.assertEqual(self.sent, ["setpoint"])

    def test_heartbeat_confirms_active(self):
        self.ack()
        self.node.state.mode = "CMODE(3)"
        self.node._control_tick()
        self.assertEqual(self.node.phase, "ACTIVE")

    def test_missing_confirmation_requests_hold(self):
        self.ack()
        self.node._now = lambda: 104.0
        self.node._control_tick()
        self.assertEqual(self.modes, [(5, "stop")])
        self.assertEqual(self.node.phase, "STOPPING")

    def test_late_ack_cannot_reactivate_disarmed_vehicle(self):
        self.node.state.armed = False
        self.ack()
        self.assertEqual(self.node.phase, "IDLE")

    def test_manual_mode_change_releases_ros(self):
        self.node.phase = "ACTIVE"
        self.node._state_callback(NS(connected=True, armed=True, mode="CMODE(2)"))
        self.assertEqual(self.node.phase, "IDLE")


class VelocityCLITests(unittest.TestCase):
    def test_messages_do_not_accelerate_publish_rate(self):
        now = [100.0]
        # Simulate many incoming callbacks that wake spin_once immediately.
        ros = NS(ok=lambda: True, spin_once=lambda node, timeout_sec: now.__setitem__(0, now[0]+.001))
        cls = methods("control_cli.py", "ControlCLI", {"publish_repeated"},
                      {"time": NS(monotonic=lambda: now[0]), "rclpy": ros})
        sent = []
        cls().publish_repeated(NS(publish=lambda msg: sent.append(now[0])), None, .2)
        self.assertEqual(len(sent), 4)
        self.assertTrue(all(b-a >= .05 for a, b in zip(sent, sent[1:])))

    def test_velocity_reports_loss_instead_of_silent_success(self):
        cls = methods("control_cli.py", "ControlCLI", {"check_velocity_control"},
                      {"time": NS(monotonic=lambda: 100.0)})
        n = cls()
        n.status_received_at = {"offboard": 99.9}
        n.status = {"offboard": "phase=IDLE connected=True armed=False"}
        with self.assertRaisesRegex(RuntimeError, "velocity test failed"):
            n.check_velocity_control()
