import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def control_source():
    return "\n".join(
        path.read_text(encoding="utf-8")
        for path in sorted((ROOT / "firmware").glob("control*.ino"))
    )


class ClientContractTests(unittest.TestCase):
    def test_android_camera_uses_aircraft_wifi_and_version_is_matched(self):
        java_root = ROOT / "android/app/src/main/java/com/osrbot/open32drone/controller"
        gradle = (ROOT / "android/app/build.gradle").read_text(encoding="utf-8")
        activity = (java_root / "MainActivity.java").read_text(encoding="utf-8")
        camera = (java_root / "MjpegStreamClient.java").read_text(encoding="utf-8")
        layout = (ROOT / "android/app/src/main/res/layout/activity_main.xml").read_text(encoding="utf-8")
        ros_manifest = (ROOT / "ros2/package.xml").read_text(encoding="utf-8")
        ros_setup = (ROOT / "ros2/setup.py").read_text(encoding="utf-8")
        self.assertIn('versionName "0.1"', gradle)
        self.assertIn("versionCode 1", gradle)
        self.assertIn('APP_BUILD_ID = "0.1 (1)"', activity)
        self.assertIn("<version>0.1.0</version>", ros_manifest)
        self.assertIn("<buildtool_depend>ament_python</buildtool_depend>", ros_manifest)
        self.assertIn("<exec_depend>launch</exec_depend>", ros_manifest)
        self.assertIn("<exec_depend>launch_ros</exec_depend>", ros_manifest)
        self.assertIn("version='0.1.0'", ros_setup)
        self.assertTrue((java_root / "MjpegFrameReader.java").exists())
        self.assertIn('android:id="@+id/cameraPreview"', layout)
        self.assertIn('android:id="@+id/cameraStatsText"', layout)
        self.assertIn('android:scaleType="fitCenter"', layout)
        self.assertNotIn('android:scaleType="centerCrop"', layout)
        self.assertIn("startCameraStream(aircraftNetwork);", activity)
        self.assertIn("stopCameraStream();", activity)
        self.assertIn("Network network", camera)
        self.assertIn("network.openConnection(new URL(streamUrl))", camera)
        self.assertIn("Process.THREAD_PRIORITY_BACKGROUND", camera)
        self.assertNotIn("Process.THREAD_PRIORITY_DEFAULT", camera)
        self.assertNotIn("new URL(streamUrl).openConnection()", camera)
        self.assertIn("State.RETRYING, error.getMessage()", camera)
        self.assertIn("camera_retrying_detail", activity)
        self.assertNotIn("mars", layout.lower())

    def test_android_keeps_one_control_screen_and_safety_actions(self):
        activity = (ROOT / "android/app/src/main/java/com/osrbot/open32drone/controller/MainActivity.java").read_text(encoding="utf-8")
        layout = (ROOT / "android/app/src/main/res/layout/activity_main.xml").read_text(encoding="utf-8")
        link = (ROOT / "android/app/src/main/java/com/osrbot/open32drone/controller/DroneLink.java").read_text(encoding="utf-8")
        policy = (ROOT / "android/app/src/main/java/com/osrbot/open32drone/controller/FlightControlPolicy.java").read_text(encoding="utf-8")
        strings = (ROOT / "android/app/src/main/res/values/strings.xml").read_text(encoding="utf-8")
        self.assertEqual(layout.count("JoystickView"), 2)
        for view in ("disarmButton", "takeoffButton", "landButton"):
            self.assertIn(f'android:id="@+id/{view}"', layout)
        self.assertNotIn('android:id="@+id/armButton"', layout)
        self.assertNotIn("requestDefaultPositionMode", activity)
        self.assertNotIn("requestArm()", activity)
        self.assertIn("STICK_EXPO = 0.30f", activity)
        self.assertIn("link.takeoffPosition(height)", activity)
        self.assertIn("长按定点起飞", strings)
        self.assertIn("DroneLink::emergencyDisarm", activity)
        self.assertIn("scheduleWithFixedDelay(this::transmitTick, 0, 40", link)
        self.assertNotIn("if (armed && !automatic) leftEnabled = true", activity)
        self.assertIn("if (!commandTracker.acknowledge(command)) break", link)
        self.assertIn("FlightControlPolicy.evaluate", activity)
        self.assertIn("policy.manualControlEnabled", activity)
        self.assertNotIn("pending || state.mode == 3", activity)
        self.assertIn("acceptedTakeoffTransition", policy)
        self.assertIn("MavlinkCodec.CMD_NAV_TAKEOFF", policy)
        self.assertIn("MavlinkCodec.CMD_NAV_LAND", policy)
        self.assertIn("controls = FlightControlMapper.ManualControl.neutral()", link)
        self.assertIn("sendSupersedingLand", link)
        self.assertIn("COMMAND_RETRY_INTERVAL_MS = 350", link)
        self.assertIn("COMMAND_MAX_ATTEMPTS = 3", link)
        self.assertIn("commandLongWithConfirmation", link)
        self.assertIn('case MavlinkCodec.CMD_NAV_TAKEOFF: return "Takeoff"', link)

    def test_android_does_not_duplicate_firmware_tof_takeoff_gate(self):
        java_root = ROOT / "android/app/src/main/java/com/osrbot/open32drone/controller"
        activity = (java_root / "MainActivity.java").read_text(encoding="utf-8")
        link = (java_root / "DroneLink.java").read_text(encoding="utf-8")
        control = control_source()
        takeoff_request = activity[
            activity.index("private void requestTakeoff()"):
            activity.index("private void updateStickGesture()")
        ]
        control_enablement = activity[
            activity.index("private void setControlsEnabled"):
            activity.index("private void updateSafetyHint")
        ]
        self.assertIn("ready = (health & PREARM_CHECK_BIT) != 0;", link)
        self.assertNotIn("&& heightSensorHealthy", link)
        self.assertNotIn("heightSensorHealthy", takeoff_request)
        self.assertNotIn("heightSensorHealthy", control_enablement)
        self.assertNotIn("!state.ready", takeoff_request)
        self.assertNotIn("connected && ready && phoneOwns", control_enablement)
        self.assertIn("takeoffButton.setEnabled(connected && phoneOwns", control_enablement)
        self.assertIn("landButton.setEnabled(connected);", control_enablement)
        self.assertIn("disarmButton.setEnabled(connected);", control_enablement)
        self.assertIn("if (!tofGroundReady())", control)
        self.assertIn('automaticFlightFailure = "ToF unavailable"', control)

    def test_android_takeoff_land_state_machine_has_no_zero_throttle_ack_race(self):
        java_root = ROOT / "android/app/src/main/java/com/osrbot/open32drone/controller"
        activity = (java_root / "MainActivity.java").read_text(encoding="utf-8")
        link = (java_root / "DroneLink.java").read_text(encoding="utf-8")
        takeoff = link[link.index("void takeoffPosition(float heightMeters)"):
                       link.index("void land()")]
        transmit = link[link.index("private void transmitTick()"):
                        link.index("private void receiveLoop()")]
        land = link[link.index("private void sendSupersedingLand()"):
                    link.index("private void transmitTick()")]
        manual = activity[activity.index("private void updateManualControl()"):
                          activity.index("private void requestTakeoff()")]
        self.assertIn("ManualControl.neutral()", takeoff)
        self.assertIn("takeoffPending", transmit)
        self.assertIn("!takeoffPending", transmit)
        self.assertIn("pendingCommand == MavlinkCodec.CMD_NAV_TAKEOFF", manual)
        self.assertIn("ManualControl.neutral()", manual)
        self.assertIn("commandTracker.clear()", land)
        self.assertIn("CMD_NAV_LAND", land)
        self.assertNotIn("pendingMode", link)

    def test_android_binds_control_socket_to_aircraft_wifi(self):
        java_root = ROOT / "android/app/src/main/java/com/osrbot/open32drone/controller"
        activity = (java_root / "MainActivity.java").read_text(encoding="utf-8")
        link = (java_root / "DroneLink.java").read_text(encoding="utf-8")
        endpoint = (java_root / "AircraftEndpoint.java").read_text(encoding="utf-8")
        updater = (java_root / "FirmwareUpdater.java").read_text(encoding="utf-8")
        self.assertIn("NetworkCapabilities.TRANSPORT_WIFI", activity)
        self.assertIn("route.matches(aircraftAddress)", activity)
        self.assertIn("!route.isDefaultRoute()", activity)
        self.assertIn("findAircraftWifiNetwork()", activity)
        self.assertNotIn("wifiFallback", activity)
        self.assertIn("WIFI_ROUTE_RETRY_MS", activity)
        self.assertIn("handler.postDelayed(retryControlLink", activity)
        self.assertIn("registerAircraftWifiCallback()", activity)
        self.assertIn("new NetworkRequest.Builder()", activity)
        self.assertIn("onLinkPropertiesChanged", activity)
        self.assertIn("MainActivity.this::refreshControlLink", activity)
        self.assertIn("aircraftNetwork.bindSocket(socket)", link)
        self.assertIn("aircraft Wi-Fi route unavailable", link)
        self.assertIn("reportLinkFailure(status)", link)
        self.assertIn("onLinkFailure(DroneLink source", activity)
        self.assertIn("stopControlLink()", activity)
        self.assertIn("boolean takeoffPending", link)
        self.assertIn("linkHealthy() && !takeoffPending", link)
        self.assertIn("PREF_AIRCRAFT_HOST", activity)
        self.assertIn("showAircraftAddressDialog()", activity)
        self.assertIn("AircraftEndpoint.cameraUrl(aircraftHost)", activity)
        self.assertIn("aircraftNetwork.getByName(aircraftHost)", link)
        self.assertIn("expectedAddress.equals(packet.getAddress())", link)
        self.assertIn('DEFAULT_HOST = "192.168.4.1"', endpoint)
        self.assertIn("normalizeIpv4", endpoint)
        self.assertIn("aircraftNetwork.openConnection", updater)
        self.assertNotIn('http://192.168.4.1:8080', updater)

    def test_android_center_controls_are_compact(self):
        layout = (ROOT / "android/app/src/main/res/layout/activity_main.xml").read_text(
            encoding="utf-8"
        )
        self.assertNotIn('android:layout_width="280dp"', layout)
        self.assertIn('android:layout_width="220dp"', layout)
        self.assertIn('android:id="@+id/flightActionPanel"', layout)
        self.assertIn('android:layout_gravity="center_vertical"', layout)
        self.assertNotIn('android:layout_marginBottom="16dp"', layout)
        self.assertIn('android:layout_width="180dp"', layout)
        self.assertIn('android:layout_height="135dp"', layout)
        self.assertEqual(layout.count('android:layout_height="40dp"'), 2)
        for view in ("disarmButton", "takeoffButton", "landButton"):
            self.assertIn(f'android:id="@+id/{view}"', layout)

    def test_ros_has_one_camera_free_launch(self):
        launches = sorted((ROOT / "ros2/launch").glob("*.launch.py"))
        self.assertEqual([path.name for path in launches], ["open32drone.launch.py"])
        launch = launches[0].read_text(encoding="utf-8")
        self.assertNotIn('executable="camera"', launch)
        self.assertNotIn("camera_url", launch)
        self.assertIn('"use_rviz"', launch)
        self.assertIn('"aircraft_ip"', launch)
        self.assertIn('"robot_name"', launch)
        self.assertIn('"frame_prefix"', launch)
        self.assertIn('"mav_sys_id"', launch)
        self.assertIn('"local_udp_port"', launch)
        self.assertIn('"fcu_url"', launch)
        self.assertIn('local_udp_port,', launch)
        self.assertIn('mavros_parameters["tgt_system"]', launch)
        self.assertIn('namespace=robot_name', launch)
        self.assertIn('namespace=mavros_namespace', launch)

    def test_ros_exposes_robot_style_topics_and_commands(self):
        bridge = (ROOT / "ros2/open32drone_driver/interface_bridge_node.py").read_text(encoding="utf-8")
        offboard = (ROOT / "ros2/open32drone_driver/offboard_control_node.py").read_text(encoding="utf-8")
        manager = (ROOT / "ros2/open32drone_driver/flight_manager_node.py").read_text(encoding="utf-8")
        for topic in ("imu/data", "odom", "range/downward", "battery", "rc/in"):
            self.assertIn(f'"{topic}"', bridge)
        self.assertIn('"cmd_vel"', offboard)
        self.assertIn('"goal_pose"', offboard)
        for service in ('"arm"', '"disarm"', '"takeoff"', '"land"', '"emergency_stop"'):
            self.assertIn(service, manager)
        self.assertIn('f"{prefix}/distance_sensor/tof"', bridge)
        self.assertNotIn('f"{prefix}/tof"', bridge)

    def test_ros_multi_aircraft_names_are_not_hardcoded_absolute(self):
        driver_root = ROOT / "ros2/open32drone_driver"
        sources = {
            path.name: path.read_text(encoding="utf-8")
            for path in driver_root.glob("*.py")
        }
        runtime_nodes = (
            "interface_bridge_node.py",
            "offboard_control_node.py",
            "flight_manager_node.py",
            "rc_bridge_node.py",
            "control_cli.py",
            "bench_test.py",
            "flight_test.py",
        )
        forbidden = (
            '"/open32drone',
            '"/imu/data',
            '"/odom"',
            '"/cmd_vel"',
            '"/goal_pose"',
            '"/arm"',
            '"/takeoff"',
            '"/land"',
        )
        for name in runtime_nodes:
            for value in forbidden:
                self.assertNotIn(value, sources[name], f"{name}: {value}")

        launch = (ROOT / "ros2/launch/open32drone.launch.py").read_text(
            encoding="utf-8"
        )
        plugin = (ROOT / "ros2/config/mavros_plugins.yaml").read_text(
            encoding="utf-8"
        )
        rviz = (ROOT / "ros2/config/open32drone.rviz").read_text(
            encoding="utf-8"
        )
        system = sources["system_control.py"]
        for argument in ("robot_name", "frame_prefix", "mav_sys_id", "local_udp_port"):
            self.assertIn(argument, launch)
        self.assertIn("send_tf: false", plugin)
        self.assertIn("/**/distance_sensor:", plugin)
        self.assertIn('"local_position/tf/send": False', launch)
        self.assertIn("Value: odom", rviz)
        self.assertNotIn("Value: /odom", rviz)
        self.assertIn("STATE_ROOT / robot_name(name)", system)
        self.assertIn('"--robot-name"', system)

    def test_ros_keeps_first_velocity_command_and_safe_rc_throttle(self):
        offboard = (ROOT / "ros2/open32drone_driver/offboard_control_node.py").read_text(encoding="utf-8")
        cli = (ROOT / "ros2/open32drone_driver/control_cli.py").read_text(encoding="utf-8")
        rc_bridge = (ROOT / "ros2/open32drone_driver/rc_bridge_node.py").read_text(
            encoding="utf-8"
        )
        self.assertGreaterEqual(
            offboard.count('if self.phase not in ("PREPARING", "WARMUP"):\n                return'),
            2,
        )
        self.assertIn('declare_parameter("max_horizontal_speed", 0.70)', offboard)
        self.assertIn(
            "limit_xy_velocity(values[0], values[1], horizontal)", offboard
        )
        self.assertIn("centered.channels[0:4] = (1023, 1023, throttle, 1023)", cli)
        self.assertIn('elif node.command("mode position") != 0', cli)
        self.assertIn('node.command("rc stop")', cli)
        self.assertIn("physical SBUS has control priority", rc_bridge)

    def test_ros_land_paths_stop_rc_and_flight_test_detects_descent(self):
        manager = (ROOT / "ros2/open32drone_driver/flight_manager_node.py").read_text(
            encoding="utf-8"
        )
        flight = (ROOT / "ros2/open32drone_driver/flight_test.py").read_text(
            encoding="utf-8"
        )
        cli = (ROOT / "ros2/open32drone_driver/control_cli.py").read_text(
            encoding="utf-8"
        )
        self.assertIn('client = self.trigger_clients["rc_stop"]', manager)
        self.assertGreaterEqual(manager.count("RC stop before landing failed"), 2)
        self.assertIn("target_height = launch_height + parsed.height", flight)
        self.assertIn("hover altitude left tolerance for 1.0 s", flight)
        self.assertIn("ExtendedState.LANDED_STATE_ON_GROUND", flight)
        self.assertIn("qos_profile_sensor_data", flight)
        self.assertIn('self.command("rc stop")', cli)
        self.assertIn('self.command("offboard start")', cli)

    def test_ros_flight_services_do_not_block_their_own_client_responses(self):
        manager = (
            ROOT / "ros2/open32drone_driver/flight_manager_node.py"
        ).read_text(encoding="utf-8")
        self.assertIn("MutuallyExclusiveCallbackGroup", manager)
        self.assertIn("self.service_callback_group", manager)
        self.assertIn("self.client_callback_group", manager)
        self.assertIn("callback_group=self.client_callback_group", manager)
        self.assertIn("callback_group=self.service_callback_group", manager)
        self.assertIn("qos_profile_sensor_data", manager)
        self.assertIn(
            'ExtendedState,\n            "UAS1/extended_state",\n'
            "            self._extended_state_callback,\n"
            "            qos_profile_sensor_data,",
            manager,
        )

    def test_ros_tests_are_small_and_explicit(self):
        setup = (ROOT / "ros2/setup.py").read_text(encoding="utf-8")
        bench = ROOT / "ros2/open32drone_driver/bench_test.py"
        flight = ROOT / "ros2/open32drone_driver/flight_test.py"
        self.assertTrue(bench.exists())
        self.assertTrue(flight.exists())
        self.assertIn("bench_test =", setup)
        self.assertIn("flight_test =", setup)
        self.assertLess(len(bench.read_text(encoding="utf-8").splitlines()), 180)
        self.assertLess(len(flight.read_text(encoding="utf-8").splitlines()), 260)
        self.assertNotIn("acceptance_test", setup)


if __name__ == "__main__":
    unittest.main()
