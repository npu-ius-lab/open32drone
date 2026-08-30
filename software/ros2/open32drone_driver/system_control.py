"""Keep the Open32Drone launch alive independently of a terminal session."""

import argparse
import os
from pathlib import Path
import signal
import subprocess
import sys
import time

from .names import DEFAULT_ROBOT_NAME, frame_prefix, robot_name


STATE_ROOT = Path.home() / ".local" / "state" / "open32drone"


def _paths(name):
    state_dir = STATE_ROOT / robot_name(name)
    return state_dir, state_dir / "driver.pid", state_dir / "driver.log"


def _read_pid(pid_file):
    try:
        return int(pid_file.read_text(encoding="ascii").strip())
    except (FileNotFoundError, ValueError):
        return None


def _running(pid):
    if not pid:
        return False
    try:
        os.kill(pid, 0)
        return True
    except ProcessLookupError:
        return False
    except PermissionError:
        return True


def _process_table():
    table = {}
    for entry in Path("/proc").iterdir():
        if not entry.name.isdigit():
            continue
        try:
            stat = (entry / "stat").read_text(encoding="ascii")
            command = (entry / "cmdline").read_bytes().replace(b"\0", b" ").decode(
                errors="replace"
            )
            # The process name may contain spaces inside parentheses. Everything
            # after the final ')' starts with state, then parent PID.
            parent = int(stat.rsplit(")", 1)[1].split()[1])
            table[int(entry.name)] = (parent, command)
        except (FileNotFoundError, PermissionError, ValueError):
            continue
    return table


def _descendants(pid, table):
    result = []
    pending = [pid]
    while pending:
        parent = pending.pop()
        children = [child for child, item in table.items() if item[0] == parent]
        result.extend(children)
        pending.extend(children)
    return result


def _driver_processes(table, name):
    own_pid = os.getpid()
    namespace = robot_name(name)
    matches = []
    for pid, (_parent, command) in table.items():
        if pid == own_pid:
            continue
        owns_namespace = (
            f"robot_name:={namespace}" in command
            or f"__ns:=/{namespace}" in command
            or f"__ns:=/{namespace}/UAS1" in command
        )
        is_stack = (
            "/open32drone_driver/" in command
            or "mavros_node" in command
            or "ros2 launch open32drone_driver open32drone" in command
        )
        if owns_namespace and is_stack:
            matches.append(pid)
    return matches


def _stop_targets(recorded_pid, table, name):
    """Return only processes proven to belong to this aircraft's ROS stack."""
    managed = set(_driver_processes(table, name))
    if recorded_pid in managed:
        managed.update(_descendants(recorded_pid, table))
    return managed


def start(args):
    namespace = robot_name(args.robot_name)
    tf_prefix = frame_prefix(args.frame_prefix, namespace)
    state_dir, pid_file, log_file = _paths(namespace)
    pid = _read_pid(pid_file)
    table = _process_table()
    managed = set(_driver_processes(table, namespace))
    if pid in managed:
        print(f"{namespace} driver is already running (pid {pid})")
        return 0
    # A PID can be reused after a reboot. Never trust the number alone: discard
    # the stale record unless the current process command still identifies this
    # aircraft's launch tree.
    if pid is not None:
        pid_file.unlink(missing_ok=True)
    if managed:
        print(
            f"Refusing to start while stale driver processes exist: {sorted(managed)}; "
            f"run system stop --robot-name {namespace}",
            file=sys.stderr,
        )
        return 1

    state_dir.mkdir(parents=True, exist_ok=True)
    command = [
        "ros2",
        "launch",
        "open32drone_driver",
        "open32drone.launch.py",
        f"robot_name:={namespace}",
        f"frame_prefix:={tf_prefix}",
        f"aircraft_ip:={args.aircraft_ip}",
        f"mav_sys_id:={args.mav_sys_id}",
        f"local_udp_port:={args.local_udp_port}",
        f"use_rviz:={'true' if args.use_rviz else 'false'}",
    ]
    with log_file.open("wb", buffering=0) as log:
        process = subprocess.Popen(
            command,
            stdin=subprocess.DEVNULL,
            stdout=log,
            stderr=subprocess.STDOUT,
            start_new_session=True,
        )
    time.sleep(2.0)
    if process.poll() is not None:
        print(f"Driver exited during startup; inspect {log_file}", file=sys.stderr)
        return 1
    pid_file.write_text(f"{process.pid}\n", encoding="ascii")
    print(f"{namespace} driver started (pid {process.pid}, log {log_file})")
    return 0


def stop(name):
    namespace = robot_name(name)
    _state_dir, pid_file, _log_file = _paths(namespace)
    pid = _read_pid(pid_file)
    table = _process_table()
    targets = _stop_targets(pid, table, namespace)
    if not targets:
        pid_file.unlink(missing_ok=True)
        print(f"{namespace} driver is not running")
        return 0
    for target in sorted(targets, reverse=True):
        try:
            os.kill(target, signal.SIGTERM)
        except ProcessLookupError:
            pass
    deadline = time.monotonic() + 8.0
    while any(_running(target) for target in targets) and time.monotonic() < deadline:
        time.sleep(0.1)
    remaining = [target for target in targets if _running(target)]
    if remaining:
        print(f"Driver processes did not stop within 8 s: {remaining}", file=sys.stderr)
        return 1
    pid_file.unlink(missing_ok=True)
    print(f"{namespace} driver stopped")
    return 0


def status(name):
    namespace = robot_name(name)
    _state_dir, pid_file, log_file = _paths(namespace)
    pid = _read_pid(pid_file)
    managed = set(_driver_processes(_process_table(), namespace))
    if pid in managed:
        print(f"{namespace}: running pid={pid} log={log_file}")
        return 0
    if managed:
        print(
            f"{namespace}: unmanaged launch processes={sorted(managed)} "
            f"log={log_file}"
        )
        return 0
    print(f"{namespace}: stopped log={log_file}")
    return 1


def main(args=None):
    parser = argparse.ArgumentParser(description="Open32Drone driver process control")
    subcommands = parser.add_subparsers(dest="command", required=True)
    start_parser = subcommands.add_parser("start")
    start_parser.add_argument("--robot-name", default=DEFAULT_ROBOT_NAME)
    start_parser.add_argument("--frame-prefix", default=None)
    start_parser.add_argument("--aircraft-ip", default="192.168.4.1")
    start_parser.add_argument("--mav-sys-id", type=int, default=1)
    start_parser.add_argument("--local-udp-port", type=int, default=14550)
    start_parser.add_argument("--use-rviz", action="store_true")
    for command in ("stop", "status", "logs"):
        command_parser = subcommands.add_parser(command)
        command_parser.add_argument("--robot-name", default=DEFAULT_ROBOT_NAME)
    parsed = parser.parse_args(args)

    if parsed.command == "start":
        if not 1 <= parsed.mav_sys_id <= 255:
            parser.error("--mav-sys-id must be within [1, 255]")
        if not 1024 <= parsed.local_udp_port <= 65535:
            parser.error("--local-udp-port must be within [1024, 65535]")
        return start(parsed)
    if parsed.command == "stop":
        return stop(parsed.robot_name)
    if parsed.command == "status":
        return status(parsed.robot_name)
    _state_dir, _pid_file, log_file = _paths(parsed.robot_name)
    print(log_file)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
