// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Board LED and low-voltage indication.
//
// GPIO21 is a dedicated indicator on the standard Open32Drone board. A low
// battery warning is intentionally visual only: it never changes the active
// flight mode, thrust, arming state, or failsafe behavior.

#ifndef LED_BUILTIN
#define LED_BUILTIN 21 // for ESP32 Dev Module
#endif

constexpr float LOW_VOLTAGE_WARNING_ENTER = 3.10f;
constexpr float LOW_VOLTAGE_WARNING_EXIT = 3.20f;
constexpr uint32_t LOW_VOLTAGE_WARNING_ENTER_MS = 1500;
constexpr uint32_t LOW_VOLTAGE_WARNING_EXIT_MS = 1000;
constexpr uint32_t LOW_VOLTAGE_WARNING_BLINK_MS = 250;

extern float voltage;
bool voltageAvailable();

bool lowVoltageWarning = false;
uint32_t lowVoltageSinceMs = 0;
uint32_t lowVoltageRecoveredSinceMs = 0;

void setupLED() {
	pinMode(LED_BUILTIN, OUTPUT);
}

void setLED(bool on) {
	static bool state = false;
	if (on == state) {
		return; // don't call digitalWrite if the state is the same
	}
	digitalWrite(LED_BUILTIN, on ? HIGH : LOW);
	state = on;
}

bool lowVoltageWarningActive() {
	return lowVoltageWarning;
}

void updateLED() {
	uint32_t now = millis();
	if (!voltageAvailable()) {
		lowVoltageWarning = false;
		lowVoltageSinceMs = 0;
		lowVoltageRecoveredSinceMs = 0;
		setLED(false);
		return;
	}

	if (!lowVoltageWarning) {
		lowVoltageRecoveredSinceMs = 0;
		if (voltage <= LOW_VOLTAGE_WARNING_ENTER) {
			if (lowVoltageSinceMs == 0) lowVoltageSinceMs = now;
			if (now - lowVoltageSinceMs >= LOW_VOLTAGE_WARNING_ENTER_MS) {
				lowVoltageWarning = true;
				lowVoltageSinceMs = 0;
			}
		} else {
			lowVoltageSinceMs = 0;
		}
	} else {
		lowVoltageSinceMs = 0;
		if (voltage >= LOW_VOLTAGE_WARNING_EXIT) {
			if (lowVoltageRecoveredSinceMs == 0) lowVoltageRecoveredSinceMs = now;
			if (now - lowVoltageRecoveredSinceMs >= LOW_VOLTAGE_WARNING_EXIT_MS) {
				lowVoltageWarning = false;
				lowVoltageRecoveredSinceMs = 0;
			}
		} else {
			lowVoltageRecoveredSinceMs = 0;
		}
	}

	setLED(lowVoltageWarning &&
		((now / LOW_VOLTAGE_WARNING_BLINK_MS) % 2U == 0U));
}
