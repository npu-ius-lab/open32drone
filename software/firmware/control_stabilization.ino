// Attitude, angular-rate, and motor-mixer control.

void controlAttitude() {
	if (!armed || attitudeTarget.invalid() || thrustTarget < 0.1) {
		ratesTarget.invalidate();
		return;
	}

	const Vector up(0, 0, 1);
	Vector upActual = attitudeBodyUp;
	Vector upTarget = Quaternion::rotateVector(up, attitudeTarget);
	Vector error = Vector::rotationVectorBetween(upTarget, upActual);

	ratesTarget.x = constrain(rollPID.update(error.x) + ratesExtra.x, -maxRate.x, maxRate.x);
	ratesTarget.y = constrain(pitchPID.update(error.y) + ratesExtra.y, -maxRate.y, maxRate.y);
	float yawError = wrapAngle(attitudeTarget.getYaw() - attitudeEuler.z);
	ratesTarget.z = constrain(yawPID.update(yawError) + ratesExtra.z, -maxRate.z, maxRate.z);
}

void controlRates() {
	if (!armed || ratesTarget.invalid() || thrustTarget < 0.1) {
		torqueTarget.invalidate();
		return;
	}

	Vector error = ratesTarget - rates;
	torqueTarget.x = rollRatePID.update(error.x);
	torqueTarget.y = pitchRatePID.update(error.y);
	torqueTarget.z = yawRatePID.update(error.z);
}

void controlTorque() {
	if (!armed) {
		mixerScale = 1.0f;
		memset(motors, 0, sizeof(motors));
		rollRatePID.reset(); pitchRatePID.reset(); yawRatePID.reset();
		rollPID.reset(); pitchPID.reset(); yawPID.reset();
		return;
	}

	if (!isfinite(thrustTarget)) {
		forceDisarm("invalid control output");
		return;
	}

	if (thrustTarget < 0.1) {
		mixerScale = 1.0f;
		for (int i = 0; i < 4; i++) motors[i] = 0.1f;
		return;
	}
	if (!torqueTarget.valid()) {
		forceDisarm("invalid control output");
		return;
	}

	float delta[4];
	delta[MOTOR_FRONT_LEFT] = torqueTarget.x - torqueTarget.y + torqueTarget.z;
	delta[MOTOR_FRONT_RIGHT] = -torqueTarget.x - torqueTarget.y - torqueTarget.z;
	delta[MOTOR_REAR_LEFT] = torqueTarget.x + torqueTarget.y - torqueTarget.z;
	delta[MOTOR_REAR_RIGHT] = -torqueTarget.x + torqueTarget.y + torqueTarget.z;

	float scale = 1.0f;
	for (int i = 0; i < 4; i++) {
		if (delta[i] > 0.0f) scale = min(scale, (1.0f - thrustTarget) / delta[i]);
		if (delta[i] < 0.0f) scale = min(scale, (thrustTarget - 0.1f) / -delta[i]);
	}
	mixerScale = constrain(scale, 0.0f, 1.0f);
	for (int i = 0; i < 4; i++) {
		motors[i] = constrain(thrustTarget + delta[i] * mixerScale, 0.1f, 1.0f);
	}
	if (mixerScale < 0.999f) {
		Vector error = ratesTarget.valid() ? ratesTarget - rates : Vector();
		rollRatePID.undoIntegral(error.x, dt);
		pitchRatePID.undoIntegral(error.y, dt);
		yawRatePID.undoIntegral(error.z, dt);
	}
}
