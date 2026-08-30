// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Implementation of command line interface

#include "pid.h"
#include "source_identity.h"
#include "vector.h"
#include "util.h"

extern const int MOTOR_REAR_LEFT, MOTOR_REAR_RIGHT, MOTOR_FRONT_RIGHT, MOTOR_FRONT_LEFT;
extern const int STAB, AUTO;
extern double t;
extern float dt, loopRate, loopDtAverageMs, loopDtP95Ms, loopDtP99Ms, loopDtMaxMs;
extern const uint32_t CONTROL_LOOP_TARGET_HZ;
extern uint32_t loopDeadlineMissCount, loopDeadlineMaxLatenessUs;
extern uint32_t loopOver5msCount, loopOver10msCount;
extern uint16_t channels[16];
extern double controlTime;
extern bool rcLinkHealthy, rcPilotActive, rcReceiverFailsafe;
// 光流定高系统状态（control.ino 定义，cli.ino 拼接在其前需前向声明）
extern bool altitudeHoldEngaged;
extern float altitudeHoldTarget;
extern float altitudeHoldCorrection;
extern float altitudeHoverThrust;
extern float thrustTarget;
extern float mixerScale;
extern float autoFlightGoalHeight;
extern int altitudeHoldRejectReason;
extern const char *externalModeFailure;
// 光流（flow.ino 定义）
extern bool tofHealthy;
extern bool opticalFlowHealthy;
extern uint32_t opticalFlowUartRestarts;
extern float opticalFlowHeight;
extern float opticalFlowVelocityX;
extern float opticalFlowVelocityY;
extern int mode;
extern bool armed;
extern bool flightWasAirborne;
extern float voltage; // from power.ino
extern int voltagePin;
extern float voltageScale;
extern float voltageCompensationReference, voltageCompensationSlope, voltageCompensationMax;
extern uint32_t voltageAdcMilliVolts;
bool voltageAvailable();
float voltageThrustCompensationFactor();
float altitudeHoverFeedForward();
bool lowVoltageWarningActive();
extern LowPassFilter<Vector> gyroBiasFilter; // from imu.ino
extern Vector velocity; // 水平为机体系 FLU，垂直为世界系向上
extern Vector flowFilteredBodyVel, flowGyroBodyVel;
extern float flowGyroCompPitch, flowGyroCompRoll, flowGyroDelayMs;
extern bool flowCtrlUsingFlow; // 光流数据是否用于控制（estimate.ino）
extern bool flowBiasReady;
extern bool flowBiasFallbackActive;
extern uint32_t flowBiasSamples;
extern Vector flowBias;
extern int flowRejectReason; // 光流拒绝原因（estimate.ino）
extern uint32_t opticalFlowSequence;
extern uint32_t opticalFlowTimestamp;
extern float opticalFlowSampleDt;
extern uint32_t tofSequence;
extern uint32_t tofTimestamp;
extern float tofSampleDt;
extern bool tofPacketHealthy, tofRangeInBlindZone;
extern uint32_t tofPacketTimestamp;
extern uint16_t opticalFlowTofDistanceMm;
extern bool flowPositionGateOpen;
extern bool posHoldGateOpen;
extern bool posHoldLocked;
extern int posHoldRejectReason;
extern float posRollCmd, posPitchCmd;
extern uint32_t opticalFlowValidPackets, opticalFlowInvalidPackets;
extern uint32_t tofValidPackets, tofInvalidPackets;
extern uint8_t opticalFlowSensorPacketSequence, opticalFlowTofStrength, opticalFlowModuleVersion;
extern uint16_t opticalFlowIntegrationTimeUs, opticalFlowIntegrationTimeMinUs, opticalFlowIntegrationTimeMaxUs;
extern uint32_t opticalFlowPacketGapCount, opticalFlowPacketDuplicateCount, opticalFlowPacketOutOfOrderCount;
extern bool parameterStorageHealthy;

const char* motd =
"\nWelcome to\n"
" _______  __       __  ___   ___\n"
"|   ____||  |     |  | \\  \\ /  /\n"
"|  |__   |  |     |  |  \\  V  /\n"
"|   __|  |  |     |  |   >   <\n"
"|  |     |  `----.|  |  /  .  \\\n"
"|__|     |_______||__| /__/ \\__\\\n\n"
"Commands:\n\n"
"help - show help\n"
"p - show all parameters\n"
"p <name> - show parameter\n"
"p <name> <value> - set parameter\n"
"preset - reset parameters\n"
"time - show time info\n"
"perf [reset] - show/reset sampled loop-stage timing (disarmed only)\n"
"ps - show pitch/roll/yaw\n"
"psq - show attitude quaternion\n"
"imu - show IMU data\n"
"arm - arm the drone\n"
"disarm - disarm the drone\n"
"stab - select stabilized mode on the ground\n"
"auto - show automatic-flight ownership rule\n"
"rc - show RC data\n"
"wifi - show Wi-Fi info\n"
"ota - show A/B update status\n"
"ap <ssid> <pass> - set AP SSID/password (reboot to apply)\n"
"sta <ssid> <pass> - join a router (reboot to apply)\n"
"pw - show battery voltage\n"
"alt - show altitude-hold state (target/ToF/thrust)\n"
"flow - show optical flow info (health, TOF, vel)\n"
"mot - show motor output\n"
"log [dump] - print log header [and data]\n"
"cr - calibrate RC\n"
"ca - calibrate accel\n"
"cg - restart gyro calibration\n"
"mfr, mfl, mrr, mrl - test motor (remove props)\n"
"sys - show system info\n"
"reset - reset drone's state\n"
"reboot - reboot the drone\n";

void print(const char* format, ...) {
	char buf[1000];
	va_list args;
	va_start(args, format);
	vsnprintf(buf, sizeof(buf), format, args);
	va_end(args);
	Serial.print(buf);
#if WIFI_ENABLED
	mavlinkPrint(buf);
#endif
}

void pause(float duration) {
	double start = t;
	while (t - start < duration) {
		step();
		handleInput();
#if WIFI_ENABLED
		processMavlink();
#endif
		delay(50);
	}
}

void doCommand(String str, bool echo = false) {
	// parse command
	String command, arg0, arg1;
	splitString(str, command, arg0, arg1);
	if (command.isEmpty()) return;

	// echo command
	if (echo) {
		print("> %s\n", str.c_str());
	}

	command.toLowerCase();
	auto rejectWhileArmed = [&]() {
		if (!armed) return false;
		print("Command rejected while armed: %s\n", command.c_str());
		return true;
	};
	auto heavyCommand = [&]() {
		return command == "help" || command == "motd" ||
			(command == "p" && arg0 == "") || command == "imu" ||
			command == "wifi" || command == "ota" || command == "flow" ||
			command == "log" || command == "sys" || command == "perf";
	};
	if (armed && heavyCommand()) {
		print("Diagnostic command rejected while armed: %s\n", command.c_str());
		return;
	}

	// execute command
	if (command == "help" || command == "motd") {
		print("%s\n", motd);
	} else if (command == "p" && arg0 == "") {
		printParameters();
	} else if (command == "p" && arg0 != "" && arg1 == "") {
		print("%s = %g\n", arg0.c_str(), getParameter(arg0.c_str()));
	} else if (command == "p") {
		char *end = nullptr;
		float requested = strtof(arg1.c_str(), &end);
		bool parsed = end != arg1.c_str() && *end == '\0';
		bool success = parsed && setParameter(arg0.c_str(), requested);
		if (success) {
			print("%s = %g\n", arg0.c_str(), requested);
		} else {
			print("Parameter rejected: %s\n", arg0.c_str());
		}
	} else if (command == "preset") {
		if (!rejectWhileArmed()) resetParameters();
	} else if (command == "time") {
		print("Time: %f\n", t);
		print("Loop: %.0fHz dt %.6fs avg %.3fms p95 %.3fms p99 %.3fms max %.3fms\n",
			loopRate, dt, loopDtAverageMs, loopDtP95Ms, loopDtP99Ms, loopDtMaxMs);
		print("Schedule: target %uHz misses %u max late %uus; >5ms %u >10ms %u\n",
			CONTROL_LOOP_TARGET_HZ, loopDeadlineMissCount, loopDeadlineMaxLatenessUs,
			loopOver5msCount, loopOver10msCount);
	} else if (command == "perf") {
		if (arg0 == "reset") {
			resetPerformanceInfo();
			print("Performance counters reset\n");
		} else {
			printPerformanceInfo();
		}
	} else if (command == "ps") {
		print("roll: %f pitch: %f yaw: %f\n",
			degrees(attitudeEuler.x), degrees(attitudeEuler.y), degrees(attitudeEuler.z));
	} else if (command == "psq") {
		print("qw: %f qx: %f qy: %f qz: %f\n", attitude.w, attitude.x, attitude.y, attitude.z);
	} else if (command == "imu") {
		printIMUInfo();
		printIMUCalibration();
		print("landed: %d\n", landed);
	} else if (command == "arm") {
		requestArm("CLI");
	} else if (command == "disarm") {
		forceDisarm("CLI");
	} else if (command == "stab") {
		if (!rejectWhileArmed() && !requestExternalMode(STAB)) print("Mode rejected: %s\n", externalModeFailure);
	} else if (command == "auto") {
		print("AUTO is entered only by an accepted takeoff/landing command\n");
	} else if (command == "rc") {
		print("channels: ");
		for (int i = 0; i < 16; i++) {
			print("%u ", channels[i]);
		}
		print("\nroll: %g pitch: %g yaw: %g throttle: %g mode: %g\n",
			controlRoll, controlPitch, controlYaw, controlThrottle, controlMode);
		print("time: %.1f\n", controlTime);
		print("SBUS frame healthy: %d pilot active: %d receiver failsafe: %d\n",
			rcLinkHealthy, rcPilotActive, rcReceiverFailsafe);
		print("mode: %s\n", getModeName());
		print("armed: %d\n", armed);
	} else if (command == "wifi") {
#if WIFI_ENABLED
		printWiFiInfo();
#endif
	} else if (command == "ota") {
#if WIFI_ENABLED
		printOtaInfo();
#endif
	} else if (command == "pw") {
		if (voltagePin < 0) {
			print("Voltage: disabled\n");
		} else if (!voltageAvailable()) {
			print("Voltage: unavailable (GPIO%d ADC %lu mV scale %.4f)\n",
				voltagePin, (unsigned long)voltageAdcMilliVolts, voltageScale);
		} else {
			print("Voltage: %.3f V (GPIO%d ADC %lu mV scale %.4f)\n",
				voltage, voltagePin, (unsigned long)voltageAdcMilliVolts, voltageScale);
		}
		float compensationMinimum = voltageCompensationMax > 1.0f ?
			1.0f / voltageCompensationMax : 1.0f;
		print("Assisted thrust compensation: %.3fx (reference %.3f V, slope %.3f/V, range %.3fx..%.3fx)\n",
			voltageThrustCompensationFactor(), voltageCompensationReference,
			voltageCompensationSlope, compensationMinimum, voltageCompensationMax);
		print("Low-voltage LED: %s (GPIO21, enter 3.10 V, clear 3.20 V)\n",
			lowVoltageWarningActive() ? "BLINKING" : "off");
	} else if (command == "alt") {
		// ToF altitude state: configured hover feed-forward plus PID.
		print("Mode: %s\n", getModeName());
		print("AltHold: %d\n", altitudeHoldEngaged);
		print("Reject: %d\n", altitudeHoldRejectReason);
		print("Target: %.2f m\n", altitudeHoldTarget);
		print("Auto goal: %.2f m\n", autoFlightGoalHeight);
		print("Alt(TOF): %.2f m\n", getHoldAltitude());
		print("Thrust: %.1f%%\n", thrustTarget * 100);
		print("Hover configured: %.1f%% effective %.1f%% correction %.1f%%\n",
			altitudeHoverThrust * 100, altitudeHoverFeedForward() * 100,
			altitudeHoldCorrection * 100);
		print("Auto phase: %d mixer scale: %.3f\n", getAutoFlightPhaseValue(), mixerScale);
		print("Airborne latched: %d\n", flightWasAirborne);
	} else if (command == "flow") {
		// 光流诊断：原始体轴速度 + 估计速度（世界系，验证方向）+ 状态
		print("Flow healthy: %d\n", opticalFlowHealthy);
		print("TOF healthy: %d\n", tofHealthy);
		print("TOF UART healthy: %d raw: %umm blind-zone: %d age: %ums\n",
			tofPacketHealthy, (unsigned int)opticalFlowTofDistanceMm, tofRangeInBlindZone,
			tofPacketTimestamp == 0 ? 0 : millis() - tofPacketTimestamp);
		print("TOF active: %d\n", tofHealthy && opticalFlowHeight > 0.05f && opticalFlowHeight < 6.0f);
		print("UART recoveries: %lu\n", (unsigned long)opticalFlowUartRestarts);
		print("TF-0850 version: %u packet seq: %u\n",
			(unsigned int)opticalFlowModuleVersion, (unsigned int)opticalFlowSensorPacketSequence);
		print("Packet gaps: %lu duplicates: %lu out-of-order: %lu\n",
			(unsigned long)opticalFlowPacketGapCount,
			(unsigned long)opticalFlowPacketDuplicateCount,
			(unsigned long)opticalFlowPacketOutOfOrderCount);
		print("TOF height: %.2f m strength: %u/100\n",
			opticalFlowHeight, (unsigned int)opticalFlowTofStrength);
		print("TOF seq: %u age: %ums dt: %.3fs\n", tofSequence,
			tofTimestamp == 0 ? 0 : millis() - tofTimestamp, tofSampleDt);
		print("Flow integration: %uus min: %uus max: %uus\n",
			(unsigned int)opticalFlowIntegrationTimeUs,
			(unsigned int)opticalFlowIntegrationTimeMinUs,
			(unsigned int)opticalFlowIntegrationTimeMaxUs);
		print("RawVel X: %.2f m/s\n", opticalFlowVelocityX);   // 光流原始体轴速度
		print("RawVel Y: %.2f m/s\n", opticalFlowVelocityY);
		print("Filtered body X/Y: %.2f %.2f m/s\n", flowFilteredBodyVel.x, flowFilteredBodyVel.y);
		print("Gyro apparent X/Y: %.2f %.2f m/s\n", flowGyroBodyVel.x, flowGyroBodyVel.y);
		print("Gyro compensation P/R: %.3f %.3f delay: %.0f ms\n",
			flowGyroCompPitch, flowGyroCompRoll, flowGyroDelayMs);
		print("Ground bias: %.3f %.3f m/s samples: %u/30 ready: %d fallback: %d\n",
			flowBias.x, flowBias.y, flowBiasSamples, flowBiasReady, flowBiasFallbackActive);
		print("Est body X/Y: %.2f %.2f m/s\n", velocity.x, velocity.y);
		print("Flow seq: %u age: %ums dt: %.3fs\n", opticalFlowSequence,
			opticalFlowTimestamp == 0 ? 0 : millis() - opticalFlowTimestamp, opticalFlowSampleDt);
		print("Position X/Y: %.2f %.2f m\n", position.x, position.y);
		print("UsingFlow: %d PosGate: %d HoldGate: %d Locked: %d FlowReject: %d PosReject: %d\n",
			flowCtrlUsingFlow, flowPositionGateOpen, posHoldGateOpen, posHoldLocked,
			flowRejectReason, posHoldRejectReason);
		print("PosCmd roll/pitch: %.3f %.3f rad\n", posRollCmd, posPitchCmd);
	} else if (command == "ap") {
#if WIFI_ENABLED
		if (!rejectWhileArmed()) configWiFi(true, arg0.c_str(), arg1.c_str());
#endif
	} else if (command == "sta") {
#if WIFI_ENABLED
		if (!rejectWhileArmed()) configWiFi(false, arg0.c_str(), arg1.c_str());
#endif
	} else if (command == "mot") {
		print("front-right %g front-left %g rear-right %g rear-left %g\n",
			motors[MOTOR_FRONT_RIGHT], motors[MOTOR_FRONT_LEFT], motors[MOTOR_REAR_RIGHT], motors[MOTOR_REAR_LEFT]);
	} else if (command == "log") {
		printLogHeader();
		if (arg0 == "dump") printLogData();
	} else if (command == "cr") {
		if (armed) print("RC calibration rejected while armed\n"); else calibrateRC();
	} else if (command == "ca") {
		if (armed) print("Accel calibration rejected while armed\n"); else calibrateAccel();
	} else if (command == "cg") {
		if (armed) print("Gyro calibration rejected while armed\n");
		else { resetGyroCalibration(); printGyroCalibrationStatus(); }
	} else if (command == "mfr") {
		testMotor(MOTOR_FRONT_RIGHT);
	} else if (command == "mfl") {
		testMotor(MOTOR_FRONT_LEFT);
	} else if (command == "mrr") {
		testMotor(MOTOR_REAR_RIGHT);
	} else if (command == "mrl") {
		testMotor(MOTOR_REAR_LEFT);
	} else if (command == "sys") {
#ifdef ESP32
		print("Build: %s\n", OPEN32DRONE_BUILD_ID);
		print("Source SHA-256: %s\n", OPEN32DRONE_FIRMWARE_SOURCE_SHA256);
		print("Chip: %s\n", ESP.getChipModel());
		print("Temperature: %.1f °C\n", temperatureRead());
		print("Free heap: %d\n", ESP.getFreeHeap());
		print("Loop: %.0fHz avg %.3fms p95 %.3fms p99 %.3fms max %.3fms\n",
			loopRate, loopDtAverageMs, loopDtP95Ms, loopDtP99Ms, loopDtMaxMs);
		print("Control schedule: %uHz misses %u max late %uus\n",
			CONTROL_LOOP_TARGET_HZ, loopDeadlineMissCount, loopDeadlineMaxLatenessUs);
		print("Flow packets: valid %u invalid %u\n", opticalFlowValidPackets, opticalFlowInvalidPackets);
		print("TOF packets: valid %u invalid %u\n", tofValidPackets, tofInvalidPackets);
		print("Parameter storage: %s\n", parameterStorageHealthy ? "OK" : "ERROR");
		// Print tasks table
		print("Num  Task                Stack  Prio  Core  CPU%%\n");
		int taskCount = uxTaskGetNumberOfTasks();
		TaskStatus_t *systemState = new TaskStatus_t[taskCount];
		uint32_t totalRunTime = 0;
		uxTaskGetSystemState(systemState, taskCount, &totalRunTime);
		uint32_t runTimePercent = max(totalRunTime / 100, (uint32_t)1);
		for (int i = 0; i < taskCount; i++) {
			String core = systemState[i].xCoreID == tskNO_AFFINITY ? "*" : String(systemState[i].xCoreID);
			int cpuPercentage = systemState[i].ulRunTimeCounter / runTimePercent;
			print("%-5d%-20s%-7d%-6d%-6s%d\n",systemState[i].xTaskNumber, systemState[i].pcTaskName,
				systemState[i].usStackHighWaterMark, systemState[i].uxCurrentPriority, core, cpuPercentage);
		}
		delete[] systemState;
#endif
	} else if (command == "reset") {
		if (!rejectWhileArmed()) {
			attitude = Quaternion();
			resetGyroCalibration();
		}
	} else if (command == "reboot") {
		if (!rejectWhileArmed()) ESP.restart();
	} else {
		print("Invalid command: %s\n", command.c_str());
	}
}

void handleInput() {
	static bool showMotd = true;
	static String input;

	if (showMotd) {
		print("%s\n", motd);
		showMotd = false;
	}

	while (Serial.available()) {
		char c = Serial.read();
		if (c == '\n') {
			doCommand(input);
			input.clear();
		} else if (c != '\r' && input.length() < 256) {
			input += c;
		} else if (input.length() >= 256) {
			input.clear();
			print("Command rejected: input too long\n");
		}
	}
}
