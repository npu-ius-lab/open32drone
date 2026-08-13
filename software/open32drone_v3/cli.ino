// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Implementation of command line interface

#include "pid.h"
#include "vector.h"
#include "util.h"

extern const int MOTOR_REAR_LEFT, MOTOR_REAR_RIGHT, MOTOR_FRONT_RIGHT, MOTOR_FRONT_LEFT;
extern const int RAW, ACRO, STAB, AUTO;
extern float t, dt, loopRate, loopDtAverageMs, loopDtMaxMs;
extern uint32_t loopOverrunCount;
extern uint16_t channels[16];
extern float controlTime;
// 光流定高系统状态（control.ino 定义，cli.ino 拼接在其前需前向声明）
extern bool altitudeHoldEngaged;
extern float altitudeHoldTarget;
extern float thrustTarget;
extern int altitudeHoldRejectReason;
// 光流（flow.ino 定义）
extern bool opticalFlowHealthy;
extern float opticalFlowHeight;
extern float opticalFlowVelocityX;
extern float opticalFlowVelocityY;
extern int mode;
extern bool armed;
extern float voltage; // from power.ino
extern LowPassFilter<Vector> gyroBiasFilter; // from imu.ino
extern Vector velocity; // 估计速度（estimate.ino estimateHorizontalVelocity，世界系）
extern Vector flowFilteredBodyVel, flowGyroBodyVel;
extern float flowGyroCompPitch, flowGyroCompRoll, flowGyroDelayMs;
extern bool flowCtrlUsingFlow; // 光流数据是否用于控制（estimate.ino）
extern int flowRejectReason; // 光流拒绝原因（estimate.ino）
extern uint32_t opticalFlowSequence;
extern uint32_t opticalFlowTimestamp;
extern float opticalFlowSampleDt;
extern bool flowPositionGateOpen;
extern bool posHoldGateOpen;
extern bool posHoldLocked;
extern int posHoldRejectReason;
extern float posRollCmd, posPitchCmd;
extern uint32_t opticalFlowValidPackets, opticalFlowInvalidPackets;
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
"ps - show pitch/roll/yaw\n"
"psq - show attitude quaternion\n"
"imu - show IMU data\n"
"arm - arm the drone\n"
"disarm - disarm the drone\n"
"raw/stab/acro/auto - set mode\n"
"rc - show RC data\n"
"wifi - show Wi-Fi info\n"
"ap <ssid> <pass> - set AP SSID/password (reboot to apply)\n"
"pw - show battery voltage\n"
"alt - show alt-hold state (target/baro/thrust)\n"
"flow - show optical flow info (health, TOF, vel)\n"
"mot - show motor output\n"
"log [dump] - print log header [and data]\n"
"cr - calibrate RC\n"
"ca - calibrate accel\n"
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
	float start = t;
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

	// execute command
	if (command == "help" || command == "motd") {
		print("%s\n", motd);
	} else if (command == "p" && arg0 == "") {
		printParameters();
	} else if (command == "p" && arg0 != "" && arg1 == "") {
		print("%s = %g\n", arg0.c_str(), getParameter(arg0.c_str()));
	} else if (command == "p") {
		bool success = setParameter(arg0.c_str(), arg1.toFloat());
		if (success) {
			print("%s = %g\n", arg0.c_str(), arg1.toFloat());
		} else {
			print("Parameter not found: %s\n", arg0.c_str());
		}
	} else if (command == "preset") {
		resetParameters();
	} else if (command == "time") {
		print("Time: %f\n", t);
		print("Loop rate: %.0f\n", loopRate);
		print("dt: %f avg: %.3fms max: %.3fms overruns: %u\n",
			dt, loopDtAverageMs, loopDtMaxMs, loopOverrunCount);
	} else if (command == "ps") {
		Vector a = attitude.toEuler();
		print("roll: %f pitch: %f yaw: %f\n", degrees(a.x), degrees(a.y), degrees(a.z));
	} else if (command == "psq") {
		print("qw: %f qx: %f qy: %f qz: %f\n", attitude.w, attitude.x, attitude.y, attitude.z);
	} else if (command == "imu") {
		printIMUInfo();
		printIMUCalibration();
		print("landed: %d\n", landed);
	} else if (command == "arm") {
		armed = true;
	} else if (command == "disarm") {
		armed = false;
	} else if (command == "raw") {
		mode = RAW;
	} else if (command == "stab") {
		mode = STAB;
	} else if (command == "acro") {
		mode = ACRO;
	} else if (command == "auto") {
		mode = AUTO;
	} else if (command == "rc") {
		print("channels: ");
		for (int i = 0; i < 16; i++) {
			print("%u ", channels[i]);
		}
		print("\nroll: %g pitch: %g yaw: %g throttle: %g mode: %g\n",
			controlRoll, controlPitch, controlYaw, controlThrottle, controlMode);
		print("time: %.1f\n", controlTime);
		print("mode: %s\n", getModeName());
		print("armed: %d\n", armed);
	} else if (command == "wifi") {
#if WIFI_ENABLED
		printWiFiInfo();
#endif
	} else if (command == "pw") {
		print("Voltage: %.1f V\n", voltage);
	} else if (command == "alt") {
		// TOF 定高状态（叠加式：基础油门=杆位，PID 修正叠加）
		print("Mode: %s\n", getModeName());
		print("AltHold: %d\n", altitudeHoldEngaged);
		print("Reject: %d\n", altitudeHoldRejectReason);
		print("Target: %.2f m\n", altitudeHoldTarget);
		print("Alt(TOF): %.2f m\n", getHoldAltitude());
		print("Thrust: %.1f%%\n", thrustTarget * 100);
	} else if (command == "flow") {
		// 光流诊断：原始体轴速度 + 估计速度（世界系，验证方向）+ 状态
		print("Flow healthy: %d\n", opticalFlowHealthy);
		print("TOF active: %d\n", opticalFlowHealthy && opticalFlowHeight > 0.05f && opticalFlowHeight < 6.0f);
		print("TOF height: %.2f m\n", opticalFlowHeight);
		print("RawVel X: %.2f m/s\n", opticalFlowVelocityX);   // 光流原始体轴速度
		print("RawVel Y: %.2f m/s\n", opticalFlowVelocityY);
		print("Filtered body X/Y: %.2f %.2f m/s\n", flowFilteredBodyVel.x, flowFilteredBodyVel.y);
		print("Gyro apparent X/Y: %.2f %.2f m/s\n", flowGyroBodyVel.x, flowGyroBodyVel.y);
		print("Gyro compensation P/R: %.3f %.3f delay: %.0f ms\n",
			flowGyroCompPitch, flowGyroCompRoll, flowGyroDelayMs);
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
		configWiFi(true, arg0.c_str(), arg1.c_str());
#endif
	} else if (command == "mot") {
		print("front-right %g front-left %g rear-right %g rear-left %g\n",
			motors[MOTOR_FRONT_RIGHT], motors[MOTOR_FRONT_LEFT], motors[MOTOR_REAR_RIGHT], motors[MOTOR_REAR_LEFT]);
	} else if (command == "log") {
		printLogHeader();
		if (arg0 == "dump") printLogData();
	} else if (command == "cr") {
		calibrateRC();
	} else if (command == "ca") {
		calibrateAccel();
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
		print("Chip: %s\n", ESP.getChipModel());
		print("Temperature: %.1f °C\n", temperatureRead());
		print("Free heap: %d\n", ESP.getFreeHeap());
		print("Loop: %.0fHz avg %.3fms max %.3fms overruns %u\n",
			loopRate, loopDtAverageMs, loopDtMaxMs, loopOverrunCount);
		print("Flow packets: valid %u invalid %u\n", opticalFlowValidPackets, opticalFlowInvalidPackets);
		print("Parameter storage: %s\n", parameterStorageHealthy ? "OK" : "ERROR");
		// Print tasks table
		print("Num  Task                Stack  Prio  Core  CPU%%\n");
		int taskCount = uxTaskGetNumberOfTasks();
		TaskStatus_t *systemState = new TaskStatus_t[taskCount];
		uint32_t totalRunTime;
		uxTaskGetSystemState(systemState, taskCount, &totalRunTime);
		for (int i = 0; i < taskCount; i++) {
			String core = systemState[i].xCoreID == tskNO_AFFINITY ? "*" : String(systemState[i].xCoreID);
			int cpuPercentage = systemState[i].ulRunTimeCounter / (totalRunTime / 100);
			print("%-5d%-20s%-7d%-6d%-6s%d\n",systemState[i].xTaskNumber, systemState[i].pcTaskName,
				systemState[i].usStackHighWaterMark, systemState[i].uxCurrentPriority, core, cpuPercentage);
		}
		delete[] systemState;
#endif
	} else if (command == "reset") {
		attitude = Quaternion();
		gyroBiasFilter.reset(); // also reset gyro bias filter
	} else if (command == "reboot") {
		ESP.restart();
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
		} else {
			input += c;
		}
	}
}
