// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Main firmware file

#include "vector.h"
#include "quaternion.h"
#include "util.h"

#define WIFI_ENABLED 1
#define OPTICAL_FLOW_ENABLED 1

extern float t, dt;
extern float controlRoll, controlPitch, controlYaw, controlThrottle, controlMode;
extern Vector gyro, acc;
extern Vector rates;
extern Quaternion attitude;
extern bool landed;
extern float motors[4];

Vector position; // estimated position in world frame, m
Vector velocity; // x/y body velocity, z world velocity, m/s
bool opticalFlowHealthy = false;
float opticalFlowVelocityX = 0.0f;
float opticalFlowVelocityY = 0.0f;
float opticalFlowHeight = 0.0f;

void controlPilotLoop();
void controlPositionLoop();
void controlAttitudeLoop();
void controlRateTorqueLoop();
void setupConsole();
#if OPTICAL_FLOW_ENABLED
void setupOpticalFlow();
void readOpticalFlow();
#endif

void setup() {
	setupConsole();
	print("Initializing flix\n");
	disableBrownOut();
	setupParameters();
	setupLED();
	setupMotors();
	setLED(true);
#if WIFI_ENABLED
	setupWiFi();
#endif
	setupIMU();
	setupRC();
#if OPTICAL_FLOW_ENABLED
	setupOpticalFlow();
#endif
	setLED(false);
	print("Initializing complete\n");
}

void loop() {
	static Rate pilotRate(80);
	static Rate positionRate(40);
	static Rate attitudeRate(150);
	static Rate innerRate(400);

	readIMU();
	step();
	readRC();
#if OPTICAL_FLOW_ENABLED
	readOpticalFlow();
#endif
	estimate();
	if (pilotRate) controlPilotLoop();
	if (positionRate) controlPositionLoop();
	if (attitudeRate) controlAttitudeLoop();
	if (innerRate) controlRateTorqueLoop();
	sendMotors();
	handleInput();
#if WIFI_ENABLED
	processMavlink();
#endif
	logData();
	syncParameters();
}
