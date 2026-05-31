// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Attitude, height and optical-flow position estimation

#include "quaternion.h"
#include "vector.h"
#include "lpf.h"
#include "util.h"
#include "kalman_angle.h"

KalmanAngle kalmanRoll;
KalmanAngle kalmanPitch;
float yawAccumulated = 0.0f;

#define ACC_Z_DEADBAND 1.5f
#define VEL_Z_DAMPING 0.99f
#define POS_Z_CORRECTION_GAIN 0.05f
#define VEL_Z_CORRECTION_GAIN 0.10f
#define ACC_CLIP_THRESHOLD_G 14.0f
#define ACC_ATT_LPF_ALPHA 0.02f
#define ATT_RATES_LPF_ALPHA 0.03f
#define ATT_RATES_GROUND_DB radians(3.0f)
#define ATT_YAW_GROUND_DB radians(2.0f)
#define ATT_ACCEL_TRUST_DISABLE 0.15f
#define ATT_R_MEASURE_MAX 1.50f
#define ATT_R_MEASURE_NOISY_MIN 1.20f

float boardAlignPitch = 0.019f;
float boardAlignRoll = 0.0f;
float flowGyroCompPitch = -1.1f;
float flowGyroCompRoll = -1.1f;
float flowVelocitySmoothing = 0.18f;
float flowInnovationLimit = 0.5f;
float flowTiltCosMin = 0.90f;
float imuRateAlpha = 0.1f;
float flowBiasAdapt = 0.02f;
float flowVelocityZeroDeadband = 0.03f;
float flowPositionZeroDeadband = 0.015f;
float flowScaleX = 1.0f;
float flowScaleY = 1.0f;
float flowArmMinHeight = 0.22f;
float flowArmMinThrottle = 0.12f;
float flowStationaryGyro = radians(8.0f);
float flowStationaryVel = 0.06f;

Vector rates;
Quaternion attitude;
bool landed;

Vector flowRawBodyVel;
Vector flowGyroBodyVel;
Vector flowCompBodyVel;
Vector flowBias;
Vector flowInnov;
Vector rawBodyVel;
Vector rawWorldPos;
bool flowStationary = true;
bool flowAirborne = false;
bool flowCtrlZeroLocked = true;
bool flowCtrlUsingFlow = false;
int flowRejectReason = 0;

float imuAccelTrustDebug = 1.0f;
float imuAccelMeasureRDebug = 0.03f;
bool imuAccelClippedDebug = false;
bool imuHeightAccelValidDebug = true;
bool imuAccelAngleUsedDebug = true;
float imuAccelAngleBlendDebug = 1.0f;
float imuAccRollAngleDebug = 0.0f;
float imuAccPitchAngleDebug = 0.0f;
float imuAccNormDebug = 0.0f;
float imuAccAttNormDebug = 0.0f;
float imuAccVibeDebug = 0.0f;
Vector imuAccForAttitudeDebug;
Vector imuAttitudeRatesDebug;
bool gyroBiasLearnAllowedDebug = false;

extern Vector gyro;
extern Vector acc;
extern float dt;
extern bool armed;
extern float controlThrottle;
extern Vector velocity;
extern Vector position;
extern bool opticalFlowHealthy;
extern float opticalFlowVelocityX;
extern float opticalFlowVelocityY;
extern float opticalFlowHeight;

extern bool motorsActive();

void estimateHeight();
void estimateHorizontalVelocity();
float getDynamicRateAlpha();
float getDynamicAttitudeRateAlpha();
float getDynamicAccelTrust(float accNorm, bool accClipped);
bool isAccelClipped();
float applyAxisDeadband(float value, float deadband);
float blendAngleToward(float currentAngle, float measuredAngle, float blend);

float applyAxisDeadband(float value, float deadband) {
	if (abs(value) <= deadband) return 0.0f;
	return value > 0.0f ? value - deadband : value + deadband;
}

float blendAngleToward(float currentAngle, float measuredAngle, float blend) {
	float delta = wrapAngle(measuredAngle - currentAngle);
	return wrapAngle(currentAngle + delta * constrain(blend, 0.0f, 1.0f));
}

bool isAccelClipped() {
	float clipLimit = ONE_G * ACC_CLIP_THRESHOLD_G;
	return abs(acc.x) >= clipLimit || abs(acc.y) >= clipLimit || abs(acc.z) >= clipLimit;
}

float getDynamicRateAlpha() {
	float baseAlpha = constrain(imuRateAlpha, 0.001f, 1.0f);
	if (!motorsActive()) return baseAlpha;

	float throttleBlend = constrain(mapf(controlThrottle, 0.08f, 0.25f, 0.0f, 1.0f), 0.0f, 1.0f);
	float nearGroundBlend = constrain(1.0f - position.z / 0.20f, 0.0f, 1.0f);
	float filteredAlpha = imuAccelClippedDebug ? 0.010f : 0.020f;
	return baseAlpha + (min(baseAlpha, filteredAlpha) - baseAlpha) * throttleBlend * nearGroundBlend;
}

float getDynamicAttitudeRateAlpha() {
	float alpha = ATT_RATES_LPF_ALPHA;
	if (!motorsActive()) return alpha;

	float throttleBlend = constrain(mapf(controlThrottle, 0.08f, 0.25f, 0.0f, 1.0f), 0.0f, 1.0f);
	float nearGroundBlend = constrain(1.0f - position.z / 0.25f, 0.0f, 1.0f);
	float filteredAlpha = imuAccelClippedDebug ? 0.004f : 0.008f;
	return alpha + (filteredAlpha - alpha) * throttleBlend * nearGroundBlend;
}

float getDynamicAccelTrust(float accNorm, bool accClipped) {
	float normError = abs(accNorm - ONE_G) / ONE_G;
	float normTrust = constrain(mapf(normError, 0.02f, 0.12f, 1.0f, 0.0f), 0.0f, 1.0f);
	if (accClipped) return 0.0f;
	if (!motorsActive()) return normTrust;

	float throttleBlend = constrain(mapf(controlThrottle, 0.08f, 0.25f, 0.0f, 1.0f), 0.0f, 1.0f);
	float nearGroundBlend = constrain(1.0f - position.z / 0.30f, 0.0f, 1.0f);
	float motorPenalty = throttleBlend * nearGroundBlend;
	return constrain(normTrust * (1.0f - motorPenalty * 0.98f), 0.0f, 1.0f);
}

void estimate() {
	if (!(dt > 0.0f)) return;

	float accNorm = acc.norm();
	bool accClipped = isAccelClipped();
	bool opticalHeightValid = opticalFlowHealthy && opticalFlowHeight > 0.05f;
	imuAccelClippedDebug = accClipped;

	static LowPassFilter<Vector> ratesFilter(0.1f);
	static LowPassFilter<Vector> attitudeRatesFilter(ATT_RATES_LPF_ALPHA);
	static LowPassFilter<float> accelTrustFilter(0.08f);
	static LowPassFilter<Vector> accAttitudeFilter(ACC_ATT_LPF_ALPHA);

	ratesFilter.alpha = getDynamicRateAlpha();
	rates = ratesFilter.update(gyro);
	attitudeRatesFilter.alpha = getDynamicAttitudeRateAlpha();
	Vector attitudeRates = attitudeRatesFilter.update(rates);

	Vector accForAttitude = accAttitudeFilter.update(acc);
	imuAccNormDebug = accNorm;
	imuAccAttNormDebug = accForAttitude.norm();
	imuAccVibeDebug = (acc - accForAttitude).norm();
	imuAccForAttitudeDebug = accForAttitude;

	float accelTrust = accelTrustFilter.update(getDynamicAccelTrust(accNorm, accClipped));
	bool nearGroundMotorNoise = motorsActive() && !opticalHeightValid && position.z < 0.25f;
	if (nearGroundMotorNoise) {
		attitudeRates.x = applyAxisDeadband(attitudeRates.x, ATT_RATES_GROUND_DB);
		attitudeRates.y = applyAxisDeadband(attitudeRates.y, ATT_RATES_GROUND_DB);
		attitudeRates.z = applyAxisDeadband(attitudeRates.z, ATT_YAW_GROUND_DB);
	}
	imuAttitudeRatesDebug = attitudeRates;

	float accelAngleBlend = constrain(mapf(accelTrust, ATT_ACCEL_TRUST_DISABLE, 1.0f, 0.0f, 1.0f), 0.0f, 1.0f);
	if (nearGroundMotorNoise) accelAngleBlend = min(accelAngleBlend, 0.05f);
	if (accelTrust <= ATT_ACCEL_TRUST_DISABLE || accClipped) accelAngleBlend = 0.0f;

	float dynamicRMeasure = 0.03f + (ATT_R_MEASURE_MAX - 0.03f) * (1.0f - accelTrust);
	if (nearGroundMotorNoise) dynamicRMeasure = max(dynamicRMeasure, ATT_R_MEASURE_NOISY_MIN);
	kalmanRoll.R_measure = dynamicRMeasure;
	kalmanPitch.R_measure = dynamicRMeasure;
	imuAccelTrustDebug = accelTrust;
	imuAccelMeasureRDebug = dynamicRMeasure;
	imuAccelAngleUsedDebug = accelAngleBlend > 0.001f;
	imuAccelAngleBlendDebug = accelAngleBlend;

	float accRollAngleMeasured = atan2(accForAttitude.y, accForAttitude.z) + boardAlignRoll;
	float accPitchAngleMeasured = atan2(-accForAttitude.x, sqrt(accForAttitude.y * accForAttitude.y + accForAttitude.z * accForAttitude.z)) + boardAlignPitch;
	imuAccRollAngleDebug = accRollAngleMeasured;
	imuAccPitchAngleDebug = accPitchAngleMeasured;

	float accRollAngle = imuAccelAngleUsedDebug ? blendAngleToward(kalmanRoll.angle, accRollAngleMeasured, accelAngleBlend) : kalmanRoll.angle;
	float accPitchAngle = imuAccelAngleUsedDebug ? blendAngleToward(kalmanPitch.angle, accPitchAngleMeasured, accelAngleBlend) : kalmanPitch.angle;

	float estimatedRoll = kalmanRoll.getAngle(accRollAngle, attitudeRates.x, dt);
	float estimatedPitch = kalmanPitch.getAngle(accPitchAngle, attitudeRates.y, dt);
	yawAccumulated = wrapAngle(yawAccumulated + attitudeRates.z * dt);
	attitude = Quaternion::fromEuler(Vector(estimatedRoll, estimatedPitch, yawAccumulated));

	landed = !motorsActive() && abs(accNorm - ONE_G) < ONE_G * 0.1f;
	estimateHeight();
	estimateHorizontalVelocity();
}

void estimateHeight() {
	bool opticalHeightValid = opticalFlowHealthy && opticalFlowHeight > 0.05f;
	bool allowAccelHeight = !imuAccelClippedDebug && (!motorsActive() || opticalHeightValid || position.z > 0.20f);
	imuHeightAccelValidDebug = allowAccelHeight;

	if (allowAccelHeight) {
		Vector accWorld = Quaternion::rotateVector(acc, attitude);
		float accDiff = accWorld.z - ONE_G;
		float accZLinear = abs(accDiff) > ACC_Z_DEADBAND ? accDiff : 0.0f;
		position.z += velocity.z * dt + 0.5f * accZLinear * dt * dt;
		velocity.z += accZLinear * dt;
		velocity.z *= VEL_Z_DAMPING;
	} else {
		velocity.z *= 0.70f;
		if (!opticalHeightValid && position.z < 0.25f) {
			position.z += (0.0f - position.z) * 0.20f;
			if (abs(position.z) < 0.003f) position.z = 0.0f;
		} else {
			position.z += velocity.z * dt;
		}
	}

	if (opticalHeightValid) {
		float posError = opticalFlowHeight - position.z;
		if (abs(posError) > 0.30f && abs(velocity.z) < 1.0f) position.z += posError * 0.02f;
		else {
			position.z += posError * POS_Z_CORRECTION_GAIN;
			velocity.z += posError * VEL_Z_CORRECTION_GAIN;
		}
	}

	if (position.z < 0.0f) {
		position.z = 0.0f;
		if (velocity.z < 0.0f) velocity.z = 0.0f;
	}
}

void estimateHorizontalVelocity() {
	static LowPassFilter<float> lpfGyroX(0.08f);
	static LowPassFilter<float> lpfGyroY(0.08f);
	static Delay airborneDelay(0.25f);
	static Vector ctrlAnchor;
	static bool ctrlAnchorValid = false;

	flowRawBodyVel = Vector();
	flowGyroBodyVel = Vector();
	flowCompBodyVel = Vector();
	rawBodyVel = Vector();
	flowInnov = Vector();
	flowCtrlUsingFlow = false;
	flowRejectReason = 0;

	Vector up = Quaternion::rotateVector(Vector(0, 0, 1), attitude);
	bool tiltRejected = abs(up.z) < flowTiltCosMin;
	bool heightRejected = opticalFlowHeight <= 0.05f;
	bool flowAvailable = opticalFlowHealthy && !heightRejected && !tiltRejected;
	bool airborneCandidate = armed && controlThrottle > flowArmMinThrottle && position.z > flowArmMinHeight;
	flowAirborne = airborneDelay.update(airborneCandidate);
	bool stationaryCandidate = false;

	if (flowAvailable) {
		flowGyroBodyVel.x = lpfGyroX.update(gyro.y * opticalFlowHeight);
		flowGyroBodyVel.y = lpfGyroY.update(-gyro.x * opticalFlowHeight);

		flowRawBodyVel.x = (-opticalFlowVelocityY) * flowScaleX;
		flowRawBodyVel.y = opticalFlowVelocityX * flowScaleY;
		flowCompBodyVel.x = flowRawBodyVel.x - flowGyroCompPitch * flowGyroBodyVel.x;
		flowCompBodyVel.y = flowRawBodyVel.y - flowGyroCompRoll * flowGyroBodyVel.y;

		stationaryCandidate =
			!flowAirborne &&
			abs(flowCompBodyVel.x) < flowStationaryVel &&
			abs(flowCompBodyVel.y) < flowStationaryVel &&
			abs(rates.x) < flowStationaryGyro &&
			abs(rates.y) < flowStationaryGyro &&
			controlThrottle < flowArmMinThrottle;

		if (stationaryCandidate) {
			flowBias.x += (flowCompBodyVel.x - flowBias.x) * flowBiasAdapt;
			flowBias.y += (flowCompBodyVel.y - flowBias.y) * flowBiasAdapt;
		}

		rawBodyVel.x = flowCompBodyVel.x - flowBias.x;
		rawBodyVel.y = flowCompBodyVel.y - flowBias.y;
		Vector rawWorldVel = Quaternion::rotateVector(Vector(rawBodyVel.x, rawBodyVel.y, 0), attitude);
		rawWorldPos.x += rawWorldVel.x * dt;
		rawWorldPos.y += rawWorldVel.y * dt;
	}

	flowStationary = stationaryCandidate || (!flowAirborne && controlThrottle < flowArmMinThrottle * 0.5f);
	flowCtrlZeroLocked = flowStationary || !flowAirborne;

	if (!flowAvailable) {
		if (!opticalFlowHealthy) flowRejectReason = 2;
		else if (heightRejected) flowRejectReason = 3;
		else if (tiltRejected) flowRejectReason = 4;
	} else if (!flowAirborne) {
		flowRejectReason = 5;
	} else if (flowStationary) {
		flowRejectReason = 1;
	}

	if (flowCtrlZeroLocked) {
		if (!ctrlAnchorValid) {
			ctrlAnchor.x = position.x;
			ctrlAnchor.y = position.y;
			ctrlAnchorValid = true;
		}

		velocity.x *= 0.3f;
		velocity.y *= 0.3f;
		if (abs(velocity.x) < flowVelocityZeroDeadband) velocity.x = 0.0f;
		if (abs(velocity.y) < flowVelocityZeroDeadband) velocity.y = 0.0f;

		position.x += (ctrlAnchor.x - position.x) * 0.35f;
		position.y += (ctrlAnchor.y - position.y) * 0.35f;
		if (abs(position.x - ctrlAnchor.x) < flowPositionZeroDeadband) position.x = ctrlAnchor.x;
		if (abs(position.y - ctrlAnchor.y) < flowPositionZeroDeadband) position.y = ctrlAnchor.y;
		return;
	}

	ctrlAnchorValid = false;

	if (flowAvailable) {
		flowInnov.x = rawBodyVel.x - velocity.x;
		flowInnov.y = rawBodyVel.y - velocity.y;
		velocity.x += constrain(flowInnov.x, -flowInnovationLimit, flowInnovationLimit) * flowVelocitySmoothing;
		velocity.y += constrain(flowInnov.y, -flowInnovationLimit, flowInnovationLimit) * flowVelocitySmoothing;
		flowCtrlUsingFlow = true;
	} else {
		velocity.x *= 0.95f;
		velocity.y *= 0.95f;
	}

	if (abs(velocity.x) < flowVelocityZeroDeadband) velocity.x = 0.0f;
	if (abs(velocity.y) < flowVelocityZeroDeadband) velocity.y = 0.0f;

	Vector worldVel = Quaternion::rotateVector(Vector(velocity.x, velocity.y, 0), attitude);
	if (abs(worldVel.x) < flowPositionZeroDeadband) worldVel.x = 0.0f;
	if (abs(worldVel.y) < flowPositionZeroDeadband) worldVel.y = 0.0f;
	position.x += worldVel.x * dt;
	position.y += worldVel.y * dt;
}
