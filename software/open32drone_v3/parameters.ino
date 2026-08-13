// Copyright (c) 2024 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Parameters storage in flash memory
// merged Parameter struct with callback support + int/float union

#include <Preferences.h>
#include "util.h"
#include "lpf.h"
#include "vector.h"

extern float channelZero[16];
extern float channelMax[16];
extern float rollChannel, pitchChannel, throttleChannel, yawChannel, armedChannel, modeChannel;
extern int flightModes[3];
extern float levelWeight;
extern LowPassFilter<Vector> gyroBiasFilter;
extern LowPassFilter<Vector> accFilter;
extern float positionHoldP, positionStickMaxSpeed;
extern float holdPX, holdIX, holdDX, holdPY, holdIY, holdDY, maxFlowAngleRate;
extern float flowVelocitySmoothing, flowInnovationLimit, flowGyroCompPitch, flowGyroCompRoll, flowGyroDelayMs, flowBiasAdapt;
extern int wifiMode, udpLocalPort, udpRemotePort;
extern int mavlinkSysId;
extern Rate telemetrySlow, telemetryFast;
extern int voltagePin;
extern float voltageScale;
extern LowPassFilter<float> voltageFilter;
extern float rcLossTimeout, descendTime;

Preferences storage;
bool parameterStorageHealthy = true;

struct Parameter {
	const char *name; // max length is 15 (Preferences key limit)
	bool integer;
	union { float *f; int *i; }; // pointer to the variable
	float cache; // what's stored in flash
	void (*callback)(); // called after parameter change
	Parameter(const char *name, float *variable, void (*callback)() = nullptr) : name(name), integer(false), f(variable), callback(callback) {};
	Parameter(const char *name, int *variable, void (*callback)() = nullptr) : name(name), integer(true), i(variable), callback(callback) {};
	float getValue() const { return integer ? *i : *f; };
	void setValue(const float value) { if (integer) *i = value; else *f = value; };
};

bool validParameterValue(bool integer, float *pointer, float value) {
	if (!isfinite(value)) return false;
	if (integer) return true;
	if (pointer == &accFilter.alpha) return value > 0.0f && value <= 1.0f;
	if (pointer == &accWeight) return value >= 0.0f && value <= 0.02f;
	if (pointer == &levelWeight) return value >= 0.0f && value <= 0.0005f;
	if (pointer == &positionHoldP) return value >= 0.0f && value <= 3.0f;
	if (pointer == &positionStickMaxSpeed) return value >= 0.0f && value <= 3.0f;
	if (pointer == &holdPX || pointer == &holdPY) return value >= 0.0f && value <= 2.0f;
	if (pointer == &holdIX || pointer == &holdIY) return value >= 0.0f && value <= 1.0f;
	if (pointer == &holdDX || pointer == &holdDY) return value >= 0.0f && value <= 1.0f;
	if (pointer == &maxFlowAngleRate) return value >= 0.05f && value <= 10.0f;
	if (pointer == &flowVelocitySmoothing) return value > 0.0f && value <= 1.0f;
	if (pointer == &flowInnovationLimit) return value >= 0.01f && value <= 5.0f;
	if (pointer == &flowGyroCompPitch || pointer == &flowGyroCompRoll) return abs(value) <= 5.0f;
	if (pointer == &flowGyroDelayMs) return value >= 0.0f && value <= 100.0f;
	if (pointer == &flowBiasAdapt) return value >= 0.0f && value <= 1.0f;
	return true;
}

Parameter parameters[] = {
	// control
	{"CTL_R_RATE_P", &rollRatePID.p},
	{"CTL_R_RATE_I", &rollRatePID.i},
	{"CTL_R_RATE_D", &rollRatePID.d},
	{"CTL_R_RATE_WU", &rollRatePID.windup},
	{"CTL_R_RATE_D_A", &rollRatePID.lpf.alpha},
	{"CTL_P_RATE_P", &pitchRatePID.p},
	{"CTL_P_RATE_I", &pitchRatePID.i},
	{"CTL_P_RATE_D", &pitchRatePID.d},
	{"CTL_P_RATE_WU", &pitchRatePID.windup},
	{"CTL_P_RATE_D_A", &pitchRatePID.lpf.alpha},
	{"CTL_Y_RATE_P", &yawRatePID.p},
	{"CTL_Y_RATE_I", &yawRatePID.i},
	{"CTL_Y_RATE_D", &yawRatePID.d},
	{"CTL_Y_RATE_D_A", &yawRatePID.lpf.alpha},
	{"CTL_R_P", &rollPID.p},
	{"CTL_R_I", &rollPID.i},
	{"CTL_R_D", &rollPID.d},
	{"CTL_P_P", &pitchPID.p},
	{"CTL_P_I", &pitchPID.i},
	{"CTL_P_D", &pitchPID.d},
	{"CTL_Y_P", &yawPID.p},
	{"CTL_P_RATE_MAX", &maxRate.y},
	{"CTL_R_RATE_MAX", &maxRate.x},
	{"CTL_Y_RATE_MAX", &maxRate.z},
	{"CTL_TILT_MAX", &tiltMax},
	{"CTL_FLT_MODE_0", &flightModes[0]},
	{"CTL_FLT_MODE_1", &flightModes[1]},
	{"CTL_FLT_MODE_2", &flightModes[2]},
	// imu
	{"IMU_ROT_ROLL", &imuRotation.x},
	{"IMU_ROT_PITCH", &imuRotation.y},
	{"IMU_ROT_YAW", &imuRotation.z},
	{"IMU_ACC_BIAS_X", &accBias.x},
	{"IMU_ACC_BIAS_Y", &accBias.y},
	{"IMU_ACC_BIAS_Z", &accBias.z},
	{"IMU_ACC_SCALE_X", &accScale.x},
	{"IMU_ACC_SCALE_Y", &accScale.y},
	{"IMU_ACC_SCALE_Z", &accScale.z},
	{"IMU_GYRO_BIAS_A", &gyroBiasFilter.alpha},
	{"IMU_ACC_LPF_A", &accFilter.alpha},
	// estimate
	{"EST_ACC_WEIGHT", &accWeight},
	{"EST_LVL_WEIGHT", &levelWeight},
	{"EST_RATES_LPF_A", &ratesFilter.alpha},
	// position hold / optical flow
	{"POS_HOLD_P", &positionHoldP},
	{"POS_STICK_V", &positionStickMaxSpeed},
	{"POS_VEL_P_X", &holdPX},
	{"POS_VEL_P_Y", &holdPY},
	{"POS_VEL_I_X", &holdIX},
	{"POS_VEL_I_Y", &holdIY},
	{"POS_VEL_D_X", &holdDX},
	{"POS_VEL_D_Y", &holdDY},
	{"POS_CMD_RATE", &maxFlowAngleRate},
	{"FLOW_VEL_ALPHA", &flowVelocitySmoothing},
	{"FLOW_INNOV_LIM", &flowInnovationLimit},
	{"FLOW_GYRO_P", &flowGyroCompPitch},
	{"FLOW_GYRO_R", &flowGyroCompRoll},
	{"FLOW_GYRO_DLY", &flowGyroDelayMs},
	{"FLOW_BIAS_A", &flowBiasAdapt},
	// rc
	{"RC_ZERO_0", &channelZero[0]},
	{"RC_ZERO_1", &channelZero[1]},
	{"RC_ZERO_2", &channelZero[2]},
	{"RC_ZERO_3", &channelZero[3]},
	{"RC_ZERO_4", &channelZero[4]},
	{"RC_ZERO_5", &channelZero[5]},
	{"RC_ZERO_6", &channelZero[6]},
	{"RC_ZERO_7", &channelZero[7]},
	{"RC_MAX_0", &channelMax[0]},
	{"RC_MAX_1", &channelMax[1]},
	{"RC_MAX_2", &channelMax[2]},
	{"RC_MAX_3", &channelMax[3]},
	{"RC_MAX_4", &channelMax[4]},
	{"RC_MAX_5", &channelMax[5]},
	{"RC_MAX_6", &channelMax[6]},
	{"RC_MAX_7", &channelMax[7]},
	{"RC_ROLL", &rollChannel},
	{"RC_PITCH", &pitchChannel},
	{"RC_THROTTLE", &throttleChannel},
	{"RC_YAW", &yawChannel},
	{"RC_MODE", &modeChannel},
	// wifi
	{"WIFI_MODE", &wifiMode},
	{"WIFI_PORT_LOC", &udpLocalPort},
	{"WIFI_PORT_REM", &udpRemotePort},
	// mavlink
	{"MAV_SYS_ID", &mavlinkSysId},
	{"MAV_RATE_SLOW", &telemetrySlow.rate},
	{"MAV_RATE_FAST", &telemetryFast.rate},
	// power
	{"PWR_VOLT_PIN", &voltagePin},
	{"PWR_VOLT_SCALE", &voltageScale},
	{"PWR_VOLT_LPF_A", &voltageFilter.alpha},
	// safety
	{"SF_RC_LOSS_TIME", &rcLossTimeout},
	{"SF_DESCEND_TIME", &descendTime},
};

void setupParameters() {
	parameterStorageHealthy = storage.begin("flix", false);
	if (!parameterStorageHealthy) {
		for (auto &parameter : parameters) parameter.cache = parameter.getValue();
		print("Parameter storage unavailable, using defaults\n");
		return;
	}
	// Missing keys use compiled defaults. Do not pre-fill NVS with every default:
	// Preferences stores each key as a separate entry and the old behavior could
	// exhaust the small NVS partition before MOT_IDLE/Wi-Fi settings were saved.
	for (auto &parameter : parameters) {
		if (storage.isKey(parameter.name)) {
			float stored = storage.getFloat(parameter.name, parameter.getValue());
			if (validParameterValue(parameter.integer, parameter.integer ? nullptr : parameter.f, stored)) parameter.setValue(stored);
			else print("Ignoring invalid parameter: %s\n", parameter.name);
		}
		parameter.cache = parameter.getValue();
	}
	// Migrate the old v3 value (0.001) to the conservative Flix 1.5 default.
	// A value tuned within this safe range, including zero, remains configurable.
	if (!(levelWeight >= 0.0f && levelWeight <= 0.0005f)) {
		levelWeight = 0.0002f;
	}
}

int parametersCount() {
	return sizeof(parameters) / sizeof(parameters[0]);
}

const char *getParameterName(int index) {
	if (index < 0 || index >= parametersCount()) return "";
	return parameters[index].name;
}

float getParameter(int index) {
	if (index < 0 || index >= parametersCount()) return NAN;
	return parameters[index].getValue();
}

float getParameter(const char *name) {
	for (auto &parameter : parameters) {
		if (strcasecmp(parameter.name, name) == 0) {
			return parameter.getValue();
		}
	}
	return NAN;
}

bool setParameter(const char *name, const float value) {
	for (auto &parameter : parameters) {
		if (strcasecmp(parameter.name, name) == 0) {
			if (!validParameterValue(parameter.integer, parameter.integer ? nullptr : parameter.f, value)) return false;
			parameter.setValue(value);
			if (parameter.callback) parameter.callback();
			return true;
		}
	}
	return false;
}

void syncParameters() {
	static Rate rate(1);
	if (!rate) return; // sync once per second
	if (motorsActive()) return; // don't use flash while flying, it may cause a delay
	if (!parameterStorageHealthy) return;

	for (auto &parameter : parameters) {
		if (parameter.getValue() == parameter.cache) continue; // no change
		if (isnan(parameter.getValue()) && isnan(parameter.cache)) continue; // both are NAN

		size_t written = storage.putFloat(parameter.name, parameter.getValue());
		if (written != sizeof(float)) {
			parameterStorageHealthy = false;
			print("Parameter save failed: %s\n", parameter.name);
			break;
		}
		parameter.cache = parameter.getValue(); // update cache
	}
}

void printParameters() {
	for (auto &parameter : parameters) {
		print("%s = %g\n", parameter.name, parameter.getValue());
	}
}

void resetParameters() {
	storage.clear();
	ESP.restart();
}
