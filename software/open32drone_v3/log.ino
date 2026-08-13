// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// In-RAM logging

#include "vector.h"
#include "util.h"

constexpr int LOG_RATE = 50;
constexpr int LOG_DURATION = 6;
constexpr int LOG_SIZE = LOG_DURATION * LOG_RATE;

Vector attitudeEuler;
Vector attitudeTargetEuler;
float logFlowSequence;
float logFlowAgeMs;
float logPosHoldActive;
float logMode;
float logFlowPositionGate;
float logPosHoldGate;
float logFlowRejectReason;
float logPosHoldRejectReason;
float logAltitudeHoldRejectReason;
float logAccNorm;
float logAccRawNorm;
float logAccCorrectionActive;
float logPosControlSaturated;
float logMotorSaturated;

struct LogEntry {
	const char *name;
	float *value;
};

LogEntry logEntries[] = {
	{"t", &t},
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
		{"flowSeq", &logFlowSequence},
		{"flowAgeMs", &logFlowAgeMs},
		{"posHoldActive", &logPosHoldActive},
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
	attitudeEuler = attitude.toEuler();
	attitudeTargetEuler = attitudeTarget.toEuler();
	logFlowSequence = opticalFlowSequence;
	logFlowAgeMs = opticalFlowTimestamp == 0 ? -1.0f : millis() - opticalFlowTimestamp;
	logPosHoldActive = usePosCmd ? 1.0f : 0.0f;
	logMode = mode;
	logFlowPositionGate = flowPositionGateOpen ? 1.0f : 0.0f;
	logPosHoldGate = posHoldGateOpen ? 1.0f : 0.0f;
	logFlowRejectReason = flowRejectReason;
	logPosHoldRejectReason = posHoldRejectReason;
	logAltitudeHoldRejectReason = altitudeHoldRejectReason;
	logAccNorm = acc.norm();
	logAccRawNorm = accRaw.norm();
	logAccCorrectionActive = flightAccCorrectionActive ? 1.0f : 0.0f;
	logPosControlSaturated = posControlSaturated ? 1.0f : 0.0f;
	logMotorSaturated =
		(motors[0] <= 0.101f || motors[0] >= 0.999f ||
		 motors[1] <= 0.101f || motors[1] >= 0.999f ||
		 motors[2] <= 0.101f || motors[2] >= 0.999f ||
		 motors[3] <= 0.101f || motors[3] >= 0.999f) ? 1.0f : 0.0f;
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
