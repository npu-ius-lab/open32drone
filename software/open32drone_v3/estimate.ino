// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Attitude estimation from gyro and accelerometer
// estimateHeight(): 纯 ToF 高度与相邻有效样本垂直速度

#include "quaternion.h"
#include "vector.h"
#include "lpf.h"
#include "util.h"

Vector rates; // estimated angular rates, rad/s
Quaternion attitude; // estimated attitude
bool landed;

float accWeight = 0.003;
float levelWeight = 0.0002f; // retained parameter name: gated in-flight gravity correction weight
LowPassFilter<Vector> ratesFilter(0.2); // cutoff frequency ~ 40 Hz
bool flightAccCorrectionActive = false;
float accFilteredNorm = ONE_G;

extern uint32_t opticalFlowSequence;
extern uint32_t opticalFlowTimestamp;
extern float opticalFlowSampleDt;

void estimate() {
	applyGyro();
	applyAcc();
}

void applyGyro() {
	// filter gyro to get angular rates
	rates = ratesFilter.update(gyro);

	// apply rates to attitude
	attitude = Quaternion::rotate(attitude, Quaternion::fromRotationVector(rates * dt));
}

void applyAcc() {
	accFilteredNorm = acc.norm();
	landed = !motorsActive() && abs(accFilteredNorm - ONE_G) < ONE_G * 0.1f;
	bool flightAccReliable = motorsActive() && thrustTarget >= 0.1f &&
		abs(accFilteredNorm - ONE_G) < ONE_G * 0.15f &&
		rates.norm() < radians(200.0f);
	flightAccCorrectionActive = !landed && flightAccReliable && levelWeight > 0.0f;
	if (!landed && !flightAccCorrectionActive) return;

	// Correct the estimated gravity direction, not toward an assumed level frame.
	// The in-flight weight is deliberately much weaker than the landed weight.
	Vector up = Quaternion::rotateVector(Vector(0, 0, 1), attitude);
	float weight = landed ? accWeight : levelWeight;
	Vector correction = Vector::rotationVectorBetween(acc, up) * weight;

	attitude = Quaternion::rotate(attitude, Quaternion::fromRotationVector(correction));
}

// ==================== ToF height estimation ====================
// High prop vibration made vertical accelerometer integration diverge. Keep
// position.z/velocity.z tied to the direct ToF measurement until vibration is
// low enough for a separately validated inertial vertical estimator.
#define FLOW_HEIGHT_MAX_RATE 2.0f
#define FLOW_HEIGHT_JUMP_REJECT 0.45f

void estimateHeight() {
	static uint32_t lastHeightSequence = 0;
	static bool heightValid = false;
	static LowPassFilter<float> verticalSpeedFilter(0.18f);
	bool sampleValid = opticalFlowHealthy && opticalFlowHeight > 0.05f && opticalFlowHeight < 6.0f;
	bool newHeightSample = sampleValid && opticalFlowSequence != lastHeightSequence;

	if (newHeightSample) {
		lastHeightSequence = opticalFlowSequence;
		float sampleDt = constrain(opticalFlowSampleDt, 0.01f, 0.20f);
		if (!heightValid) {
			position.z = opticalFlowHeight;
			velocity.z = 0.0f;
			verticalSpeedFilter.reset();
			heightValid = true;
			return;
		}

		float heightDelta = opticalFlowHeight - position.z;
		float acceptedDelta = heightDelta;
		if (abs(heightDelta) > FLOW_HEIGHT_JUMP_REJECT) {
			acceptedDelta *= 0.10f;
		}
		float maxStep = max(0.01f, FLOW_HEIGHT_MAX_RATE * sampleDt);
		acceptedDelta = constrain(acceptedDelta, -maxStep, maxStep);
		velocity.z = verticalSpeedFilter.update(acceptedDelta / sampleDt);
		position.z += acceptedDelta;
		return;
	}

	if (!sampleValid) {
		velocity.z *= 0.90f;
		if (!motorsActive()) {
			position.z = 0.0f;
			velocity.z = 0.0f;
			heightValid = false;
			lastHeightSequence = 0;
			verticalSpeedFilter.reset();
		}
	}
}

// ==================== 光流水平速度估计（定点数据基础）====================
// 光流体轴速度 → 陀螺补偿 → 偏置自适应 → 世界系速度/位置积分

// Bench rotation fit for the v3 mounting. Optical-flow packets describe an
// integration window that ends before UART delivery, so use delayed rates.
float flowGyroCompPitch = -0.78f;
float flowGyroCompRoll = -0.77f;
float flowGyroDelayMs = 40.0f;
float flowVelocitySmoothing = 0.12f;
float flowInnovationLimit = 0.8f;
float flowTiltCosMin = 0.85f; // keep flow available throughout the global 30-degree attitude envelope
float flowBiasAdapt = 0.02f;
float flowVelocityZeroDeadband = 0.03f;
float flowPositionZeroDeadband = 0.015f;
float flowScaleX = 1.0f;
float flowScaleY = 1.0f;
float flowRawVelocityLimit = 1.5f;
float flowSpikeRejectVelocity = 2.5f;
float flowPositionVelocityLimit = 1.2f;
float flowArmMinThrottle = 0.12f;
float flowStationaryGyro = radians(8.0f);
float flowStationaryVel = 0.06f;

Vector flowRawBodyVel;
Vector flowGyroBodyVel;
Vector flowCompBodyVel;
Vector flowFilteredBodyVel;
Vector flowBias;
Vector flowInnov;
Vector rawBodyVel;
Vector rawWorldPos;
bool flowStationary = true;
bool flowAirborne = false;
bool flowCtrlZeroLocked = true;
bool flowCtrlUsingFlow = false;
bool flowPositionGateOpen = false;
int flowRejectReason = 0;

float median3(float a, float b, float c) {
	return max(min(a, b), min(max(a, b), c));
}

void estimateHorizontalVelocity() {
	struct TimedRates {
		uint32_t timestamp;
		Vector value;
	};
	constexpr int RATE_HISTORY_SIZE = 64;
	static TimedRates rateHistory[RATE_HISTORY_SIZE];
	static int rateHistoryWrite = 0;
	static int rateHistoryCount = 0;
	static Delay airborneDelay(0.25f);
	static Vector ctrlAnchor;
	static bool ctrlAnchorValid = false;
	static uint32_t lastProcessedSequence = 0;
	static bool flowRecentlyUsable = false;
	static Vector flowHistory[3];
	static int flowHistoryCount = 0;
	static int flowHistoryIndex = 0;

	uint32_t now = millis();
	rateHistory[rateHistoryWrite].timestamp = now;
	rateHistory[rateHistoryWrite].value = rates;
	rateHistoryWrite = (rateHistoryWrite + 1) % RATE_HISTORY_SIZE;
	if (rateHistoryCount < RATE_HISTORY_SIZE) rateHistoryCount++;

	Vector up = Quaternion::rotateVector(Vector(0, 0, 1), attitude);
	bool sampleFresh = opticalFlowHealthy && opticalFlowSequence != 0 &&
		(now - opticalFlowTimestamp <= 150);
	bool tiltRejected = abs(up.z) < flowTiltCosMin;
	bool heightRejected = opticalFlowHeight <= 0.05f;
	bool flowAvailable = sampleFresh && !heightRejected && !tiltRejected;
	// Height is only checked for sensor validity. There is no position-hold
	// engagement altitude or commanded flight-height limit.
	bool airborneCandidate = armed && controlThrottle > flowArmMinThrottle &&
		sampleFresh && !heightRejected;
	flowAirborne = airborneDelay.update(airborneCandidate);
	bool newFlowSample = flowAvailable && opticalFlowSequence != lastProcessedSequence;
	bool flowSpikeRejected =
		abs(flowCompBodyVel.x) > flowSpikeRejectVelocity ||
		abs(flowCompBodyVel.y) > flowSpikeRejectVelocity;
	bool stationaryCandidate = false;
	flowRejectReason = 0;

	if (newFlowSample) {
		lastProcessedSequence = opticalFlowSequence;
		Vector delayedRates = rates;
		if (rateHistoryCount > 0) {
			int oldest = (rateHistoryWrite - rateHistoryCount + RATE_HISTORY_SIZE) % RATE_HISTORY_SIZE;
			delayedRates = rateHistory[oldest].value;
			uint32_t delayMs = (uint32_t)constrain(flowGyroDelayMs, 0.0f, 100.0f);
			uint32_t targetTimestamp = opticalFlowTimestamp > delayMs ? opticalFlowTimestamp - delayMs : 0;
			for (int i = 1; i <= rateHistoryCount; i++) {
				int index = (rateHistoryWrite - i + RATE_HISTORY_SIZE) % RATE_HISTORY_SIZE;
				if ((int32_t)(rateHistory[index].timestamp - targetTimestamp) <= 0) {
					delayedRates = rateHistory[index].value;
					break;
				}
			}
		}
		flowGyroBodyVel.x = delayedRates.y * opticalFlowHeight;
		flowGyroBodyVel.y = -delayedRates.x * opticalFlowHeight;

		// 方向映射（实测推导：光流Y=前向取反、光流X=左向取反；FLU：X=前、Y=左）
		flowRawBodyVel.x = (-opticalFlowVelocityY) * flowScaleX; // 前向速度（+X=机头）
		flowRawBodyVel.y = (-opticalFlowVelocityX) * flowScaleY; // 左向速度（+Y=左）
		flowCompBodyVel.x = flowRawBodyVel.x - flowGyroCompPitch * flowGyroBodyVel.x;
		flowCompBodyVel.y = flowRawBodyVel.y - flowGyroCompRoll * flowGyroBodyVel.y;
		flowSpikeRejected =
			abs(flowCompBodyVel.x) > flowSpikeRejectVelocity ||
			abs(flowCompBodyVel.y) > flowSpikeRejectVelocity;

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
		if (!flowSpikeRejected) {
			// Rejected samples must not poison the median window used by the next
			// otherwise-valid packet.
			flowHistory[flowHistoryIndex] = rawBodyVel;
			flowHistoryIndex = (flowHistoryIndex + 1) % 3;
			if (flowHistoryCount < 3) flowHistoryCount++;
			if (flowHistoryCount < 3) {
				flowFilteredBodyVel = rawBodyVel;
			} else {
				flowFilteredBodyVel.x = median3(flowHistory[0].x, flowHistory[1].x, flowHistory[2].x);
				flowFilteredBodyVel.y = median3(flowHistory[0].y, flowHistory[1].y, flowHistory[2].y);
			}
			float yaw = attitude.getYaw();
			Vector rawWorldVel(
				flowFilteredBodyVel.x * cos(yaw) - flowFilteredBodyVel.y * sin(yaw),
				flowFilteredBodyVel.x * sin(yaw) + flowFilteredBodyVel.y * cos(yaw),
				0);
			rawWorldPos.x += rawWorldVel.x * opticalFlowSampleDt;
			rawWorldPos.y += rawWorldVel.y * opticalFlowSampleDt;
		}
	}

	flowStationary = stationaryCandidate || (!flowAirborne && controlThrottle < flowArmMinThrottle * 0.5f);
	flowCtrlZeroLocked = flowStationary || !flowAirborne;

	if (!sampleFresh) {
		flowRejectReason = 2;
	} else if (!flowAvailable) {
		if (!opticalFlowHealthy) flowRejectReason = 2;
		else if (heightRejected) flowRejectReason = 3;
		else if (tiltRejected) flowRejectReason = 4;
	} else if (!flowAirborne) {
		flowRejectReason = 5;
	} else if (flowStationary) {
		flowRejectReason = 1;
	} else if (flowSpikeRejected) {
		flowRejectReason = 6;
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

	if (!sampleFresh) {
		velocity.x = 0.0f;
		velocity.y = 0.0f;
		flowRecentlyUsable = false;
		flowCtrlUsingFlow = false;
		flowPositionGateOpen = false;
		return;
	}
	if (!flowAvailable) {
		flowRecentlyUsable = false;
		flowCtrlUsingFlow = false;
		flowPositionGateOpen = false;
		return;
	}

	bool usableFlow = flowAvailable && !flowSpikeRejected;

	if (usableFlow && newFlowSample) {
		flowFilteredBodyVel.x = constrain(flowFilteredBodyVel.x, -flowRawVelocityLimit, flowRawVelocityLimit);
		flowFilteredBodyVel.y = constrain(flowFilteredBodyVel.y, -flowRawVelocityLimit, flowRawVelocityLimit);
		flowInnov.x = flowFilteredBodyVel.x - velocity.x;
		flowInnov.y = flowFilteredBodyVel.y - velocity.y;
		velocity.x += constrain(flowInnov.x, -flowInnovationLimit, flowInnovationLimit) * flowVelocitySmoothing;
		velocity.y += constrain(flowInnov.y, -flowInnovationLimit, flowInnovationLimit) * flowVelocitySmoothing;
		flowRecentlyUsable = true;
		flowCtrlUsingFlow = true;
	} else if (usableFlow && flowRecentlyUsable) {
		flowCtrlUsingFlow = true;
	} else if (newFlowSample) {
		velocity.x *= 0.80f;
		velocity.y *= 0.80f;
		flowRecentlyUsable = false;
		flowCtrlUsingFlow = false;
	}

	if (!flowAirborne) {
		if (abs(velocity.x) < flowVelocityZeroDeadband) velocity.x = 0.0f;
		if (abs(velocity.y) < flowVelocityZeroDeadband) velocity.y = 0.0f;
	}

	velocity.x = constrain(velocity.x, -flowPositionVelocityLimit, flowPositionVelocityLimit);
	velocity.y = constrain(velocity.y, -flowPositionVelocityLimit, flowPositionVelocityLimit);

	// Position is integrated once per valid optical-flow packet, never at IMU-loop rate.
	if (!flowCtrlUsingFlow) {
		flowPositionGateOpen = false;
		return;
	}

	// TOF is the direct height measurement. The integrated vertical state is too
	// noisy during takeoff to be a hard horizontal-position gate.
	bool positionHeightStable = opticalFlowHealthy &&
		opticalFlowHeight > 0.05f && opticalFlowHeight < 6.0f;
	bool positionTiltStable = abs(up.z) > flowTiltCosMin;
	bool positionFlowStable =
		abs(flowFilteredBodyVel.x) < flowRawVelocityLimit &&
		abs(flowFilteredBodyVel.y) < flowRawVelocityLimit &&
		!flowSpikeRejected;

	flowPositionGateOpen = positionHeightStable && positionTiltStable && positionFlowStable;
	if (!newFlowSample || !flowPositionGateOpen) return;

	float yaw = attitude.getYaw();
	Vector worldVel(
		velocity.x * cos(yaw) - velocity.y * sin(yaw),
		velocity.x * sin(yaw) + velocity.y * cos(yaw),
		0);
	if (abs(worldVel.x) < flowPositionZeroDeadband) worldVel.x = 0.0f;
	if (abs(worldVel.y) < flowPositionZeroDeadband) worldVel.y = 0.0f;
	position.x += worldVel.x * opticalFlowSampleDt;
	position.y += worldVel.y * opticalFlowSampleDt;
}
