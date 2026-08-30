import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
FIRMWARE = ROOT / "firmware"
CONTROL_MODULES = (
    "control.ino",
    "control_altitude.ino",
    "control_auto_flight.ino",
    "control_modes.ino",
    "control_offboard.ino",
    "control_position.ino",
    "control_stabilization.ino",
)


def source(name):
    return (FIRMWARE / name).read_text(encoding="utf-8")


def control_source():
    return "\n".join(source(name) for name in CONTROL_MODULES)


class FirmwareContractTests(unittest.TestCase):
    def test_control_source_is_split_by_owner_without_reordering_pipeline(self):
        self.assertEqual(
            {path.name for path in FIRMWARE.glob("control*.ino")},
            set(CONTROL_MODULES),
        )
        self.assertLess(len(source("control.ino").splitlines()), 300)
        for name, function in (
            ("control_modes.ino", "void interpretControls()"),
            ("control_offboard.ino", "void clearOffboardLocalControl()"),
            ("control_auto_flight.ino", "bool beginAutomaticTakeoff("),
            ("control_altitude.ino", "void updateAltitudeHoldControl()"),
            ("control_position.ino", "void updatePositionControlSplit("),
            ("control_stabilization.ino", "void controlAttitude()"),
        ):
            self.assertIn(function, source(name))

        pipeline = source("control.ino")
        calls = (
            "interpretControls();",
            "failsafe();",
            "updateAutoFlightControl();",
            "updateAltitudeHoldControl();",
            "updatePositionControlSplit(dt);",
            "controlAttitude();",
            "controlRates();",
            "controlTorque();",
        )
        offsets = [pipeline.index(call) for call in calls]
        self.assertEqual(offsets, sorted(offsets))

    def test_minimal_build_identity_and_hardware_map(self):
        main = source("firmware.ino")
        motors = source("motors.ino")
        rc = source("rc.ino")
        imu = source("imu.ino")
        self.assertIn('OPEN32DRONE_BUILD_ID[] = "minimal"', main)
        self.assertIn("(0U << 24) | (1U << 16) | (0U << 8) | 255U", main)
        for pin, label in ((4, "MOTOR_0_PIN"), (3, "MOTOR_1_PIN"),
                           (6, "MOTOR_2_PIN"), (5, "MOTOR_3_PIN")):
            self.assertIn(f"#define {label} {pin}", motors)
        self.assertIn("SBUS rc(Serial2,44,9)", rc)
        self.assertIn("#define I2C_SDA 2", imu)
        self.assertIn("#define I2C_SCL 43", imu)

    def test_imu_backend_is_build_selectable_and_control_is_fixed_rate(self):
        imu = source("imu.ino")
        backend = source("imu_backend.h")
        main = source("firmware.ino")
        timing = source("time.ino")
        self.assertIn('#include "imu_backend.h"', imu)
        self.assertIn("#include <FlixPeriph.h>", backend)
        for name in ("OPEN32DRONE_IMU_MPU9250", "OPEN32DRONE_IMU_ICM20948",
                     "OPEN32DRONE_IMU_MPU6050"):
            self.assertIn(name, backend)
        self.assertIn("using Open32DroneImu = MPU9250", backend)
        self.assertIn("using Open32DroneImu = ICM20948", backend)
        self.assertIn("using Open32DroneImu = MPU6050", backend)
        self.assertIn("bool acquireIMUSample(bool waitForFresh)", imu)
        self.assertIn("if (imu.read() && imu.status() == 0)", imu)
        self.assertIn("constexpr uint32_t CONTROL_LOOP_TARGET_HZ = 300", timing)
        self.assertIn("waitForControlLoopTick();", main)
        self.assertIn("controlLoopEvery(2)", main)
        self.assertIn("controlLoopEvery(3)", main)
        self.assertNotIn("mpu6xxx_i2c", "\n".join(
            path.name for path in FIRMWARE.iterdir()
        ))

    def test_camera_is_background_only_and_ledc_isolated(self):
        main = source("firmware.ino")
        camera = source("camera.ino")
        setup = main[main.index("void setup()") : main.index("void loop()")]
        loop = main[main.index("void loop()") :]

        self.assertIn("setupCamera();", setup)
        self.assertLess(setup.index("setupMotors();"), setup.index("setupCamera();"))
        self.assertLess(setup.index("setupCamera();"), setup.index("setupWiFi();"))
        self.assertLess(setup.index("setupWiFi();"), setup.index("setupCameraStream();"))
        for token in ("setupCamera", "esp_camera", "camera_fb_t", "httpd_"):
            self.assertNotIn(token, loop)

        self.assertIn("CAMERA_LEDC_CHANNEL = LEDC_CHANNEL_0", camera)
        self.assertIn("CAMERA_LEDC_TIMER = LEDC_TIMER_1", camera)
        self.assertIn("config.task_priority = 1", camera)
        self.assertIn("config.core_id = 0", camera)
        self.assertIn("constexpr int CAMERA_STREAM_FPS = 10", camera)
        self.assertNotIn("15 FPS", camera)
        self.assertIn("WiFi.softAPIP() : WiFi.localIP()", camera)
        self.assertIn("config.send_wait_timeout = 1", camera)
        self.assertIn("if (!wifiTransportHealthy())", camera)
        self.assertIn("sensor->set_hmirror(sensor, 0)", camera)
        self.assertIn("sensor->set_vflip(sensor, 1)", camera)
        self.assertNotIn("log_i(", camera)

    def test_wifi_sta_is_operator_reachable_and_falls_back_without_rewriting_mode(self):
        wifi = source("wifi.ino")
        cli = source("cli.ino")
        parameters = source("parameters.ino")
        ignore_rules = (ROOT / ".gitignore").read_text(encoding="utf-8").splitlines()
        self.assertIn("const int W_AP = 1, W_STA = 2", wifi)
        self.assertIn("OPEN32DRONE_WIFI_BOOT_MODE", wifi)
        self.assertIn('storage.getString("WIFI_STA_SSID", OPEN32DRONE_WIFI_STA_SSID)', wifi)
        self.assertIn('storage.getString("WIFI_STA_PASS", OPEN32DRONE_WIFI_STA_PASS)', wifi)
        self.assertIn("/secrets/", ignore_rules)
        self.assertIn("WIFI_STA_CONNECT_TIMEOUT_MS = 8000", wifi)
        self.assertIn("bool startWiFiStation()", wifi)
        self.assertIn("if (!startWiFiStation()) startWiFiAccessPoint(true);", wifi)
        self.assertIn("wifiStaFallbackActive", wifi)
        self.assertIn('storage.putFloat("WIFI_MODE", requestedMode)', wifi)
        self.assertIn('"sta <ssid> <pass>', cli)
        self.assertIn('command == "sta"', cli)
        self.assertIn("configWiFi(false", cli)
        self.assertIn('{"WIFI_MODE", &wifiMode}', parameters)

    def test_mavlink_parameter_and_diagnostic_text_paths_remain_independent(self):
        mavlink = source("mavlink.ino")
        for message in (
            "MAVLINK_MSG_ID_PARAM_REQUEST_LIST",
            "MAVLINK_MSG_ID_PARAM_REQUEST_READ",
            "MAVLINK_MSG_ID_PARAM_SET",
            "mavlink_msg_param_value_pack",
        ):
            self.assertIn(message, mavlink)
        self.assertIn("mavlink_msg_serial_control_pack", mavlink)
        self.assertIn("SERIAL_CONTROL_DEV_SHELL", mavlink)

    def test_motor_pwm_is_explicit_and_checked(self):
        motors = source("motors.ino")
        self.assertIn("#define PWM_FREQUENCY 10000", motors)
        self.assertEqual(motors.count("ledcAttachChannel(MOTOR_"), 4)
        self.assertIn("bool motorPwmHealthy()", motors)
        self.assertIn("if (!motorsInitialized) return;", motors)

    def test_zero_duty_does_not_make_pwm_prearm_fail(self):
        motors = source("motors.ino")
        health = motors[motors.index("bool motorPwmHealthy()"):
                        motors.index("int getDutyCycle")]
        self.assertIn("return motorsInitialized", health)
        self.assertNotIn("ledcReadFreq(", health)
        self.assertEqual(motors.count("ledcWrite(MOTOR_"), 4)
        self.assertIn("four channels attached", motors)

    def test_prearm_fails_closed_on_core_health(self):
        safety = source("safety.ino")
        for condition in (
            "parameter storage unavailable",
            "motor PWM unavailable",
            "IMU unavailable or stale",
            "gyro calibration incomplete",
            "invalid RC calibration/mapping",
            "GCS link unavailable",
            "throttle not low",
            "control loop too slow",
        ):
            self.assertIn(condition, safety)

    def test_rc_calibration_restores_previous_set_on_failure(self):
        rc = source("rc.ino")
        self.assertIn("float oldZero[16], oldMax[16]", rc)
        self.assertIn("if (!validRCConfiguration())", rc)
        self.assertIn("memcpy(channelZero, oldZero", rc)
        self.assertIn("previous calibration restored", rc)
        self.assertIn("RC calibration accepted", rc)

    def test_rc_mapping_matches_persisted_channels_and_is_bounded(self):
        rc = source("rc.ino")
        parameters = source("parameters.ino")
        self.assertIn("channel <= 7.0f", rc)
        self.assertIn("value <= 7.0f", parameters)
        self.assertIn("if (mapped[j] >= 0 && mapped[j] == channel) return false", rc)
        self.assertIn("constrain(controls[(int)rollChannel], -1.0f, 1.0f)", rc)
        self.assertIn("constrain(controls[(int)throttleChannel], 0.0f, 1.0f)", rc)
        self.assertIn("for (int i = 0; i < 8; i++)", rc)

    def test_armed_motor_idle_remains_ground_ready(self):
        control = control_source()
        estimate = source("estimate.ino")
        mavlink = source("mavlink.ino")
        ground_ready = control[control.index("bool tofGroundReady()"):
                               control.index("bool offboardLocalSensorsReady()")]
        self.assertIn("assistedTakeoffGroundIdle && !flightWasAirborne", ground_ready)
        self.assertNotIn("landed &&", ground_ready)
        self.assertIn("bool armedGroundIdle = armed && assistedTakeoffGroundIdle", estimate)
        self.assertIn("(!motorsActive() || armedGroundIdle)", estimate)
        self.assertIn("if (tofGroundReady())", mavlink)

    def test_assisted_takeoff_and_landing_keep_pilot_attitude_authority(self):
        control = control_source()
        self.assertIn("bool automaticPilotAttitude = pilotControlFresh()", control)
        self.assertIn("pilotRoll * tiltMax", control)
        self.assertIn("pilotPitch * tiltMax", control)
        self.assertIn("-pilotYaw * maxRate.z", control)
        self.assertIn("bool stickMoving = !offboardLocalActive", control)
        self.assertIn("bool landingPilotAttitudeOverride = automaticLandingActive", control)
        self.assertIn("pilotControlFresh() && controlThrottle > 0.60f", control)
        self.assertIn("double autoTakeoffControlTimeAtStart = 0.0", control)
        self.assertIn("controlTime > autoTakeoffControlTimeAtStart", control)
        self.assertIn("takeoffPilotSampleAfterStart &&", control)

    def test_position_speed_is_vector_bounded_and_gate_loss_stays_bounded(self):
        position = source("control_position.ino")
        estimate = source("estimate.ino")
        mavlink = source("mavlink.ino")
        log = source("log.ino")

        self.assertIn(
            "void limitHorizontalSpeedCommand(float &x, float &y)", position
        )
        self.assertGreaterEqual(position.count("limitHorizontalSpeedCommand("), 3)
        self.assertIn("limitHorizontalSpeedCommand(", mavlink)
        self.assertIn("stagedOffboardTargetVX, stagedOffboardTargetVY);", mavlink)
        self.assertNotIn("constrain(m.vx, -0.50f, 0.50f)", mavlink)

        # Estimator plausibility rejection is separate from the operator speed
        # command. A value clamped to a gate threshold must never close its own
        # gate and hand control to the wider STAB attitude envelope.
        self.assertNotIn("flowRawVelocityLimit", estimate)
        self.assertNotIn("flowPositionVelocityLimit", estimate)
        self.assertIn("bool positionFlowStable = !flowSpikeRejected", estimate)

        self.assertIn("bool posHoldFallbackActive = false", position)
        fallback = position[
            position.index("void updateBoundedPositionFallback("):
            position.index("void updatePositionControlSplit(")
        ]
        self.assertIn("min(tiltMax, positionTiltLimit)", fallback)
        self.assertIn("maxFlowAngleRate", fallback)
        self.assertIn("usePosCmd = true", fallback)
        self.assertIn('{"posFallback", &logPosHoldFallback}', log)

    def test_mavlink_takeoff_hands_over_to_position_without_stale_rc_mode(self):
        control = control_source()
        automatic = source("control_auto_flight.ino")
        mavlink_takeoff = automatic[
            automatic.index("bool startAutomaticTakeoff(float targetHeight)"):
            automatic.index("bool startPilotAssistedTakeoff")
        ]
        self.assertIn(
            "return beginAutomaticTakeoff(targetHeight, false, POS_HOLD);",
            mavlink_takeoff,
        )
        self.assertNotIn("mode == ALT_HOLD", mavlink_takeoff)
        self.assertIn(
            "if (!externalModeOverride && rcPilotActive && isfinite(controlMode))",
            control,
        )
        self.assertIn("int returnMode = autoFlightReturnMode", control)
        self.assertIn("bool pilotTriggered = autoFlightPilotTriggered", control)
        self.assertIn("externalModeOverride = !pilotTriggered", control)

    def test_mavlink_takeoff_retry_is_idempotent_and_ack_is_prioritized(self):
        mavlink = source("mavlink.ino")
        takeoff = mavlink[
            mavlink.index("if (m.command == MAV_CMD_NAV_TAKEOFF)"):
            mavlink.index("if (m.command == MAV_CMD_NAV_LAND)")
        ]
        ack = mavlink[
            mavlink.index("// ACK precedes"):
            mavlink.index("\n\t}\n}", mavlink.index("// ACK precedes"))
        ]
        self.assertIn("m.confirmation > 0", takeoff)
        self.assertIn("autoFlightPhase == AUTO_TAKEOFF", takeoff)
        self.assertIn("autoFlightSource == AUTO_SOURCE_MAVLINK", takeoff)
        self.assertIn("abs(requestedGoal - autoFlightGoalHeight) < 0.01f", takeoff)
        self.assertLess(ack.index("sendMessage(&ack);"),
                        ack.index("sendMavlinkStatusText(deferredStatusSeverity"))

    def test_automatic_landing_does_not_recapture_a_higher_altitude_target(self):
        automatic = source("control_auto_flight.ino")
        landing = automatic[
            automatic.index("bool beginAutomaticLanding("):
            automatic.index("bool startAutomaticLanding()")
        ]
        self.assertIn("bool preserveAltitudeControl = altitudeHoldEngaged", landing)
        self.assertIn("landingTargetHeight = min(landingTargetHeight", landing)
        self.assertIn("constrain(altitudeHoldTarget, 0.05f, 5.80f)", landing)
        self.assertIn("if (!preserveAltitudeControl)", landing)
        self.assertIn("autoFlightTargetHeight = landingTargetHeight", landing)
        self.assertIn("altitudeHoldTarget = landingTargetHeight", landing)
        self.assertNotIn(
            "autoFlightTargetHeight = max(position.z - 0.02f, 0.05f)",
            landing,
        )

    def test_minimal_flight_surface_has_no_debug_modes_or_hover_trim(self):
        control = control_source()
        parameters = source("parameters.ino")
        cli = source("cli.ino")
        mavlink = source("mavlink.ino")
        estimate = source("estimate.ino")
        for token in ("RAW", "ACRO", "hoverTrimRoll", "hoverTrimPitch"):
            self.assertNotIn(token, control)
        self.assertNotIn("CTL_TRIM_R", parameters)
        self.assertNotIn("CTL_TRIM_P", parameters)
        self.assertNotIn('command == "raw"', cli)
        self.assertNotIn('command == "acro"', cli)
        self.assertNotIn("m.param2 >= RAW", mavlink)
        self.assertNotIn("flowScaleX", estimate)
        self.assertNotIn("flowScaleY", estimate)
        self.assertNotIn("rawWorldPos", estimate)
        self.assertIn(
            "value == STAB || value == ALT_HOLD || value == POS_HOLD",
            parameters,
        )
        self.assertNotIn("armedChannel", parameters)

    def test_unused_diagnostic_counters_are_absent(self):
        self.assertNotIn("imuReadErrors", source("imu.ino"))
        rc = source("rc.ino")
        self.assertNotIn("rcLostFrameCount", rc)
        self.assertNotIn("rcFailsafeFrameCount", rc)

    def test_hot_path_caches_static_and_derived_rotations(self):
        imu = source("imu.ino")
        parameters = source("parameters.ino")
        estimate = source("estimate.ino")
        main = source("firmware.ino")
        read = imu[imu.index("void readIMU()"):
                   imu.index("void resetGyroCalibrationWindow")]
        self.assertIn("Quaternion imuRotationInverse", imu)
        self.assertIn("void updateIMURotation()", imu)
        self.assertNotIn("Quaternion::fromEuler(imuRotation)", read)
        self.assertEqual(read.count("imuRotationInverse"), 2)
        for axis in ("ROLL", "PITCH", "YAW"):
            self.assertIn(
                f'{{"IMU_ROT_{axis}", &imuRotation.', parameters
            )
        self.assertGreaterEqual(parameters.count("updateIMURotation}"), 3)
        self.assertIn("Vector attitudeEuler", estimate)
        self.assertIn("Vector attitudeBodyUp", estimate)
        self.assertIn("beginPerformanceCycle();", main)
        self.assertIn("markPerformanceStage(PERF_IMU);", main)

    def test_performance_profiler_is_sampled_and_diagnostics_are_ground_only(self):
        timing = source("time.ino")
        cli = source("cli.ino")
        self.assertIn("performanceCycleCounter & 0x0fU", timing)
        self.assertIn("loopDtP95Ms", timing)
        self.assertIn("loopDtP99Ms", timing)
        self.assertIn('command == "perf"', cli)
        self.assertIn("Diagnostic command rejected while armed", cli)

    def test_control_parameter_guards_are_not_shadowed(self):
        parameters = source("parameters.ino")
        rate_guard = parameters.index('if (strstr(name, "RATE_MAX"))')
        generic_pid_guard = parameters.index('if (strncmp(name, "CTL_", 4)')
        self.assertLess(rate_guard, generic_pid_guard)
        self.assertIn('strcmp(name, "ALT_CORR_MAX")', parameters)

    def test_operator_confirmed_attitude_and_rate_defaults_are_locked(self):
        control = source("control.ino")
        self.assertIn("#define PITCHRATE_P 0.05", control)
        self.assertIn("#define PITCHRATE_I 0.2", control)
        self.assertIn("#define PITCHRATE_D 0.001", control)
        self.assertIn("#define ROLLRATE_P PITCHRATE_P", control)
        self.assertIn("#define ROLLRATE_I PITCHRATE_I", control)
        self.assertIn("#define ROLLRATE_D PITCHRATE_D", control)
        self.assertIn("#define ROLL_P 4.47f", control)
        self.assertIn("#define PITCH_P ROLL_P", control)
        self.assertIn(
            "float altitudeHoldP = 0.747f;",
            source("control_altitude.ino"),
        )
        self.assertIn("float autoTakeoffThrustLimit = 0.20f", control)
        self.assertIn("const float altitudeTakeoffThrustSlew = 0.45f", control)
        self.assertIn(
            "autoTakeoffThrustLimit = 0.20f;",
            source("control_auto_flight.ino"),
        )

    def test_standard_rc_defaults_are_missing_key_fallbacks(self):
        rc = source("rc.ino")
        parameters = source("parameters.ino")
        self.assertIn(
            "float channelZero[16] = {1023.0f, 1023.0f, 240.0f, 1023.0f, "
            "0, 0, 240.0f, 0",
            rc,
        )
        self.assertIn(
            "float channelMax[16] = {1807.0f, 1807.0f, 1807.0f, 1807.0f, "
            "0, 0, 1807.0f, 0",
            rc,
        )
        setup = parameters[parameters.index("void setupParameters()"):
                           parameters.index("int parametersCount()")]
        self.assertIn("Missing keys use compiled defaults", setup)
        self.assertIn("if (storage.isKey(parameter.name))", setup)
        self.assertIn("parameter.setValue(stored)", setup)

    def test_parameters_have_no_implicit_profile_migration(self):
        parameters = source("parameters.ino")
        setup = parameters[parameters.index("void setupParameters()"):
                           parameters.index("int parametersCount()")]
        self.assertIn("Missing keys use compiled defaults", setup)
        self.assertIn("Stored values are never rewritten implicitly", setup)
        self.assertNotIn("migration", setup.lower().replace("migrations", ""))
        self.assertIn("if (motorsActive()) return", parameters)

    def test_voltage_sensing_is_retained(self):
        power = source("power.ino")
        parameters = source("parameters.ino")
        mavlink = source("mavlink.ino")
        self.assertIn("int voltagePin = A0", power)
        self.assertIn("analogSetPinAttenuation(voltagePin, ADC_11db)", power)
        self.assertIn("analogReadMilliVolts(voltagePin)", power)
        self.assertIn("bool voltageAvailable()", power)
        for name in (
            "PWR_VOLT_PIN",
            "PWR_VOLT_SCALE",
            "PWR_VOLT_LPF_A",
            "PWR_COMP_REF",
            "PWR_COMP_SLP",
            "PWR_COMP_MAX",
        ):
            self.assertIn(name, parameters)
        self.assertIn('value == -1 || value == A0', parameters)
        self.assertIn("mavlink_msg_battery_status_pack", mavlink)
        self.assertIn("MAV_BATTERY_CHARGE_STATE_UNDEFINED", mavlink)
        self.assertNotIn("mapf(voltage", mavlink)

    def test_voltage_compensation_changes_only_assisted_collective_feedforward(self):
        power = source("power.ino")
        altitude = source("control_altitude.ino")
        modes = source("control_modes.ino")
        automatic = source("control_auto_flight.ino")
        mixer = source("control_stabilization.ino")

        self.assertIn("float voltageCompensationReference = 3.28f", power)
        self.assertIn("float voltageCompensationSlope = 0.472f", power)
        self.assertIn("float voltageCompensationMax = 1.20f", power)
        self.assertIn("if (!voltageAvailable() || voltageCompensationMax <= 1.0f) return 1.0f", power)
        self.assertIn("1.0f + voltageCompensationSlope", power)
        self.assertIn("1.0f / voltageCompensationMax", power)
        self.assertIn("altitudeHoverThrust * voltageThrustCompensationFactor()", altitude)
        self.assertIn("automaticTakeoffHoverFeedForwardCap = 0.49f", altitude)
        self.assertIn("altitudeHoverFeedForwardSlew = 0.05f", altitude)
        self.assertIn("if (!altitudeVoltageCompensationEnabled())", altitude)
        self.assertIn("altitudeHoverFeedForwardEffective = altitudeHoverThrust", altitude)
        self.assertIn("altitudeHoverFeedForward() + altitudeHoldCorrection", altitude)
        self.assertIn("if (mode == STAB) thrustTarget = controlThrottle", modes)
        self.assertIn("altitudeHoverFeedForward() + 0.08f", automatic)
        self.assertIn("min(altitudeTakeoffMaxThrust", automatic)
        self.assertNotIn("voltageThrustCompensationFactor", mixer)

    def test_gpio21_low_voltage_warning_is_visual_only(self):
        led = source("led.ino")
        main = source("firmware.ino")
        control = control_source()

        self.assertIn("#define LED_BUILTIN 21", led)
        self.assertIn("LOW_VOLTAGE_WARNING_ENTER = 3.10f", led)
        self.assertIn("LOW_VOLTAGE_WARNING_EXIT = 3.20f", led)
        self.assertIn("LOW_VOLTAGE_WARNING_ENTER_MS = 1500", led)
        self.assertIn("LOW_VOLTAGE_WARNING_BLINK_MS = 250", led)
        self.assertIn("if (!voltageAvailable())", led)
        self.assertIn("void updateLED()", led)
        self.assertLess(main.index("readVoltage();"), main.index("updateLED();"))
        self.assertNotIn("lowVoltageWarning", control)

    def test_supported_mavlink_surface_excludes_missions_and_direct_motors(self):
        mavlink = source("mavlink.ino")
        control = control_source()
        for required in (
            "MAVLINK_MSG_ID_MANUAL_CONTROL",
            "MAVLINK_MSG_ID_SET_ATTITUDE_TARGET",
            "MAVLINK_MSG_ID_SET_POSITION_TARGET_LOCAL_NED",
            "MAV_CMD_COMPONENT_ARM_DISARM",
            "MAV_CMD_NAV_TAKEOFF",
            "MAV_CMD_NAV_LAND",
        ):
            self.assertIn(required, mavlink)
        self.assertNotIn("MAVLINK_MSG_ID_MISSION_REQUEST_LIST", mavlink)
        self.assertNotIn("MAVLINK_MSG_ID_SET_ACTUATOR_CONTROL_TARGET", mavlink)
        self.assertNotIn("ACTUATOR_OFFBOARD_DIRECT", control)

    def test_assisted_mode_releases_only_mavlink_manual_lease(self):
        mavlink = source("mavlink.ino")
        helper = mavlink[
            mavlink.index("void releaseMavlinkManualControlForMode"):
            mavlink.index("struct MavlinkFlightModeInfo")
        ]
        self.assertIn("requestedMode != ALT_HOLD", helper)
        self.assertIn("requestedMode != POS_HOLD", helper)
        self.assertIn("mavlinkManualControlActive = false", helper)
        self.assertIn("mavlinkManualControlLastMs = 0", helper)
        self.assertIn("if (!rcPilotActive)", helper)
        self.assertIn("controlThrottle = 0.5f", helper)
        self.assertIn("controlTime = 0.0", helper)
        self.assertGreaterEqual(
            mavlink.count("releaseMavlinkManualControlForMode"), 5
        )

    def test_ota_is_ab_ground_only_and_hash_checked(self):
        ota = source("ota.ino")
        for required in (
            "esp_ota_get_app_partition_count() < 2",
            "if (armed)",
            "if (!landed)",
            "motorsActive()",
            "X-Firmware-SHA256",
            "esp_ota_set_boot_partition",
            "pending boot validation",
        ):
            self.assertIn(required, ota)

    def test_failsafe_and_rc_emergency_paths_remain(self):
        safety = source("safety.ino")
        rc = source("rc.ino")
        self.assertIn("offboardTimeout = 0.30f", safety)
        self.assertIn("mavlinkManualControlHealthy", safety)
        self.assertIn('"Failsafe descent: %s"', safety)
        self.assertIn("controlled descent", safety.lower())
        self.assertIn("RC_EMERGENCY_HOLD_MS = 150", rc)
        self.assertIn("rcEmergencyDisarmRequested", rc)

    def test_tip_over_guard_is_minimal_and_mode_independent(self):
        safety = source("safety.ino")
        failsafe = safety[safety.index("void failsafe()"):
                          safety.index("void sensorFailsafe()")]
        guard = safety[safety.index("void tipOverFailsafe()"):
                       safety.index("void offboardFailsafe()")]
        self.assertIn("tipOverCosLimit = 0.342f", safety)
        self.assertIn("tipOverHoldTime = 0.25f", safety)
        self.assertIn("tipOverFailsafe();", failsafe)
        self.assertIn("if (!armed || !attitude.valid())", guard)
        self.assertIn("attitudeBodyUp.z < tipOverCosLimit", guard)
        self.assertIn("tipOverDelay.update(tipped)", guard)
        self.assertIn('failsafeReason = "tip-over"', guard)
        self.assertIn("forceDisarm(failsafeReason)", guard)
        self.assertNotIn("tof", guard.lower())
        self.assertNotIn("acc.", guard.lower())

    def test_mavlink_parameter_tuning_is_bounded_and_authoritative(self):
        mavlink = source("mavlink.ino")
        parameters = source("parameters.ino")
        self.assertIn("Rate mavlinkParameterStreamRate(20)", mavlink)
        self.assertIn("mavlinkParameterStreamIndex++", mavlink)
        self.assertIn("copyMavlinkParameterId", mavlink)
        self.assertIn("findParameterIndex(requestedName)", mavlink)
        self.assertIn("findParameterIndex(name)", mavlink)
        self.assertIn("Parameter write denied while armed", mavlink)
        self.assertIn("getParameterName(index), actual", mavlink)
        self.assertIn("int findParameterIndex", parameters)

    def test_mavlink_telemetry_and_logs_are_spread_across_loops(self):
        mavlink = source("mavlink.ino")
        log = source("log.ino")
        self.assertIn("mavlinkSlowTelemetryPending", mavlink)
        self.assertIn("mavlinkFastTelemetryPending", mavlink)
        self.assertIn("if (!mavlinkTxUsedThisLoop) sendPendingMavlinkParameter()", mavlink)
        self.assertIn("if (!mavlinkTxUsedThisLoop) sendPendingMavlinkLogData()", mavlink)
        self.assertIn("MAVLINK_MSG_ID_LOG_REQUEST_LIST", mavlink)
        self.assertIn("MAVLINK_MSG_ID_LOG_REQUEST_END", mavlink)
        self.assertNotIn("const uint32_t total = sizeof(logBuffer)", mavlink)
        self.assertIn("uint32_t logDataSizeBytes()", log)
        self.assertIn("copyLogDataBytes", log)


if __name__ == "__main__":
    unittest.main()
