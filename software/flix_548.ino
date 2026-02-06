// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Main firmware file

#include "vector.h"
#include "quaternion.h"
#include "util.h"

#define WIFI_ENABLED 1
// osrbot start
#define OPTICAL_FLOW_ENABLED 1
// Position estimation variables
Vector position; // estimated position in world frame, m
Vector velocity; // estimated velocity in world frame, m/s
float height; // estimated height above ground, m
bool positionValid; // is position estimate valid

bool opticalFlowHealthy = false;
float opticalFlowVelocityX = 0;
float opticalFlowVelocityY = 0;
float opticalFlowHeight = 0;
// osrbot end

float t = NAN; // current step time, s
float dt; // time delta from previous step, s
float controlRoll, controlPitch, controlYaw, controlThrottle; // pilot's inputs, range [-1, 1]
float controlMode = NAN;
Vector gyro; // gyroscope data
Vector acc; // accelerometer data, m/s/s
Vector rates; // filtered angular rates, rad/s
Quaternion attitude; // estimated attitude
bool landed; // are we landed and stationary
float motors[4]; // normalized motors thrust in range [0..1]

void setup() {
	Serial.begin(115200);
	print("Initializing flix\n");
	disableBrownOut();
	setupParameters();
	setupLED();
	setupMotors();
	setLED(true);
#if WIFI_ENABLED
	setupWiFi();
	print("333");
#endif
	setupIMU();
	print("222");
	setupRC();
	print("111");
#if OPTICAL_FLOW_ENABLED
	print("start flow\n");
	setupOpticalFlow();
	print("444");
#endif
	setLED(false);
	print("Initializing complete\n");
}

void loop() {
	readIMU();
	step();
	readRC();
#if OPTICAL_FLOW_ENABLED
	readOpticalFlow();
#endif
	estimate();
	control();
	sendMotors();
	handleInput();
#if WIFI_ENABLED
	processMavlink();
#endif
	logData();
	syncParameters();
}