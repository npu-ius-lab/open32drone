// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Implementation of command line interface

#include "pid.h"
#include "vector.h"
#include "quaternion.h"
#include "util.h"

#if defined(ESP32) && defined(ARDUINO_USB_MODE) && ARDUINO_USB_MODE && !ARDUINO_USB_CDC_ON_BOOT
#include "HWCDC.h"
HWCDC USBConsoleSerial;
#define USB_CONSOLE_ENABLED 1
#else
#define USB_CONSOLE_ENABLED 0
#endif

extern const int MOTOR_REAR_LEFT, MOTOR_REAR_RIGHT, MOTOR_FRONT_RIGHT, MOTOR_FRONT_LEFT;
extern const int RAW, ACRO, STAB, AUTO, ALT_HOLD, POS_HOLD;
extern float t, dt, loopRate;
extern uint16_t channels[16];
extern float controlTime;
extern float controlRoll, controlPitch, controlThrottle, controlYaw, controlMode;
extern int mode;
extern bool armed;
extern Quaternion attitude;
extern Quaternion attitudeTarget;
extern Vector position;
extern Vector velocity;
extern Vector rawWorldPos;
extern Vector flowRawBodyVel;
extern Vector flowGyroBodyVel;
extern Vector flowBias;
extern Vector flowInnov;
extern Vector rawBodyVel;
extern Vector flowCompBodyVel;
extern Vector rates;
extern Vector ratesTarget;
extern Vector torqueTarget;
extern float thrustTarget;
extern PID rollRatePID, pitchRatePID, yawRatePID, yawPID;
extern bool opticalFlowHealthy;
extern bool flowCtrlUsingFlow;
extern bool flowStationary;
extern bool flowAirborne;
extern bool flowCtrlZeroLocked;
extern bool posHoldGateOpen;
extern int flowRejectReason;
extern float flowArmMinHeight;
extern float flowArmMinThrottle;
extern float targetZ, altClimbRateTarget;
extern bool altHoldEngaged, autoTakeoffActive, autoTakeoffComplete;
extern float currentTiltLimitDebug, currentStabAttScaleDebug, currentStabRateScaleDebug, currentStabYawScaleDebug;
extern float imuAccelTrustDebug, imuAccelMeasureRDebug;
extern bool imuAccelClippedDebug, imuHeightAccelValidDebug;
extern bool imuAccelAngleUsedDebug;
extern float imuAccelAngleBlendDebug;
extern float imuAccRollAngleDebug, imuAccPitchAngleDebug;
extern float imuAccNormDebug, imuAccAttNormDebug, imuAccVibeDebug;
extern Vector imuAccForAttitudeDebug, imuAttitudeRatesDebug;
extern bool gyroBiasLearnAllowedDebug;
extern Vector acc, gyro;
extern float motors[4];
const char* getTakeoffStateName();
#if WIFI_ENABLED
bool connectWiFi(const char *ssid, const char *password, uint32_t timeoutMs);
bool configureWiFi(const char *ssid, const char *password);
void clearSavedWiFi();
void scanWiFi();
#endif

bool poseMonitorEnabled = false;
float poseMonitorRateHz = 5.0f;
float poseMonitorLastPrint = 0.0f;

const int MONITOR_OFF = 0;
const int MONITOR_FLOW_RAW = 1;
const int MONITOR_FLOW_CTRL = 2;
const int MONITOR_RATE = 3;
const int MONITOR_MOTOR = 4;
const int MONITOR_DIAG = 5;

int monitorMode = MONITOR_OFF;
float monitorRateHz = 5.0f;
float monitorLastPrint = 0.0f;
int pausedMonitorMode = MONITOR_OFF;
float pausedMonitorRateHz = 0.0f;

const char *getFlowRejectReasonName(int reason) {
	if (reason == 0) return "ok";
	if (reason == 1) return "stationary";
	if (reason == 2) return "no_flow";
	if (reason == 3) return "bad_height";
	if (reason == 4) return "tilt";
	if (reason == 5) return "not_airborne";
	return "unknown";
}

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
"pose [hz] - show/start rolling pose output\n"
"poseoff - stop rolling pose output\n"
"flowraw [hz] - show/start raw optical-flow debug output\n"
"flowctrl [hz] - show/start control optical-flow debug output\n"
"rate [hz] - show/start rate-loop debug output\n"
"mot [hz] - show/start motor mix debug output\n"
"diag [hz] - show/start estimator diagnostic output\n"
"monoff - stop flow/rate/motor/diag debug output\n"
"monpause/monresume - pause/resume the current debug output\n"
"imu - show IMU data\n"
"arm - arm the drone\n"
"disarm - disarm the drone\n"
"raw/stab/acro/auto/alt/pos - set mode\n"
"rc - show RC data\n"
"wifi [ssid password|reset] - show/connect/reset Wi-Fi\n"
"wifiscan - scan nearby Wi-Fi networks\n"
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
#if USB_CONSOLE_ENABLED
	USBConsoleSerial.print(buf);
#endif
#if WIFI_ENABLED
	mavlinkPrint(buf);
#endif
}

void setupConsole() {
	Serial.begin(115200);
#if USB_CONSOLE_ENABLED
	USBConsoleSerial.begin(115200);
#endif
	delay(300);
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

void printPoseSnapshot() {
	Vector euler = attitude.toEuler();
	print("pose: t:%.3f mode:%s arm:%d flow:%d use:%d air:%d gate:%d rej:%d(%s) thr:%.3f\n",
		t, getModeName(), armed ? 1 : 0,
		opticalFlowHealthy ? 1 : 0,
		flowCtrlUsingFlow ? 1 : 0,
		flowAirborne ? 1 : 0,
		posHoldGateOpen ? 1 : 0,
		flowRejectReason,
		getFlowRejectReasonName(flowRejectReason),
		controlThrottle);
	print("pose: att_deg r:%+.2f p:%+.2f y:%+.2f pos_m x:%+.3f y:%+.3f z:%+.3f vel_mps x:%+.3f y:%+.3f z:%+.3f\n",
		degrees(euler.x), degrees(euler.y), degrees(euler.z),
		position.x, position.y, position.z,
		velocity.x, velocity.y, velocity.z);
	print("pose: raw_flow pos_xy x:%+.3f y:%+.3f body_vel x:%+.3f y:%+.3f comp_vel x:%+.3f y:%+.3f air_min z:%.2f thr:%.2f\n",
		rawWorldPos.x, rawWorldPos.y,
		rawBodyVel.x, rawBodyVel.y,
		flowCompBodyVel.x, flowCompBodyVel.y,
		flowArmMinHeight, flowArmMinThrottle);
}

void updatePoseMonitor() {
	if (!poseMonitorEnabled) return;
	if (!(poseMonitorRateHz > 0.0f)) poseMonitorRateHz = 5.0f;
	if (valid(poseMonitorLastPrint) && t - poseMonitorLastPrint < 1.0f / poseMonitorRateHz) return;
	poseMonitorLastPrint = t;
	printPoseSnapshot();
}

void printFlowRawSnapshot() {
	print("flowraw: meas(%.3f,%.3f) gyro(%.3f,%.3f) comp(%.3f,%.3f) bias(%.3f,%.3f)\n",
		flowRawBodyVel.x, flowRawBodyVel.y,
		flowGyroBodyVel.x, flowGyroBodyVel.y,
		flowCompBodyVel.x, flowCompBodyVel.y,
		flowBias.x, flowBias.y);
	print("flowraw: body_vel(%.3f,%.3f) pos_xy(%.3f,%.3f) innov(%.3f,%.3f) h:%.3f ok:%d\n",
		rawBodyVel.x, rawBodyVel.y,
		rawWorldPos.x, rawWorldPos.y,
		flowInnov.x, flowInnov.y,
		opticalFlowHeight, opticalFlowHealthy ? 1 : 0);
}

void printFlowCtrlSnapshot() {
	print("flowctrl: st:%d air:%d zero:%d use:%d gate:%d rej:%d(%s) tk:%s alt:%d\n",
		flowStationary ? 1 : 0,
		flowAirborne ? 1 : 0,
		flowCtrlZeroLocked ? 1 : 0,
		flowCtrlUsingFlow ? 1 : 0,
		posHoldGateOpen ? 1 : 0,
		flowRejectReason,
		getFlowRejectReasonName(flowRejectReason),
		getTakeoffStateName(),
		altHoldEngaged ? 1 : 0);
	print("flowctrl: pos(%.3f,%.3f,%.3f) vel(%.3f,%.3f,%.3f) h:%.3f tz:%.2f vzT:%.2f tilt:%.1f\n",
		position.x, position.y, position.z,
		velocity.x, velocity.y, velocity.z,
		opticalFlowHeight, targetZ, altClimbRateTarget,
		degrees(currentTiltLimitDebug));
}

void printRateSnapshot() {
	Vector attitudeEuler = attitude.toEuler();
	Vector targetEuler = attitudeTarget.toEuler();
	float rollRate = degrees(rates.x);
	float pitchRate = degrees(rates.y);
	float yawRate = degrees(rates.z);
	float rollTarget = degrees(ratesTarget.x);
	float pitchTarget = degrees(ratesTarget.y);
	float yawTarget = degrees(ratesTarget.z);

	print("rate: mode:%s arm:%d thr:%.3f cmd r/p/y %.3f %.3f %.3f tk:%s attS:%.2f rateS:%.2f yawS:%.2f\n",
		getModeName(), armed ? 1 : 0, controlThrottle,
		controlRoll, controlPitch, controlYaw,
		getTakeoffStateName(),
		currentStabAttScaleDebug, currentStabRateScaleDebug, currentStabYawScaleDebug);
	print("rate: att cur(%.1f,%.1f,%.1f) tgt(%.1f,%.1f,%.1f)\n",
		degrees(attitudeEuler.x), degrees(attitudeEuler.y), degrees(attitudeEuler.z),
		degrees(targetEuler.x), degrees(targetEuler.y), degrees(targetEuler.z));
	print("rate: R cur:%+.1f tgt:%+.1f tq:%+.3f | P cur:%+.1f tgt:%+.1f tq:%+.3f | Y cur:%+.1f tgt:%+.1f tq:%+.3f\n",
		rollRate, rollTarget, torqueTarget.x,
		pitchRate, pitchTarget, torqueTarget.y,
		yawRate, yawTarget, torqueTarget.z);
	print("rate: pid R %.3f/%.3f/%.3f P %.3f/%.3f/%.3f Y %.3f/%.3f/%.3f YawP %.3f\n",
		rollRatePID.p, rollRatePID.i, rollRatePID.d,
		pitchRatePID.p, pitchRatePID.i, pitchRatePID.d,
		yawRatePID.p, yawRatePID.i, yawRatePID.d,
		yawPID.p);
}

void printMotorSnapshot() {
	float frontLeft = motors[MOTOR_FRONT_LEFT];
	float frontRight = motors[MOTOR_FRONT_RIGHT];
	float rearLeft = motors[MOTOR_REAR_LEFT];
	float rearRight = motors[MOTOR_REAR_RIGHT];
	float leftTotal = frontLeft + rearLeft;
	float rightTotal = frontRight + rearRight;
	float frontTotal = frontLeft + frontRight;
	float rearTotal = rearLeft + rearRight;

	print("mot: FL:%.3f FR:%.3f RL:%.3f RR:%.3f\n",
		frontLeft, frontRight, rearLeft, rearRight);
	print("mot: LRdiff:%+.3f FBdiff:%+.3f thr:%.3f thrustT:%.3f tq(%.3f,%.3f,%.3f) mode:%s arm:%d tk:%s\n",
		leftTotal - rightTotal, frontTotal - rearTotal,
		controlThrottle, thrustTarget,
		torqueTarget.x, torqueTarget.y, torqueTarget.z,
		getModeName(), armed ? 1 : 0, getTakeoffStateName());
}

void printDiagSnapshot() {
	Vector attitudeEuler = attitude.toEuler();
	print("diag: t:%.2f arm:%d mode:%s tk:%s accNorm:%.2f accFNorm:%.2f vibe:%.2f trust:%.2f R:%.3f blend:%.2f clip:%d zAcc:%d bias:%d\n",
		t, armed ? 1 : 0, getModeName(), getTakeoffStateName(),
		imuAccNormDebug, imuAccAttNormDebug, imuAccVibeDebug,
		imuAccelTrustDebug, imuAccelMeasureRDebug, imuAccelAngleBlendDebug,
		imuAccelClippedDebug ? 1 : 0,
		imuHeightAccelValidDebug ? 1 : 0,
		gyroBiasLearnAllowedDebug ? 1 : 0);
	print("diag: acc raw(%.2f,%.2f,%.2f) filt(%.2f,%.2f,%.2f) accAng(%.1f,%.1f) att(%.1f,%.1f,%.1f)\n",
		acc.x, acc.y, acc.z,
		imuAccForAttitudeDebug.x, imuAccForAttitudeDebug.y, imuAccForAttitudeDebug.z,
		degrees(imuAccRollAngleDebug), degrees(imuAccPitchAngleDebug),
		degrees(attitudeEuler.x), degrees(attitudeEuler.y), degrees(attitudeEuler.z));
	print("diag: gyro raw(%.1f,%.1f,%.1f) rates(%.1f,%.1f,%.1f) used(%.1f,%.1f,%.1f)\n",
		degrees(gyro.x), degrees(gyro.y), degrees(gyro.z),
		degrees(rates.x), degrees(rates.y), degrees(rates.z),
		degrees(imuAttitudeRatesDebug.x), degrees(imuAttitudeRatesDebug.y), degrees(imuAttitudeRatesDebug.z));
}

void printMonitorSnapshot(int mode) {
	if (mode == MONITOR_FLOW_RAW) printFlowRawSnapshot();
	else if (mode == MONITOR_FLOW_CTRL) printFlowCtrlSnapshot();
	else if (mode == MONITOR_RATE) printRateSnapshot();
	else if (mode == MONITOR_MOTOR) printMotorSnapshot();
	else if (mode == MONITOR_DIAG) printDiagSnapshot();
}

const char* getMonitorName(int mode) {
	if (mode == MONITOR_FLOW_RAW) return "flowraw";
	if (mode == MONITOR_FLOW_CTRL) return "flowctrl";
	if (mode == MONITOR_RATE) return "rate";
	if (mode == MONITOR_MOTOR) return "mot";
	if (mode == MONITOR_DIAG) return "diag";
	return "off";
}

void setMonitorMode(int mode, float rateHz) {
	monitorMode = mode;
	monitorRateHz = constrain(rateHz, 1.0f, 50.0f);
	monitorLastPrint = NAN;
	print("%s monitor: %.1f Hz\n", getMonitorName(monitorMode), monitorRateHz);
}

void updateMonitor() {
	if (monitorMode == MONITOR_OFF) return;
	if (valid(monitorLastPrint) && t - monitorLastPrint < 1.0f / monitorRateHz) return;
	monitorLastPrint = t;
	printMonitorSnapshot(monitorMode);
}

float parseMonitorRate(String arg0) {
	float rateHz = arg0.toFloat();
	if (!valid(rateHz) || rateHz <= 0.0f) rateHz = 5.0f;
	return rateHz;
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
		print("dt: %f\n", dt);
	} else if (command == "ps") {
		Vector a = attitude.toEuler();
		print("roll: %f pitch: %f yaw: %f\n", degrees(a.x), degrees(a.y), degrees(a.z));
	} else if (command == "psq") {
		print("qw: %f qx: %f qy: %f qz: %f\n", attitude.w, attitude.x, attitude.y, attitude.z);
	} else if (command == "pose") {
		if (arg0 == "") {
			printPoseSnapshot();
		} else {
			poseMonitorRateHz = constrain(arg0.toFloat(), 1.0f, 50.0f);
			poseMonitorEnabled = true;
			poseMonitorLastPrint = NAN;
			print("Pose monitor: %.1f Hz\n", poseMonitorRateHz);
		}
	} else if (command == "poseoff") {
		poseMonitorEnabled = false;
		print("Pose monitor stopped\n");
	} else if (command == "flowraw") {
		if (arg0 == "") printFlowRawSnapshot();
		else setMonitorMode(MONITOR_FLOW_RAW, parseMonitorRate(arg0));
	} else if (command == "flowctrl") {
		if (arg0 == "") printFlowCtrlSnapshot();
		else setMonitorMode(MONITOR_FLOW_CTRL, parseMonitorRate(arg0));
	} else if (command == "rate") {
		if (arg0 == "") printRateSnapshot();
		else setMonitorMode(MONITOR_RATE, parseMonitorRate(arg0));
	} else if (command == "mot") {
		if (arg0 == "") printMotorSnapshot();
		else setMonitorMode(MONITOR_MOTOR, parseMonitorRate(arg0));
	} else if (command == "diag") {
		if (arg0 == "") printDiagSnapshot();
		else setMonitorMode(MONITOR_DIAG, parseMonitorRate(arg0));
	} else if (command == "monoff" || command == "flowoff" || command == "rateoff" || command == "motoff" || command == "diagoff") {
		monitorMode = MONITOR_OFF;
		print("Monitor stopped\n");
	} else if (command == "monpause") {
		pausedMonitorMode = monitorMode;
		pausedMonitorRateHz = monitorRateHz;
		monitorMode = MONITOR_OFF;
		print("Monitor paused: %s %.1f Hz\n", getMonitorName(pausedMonitorMode), pausedMonitorRateHz);
	} else if (command == "monresume") {
		if (pausedMonitorMode == MONITOR_OFF) {
			print("No paused monitor\n");
		} else {
			setMonitorMode(pausedMonitorMode, pausedMonitorRateHz);
		}
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
	} else if (command == "alt") {
		mode = ALT_HOLD;
	} else if (command == "pos") {
		mode = POS_HOLD;
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
		if (arg0 == "") {
			printWiFiInfo();
		} else if (arg0 == "reset") {
			clearSavedWiFi();
		} else {
			configureWiFi(arg0.c_str(), arg1.c_str());
			printWiFiInfo();
		}
#endif
	} else if (command == "wifiscan") {
#if WIFI_ENABLED
		scanWiFi();
#endif
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
		print("Tasks: %d\n", uxTaskGetNumberOfTasks());
#endif
	} else if (command == "reset") {
		attitude = Quaternion();
	} else if (command == "reboot") {
		ESP.restart();
	} else {
		print("Invalid command: %s\n", command.c_str());
	}
}

void handleInput() {
	static bool showMotd = true;
	static String serialInput;
#if USB_CONSOLE_ENABLED
	static String usbInput;
#endif

	if (showMotd) {
		print("%s\n", motd);
		showMotd = false;
	}

	readConsoleInput(Serial, serialInput);
#if USB_CONSOLE_ENABLED
	readConsoleInput(USBConsoleSerial, usbInput);
#endif

	updatePoseMonitor();
	updateMonitor();
}

void readConsoleInput(Stream &stream, String &input) {
	while (stream.available()) {
		char c = stream.read();
		if (c == '\r') continue;
		if (c == '\n') {
			doCommand(input);
			input.clear();
		} else {
			input += c;
		}
	}
}
