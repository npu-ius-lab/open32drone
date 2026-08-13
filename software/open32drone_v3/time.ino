// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Time related functions

float t = NAN; // current time, s
float dt; // time delta with the previous step, s
float loopRate; // Hz
float loopDtAverageMs;
float loopDtMaxMs;
uint32_t loopOverrunCount;

void step() {
	float now = micros() / 1000000.0;
	dt = now - t;
	t = now;

	if (!(dt > 0)) {
		dt = 0; // assume dt to be zero on first step and on reset
	}

	computeLoopRate();
}

void computeLoopRate() {
	static float windowStart = 0;
	static uint32_t rate = 0;
	static float dtSum = 0.0f;
	static float dtMax = 0.0f;
	static uint32_t overruns = 0;
	rate++;
	if (dt > 0.0f) {
		dtSum += dt;
		dtMax = max(dtMax, dt);
		if (dt > 0.005f) overruns++; // control loop slower than 200 Hz
	}
	if (t - windowStart >= 1) { // 1 second window
		loopRate = rate;
		loopDtAverageMs = rate > 0 ? dtSum * 1000.0f / rate : 0.0f;
		loopDtMaxMs = dtMax * 1000.0f;
		loopOverrunCount = overruns;
		windowStart = t;
		rate = 0;
		dtSum = 0.0f;
		dtMax = 0.0f;
		overruns = 0;
	}
}
