// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Flight control

#include "vector.h"
#include "quaternion.h"
#include "pid.h"
#include "lpf.h"
#include "util.h"

#define PITCHRATE_P 0.06f
#define PITCHRATE_I 0.0f
#define PITCHRATE_D 0.004f
#define PITCHRATE_I_LIM 0.03f
#define ROLLRATE_P PITCHRATE_P
#define ROLLRATE_I PITCHRATE_I
#define ROLLRATE_D PITCHRATE_D
#define ROLLRATE_I_LIM PITCHRATE_I_LIM
#define YAWRATE_P 0.22f
#define YAWRATE_I 0.0f
#define YAWRATE_D 0.0f
#define YAWRATE_I_LIM 0.3f
#define ROLL_P 3.2f
#define ROLL_I 0
#define ROLL_D 0
#define PITCH_P ROLL_P
#define PITCH_I ROLL_I
#define PITCH_D ROLL_D
#define YAW_P 2.2f
#define PITCHRATE_MAX 0.70f
#define ROLLRATE_MAX 0.70f
#define YAWRATE_MAX radians(220)
#define TILT_MAX radians(25)
#define RATES_D_LPF_ALPHA 0.2f

float hoverThrottle = 0.45f;
float altP = 1.00f;
float altI = 0.010f;
float altIMax = 0.08f;
float altVelP = 0.50f;
float altStickDeadband = 0.08f;
float altClimbRateMax = 0.60f;
float altDescendRateMax = 0.50f;

float positionHoldP = 0.25f;
float positionHoldMaxSpeed = 0.12f;
float holdPX = 0.015f;
float holdIX = 0.006f;
float holdDX = 0.0010f;
float holdPY = 0.014f;
float holdIY = 0.006f;
float holdDY = 0.0010f;
float holdIntegralLimit = 0.08f;
float maxFlowAngle = 0.04f;
float posHoldMinHeight = 0.22f;
float posStickDeadband = 0.10f;
float yawUnlockThreshold = 0.10f;
float motorIdleThrust = 0.015f;

float rollRateDAlpha = 0.10f;
float pitchRateDAlpha = 0.10f;
float stabGroundYawScale = 0.50f;
float stabGroundTiltMax = radians(8.0f);
float stabTakeoffHeight = 0.18f;
float stabTakeoffThrottleStart = 0.12f;
float stabGroundAttScale = 0.45f;
float stabGroundRateScale = 0.60f;

const int RAW = 0, MANUAL = 0, ACRO = 1, STAB = 2, AUTO = 3, ALT_HOLD = 4, POS_HOLD = 5;
int mode = STAB;
bool armed = false;

PID rollRatePID(ROLLRATE_P, ROLLRATE_I, ROLLRATE_D, ROLLRATE_I_LIM, RATES_D_LPF_ALPHA);
PID pitchRatePID(PITCHRATE_P, PITCHRATE_I, PITCHRATE_D, PITCHRATE_I_LIM, RATES_D_LPF_ALPHA);
PID yawRatePID(YAWRATE_P, YAWRATE_I, YAWRATE_D, YAWRATE_I_LIM);
PID rollPID(ROLL_P, ROLL_I, ROLL_D);
PID pitchPID(PITCH_P, PITCH_I, PITCH_D);
PID yawPID(YAW_P, 0, 0);
Vector maxRate(ROLLRATE_MAX, PITCHRATE_MAX, YAWRATE_MAX);
float tiltMax = TILT_MAX;

Quaternion attitudeTarget;
Vector ratesTarget;
Vector ratesExtra;
Vector torqueTarget;
float thrustTarget;

extern const int MOTOR_REAR_LEFT, MOTOR_REAR_RIGHT, MOTOR_FRONT_RIGHT, MOTOR_FRONT_LEFT;
extern float controlRoll, controlPitch, controlThrottle, controlYaw, controlMode;
extern Quaternion attitude;
extern Vector rates;
extern float t;
extern float motors[4];
extern Vector velocity;
extern Vector position;
extern bool opticalFlowHealthy;
extern bool landed;
extern bool flowCtrlUsingFlow;
extern bool flowAirborne;

void controlAttitude();
void controlRates();
void controlTorque();

float targetZ = 0.0f;
bool altHoldEngaged = false;
bool posHoldLocked = false;
bool posHoldGateOpen = false;
float targetPosX = 0.0f;
float targetPosY = 0.0f;
float altClimbRateTarget = 0.0f;
float currentTiltLimitDebug = TILT_MAX;
float currentStabAttScaleDebug = 1.0f;
float currentStabRateScaleDebug = 1.0f;
float currentStabYawScaleDebug = 1.0f;

const int TAKEOFF_GROUND = 0;
const int TAKEOFF_LIFTOFF = 1;
const int TAKEOFF_AIRBORNE = 2;
int takeoffState = TAKEOFF_GROUND;

static float altIntegral = 0.0f;
static float velIntegralX = 0.0f;
static float velIntegralY = 0.0f;
static float lastVelErrorX = 0.0f;
static float lastVelErrorY = 0.0f;
static uint32_t lastFlowGoodTime = 0;
static float manualRollCmd = 0.0f;
static float manualPitchCmd = 0.0f;
static float posRollCmd = 0.0f;
static float posPitchCmd = 0.0f;
static bool usePosCmd = false;

bool autoTakeoffActive = false;
bool autoTakeoffComplete = false;

const char* getTakeoffStateName() {
	if (takeoffState == TAKEOFF_GROUND) return "GROUND";
	if (takeoffState == TAKEOFF_LIFTOFF) return "LIFTOFF";
	if (takeoffState == TAKEOFF_AIRBORNE) return "AIRBORNE";
	return "UNKNOWN";
}

void resetControlPIDs() {
	rollRatePID.reset();
	pitchRatePID.reset();
	yawRatePID.reset();
	rollPID.reset();
	pitchPID.reset();
	yawPID.reset();
}

void resetAttitudeCommandState() {
	manualRollCmd = 0.0f;
	manualPitchCmd = 0.0f;
	posRollCmd = 0.0f;
	posPitchCmd = 0.0f;
	usePosCmd = false;
	ratesExtra = Vector(0, 0, 0);
}

void clearHorizontalHoldState() {
	velIntegralX = 0.0f;
	velIntegralY = 0.0f;
	lastVelErrorX = 0.0f;
	lastVelErrorY = 0.0f;
	targetPosX = position.x;
	targetPosY = position.y;
	posHoldLocked = false;
	posHoldGateOpen = false;
	posRollCmd = 0.0f;
	posPitchCmd = 0.0f;
	usePosCmd = false;
}

void clearAltitudeHoldState() {
	altHoldEngaged = false;
	altIntegral = 0.0f;
	targetZ = position.z;
	altClimbRateTarget = 0.0f;
	autoTakeoffActive = false;
	autoTakeoffComplete = false;
}

void resetControlStateOnDisarm() {
	clearAltitudeHoldState();
	clearHorizontalHoldState();
	resetAttitudeCommandState();
	resetControlPIDs();
	thrustTarget = 0.0f;
	ratesTarget = Vector(0, 0, 0);
	torqueTarget = Vector(0, 0, 0);
	takeoffState = TAKEOFF_GROUND;
}

void updateModeAndArmState() {
	if (valid(controlMode) && mode != RAW && mode != ACRO && mode != AUTO) {
		if (controlMode < 0.33f) mode = STAB;
		else if (controlMode < 0.66f) mode = ALT_HOLD;
		else mode = POS_HOLD;
	}

	bool armCommand = controlThrottle < 0.05f && controlYaw > 0.95f;
	bool disarmCommand = controlThrottle < 0.05f && controlYaw < -0.95f;

	if (armCommand && !armed) {
		armed = true;
		resetControlPIDs();
		resetAttitudeCommandState();
		ratesTarget = Vector(0, 0, 0);
		torqueTarget = Vector(0, 0, 0);
		thrustTarget = motorIdleThrust;
	}
	if (disarmCommand && armed) {
		armed = false;
		resetControlStateOnDisarm();
	}
}

void updateTakeoffState() {
	if (!armed) {
		takeoffState = TAKEOFF_GROUND;
		return;
	}

	static Delay liftoffDelay(0.06f);
	static Delay airborneDelay(0.10f);
	bool throttleActive = controlThrottle > stabTakeoffThrottleStart || thrustTarget > stabTakeoffThrottleStart;
	bool heightLifted = position.z > max(0.04f, stabTakeoffHeight * 0.25f);
	bool heightAirborne = position.z > max(0.08f, stabTakeoffHeight * 0.60f);
	bool verticalLifted = abs(velocity.z) > 0.08f;

	if (airborneDelay.update(heightAirborne)) takeoffState = TAKEOFF_AIRBORNE;
	else if ((throttleActive && (heightLifted || verticalLifted)) || liftoffDelay.update(heightLifted)) takeoffState = TAKEOFF_LIFTOFF;
	else takeoffState = TAKEOFF_GROUND;
}

float getStabAirBlend() {
	if (!armed || mode == ACRO) return 1.0f;
	if (takeoffState == TAKEOFF_GROUND) return 0.0f;
	if (takeoffState == TAKEOFF_AIRBORNE) return 1.0f;

	float throttleBlend = constrain(mapf(controlThrottle, stabTakeoffThrottleStart, hoverThrottle, 0.0f, 1.0f), 0.0f, 1.0f);
	float heightBlend = constrain(position.z / stabTakeoffHeight, 0.0f, 1.0f);
	return max(throttleBlend, heightBlend);
}

float getStabTiltLimitNow() {
	float airBlend = getStabAirBlend();
	currentTiltLimitDebug = stabGroundTiltMax + (tiltMax - stabGroundTiltMax) * airBlend;
	return currentTiltLimitDebug;
}

float getStabAttitudeScale() {
	float airBlend = getStabAirBlend();
	currentStabAttScaleDebug = stabGroundAttScale + (1.0f - stabGroundAttScale) * airBlend;
	return currentStabAttScaleDebug;
}

float getStabRateScale() {
	float airBlend = getStabAirBlend();
	currentStabRateScaleDebug = stabGroundRateScale + (1.0f - stabGroundRateScale) * airBlend;
	return currentStabRateScaleDebug;
}

float getStabYawScale() {
	float airBlend = getStabAirBlend();
	currentStabYawScaleDebug = stabGroundYawScale + (1.0f - stabGroundYawScale) * airBlend;
	return currentStabYawScaleDebug;
}

void updateFinalAttitudeTarget() {
	bool yawActive = abs(controlYaw) > yawUnlockThreshold;
	bool stickMoving = abs(controlRoll) > posStickDeadband || abs(controlPitch) > posStickDeadband;
	float finalRoll = manualRollCmd;
	float finalPitch = manualPitchCmd;

	if (mode == POS_HOLD && posHoldGateOpen && usePosCmd && !stickMoving) {
		finalRoll = posRollCmd;
		finalPitch = posPitchCmd;
	}

	float yawTarget = attitudeTarget.getYaw();
	if (invalid(yawTarget) || yawActive) yawTarget = attitude.getYaw();
	attitudeTarget = Quaternion::fromEuler(Vector(finalRoll, finalPitch, yawTarget));
	ratesExtra = Vector(0, 0, -controlYaw * maxRate.z);
}

void updatePilotControlSplit() {
	updateModeAndArmState();

	if (!armed) {
		resetControlStateOnDisarm();
		return;
	}
	if (mode == AUTO) return;

	if (mode == ACRO || mode == RAW) {
		thrustTarget = controlThrottle;
		attitudeTarget.invalidate();
		clearAltitudeHoldState();
		clearHorizontalHoldState();
		if (mode == ACRO) {
			ratesTarget.x = controlRoll * maxRate.x;
			ratesTarget.y = controlPitch * maxRate.y;
			ratesTarget.z = -controlYaw * maxRate.z;
		} else {
			ratesTarget.invalidate();
			torqueTarget = Vector(controlRoll, controlPitch, -controlYaw) * 0.1f;
		}
		updateTakeoffState();
		return;
	}

	if (mode == STAB) {
		thrustTarget = controlThrottle;
		clearAltitudeHoldState();
		clearHorizontalHoldState();
	}

	updateTakeoffState();
	float currentTiltLimit = getStabTiltLimitNow();
	manualRollCmd = controlRoll * currentTiltLimit;
	manualPitchCmd = controlPitch * currentTiltLimit;
	updateFinalAttitudeTarget();
}

void updateAltitudeControlSplit(float controlDt) {
	if (!armed || mode == AUTO || (mode != ALT_HOLD && mode != POS_HOLD)) return;

	if (!altHoldEngaged) {
		targetZ = position.z;
		altHoldEngaged = true;
		altIntegral = 0.0f;
	}

	if (controlThrottle < 0.05f && position.z < 0.08f) {
		altClimbRateTarget = 0.0f;
		altIntegral = 0.0f;
		targetZ = position.z;
		thrustTarget = motorIdleThrust;
		return;
	}

	float stick = controlThrottle - 0.5f;
	if (abs(stick) <= altStickDeadband) {
		altClimbRateTarget = 0.0f;
	} else if (stick > 0.0f) {
		altClimbRateTarget = mapf(controlThrottle, 0.5f + altStickDeadband, 1.0f, 0.0f, altClimbRateMax);
	} else {
		altClimbRateTarget = -mapf(controlThrottle, 0.5f - altStickDeadband, 0.0f, 0.0f, altDescendRateMax);
	}

	altClimbRateTarget = constrain(altClimbRateTarget, -altDescendRateMax, altClimbRateMax);
	targetZ = constrain(targetZ + altClimbRateTarget * controlDt, 0.0f, 3.5f);

	float altError = targetZ - position.z;
	altIntegral = constrain(altIntegral + altError * controlDt * altI, -altIMax, altIMax);
	if (position.z < 0.10f) altIntegral = 0.0f;

	float altCorrection = constrain(altError * altP, -0.25f, 0.25f);
	float velCorrection = constrain(velocity.z * altVelP, -0.20f, 0.20f);
	thrustTarget = constrain(hoverThrottle + altIntegral + altCorrection - velCorrection, motorIdleThrust, 0.90f);
	autoTakeoffActive = altClimbRateTarget > 0.0f && position.z < targetZ - 0.05f;
	autoTakeoffComplete = altHoldEngaged && !autoTakeoffActive;
}

void updatePositionControlSplit(float controlDt) {
	static Delay posGateDelay(0.30f);

	if (!armed || mode != POS_HOLD) {
		posGateDelay.update(false);
		clearHorizontalHoldState();
		return;
	}

	uint32_t now = millis();
	if (opticalFlowHealthy) lastFlowGoodTime = now;
	bool flowIsStable = lastFlowGoodTime != 0 && now - lastFlowGoodTime < 150;
	bool yawActive = abs(controlYaw) > yawUnlockThreshold;
	bool stickMoving = abs(controlRoll) > posStickDeadband || abs(controlPitch) > posStickDeadband;
	bool safeAltitude = position.z > posHoldMinHeight;
	bool levelEnough = abs(attitude.getRoll()) < radians(12.0f) && abs(attitude.getPitch()) < radians(12.0f);
	bool gateCandidate = altHoldEngaged && flowCtrlUsingFlow && flowAirborne && flowIsStable && safeAltitude && levelEnough;
	posHoldGateOpen = posGateDelay.update(gateCandidate);

	if (!posHoldGateOpen || stickMoving || yawActive) {
		clearHorizontalHoldState();
		updateFinalAttitudeTarget();
		return;
	}

	if (!posHoldLocked) {
		targetPosX = position.x;
		targetPosY = position.y;
		velIntegralX = 0.0f;
		velIntegralY = 0.0f;
		posHoldLocked = true;
	}

	float errX = targetPosX - position.x;
	float errY = targetPosY - position.y;
	float vWorldX = constrain(errX * positionHoldP, -positionHoldMaxSpeed, positionHoldMaxSpeed);
	float vWorldY = constrain(errY * positionHoldP, -positionHoldMaxSpeed, positionHoldMaxSpeed);

	float yaw = attitude.getYaw();
	float c = cos(yaw);
	float s = sin(yaw);
	float targetVelBodyX = vWorldX * c + vWorldY * s;
	float targetVelBodyY = -vWorldX * s + vWorldY * c;

	float velErrorX = targetVelBodyX - velocity.x;
	float velErrorY = targetVelBodyY - velocity.y;
	float dTermX = 0.0f;
	float dTermY = 0.0f;
	if (controlDt > 0.0f && controlDt < 0.2f) {
		dTermX = (velErrorX - lastVelErrorX) / controlDt * holdDX;
		dTermY = (velErrorY - lastVelErrorY) / controlDt * holdDY;
	}
	lastVelErrorX = velErrorX;
	lastVelErrorY = velErrorY;

	if (abs(velErrorX) > 0.04f) {
		velIntegralX = constrain(velIntegralX + velErrorX * controlDt * holdIX, -holdIntegralLimit, holdIntegralLimit);
	}
	if (abs(velErrorY) > 0.04f) {
		velIntegralY = constrain(velIntegralY + velErrorY * controlDt * holdIY, -holdIntegralLimit, holdIntegralLimit);
	}

	posPitchCmd = constrain(velErrorX * holdPX + velIntegralX + dTermX, -maxFlowAngle, maxFlowAngle);
	posRollCmd = constrain(velErrorY * holdPY + velIntegralY + dTermY, -maxFlowAngle, maxFlowAngle);
	usePosCmd = true;
	updateFinalAttitudeTarget();
}

void controlPilotLoop() {
	static float lastPilotLoopTime = NAN;
	float controlDt = valid(lastPilotLoopTime) ? t - lastPilotLoopTime : 0.0f;
	lastPilotLoopTime = t;
	if (!(controlDt > 0.0f) || controlDt > 0.2f) controlDt = 1.0f / 80.0f;

	updatePilotControlSplit();
	updateAltitudeControlSplit(controlDt);
	failsafe();
}

void controlPositionLoop() {
	static float lastPositionLoopTime = NAN;
	float controlDt = valid(lastPositionLoopTime) ? t - lastPositionLoopTime : 0.0f;
	lastPositionLoopTime = t;
	if (!(controlDt > 0.0f) || controlDt > 0.2f) controlDt = 1.0f / 40.0f;

	updatePositionControlSplit(controlDt);
}

void controlAttitudeLoop() {
	if (mode != ACRO && mode != RAW) controlAttitude();
}

void controlRateTorqueLoop() {
	controlRates();
	controlTorque();
}

void control() {
	static Rate pilotRate(80);
	static Rate positionRate(40);
	static Rate attitudeRate(150);
	static Rate innerRate(400);

	if (pilotRate) controlPilotLoop();
	if (positionRate) controlPositionLoop();
	if (attitudeRate) controlAttitudeLoop();
	if (innerRate) controlRateTorqueLoop();
}

void controlAttitude() {
	if (!armed || attitudeTarget.invalid() || thrustTarget < 0.01f) {
		ratesTarget = Vector(0, 0, 0);
		return;
	}

	float rollError = wrapAngle(attitudeTarget.getRoll() - attitude.getRoll());
	float pitchError = wrapAngle(attitudeTarget.getPitch() - attitude.getPitch());
	float attScale = (mode == STAB || mode == ALT_HOLD || mode == POS_HOLD) ? getStabAttitudeScale() : 1.0f;

	ratesTarget.x = constrain(rollPID.update(rollError) * attScale + ratesExtra.x, -maxRate.x, maxRate.x);
	ratesTarget.y = constrain(pitchPID.update(pitchError) * attScale + ratesExtra.y, -maxRate.y, maxRate.y);
	float yawError = wrapAngle(attitudeTarget.getYaw() - attitude.getYaw());
	ratesTarget.z = (yawPID.update(yawError) + ratesExtra.z) * getStabYawScale();
}

void controlRates() {
	if (!armed || ratesTarget.invalid() || thrustTarget < 0.01f) {
		torqueTarget = Vector(0, 0, 0);
		return;
	}

	float airBlend = (mode == STAB || mode == ALT_HOLD || mode == POS_HOLD) ? getStabAirBlend() : 1.0f;
	float rollDAlphaBase = constrain(rollRateDAlpha, 0.001f, 1.0f);
	float pitchDAlphaBase = constrain(pitchRateDAlpha, 0.001f, 1.0f);
	rollRatePID.lpf.alpha = min(rollDAlphaBase, 0.04f) + (rollDAlphaBase - min(rollDAlphaBase, 0.04f)) * airBlend;
	pitchRatePID.lpf.alpha = min(pitchDAlphaBase, 0.04f) + (pitchDAlphaBase - min(pitchDAlphaBase, 0.04f)) * airBlend;

	Vector error = ratesTarget - rates;
	float rateScale = (mode == STAB || mode == ALT_HOLD || mode == POS_HOLD) ? getStabRateScale() : 1.0f;
	torqueTarget.x = rollRatePID.update(error.x) * rateScale;
	torqueTarget.y = pitchRatePID.update(error.y) * rateScale;
	torqueTarget.z = yawRatePID.update(error.z) * getStabYawScale();
}

void controlTorque() {
	if (!armed) {
		memset(motors, 0, sizeof(motors));
		return;
	}
	if (!torqueTarget.valid()) return;

	if (thrustTarget <= motorIdleThrust) {
		motors[0] = motorIdleThrust;
		motors[1] = motorIdleThrust;
		motors[2] = motorIdleThrust;
		motors[3] = motorIdleThrust;
		return;
	}

	motors[MOTOR_FRONT_LEFT] = thrustTarget + torqueTarget.x - torqueTarget.y + torqueTarget.z;
	motors[MOTOR_FRONT_RIGHT] = thrustTarget - torqueTarget.x - torqueTarget.y - torqueTarget.z;
	motors[MOTOR_REAR_LEFT] = thrustTarget + torqueTarget.x + torqueTarget.y - torqueTarget.z;
	motors[MOTOR_REAR_RIGHT] = thrustTarget - torqueTarget.x + torqueTarget.y + torqueTarget.z;

	for (int i = 0; i < 4; i++) motors[i] = constrain(motors[i], 0, 1);
}

const char* getModeName() {
	if (mode == RAW) return "RAW";
	if (mode == ACRO) return "ACRO";
	if (mode == STAB) return "STAB";
	if (mode == AUTO) return "AUTO";
	if (mode == ALT_HOLD) return "ALT_HOLD";
	if (mode == POS_HOLD) return "POS_HOLD";
	return "UNKNOWN";
}
