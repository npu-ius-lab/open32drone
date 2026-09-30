// Copyright (c) 2026 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Power management for the standard 1S board.
// GPIO1/A0 measures VBAT_SW through a 100k/100k divider. A bounded factor from
// this measurement adjusts only assisted-flight collective feed-forward. The
// factor is bidirectional around the measured crossover voltage: a fresh
// battery reduces feed-forward and a discharged battery increases it. It never
// changes arming, STAB/direct-Offboard throttle, attitude corrections, or the
// automatic takeoff cap.
// Keep the ESP32 brown-out detector enabled. An uncontrolled peripheral/CPU
// state during a supply sag is more dangerous than a clean reset.

#include "lpf.h"
#include "util.h"

constexpr float VOLTAGE_MIN_PLAUSIBLE = 2.0f;
constexpr float VOLTAGE_MAX_PLAUSIBLE = 4.5f;
constexpr uint32_t VOLTAGE_STALE_MS = 1000;

float voltage = NAN;
LowPassFilter<float> voltageFilter(0.2);
int voltagePin = A0; // XIAO ESP32-S3 A0 is physical GPIO1
float voltageScale = 2;
// Three propeller-on hover records on the standard airframe gave a crossover
// near 3.28 V and a normalized slope of 0.472 per volt. PWR_COMP_MAX is a
// symmetric factor bound: 1.20 permits 1/1.20..1.20, while 1.00 disables the
// compensation exactly and preserves the legacy ALT_HOVER path.
float voltageCompensationReference = 3.28f;
float voltageCompensationSlope = 0.472f;
float voltageCompensationMax = 1.20f;
uint32_t voltageAdcMilliVolts = 0;
uint32_t voltageLastValidMs = 0;
int configuredVoltagePin = -2;

void resetVoltageMeasurement() {
	voltage = NAN;
	voltageAdcMilliVolts = 0;
	voltageLastValidMs = 0;
	voltageFilter.reset();
}

void configureVoltageInput() {
	if (configuredVoltagePin >= 0 && configuredVoltagePin != voltagePin) {
		pinMode(configuredVoltagePin, INPUT); // detach the previous ADC owner
	}
	configuredVoltagePin = voltagePin;
	resetVoltageMeasurement();
	if (voltagePin < 0) return;

	pinMode(voltagePin, INPUT);
	analogSetPinAttenuation(voltagePin, ADC_11db);
}

void setupPower() {
	configureVoltageInput();
}

bool voltageAvailable() {
	return voltagePin >= 0 && isfinite(voltage) && voltageLastValidMs != 0 &&
		millis() - voltageLastValidMs <= VOLTAGE_STALE_MS;
}

float voltageThrustCompensationFactor() {
	if (!voltageAvailable() || voltageCompensationMax <= 1.0f) return 1.0f;
	float factor = 1.0f + voltageCompensationSlope *
		(voltageCompensationReference - voltage);
	return constrain(factor, 1.0f / voltageCompensationMax,
		voltageCompensationMax);
}

void readVoltage() {
	if (configuredVoltagePin != voltagePin) configureVoltageInput();
	if (voltagePin < 0) return;
	static Rate rate(10);
	if (!rate) return;

	voltageAdcMilliVolts = analogReadMilliVolts(voltagePin);
	float sample = voltageAdcMilliVolts * voltageScale / 1000.0f;
	if (!isfinite(sample) || sample < VOLTAGE_MIN_PLAUSIBLE || sample > VOLTAGE_MAX_PLAUSIBLE) return;

	voltage = voltageFilter.update(sample);
	voltageLastValidMs = millis();
}
