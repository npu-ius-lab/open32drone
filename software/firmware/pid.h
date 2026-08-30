// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// PID controller implementation

#pragma once

#include "lpf.h"

class PID {
public:
	float p, i, d;
	float windup;
	float dtMax;

	float derivative = 0;
	float integral = 0;

	LowPassFilter<float> lpf; // low pass filter for derivative term

	PID(float p, float i, float d, float windup = 0, float dAlpha = 1, float dtMax = 0.1) :
		p(p), i(i), d(d), windup(windup), dtMax(dtMax), lpf(dAlpha) {}

	float update(float error) {
		float dt = (float)(t - prevTime);

		if (dt > 0 && dt < dtMax) {
			integral += error * dt;
			if (i != 0.0f && windup > 0.0f) integral = constrain(integral, -windup / abs(i), windup / abs(i));
			derivative = lpf.update((error - prevError) / dt); // compute derivative and apply low-pass filter
		} else {
			integral = 0;
			derivative = 0;
		}

		prevError = error;
		prevTime = t;

		return p * error + constrain(i * integral, -windup, windup) + d * derivative; // PID
	}

	void undoIntegral(float error, float dt) {
		if (dt > 0.0f && dt < dtMax) integral -= error * dt;
	}

	void reset() {
		prevError = NAN;
		prevTime = NAN;
		integral = 0;
		derivative = 0;
		lpf.reset();
	}

private:
	float prevError = NAN;
	double prevTime = NAN;
};
