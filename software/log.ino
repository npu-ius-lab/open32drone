// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// In-RAM logging

#include "vector.h"
#include "util.h"

#define LOG_RATE 100
#define LOG_DURATION 10
#define LOG_SIZE LOG_DURATION * LOG_RATE

Vector attitudeEuler;
Vector attitudeTargetEuler;
extern Vector position;
extern Vector velocity;
extern Vector rawWorldPos;
extern Vector rawBodyVel;
extern Vector flowCompBodyVel;

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
	{"position.x", &position.x},
	{"position.y", &position.y},
	{"position.z", &position.z},
	{"velocity.x", &velocity.x},
	{"velocity.y", &velocity.y},
	{"velocity.z", &velocity.z},
	{"rawFlowPos.x", &rawWorldPos.x},
	{"rawFlowPos.y", &rawWorldPos.y},
	{"rawBodyVel.x", &rawBodyVel.x},
	{"rawBodyVel.y", &rawBodyVel.y},
	{"flowCompVel.x", &flowCompBodyVel.x},
	{"flowCompVel.y", &flowCompBodyVel.y},
	{"thrustTarget", &thrustTarget}
};

const int logColumns = sizeof(logEntries) / sizeof(logEntries[0]);
float logBuffer[LOG_SIZE][logColumns];

void prepareLogData() {
	attitudeEuler = attitude.toEuler();
	attitudeTargetEuler = attitudeTarget.toEuler();
}

void logData() {
	if (!armed) return;
	static int logPointer = 0;
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
}

void printLogHeader() {
	for (int i = 0; i < logColumns; i++) {
		print("%s%s", logEntries[i].name, i < logColumns - 1 ? "," : "\n");
	}
}

void printLogData() {
	for (int i = 0; i < LOG_SIZE; i++) {
		if (logBuffer[i][0] == 0) continue; // skip empty records
		for (int j = 0; j < logColumns; j++) {
			print("%g%s", logBuffer[i][j], j < logColumns - 1 ? "," : "\n");
		}
	}
}
