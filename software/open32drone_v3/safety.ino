// Copyright (c) 2024 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Fail-safe functions

// Configurable timeouts (params SF_RC_LOSS_TIME / SF_DESCEND_TIME)
float rcLossTimeout = 1;
float descendTime = 10;
bool rcFailsafeActive = false;
float rcFailsafeThrust = 0.0f;
bool offboardActive = false;
float offboardControlTime = 0.0f;
const float offboardTimeout = 0.30f;

extern float controlTime;
extern float controlRoll, controlPitch, controlThrottle, controlYaw;

void failsafe() {
	rcLossFailsafe();
	offboardFailsafe();
	autoFailsafe();
}

void offboardFailsafe() {
	if (!offboardActive) return;
	if (mode != AUTO || !armed) {
		offboardActive = false;
		return;
	}
	if (t - offboardControlTime <= offboardTimeout) return;
	offboardActive = false;
	armed = false;
	thrustTarget = 0.0f;
	memset(motors, 0, sizeof(motors));
}

// RC loss failsafe
void rcLossFailsafe() {
	if (controlTime == 0) return; // no RC at all
	if (!armed) {
		rcFailsafeActive = false;
		return;
	}
	if (t - controlTime <= rcLossTimeout) {
		rcFailsafeActive = false;
		return;
	}
	if (!rcFailsafeActive) {
		rcFailsafeActive = true;
		rcFailsafeThrust = constrain(thrustTarget, 0.0f, 1.0f);
	}
	descend();
}

// Smooth descend on RC lost
void descend() {
	mode = AUTO;
	attitudeTarget = Quaternion();
	float safeDescendTime = max(descendTime, 0.1f);
	rcFailsafeThrust -= dt / safeDescendTime;
	thrustTarget = max(rcFailsafeThrust, 0.0f);
	if (rcFailsafeThrust <= 0) {
		armed = false;
		rcFailsafeActive = false;
	}
}

// Allow pilot to interrupt automatic flight
void autoFailsafe() {
	static float roll, pitch, yaw, throttle;
	if (rcFailsafeActive) return; // stale stick values must not cancel RC-loss descent
	if (roll != controlRoll || pitch != controlPitch || yaw != controlYaw || abs(throttle - controlThrottle) > 0.05) {
		// controls changed
		if (mode == AUTO) mode = STAB; // regain control by the pilot
	}
	roll = controlRoll;
	pitch = controlPitch;
	yaw = controlYaw;
	throttle = controlThrottle;
}
