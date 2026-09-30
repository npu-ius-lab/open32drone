// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Motors output control using MOSFETs
// In case of using ESCs, change PWM_STOP, PWM_MIN and PWM_MAX to appropriate values in μs, decrease PWM_FREQUENCY (to 400)

#include "util.h"

// osrbot PIN_MAP
#define MOTOR_0_PIN 4 // rear left
#define MOTOR_1_PIN 3 // rear right
#define MOTOR_2_PIN 6 // front right
#define MOTOR_3_PIN 5  // front left

#define PWM_FREQUENCY 10000
#define PWM_RESOLUTION 10
#define PWM_STOP 0
#define PWM_MIN 0
#define PWM_MAX 1000000 / PWM_FREQUENCY

float motors[4]; // normalized motor thrusts in range [0..1]
bool motorsInitialized = false;

// Keep an explicit, stable channel mapping. The four channels share one timer
// because they use the same frequency and resolution.
const uint8_t MOTOR_0_CHANNEL = 1;
const uint8_t MOTOR_1_CHANNEL = 2;
const uint8_t MOTOR_2_CHANNEL = 3;
const uint8_t MOTOR_3_CHANNEL = 4;
const uint8_t MOTOR_PINS[] = {
	MOTOR_0_PIN, MOTOR_1_PIN, MOTOR_2_PIN, MOTOR_3_PIN,
};

// Motors array indexes:
const int MOTOR_REAR_LEFT = 0;
const int MOTOR_REAR_RIGHT = 1;
const int MOTOR_FRONT_RIGHT = 2;
const int MOTOR_FRONT_LEFT = 3;

void setupMotors() {
	print("Setup Motors\n");

	bool attached[4] = {
		ledcAttachChannel(MOTOR_0_PIN, PWM_FREQUENCY, PWM_RESOLUTION, MOTOR_0_CHANNEL),
		ledcAttachChannel(MOTOR_1_PIN, PWM_FREQUENCY, PWM_RESOLUTION, MOTOR_1_CHANNEL),
		ledcAttachChannel(MOTOR_2_PIN, PWM_FREQUENCY, PWM_RESOLUTION, MOTOR_2_CHANNEL),
		ledcAttachChannel(MOTOR_3_PIN, PWM_FREQUENCY, PWM_RESOLUTION, MOTOR_3_CHANNEL),
	};
	motorsInitialized = attached[0] && attached[1] && attached[2] && attached[3];
	if (!motorsInitialized) {
		for (int i = 0; i < 4; i++) {
			if (attached[i]) ledcDetach(MOTOR_PINS[i]);
		}
		print("Motor initialization failed\n");
		return;
	}

	sendMotors();
	print("Motors initialized: %u Hz configured, four channels attached\n", PWM_FREQUENCY);
}

bool motorPwmHealthy() {
	// `ledcReadFreq` is not a valid disarmed health signal: Arduino-ESP32 3.3.6
	// returns zero when duty is zero. setupMotors() already fails closed unless
	// all four explicit channel attachments succeed, so that result is the
	// authoritative pre-arm state.
	return motorsInitialized;
}

int getDutyCycle(float value) {
	value = constrain(value, 0, 1);
	float pwm = mapf(value, 0, 1, PWM_MIN, PWM_MAX);
	if (value == 0) pwm = PWM_STOP;
	float duty = mapf(pwm, 0, 1000000 / PWM_FREQUENCY, 0, (1 << PWM_RESOLUTION) - 1);
	return round(duty);
}

void sendMotors() {
	if (!motorsInitialized) return;
	ledcWrite(MOTOR_0_PIN, getDutyCycle(motors[0]));
	ledcWrite(MOTOR_1_PIN, getDutyCycle(motors[1]));
	ledcWrite(MOTOR_2_PIN, getDutyCycle(motors[2]));
	ledcWrite(MOTOR_3_PIN, getDutyCycle(motors[3]));
}

bool motorsActive() {
	return motorsInitialized &&
		(motors[0] != 0 || motors[1] != 0 || motors[2] != 0 || motors[3] != 0);
}

void testMotor(int n) {
	if (!motorPwmHealthy() || armed || n < 0 || n > 3) {
		print("Motor test rejected: disarm and remove propellers\n");
		return;
	}
	print("Testing motor %d\n", n);
	memset(motors, 0, sizeof(motors));
	motors[n] = 0.15f;
	delay(50); // ESP32 may need to wait until the end of the current cycle to change duty https://github.com/espressif/arduino-esp32/issues/5306
	sendMotors();
	pause(1);
	motors[n] = 0;
	sendMotors();
	print("Done\n");
}
