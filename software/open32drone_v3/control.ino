// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Flight control
// TOF 定高版: 光流 TOF 高度源 + 油门叠加式定高（基础油门=杆位，PID 修正叠加）

#include "vector.h"
#include "quaternion.h"
#include "pid.h"
#include "lpf.h"
#include "util.h"

#define PITCHRATE_P 0.05
#define PITCHRATE_I 0.2
#define PITCHRATE_D 0.001
#define PITCHRATE_I_LIM 0.3
#define ROLLRATE_P PITCHRATE_P
#define ROLLRATE_I PITCHRATE_I
#define ROLLRATE_D PITCHRATE_D
#define ROLLRATE_I_LIM PITCHRATE_I_LIM
#define YAWRATE_P 0.3
#define YAWRATE_I 0.0
#define YAWRATE_D 0.0
#define YAWRATE_I_LIM 0.3
#define ROLL_P 6
#define ROLL_I 0
#define ROLL_D 0
#define PITCH_P ROLL_P
#define PITCH_I ROLL_I
#define PITCH_D ROLL_D
#define YAW_P 3
#define PITCHRATE_MAX radians(360)
#define ROLLRATE_MAX radians(360)
#define YAWRATE_MAX radians(300)
#define TILT_MAX radians(30)
#define RATES_D_LPF_ALPHA 0.2 // cutoff frequency ~ 40 Hz

const int RAW = 0, ACRO = 1, STAB = 2, AUTO = 3, ALT_HOLD = 4, POS_HOLD = 5; // flight modes
int mode = STAB;
bool armed = false;

PID rollRatePID(ROLLRATE_P, ROLLRATE_I, ROLLRATE_D, ROLLRATE_I_LIM, RATES_D_LPF_ALPHA);
PID pitchRatePID(PITCHRATE_P, PITCHRATE_I, PITCHRATE_D, PITCHRATE_I_LIM, RATES_D_LPF_ALPHA);
PID yawRatePID(YAWRATE_P, YAWRATE_I, YAWRATE_D);
PID rollPID(ROLL_P, ROLL_I, ROLL_D);
PID pitchPID(PITCH_P, PITCH_I, PITCH_D);
PID yawPID(YAW_P, 0, 0);
Vector maxRate(ROLLRATE_MAX, PITCHRATE_MAX, YAWRATE_MAX);
float tiltMax = TILT_MAX;
int flightModes[] = {STAB, ALT_HOLD, POS_HOLD}; // map for rc mode switch: 中档=定高, 高档=定点

Quaternion attitudeTarget;
Vector ratesTarget;
Vector ratesExtra; // feedforward rates
Vector torqueTarget;
float thrustTarget;

extern const int MOTOR_REAR_LEFT, MOTOR_REAR_RIGHT, MOTOR_FRONT_RIGHT, MOTOR_FRONT_LEFT;
extern float controlRoll, controlPitch, controlThrottle, controlYaw, controlMode;
// 定点依赖前置声明（flowAirborne/flowCtrlUsingFlow 定义在 estimate.ino；pos*Cmd/usePosCmd 定义在 control.ino 末尾）
extern bool flowCtrlUsingFlow;
extern bool flowAirborne;
extern bool flowPositionGateOpen;
extern uint32_t opticalFlowSequence;
extern float opticalFlowSampleDt;
extern float posRollCmd;
extern float posPitchCmd;
extern bool usePosCmd;

void control() {
	interpretControls();
	failsafe();
	updateAltitudeHoldControl(); // 无条件调用：函数内部判断 mode 并复位 engaged
	// Always run the state machine so leaving POS_HOLD clears the old lock point.
	// Re-entering POS_HOLD will then capture the current position after qualification.
	updatePositionControlSplit(dt);
	// POS_HOLD：定点姿态覆盖（roll/pitch 由位置环生成）
	if (mode == POS_HOLD && usePosCmd) {
		attitudeTarget = Quaternion::fromEuler(Vector(posRollCmd, posPitchCmd, attitudeTarget.getYaw()));
	}
	controlAttitude();
	controlRates();
	controlTorque();
}

void interpretControls() {
	if (controlMode < 0.25) mode = flightModes[0];
	else if (controlMode <= 0.75) mode = flightModes[1];
	else if (controlMode > 0.75) mode = flightModes[2];

	if (mode == AUTO) return; // pilot is not effective in AUTO mode

	if (controlThrottle < 0.05 && controlYaw > 0.95) armed = true; // arm gesture
	if (controlThrottle < 0.05 && controlYaw < -0.95) armed = false; // disarm gesture

	if (abs(controlYaw) < 0.1) controlYaw = 0; // yaw dead zone
	if (abs(controlRoll) < 0.05) controlRoll = 0; // roll dead zone（消除起飞微偏漂移）
	if (abs(controlPitch) < 0.05) controlPitch = 0; // pitch dead zone（消除起飞微偏漂移）

	// STAB: 杆位直接控油；ALT_HOLD: 基础油门=杆位，PID 修正叠加（油门叠加式）
	if (mode == STAB || mode == ALT_HOLD || mode == POS_HOLD) thrustTarget = controlThrottle;

	if (mode == STAB || mode == ALT_HOLD || mode == POS_HOLD) {
		float yawTarget = attitudeTarget.getYaw();
		if (!armed || invalid(yawTarget) || controlYaw != 0) yawTarget = attitude.getYaw(); // reset yaw target
		attitudeTarget = Quaternion::fromEuler(Vector(controlRoll * tiltMax, controlPitch * tiltMax, yawTarget));
		ratesExtra = Vector(0, 0, -controlYaw * maxRate.z); // positive yaw stick means clockwise rotation in FLU
	}

	if (mode == ACRO) {
		attitudeTarget.invalidate(); // skip attitude control
		ratesTarget.x = controlRoll * maxRate.x;
		ratesTarget.y = controlPitch * maxRate.y;
		ratesTarget.z = -controlYaw * maxRate.z; // positive yaw stick means clockwise rotation in FLU
	}

	if (mode == RAW) { // direct torque control
		attitudeTarget.invalidate(); // skip attitude control
		ratesTarget.invalidate(); // skip rate control
		torqueTarget = Vector(controlRoll, controlPitch, -controlYaw) * 0.1;
	}
}

void controlAttitude() {
	if (!armed || attitudeTarget.invalid() || thrustTarget < 0.1) return; // skip attitude control

	const Vector up(0, 0, 1);
	Vector upActual = Quaternion::rotateVector(up, attitude);
	Vector upTarget = Quaternion::rotateVector(up, attitudeTarget);

	Vector error = Vector::rotationVectorBetween(upTarget, upActual);

	ratesTarget.x = rollPID.update(error.x) + ratesExtra.x;
	ratesTarget.y = pitchPID.update(error.y) + ratesExtra.y;

	float yawError = wrapAngle(attitudeTarget.getYaw() - attitude.getYaw());
	ratesTarget.z = yawPID.update(yawError) + ratesExtra.z;
}


void controlRates() {
	if (!armed || ratesTarget.invalid() || thrustTarget < 0.1) return; // skip rates control

	Vector error = ratesTarget - rates;

	// Calculate desired torque, where 0 - no torque, 1 - maximum possible torque
	torqueTarget.x = rollRatePID.update(error.x);
	torqueTarget.y = pitchRatePID.update(error.y);
	torqueTarget.z = yawRatePID.update(error.z);
}

void controlTorque() {
	if (!armed) {
		memset(motors, 0, sizeof(motors)); // stop motors if disarmed
		return;
	}

	if (!torqueTarget.valid()) return; // direct actuator mode owns motor values while armed

	if (thrustTarget < 0.1) {
		motors[0] = 0.1; // idle thrust
		motors[1] = 0.1;
		motors[2] = 0.1;
		motors[3] = 0.1;
		return;
	}

	motors[MOTOR_FRONT_LEFT] = thrustTarget + torqueTarget.x - torqueTarget.y + torqueTarget.z;
	motors[MOTOR_FRONT_RIGHT] = thrustTarget - torqueTarget.x - torqueTarget.y - torqueTarget.z;
	motors[MOTOR_REAR_LEFT] = thrustTarget + torqueTarget.x + torqueTarget.y - torqueTarget.z;
	motors[MOTOR_REAR_RIGHT] = thrustTarget - torqueTarget.x + torqueTarget.y + torqueTarget.z;

	motors[0] = constrain(motors[0], 0, 1);
	motors[1] = constrain(motors[1], 0, 1);
	motors[2] = constrain(motors[2], 0, 1);
	motors[3] = constrain(motors[3], 0, 1);
}

const char* getModeName() {
	switch (mode) {
		case RAW: return "RAW";
		case ACRO: return "ACRO";
		case STAB: return "STAB";
		case ALT_HOLD: return "ALT_HOLD";
		case POS_HOLD: return "POS_HOLD";
		case AUTO: return "AUTO";
		default: return "UNKNOWN";
	}
}

// ==================== TOF 定高（油门叠加式）====================
// 基础油门 = 杆位（interpretControls 设 thrustTarget = controlThrottle），PID 修正叠加微调
float altitudeHoldTarget = 0.0f;
bool altitudeHoldEngaged = false;
float altitudeHoldIntegral = 0.0f;
float altitudeHoldLastError = 0.0f;
float altitudeHoldCorrection = 0.0f;
uint32_t altitudeHoldLastSequence = 0;
int altitudeHoldRejectReason = 0;

#define ALT_HOLD_P 0.8
#define ALT_HOLD_I 0.1
#define ALT_HOLD_D 0.2
#define ALT_HOLD_I_LIMIT 0.3
#define ALT_HOLD_MAX_CORRECTION 0.2

// 定高高度源：纯 TOF（TOF 失效由 engage 条件/退出处理）
float getHoldAltitude() {
	return position.z;
}

void updateAltitudeHoldControl() {
	if (!armed || (mode != ALT_HOLD && mode != POS_HOLD)) { // POS_HOLD 下也保持高度
		altitudeHoldEngaged = false;
		altitudeHoldCorrection = 0.0f;
		altitudeHoldLastSequence = 0;
		altitudeHoldRejectReason = 1;
		return;
	}
	altitudeHoldRejectReason = 0;

	bool tofValid = opticalFlowHealthy && opticalFlowTimestamp != 0 &&
		millis() - opticalFlowTimestamp <= 150 && position.z > 0.05f && position.z < 6.0f;
	// Capture the current filtered ToF height; this is sensor validity, not a
	// commanded minimum/maximum flight-height restriction.
	if (!altitudeHoldEngaged && tofValid && controlThrottle >= 0.05f) {
		altitudeHoldTarget = getHoldAltitude();
		altitudeHoldEngaged = true;
		altitudeHoldIntegral = 0.0f;
		altitudeHoldLastError = 0.0f;
		altitudeHoldCorrection = 0.0f;
		altitudeHoldLastSequence = opticalFlowSequence;
		print("Alt hold engaged, target: %.2fm\n", altitudeHoldTarget);
	}

	// 拉底退出定高
	if (controlThrottle < 0.05f) {
		bool wasEngaged = altitudeHoldEngaged;
		altitudeHoldEngaged = false;
		altitudeHoldCorrection = 0.0f;
		altitudeHoldRejectReason = 2;
		if (wasEngaged) print("Alt hold disengaged\n");
		return;
	}

	// On stale ToF, return to pilot throttle and fade the last correction instead
	// of dropping it in one control frame.
	if (!tofValid) {
		altitudeHoldEngaged = false;
		altitudeHoldRejectReason = 3;
		altitudeHoldCorrection *= 0.90f;
		if (abs(altitudeHoldCorrection) < 0.002f) altitudeHoldCorrection = 0.0f;
		thrustTarget = constrain(thrustTarget + altitudeHoldCorrection, 0.0f, 1.0f);
		return;
	}

	// PID only advances on a new TOF packet. The correction is held between packets.
	float error = altitudeHoldTarget - getHoldAltitude();
	if (opticalFlowSequence != altitudeHoldLastSequence) {
		altitudeHoldLastSequence = opticalFlowSequence;
		float dtAlt = constrain(opticalFlowSampleDt, 0.01f, 0.20f);
		altitudeHoldIntegral = constrain(altitudeHoldIntegral + error * dtAlt, -ALT_HOLD_I_LIMIT, ALT_HOLD_I_LIMIT);
		float derivative = (error - altitudeHoldLastError) / dtAlt;
		altitudeHoldCorrection = constrain(
			error * ALT_HOLD_P + altitudeHoldIntegral * ALT_HOLD_I + derivative * ALT_HOLD_D,
			-ALT_HOLD_MAX_CORRECTION, ALT_HOLD_MAX_CORRECTION);
		altitudeHoldLastError = error;
	}

	thrustTarget += altitudeHoldCorrection; // persist correction for every motor update
	thrustTarget = constrain(thrustTarget, 0.0f, 1.0f);

}

// ==================== 定点 POS_HOLD（来自 flix_nwpu_v2，适配 v3 单循环）====================
// 依赖：position.x/y + velocity.x/y（estimateHorizontalVelocity 更新）+ 定高（高度稳定）

// 参数
float positionHoldP = 0.50f;
float positionStickMaxSpeed = 0.50f;
float holdPX = 0.200f;
float holdIX = 0.0f;
float holdDX = 0.0f;
float holdPY = 0.200f;
float holdIY = 0.0f;
float holdDY = 0.0f;
float holdIntegralLimit = 0.08f;
float maxFlowAngleRate = 1.20f; // rad/s, prevents a step when POS_HOLD engages
float posStickDeadband = 0.10f;
float yawUnlockThreshold = 0.10f;

// 状态
bool posHoldLocked = false;
bool posHoldGateOpen = false;
float targetPosX = 0.0f;
float targetPosY = 0.0f;
float velIntegralX = 0.0f;
float velIntegralY = 0.0f;
float lastVelErrorX = 0.0f;
float lastVelErrorY = 0.0f;
float posRollCmd = 0.0f;
float posPitchCmd = 0.0f;
float posTargetVelBodyX = 0.0f;
float posTargetVelBodyY = 0.0f;
float posVelErrorX = 0.0f;
float posVelErrorY = 0.0f;
bool posControlSaturated = false;
bool usePosCmd = false;
int posHoldRejectReason = 0;

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
	posTargetVelBodyX = 0.0f;
	posTargetVelBodyY = 0.0f;
	posVelErrorX = 0.0f;
	posVelErrorY = 0.0f;
	posControlSaturated = false;
	usePosCmd = false;
}

void updatePositionControlSplit(float controlDt) {
	static Delay posGateDelay(0.30f);
	static uint32_t lastControlFlowSequence = 0;

	if (!armed || mode != POS_HOLD) {
		posHoldRejectReason = 1; // not armed or not in POS_HOLD
		posGateDelay.update(false);
		clearHorizontalHoldState();
		return;
	}

	bool yawActive = abs(controlYaw) > yawUnlockThreshold;
	bool stickMoving = abs(controlRoll) > posStickDeadband || abs(controlPitch) > posStickDeadband;
	bool safeAltitude = opticalFlowHealthy && opticalFlowHeight > 0.05f && opticalFlowHeight < 6.0f;
	bool levelEnough = abs(attitude.getRoll()) < tiltMax && abs(attitude.getPitch()) < tiltMax;
	bool heightStable = altitudeHoldEngaged;
	bool velocityGateOpen = flowCtrlUsingFlow && flowPositionGateOpen && flowAirborne &&
		safeAltitude && levelEnough && heightStable && !yawActive;
	if (!flowCtrlUsingFlow) posHoldRejectReason = 2;
	else if (!flowPositionGateOpen) posHoldRejectReason = 3;
	else if (!flowAirborne) posHoldRejectReason = 4;
	else if (!safeAltitude) posHoldRejectReason = 5;
	else if (!levelEnough) posHoldRejectReason = 6;
	else if (!heightStable) posHoldRejectReason = 7;
	else if (yawActive) posHoldRejectReason = 8;
	else posHoldRejectReason = 0;
	posHoldGateOpen = posGateDelay.update(velocityGateOpen);

	if (!velocityGateOpen) {
		clearHorizontalHoldState();
		return;
	}
	if (!posHoldGateOpen) {
		posHoldLocked = false;
		posRollCmd = 0.0f;
		posPitchCmd = 0.0f;
		usePosCmd = false; // pilot keeps attitude control during the 300ms qualification window
		return;
	}

	if (!posHoldLocked) {
		targetPosX = position.x;
		targetPosY = position.y;
		velIntegralX = 0.0f;
		velIntegralY = 0.0f;
		posHoldLocked = true;
	}

	usePosCmd = true;
	if (opticalFlowSequence == lastControlFlowSequence) return; // hold command between 20Hz samples
	lastControlFlowSequence = opticalFlowSequence;
	controlDt = constrain(opticalFlowSampleDt, 0.01f, 0.20f);

	float yaw = attitude.getYaw();
	float c = cos(yaw);
	float s = sin(yaw);

	float stickVelBodyX = stickMoving ? controlPitch * positionStickMaxSpeed : 0.0f;
	// Positive roll tilts/thrusts to body-right (-Y in FLU), so the velocity
	// command must use the opposite sign from the body Y axis.
	float stickVelBodyY = stickMoving ? -controlRoll * positionStickMaxSpeed : 0.0f;
	if (stickMoving && posHoldGateOpen && posHoldLocked) {
		float stickVelWorldX = stickVelBodyX * c - stickVelBodyY * s;
		float stickVelWorldY = stickVelBodyX * s + stickVelBodyY * c;
		targetPosX = constrain(targetPosX + stickVelWorldX * controlDt, position.x - 1.0f, position.x + 1.0f);
		targetPosY = constrain(targetPosY + stickVelWorldY * controlDt, position.y - 1.0f, position.y + 1.0f);
		velIntegralX = 0.0f;
		velIntegralY = 0.0f;
	}

	float vWorldX = 0.0f;
	float vWorldY = 0.0f;
	if (posHoldGateOpen && posHoldLocked) {
		float errX = targetPosX - position.x;
		float errY = targetPosY - position.y;
		vWorldX = errX * positionHoldP;
		vWorldY = errY * positionHoldP;
	}

	posTargetVelBodyX = vWorldX * c + vWorldY * s + stickVelBodyX;
	posTargetVelBodyY = -vWorldX * s + vWorldY * c + stickVelBodyY;

	posVelErrorX = posTargetVelBodyX - velocity.x;
	posVelErrorY = posTargetVelBodyY - velocity.y;
	float dTermX = 0.0f;
	float dTermY = 0.0f;
	if (controlDt > 0.0f && controlDt < 0.2f) {
		dTermX = (posVelErrorX - lastVelErrorX) / controlDt * holdDX;
		dTermY = (posVelErrorY - lastVelErrorY) / controlDt * holdDY;
	}
	lastVelErrorX = posVelErrorX;
	lastVelErrorY = posVelErrorY;

	if (abs(posVelErrorX) > 0.04f) {
		velIntegralX = constrain(velIntegralX + posVelErrorX * controlDt * holdIX, -holdIntegralLimit, holdIntegralLimit);
	}
	if (abs(posVelErrorY) > 0.04f) {
		velIntegralY = constrain(velIntegralY + posVelErrorY * controlDt * holdIY, -holdIntegralLimit, holdIntegralLimit);
	}

	// No POS_HOLD-specific angle cap. Keep only the existing global attitude
	// envelope used by manual STAB mode so an estimator fault cannot request a flip.
	float pitchDemandRaw = posVelErrorX * holdPX + velIntegralX + dTermX;
	float rollDemandRaw = -(posVelErrorY * holdPY + velIntegralY + dTermY);
	posControlSaturated = abs(pitchDemandRaw) > tiltMax || abs(rollDemandRaw) > tiltMax;
	float pitchDemand = constrain(pitchDemandRaw, -tiltMax, tiltMax);
	float rollDemand = constrain(rollDemandRaw, -tiltMax, tiltMax);
	float maxCmdStep = maxFlowAngleRate * controlDt;
	posPitchCmd += constrain(pitchDemand - posPitchCmd, -maxCmdStep, maxCmdStep);
	posRollCmd += constrain(rollDemand - posRollCmd, -maxCmdStep, maxCmdStep);
}
