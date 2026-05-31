// Copyright (c) 2024 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Parameters storage in flash memory

#include <Preferences.h>
#include "util.h"

extern float channelZero[16];
extern float channelMax[16];
extern float rollChannel, pitchChannel, throttleChannel, yawChannel, modeChannel;
extern float hoverThrottle, altP, altI, altIMax, altVelP;
extern float altStickDeadband, altClimbRateMax, altDescendRateMax;
extern float positionHoldP, positionHoldMaxSpeed;
extern float holdPX, holdIX, holdDX, holdPY, holdIY, holdDY;
extern float holdIntegralLimit, maxFlowAngle, posHoldMinHeight, posStickDeadband;
extern float yawUnlockThreshold, motorIdleThrust;
extern float rollRateDAlpha, pitchRateDAlpha;
extern float stabGroundTiltMax, stabGroundAttScale, stabGroundRateScale, stabGroundYawScale;
extern float stabTakeoffHeight, stabTakeoffThrottleStart;
extern float boardAlignPitch, boardAlignRoll;
extern float flowGyroCompPitch, flowGyroCompRoll, flowVelocitySmoothing, flowInnovationLimit, flowTiltCosMin;
extern float flowBiasAdapt, flowVelocityZeroDeadband, flowPositionZeroDeadband;
extern float flowScaleX, flowScaleY, flowArmMinHeight, flowArmMinThrottle, flowStationaryGyro, flowStationaryVel;
extern float imuRateAlpha;

Preferences storage;

struct Parameter {
	const char *name; // max length is 15 (Preferences key limit)
	float *variable;
	float value; // cache
};

Parameter parameters[] = {
	// control
	{"CTL_R_RATE_P", &rollRatePID.p},
	{"CTL_R_RATE_I", &rollRatePID.i},
	{"CTL_R_RATE_D", &rollRatePID.d},
	{"CTL_R_RATE_WU", &rollRatePID.windup},
	{"CTL_P_RATE_P", &pitchRatePID.p},
	{"CTL_P_RATE_I", &pitchRatePID.i},
	{"CTL_P_RATE_D", &pitchRatePID.d},
	{"CTL_P_RATE_WU", &pitchRatePID.windup},
	{"CTL_Y_RATE_P", &yawRatePID.p},
	{"CTL_Y_RATE_I", &yawRatePID.i},
	{"CTL_Y_RATE_D", &yawRatePID.d},
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
	{"CTL_R_D_A", &rollRateDAlpha},
	{"CTL_P_D_A", &pitchRateDAlpha},
	{"ALT_HOVER", &hoverThrottle},
	{"ALT_P", &altP},
	{"ALT_I", &altI},
	{"ALT_I_MAX", &altIMax},
	{"ALT_V_P", &altVelP},
	{"ALT_ST_DB", &altStickDeadband},
	{"ALT_V_UP", &altClimbRateMax},
	{"ALT_V_DN", &altDescendRateMax},
	{"POS_P", &positionHoldP},
	{"POS_V_MAX", &positionHoldMaxSpeed},
	{"HOLD_P_X", &holdPX},
	{"HOLD_I_X", &holdIX},
	{"HOLD_D_X", &holdDX},
	{"HOLD_P_Y", &holdPY},
	{"HOLD_I_Y", &holdIY},
	{"HOLD_D_Y", &holdDY},
	{"HOLD_I_MAX", &holdIntegralLimit},
	{"HOLD_A_MAX", &maxFlowAngle},
	{"POS_MIN_Z", &posHoldMinHeight},
	{"POS_STICK_DB", &posStickDeadband},
	{"YAW_UNLOCK", &yawUnlockThreshold},
	{"MOTOR_IDLE", &motorIdleThrust},
	{"STAB_GND_TL", &stabGroundTiltMax},
	{"STAB_GND_AT", &stabGroundAttScale},
	{"STAB_GND_RT", &stabGroundRateScale},
	{"STAB_GND_YW", &stabGroundYawScale},
	{"STAB_LIFT_H", &stabTakeoffHeight},
	{"STAB_LIFT_T", &stabTakeoffThrottleStart},
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
	// estimate
	{"FLOW_B_PIT", &boardAlignPitch},
	{"FLOW_B_ROL", &boardAlignRoll},
	{"IMU_RATE_A", &imuRateAlpha},
	{"FLOW_K_PIT", &flowGyroCompPitch},
	{"FLOW_K_ROL", &flowGyroCompRoll},
	{"FLOW_SMOOTH", &flowVelocitySmoothing},
	{"FLOW_INNOV", &flowInnovationLimit},
	{"FLOW_TILT_C", &flowTiltCosMin},
	{"FLOW_SCL_X", &flowScaleX},
	{"FLOW_SCL_Y", &flowScaleY},
	{"FLOW_BIAS_A", &flowBiasAdapt},
	{"FLOW_ZERO_D", &flowVelocityZeroDeadband},
	{"FLOW_POS_D", &flowPositionZeroDeadband},
	{"FLOW_ARM_Z", &flowArmMinHeight},
	{"FLOW_ARM_T", &flowArmMinThrottle},
	{"FLOW_ST_G", &flowStationaryGyro},
	{"FLOW_ST_V", &flowStationaryVel},
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
};

void setupParameters() {
	storage.begin("flix", false);
	// Read parameters from storage
	for (auto &parameter : parameters) {
		*parameter.variable = storage.getFloat(parameter.name, *parameter.variable);
		parameter.value = *parameter.variable;
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
	return *parameters[index].variable;
}

float getParameter(const char *name) {
	for (auto &parameter : parameters) {
		if (strcmp(parameter.name, name) == 0) {
			return *parameter.variable;
		}
	}
	return NAN;
}

bool setParameter(const char *name, const float value) {
	for (auto &parameter : parameters) {
		if (strcmp(parameter.name, name) == 0) {
			*parameter.variable = value;
			return true;
		}
	}
	return false;
}

void syncParameters() {
	static Rate rate(1);
	if (!rate) return; // sync once per second
	if (motorsActive()) return; // don't use flash while flying, it may cause a delay

	for (auto &parameter : parameters) {
		if (parameter.value == *parameter.variable) continue;
		if (isnan(parameter.value) && isnan(*parameter.variable)) continue; // handle NAN != NAN
		storage.putFloat(parameter.name, *parameter.variable);
		parameter.value = *parameter.variable;
	}
}

void printParameters() {
	for (auto &parameter : parameters) {
		print("%s = %g\n", parameter.name, *parameter.variable);
	}
}

void resetParameters() {
	storage.clear();
	ESP.restart();
}
