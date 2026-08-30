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
extern float rollChannel, pitchChannel, throttleChannel, yawChannel, modeChannel;
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
extern float voltageCompensationReference, voltageCompensationSlope, voltageCompensationMax;
extern LowPassFilter<float> voltageFilter;
void configureVoltageInput();
void resetVoltageMeasurement();
void updateIMURotation();
extern float rcLossTimeout, descendTime;
extern bool armed;
extern float altitudeHoldP, altitudeHoldI, altitudeHoldD, altitudeHoldIntegralLimit;
extern float altitudeHoldMaxCorrection, altitudeHoldMaxClimbRate, altitudeStickDeadband, altitudeHoverThrust;
extern float altitudeTakeoffHeight, altitudeTakeoffTrigger;

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

bool validParameterValue(const char *name, bool integer, float *pointer, float value) {
	if (!isfinite(value)) return false;
	if (integer && value != truncf(value)) return false;
	if (strncmp(name, "CTL_FLT_MODE_", 13) == 0) {
		return value == STAB || value == ALT_HOLD || value == POS_HOLD;
	}
	if (strcmp(name, "WIFI_MODE") == 0) return value >= 0 && value <= 2;
	if (strncmp(name, "WIFI_PORT_", 10) == 0) return value >= 1 && value <= 65535;
	if (strcmp(name, "MAV_SYS_ID") == 0) return value >= 1 && value <= 255;
	if (strncmp(name, "MAV_RATE_", 9) == 0) return value >= 1 && value <= 100;
	if (strcmp(name, "PWR_VOLT_PIN") == 0) return value == -1 || value == A0;
	if (integer) return false;
	if (strncmp(name, "RC_ZERO_", 8) == 0 || strncmp(name, "RC_MAX_", 7) == 0) return value >= 0 && value <= 2047;
	if (strcmp(name, "RC_ROLL") == 0 || strcmp(name, "RC_PITCH") == 0 ||
		strcmp(name, "RC_THROTTLE") == 0 || strcmp(name, "RC_YAW") == 0 || strcmp(name, "RC_MODE") == 0) {
		// Only channels 0..7 have persistent endpoint parameters.
		return value == -1.0f || (value >= 0.0f && value <= 7.0f && value == floorf(value));
	}
	if (strstr(name, "_D_A") || strcmp(name, "EST_RATES_LPF_A") == 0 ||
		strcmp(name, "PWR_VOLT_LPF_A") == 0 || strcmp(name, "IMU_GYRO_BIAS_A") == 0) return value > 0.0f && value <= 1.0f;
	if (strstr(name, "RATE_MAX")) return value >= 0.1f && value <= 20.0f;
	if (strcmp(name, "CTL_R_RATE_WU") == 0 || strcmp(name, "CTL_P_RATE_WU") == 0) return value >= 0.0f && value <= 1.0f;
	if (strcmp(name, "CTL_TILT_MAX") == 0) return value >= radians(5.0f) && value <= radians(60.0f);
	if (strncmp(name, "CTL_", 4) == 0 && (strstr(name, "_P") || strstr(name, "_I") || strstr(name, "_D"))) return value >= 0.0f && value <= 20.0f;
	if (strncmp(name, "IMU_ROT_", 8) == 0) return abs(value) <= TWO_PI;
	if (strncmp(name, "IMU_ACC_BIAS_", 13) == 0) return abs(value) <= 20.0f;
	if (strncmp(name, "IMU_ACC_SCALE_", 14) == 0) return value >= 0.5f && value <= 1.5f;
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
	if (strcmp(name, "PWR_VOLT_SCALE") == 0) return value > 0.0f && value <= 20.0f;
	if (strcmp(name, "PWR_COMP_REF") == 0) return value >= 3.0f && value <= 4.2f;
	if (strcmp(name, "PWR_COMP_SLP") == 0) return value >= 0.0f && value <= 1.0f;
	if (strcmp(name, "PWR_COMP_MAX") == 0) return value >= 1.0f && value <= 1.25f;
	if (strcmp(name, "SF_RC_LOSS_TIME") == 0) return value >= 0.2f && value <= 10.0f;
	if (strcmp(name, "SF_DESCEND_TIME") == 0) return value >= 1.0f && value <= 60.0f;
	if (strncmp(name, "ALT_", 4) == 0) {
		if (strcmp(name, "ALT_HOVER") == 0) return value >= 0.10f && value <= 0.80f;
		if (strcmp(name, "ALT_STICK_DB") == 0) return value >= 0.02f && value <= 0.25f;
		if (strcmp(name, "ALT_VEL_MAX") == 0) return value >= 0.05f && value <= 1.5f;
		if (strcmp(name, "ALT_CORR_MAX") == 0) return value >= 0.0f && value <= 0.50f;
		if (strcmp(name, "ALT_TKO_H") == 0) return value >= 0.20f && value <= 2.0f;
		if (strcmp(name, "ALT_TKO_TRIG") == 0) return value >= 0.55f && value <= 0.90f;
		if (strcmp(name, "ALT_TKO_THR") == 0) return value >= 0.25f && value <= 0.95f;
		return value >= 0.0f && value <= 5.0f;
	}
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
	// altitude / vertical speed
	{"ALT_P", &altitudeHoldP},
	{"ALT_I", &altitudeHoldI},
	{"ALT_D", &altitudeHoldD},
	{"ALT_I_LIM", &altitudeHoldIntegralLimit},
	{"ALT_CORR_MAX", &altitudeHoldMaxCorrection},
	{"ALT_VEL_MAX", &altitudeHoldMaxClimbRate},
	{"ALT_STICK_DB", &altitudeStickDeadband},
	{"ALT_HOVER", &altitudeHoverThrust},
	{"ALT_TKO_H", &altitudeTakeoffHeight},
	{"ALT_TKO_TRIG", &altitudeTakeoffTrigger},
	{"ALT_TKO_THR", &altitudeTakeoffMaxThrust},
	// imu
	{"IMU_ROT_ROLL", &imuRotation.x, updateIMURotation},
	{"IMU_ROT_PITCH", &imuRotation.y, updateIMURotation},
	{"IMU_ROT_YAW", &imuRotation.z, updateIMURotation},
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
	{"PWR_VOLT_PIN", &voltagePin, configureVoltageInput},
	{"PWR_VOLT_SCALE", &voltageScale, resetVoltageMeasurement},
	{"PWR_VOLT_LPF_A", &voltageFilter.alpha, resetVoltageMeasurement},
	{"PWR_COMP_REF", &voltageCompensationReference},
	{"PWR_COMP_SLP", &voltageCompensationSlope},
	{"PWR_COMP_MAX", &voltageCompensationMax},
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
	// Missing keys use compiled defaults. Do not pre-fill NVS: Preferences stores
	// each key separately and the small partition must retain room for settings.
	for (auto &parameter : parameters) {
		if (storage.isKey(parameter.name)) {
			float stored = storage.getFloat(parameter.name, parameter.getValue());
			if (validParameterValue(parameter.name, parameter.integer, parameter.integer ? nullptr : parameter.f, stored)) parameter.setValue(stored);
			else print("Ignoring invalid parameter: %s\n", parameter.name);
		}
		parameter.cache = parameter.getValue();
	}
	// Stored values are never rewritten implicitly. A new tuning profile must be
	// selected explicitly (for example with `preset`) so two aircraft running the
	// same firmware cannot diverge because of hidden profile migrations.
}

int parametersCount() {
	return sizeof(parameters) / sizeof(parameters[0]);
}

int findParameterIndex(const char *name) {
	if (!name || !name[0]) return -1;
	for (int i = 0; i < parametersCount(); i++) {
		if (strcasecmp(parameters[i].name, name) == 0) return i;
	}
	return -1;
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
	int index = findParameterIndex(name);
	return index >= 0 ? parameters[index].getValue() : NAN;
}

bool setParameter(const char *name, const float value) {
	if (armed) return false;
	int index = findParameterIndex(name);
	if (index < 0) return false;
	Parameter &parameter = parameters[index];
	if (!validParameterValue(parameter.name, parameter.integer,
		parameter.integer ? nullptr : parameter.f, value)) return false;
	parameter.setValue(value);
	if (parameter.callback) parameter.callback();
	return true;
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
	if (!parameterStorageHealthy) {
		print("Parameter reset rejected: storage unavailable\n");
		return;
	}
	// Parameter defaults and Wi-Fi credentials share the `flix` namespace.
	// Remove only registered flight parameters so `preset` cannot silently erase
	// AP/STA credentials and strand a Wi-Fi-managed aircraft.
	for (auto &parameter : parameters) storage.remove(parameter.name);
	ESP.restart();
}
