// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// In-RAM logging

#include "vector.h"
#include "util.h"

extern bool rcFailsafeActive;
#if WIFI_ENABLED
extern bool mavlinkManualControlActive;
extern uint32_t mavlinkManualControlLastMs;
#endif

constexpr int LOG_RATE = 25;
constexpr int LOG_DURATION = 12;
constexpr int LOG_SIZE = LOG_DURATION * LOG_RATE;

Vector attitudeTargetEuler;
float logFlowSequence;
float logFlowAgeMs;
float logTofAgeMs;
float logTofHealthy;
float logTofStrength;
float logFlowSensorSequence;
float logFlowPacketGaps;
float logFlowIntegrationUs;
float logPosHoldActive;
float logPosHoldFallback;
float logMode;
float logFlowPositionGate;
float logPosHoldGate;
float logFlowRejectReason;
float logFlowBiasReady;
float logPosHoldRejectReason;
float logAltitudeHoldRejectReason;
float logTime;
float logAccNorm;
float logAccRawNorm;
float logAccCorrectionActive;
float logPosControlSaturated;
float logMotorSaturated;
float logArmed;
float logActuatorOwner;
float logAutoFlightPhase;
float logAutoFlightSource;
float logRcHealthy;
float logRcPilotActive;
float logImuHealthy;
float logAltitudeTarget;
float logAltitudeCorrection;
float logRcFailsafeActive;
float logManualControlActive;
float logManualControlAgeMs;
float logLandingFlareLatched;
float logVoltageCompensation;
float logHoverFeedForward;

struct LogEntry {
	const char *name;
	float *value;
};

LogEntry logEntries[] = {
	{"t", &logTime},
	{"rates.x", &rates.x},
	{"rates.y", &rates.y},
	{"rates.z", &rates.z},
	{"ratesTarget.x", &ratesTarget.x},
	{"ratesTarget.y", &ratesTarget.y},
	{"ratesTarget.z", &ratesTarget.z},
	{"attitude.x", &attitudeEuler.x},
	{"attitude.y", &attitudeEuler.y},
	{"attitude.z", &attitudeEuler.z},
	{"attitudeTarget.x", &attitudeTargetEuler.x},
	{"attitudeTarget.y", &attitudeTargetEuler.y},
		{"attitudeTarget.z", &attitudeTargetEuler.z},
		{"thrustTarget", &thrustTarget},
		{"controlRoll", &controlRoll},
		{"controlPitch", &controlPitch},
		{"controlYaw", &controlYaw},
		{"controlThrottle", &controlThrottle},
		{"controlMode", &controlMode},
		{"velocity.x", &velocity.x},
		{"velocity.y", &velocity.y},
		{"velocity.z", &velocity.z},
		{"position.z", &position.z},
		{"flowHeight", &opticalFlowHeight},
		{"flowRaw.x", &flowRawBodyVel.x},
		{"flowRaw.y", &flowRawBodyVel.y},
		{"flowComp.x", &flowCompBodyVel.x},
		{"flowComp.y", &flowCompBodyVel.y},
		{"flowFilt.x", &flowFilteredBodyVel.x},
		{"flowFilt.y", &flowFilteredBodyVel.y},
		{"flowGyro.x", &flowGyroBodyVel.x},
		{"flowGyro.y", &flowGyroBodyVel.y},
		{"flowBias.x", &flowBias.x},
		{"flowBias.y", &flowBias.y},
		{"flowBiasReady", &logFlowBiasReady},
		{"flowGyroDelayMs", &flowGyroDelayMs},
		{"position.x", &position.x},
		{"position.y", &position.y},
		{"targetPosX", &targetPosX},
		{"targetPosY", &targetPosY},
		{"posRollCmd", &posRollCmd},
		{"posPitchCmd", &posPitchCmd},
		{"targetVel.x", &posTargetVelBodyX},
		{"targetVel.y", &posTargetVelBodyY},
		{"velError.x", &posVelErrorX},
		{"velError.y", &posVelErrorY},
		{"velIntegral.x", &velIntegralX},
		{"velIntegral.y", &velIntegralY},
		{"flowSeq", &logFlowSequence},
		{"flowAgeMs", &logFlowAgeMs},
		{"tofAgeMs", &logTofAgeMs},
		{"tofHealthy", &logTofHealthy},
		{"tofStrength", &logTofStrength},
		{"flowSensorSeq", &logFlowSensorSequence},
		{"flowPacketGaps", &logFlowPacketGaps},
		{"flowIntegrationUs", &logFlowIntegrationUs},
		{"posHoldActive", &logPosHoldActive},
		{"posFallback", &logPosHoldFallback},
		{"mode", &logMode},
		{"flowPosGate", &logFlowPositionGate},
		{"posHoldGate", &logPosHoldGate},
		{"flowReject", &logFlowRejectReason},
		{"posReject", &logPosHoldRejectReason},
		{"altReject", &logAltitudeHoldRejectReason},
		{"accFiltNorm", &logAccNorm},
		{"accRawNorm", &logAccRawNorm},
		{"accCorrection", &logAccCorrectionActive},
		{"posSaturated", &logPosControlSaturated},
		{"motorSaturated", &logMotorSaturated},
		{"armed", &logArmed},
		{"actuatorOwner", &logActuatorOwner},
		{"autoPhase", &logAutoFlightPhase},
		{"autoSource", &logAutoFlightSource},
		{"autoTargetZ", &autoFlightTargetHeight},
		{"autoGoalZ", &autoFlightGoalHeight},
		{"takeoffLimit", &autoTakeoffThrustLimit},
		{"rcHealthy", &logRcHealthy},
		{"rcPilotActive", &logRcPilotActive},
		{"imuHealthy", &logImuHealthy},
		{"altTarget", &logAltitudeTarget},
		{"altCorrection", &logAltitudeCorrection},
		{"rcFailsafe", &logRcFailsafeActive},
		{"manualControl", &logManualControlActive},
		{"manualAgeMs", &logManualControlAgeMs},
		{"landingFlareLatched", &logLandingFlareLatched},
		{"landingHoldFallback", &landingHoldFallbackScale},
		{"voltage", &voltage},
		{"voltComp", &logVoltageCompensation},
		{"hoverFF", &logHoverFeedForward},
		{"mixerScale", &mixerScale},
		{"loopMaxMs", &loopDtMaxMs},
		{"motor.rl", &motors[0]},
		{"motor.rr", &motors[1]},
		{"motor.fr", &motors[2]},
		{"motor.fl", &motors[3]}
};

const int logColumns = sizeof(logEntries) / sizeof(logEntries[0]);
float logBuffer[LOG_SIZE][logColumns];
int logPointer = 0;
int logCount = 0;

void prepareLogData() {
	logTime = (float)t;
	attitudeTargetEuler = attitudeTarget.toEuler();
	logFlowSequence = opticalFlowSequence;
	logFlowAgeMs = opticalFlowTimestamp == 0 ? -1.0f : millis() - opticalFlowTimestamp;
	logTofAgeMs = tofTimestamp == 0 ? -1.0f : millis() - tofTimestamp;
	logTofHealthy = tofHealthy ? 1.0f : 0.0f;
	logTofStrength = opticalFlowTofStrength;
	logFlowSensorSequence = opticalFlowSensorPacketSequence;
	logFlowPacketGaps = opticalFlowPacketGapCount;
	logFlowIntegrationUs = opticalFlowIntegrationTimeUs;
	logPosHoldActive = usePosCmd ? 1.0f : 0.0f;
	logPosHoldFallback = posHoldFallbackActive ? 1.0f : 0.0f;
	logMode = mode;
	logFlowPositionGate = flowPositionGateOpen ? 1.0f : 0.0f;
	logPosHoldGate = posHoldGateOpen ? 1.0f : 0.0f;
	logFlowRejectReason = flowRejectReason;
	logFlowBiasReady = flowBiasReady ? 1.0f : 0.0f;
	logPosHoldRejectReason = posHoldRejectReason;
	logAltitudeHoldRejectReason = altitudeHoldRejectReason;
	logAccNorm = acc.norm();
	logAccRawNorm = accRaw.norm();
	logAccCorrectionActive = flightAccCorrectionActive ? 1.0f : 0.0f;
	logPosControlSaturated = posControlSaturated ? 1.0f : 0.0f;
	// Idle motors are not mixer saturation. Log the actual torque desaturation
	// scale so takeoff/landing analysis can distinguish the two conditions.
	logMotorSaturated = mixerScale < 0.999f ? 1.0f : 0.0f;
	logArmed = armed ? 1.0f : 0.0f;
	logActuatorOwner = actuatorOwner;
	logAutoFlightPhase = autoFlightPhase;
	logAutoFlightSource = autoFlightSource;
	logRcHealthy = rcLinkHealthy ? 1.0f : 0.0f;
	logRcPilotActive = rcPilotActive ? 1.0f : 0.0f;
	logImuHealthy = imuHealthy ? 1.0f : 0.0f;
	logAltitudeTarget = altitudeHoldTarget;
	logAltitudeCorrection = altitudeHoldCorrection;
	logRcFailsafeActive = rcFailsafeActive ? 1.0f : 0.0f;
	#if WIFI_ENABLED
		logManualControlActive = mavlinkManualControlActive ? 1.0f : 0.0f;
		logManualControlAgeMs = mavlinkManualControlLastMs == 0 ? -1.0f : millis() - mavlinkManualControlLastMs;
	#else
		logManualControlActive = 0.0f;
		logManualControlAgeMs = -1.0f;
	#endif
	logLandingFlareLatched = autoLandingFlareLatched ? 1.0f : 0.0f;
	logVoltageCompensation = voltageThrustCompensationFactor();
	logHoverFeedForward = altitudeHoverFeedForward();
}

void logData() {
	static bool wasArmed = false;
	if (armed && !wasArmed) {
		logPointer = 0;
		logCount = 0;
	}
	wasArmed = armed;
	if (!armed) return;
	static Rate period(LOG_RATE);
	if (!period) return;

	prepareLogData();

	for (int i = 0; i < logColumns; i++) {
		logBuffer[logPointer][i] = *logEntries[i].value;
	}

	logPointer++;
	if (logPointer >= LOG_SIZE) {
		logPointer = 0;
	}
	if (logCount < LOG_SIZE) logCount++;
}

void printLogHeader() {
	for (int i = 0; i < logColumns; i++) {
		print("%s%s", logEntries[i].name, i < logColumns - 1 ? "," : "\n");
	}
}

void printLogData() {
	if (armed) {
		print("Log dump blocked while armed\n");
		return;
	}
	print("Log samples: %d\n", logCount);
	int start = (logPointer - logCount + LOG_SIZE) % LOG_SIZE;
	for (int i = 0; i < logCount; i++) {
		int row = (start + i) % LOG_SIZE;
		for (int j = 0; j < logColumns; j++) {
			print("%g%s", logBuffer[row][j], j < logColumns - 1 ? "," : "\n");
		}
	}
}

uint32_t logDataSizeBytes() {
	return (uint32_t)logCount * (uint32_t)logColumns * sizeof(float);
}

uint8_t copyLogDataBytes(uint32_t offset, uint8_t *destination, uint8_t capacity) {
	if (!destination || capacity == 0) return 0;
	uint32_t total = logDataSizeBytes();
	if (offset >= total) return 0;
	uint8_t copied = (uint8_t)min((uint32_t)capacity, total - offset);
	const uint32_t rowBytes = (uint32_t)logColumns * sizeof(float);
	int start = (logPointer - logCount + LOG_SIZE) % LOG_SIZE;
	for (uint8_t i = 0; i < copied; i++) {
		uint32_t logicalByte = offset + i;
		uint32_t logicalRow = logicalByte / rowBytes;
		uint32_t byteInRow = logicalByte % rowBytes;
		int physicalRow = (start + logicalRow) % LOG_SIZE;
		const uint8_t *row = reinterpret_cast<const uint8_t *>(logBuffer[physicalRow]);
		destination[i] = row[byteInRow];
	}
	return copied;
}
