// ToF altitude and vertical-speed control.

float altitudeHoldTarget = 0.0f;
bool altitudeHoldEngaged = false;
float altitudeHoldIntegral = 0.0f;
float altitudeHoldCorrection = 0.0f;
uint32_t altitudeHoldLastSequence = 0;
int altitudeHoldRejectReason = 0;

float altitudeHoldP = 0.747f;
float altitudeHoldI = 0.10f;
float altitudeHoldD = 0.20f;
float altitudeHoldIntegralLimit = 0.30f;
float altitudeHoldMaxCorrection = 0.25f;
float altitudeHoldMaxClimbRate = 0.45f;
float altitudeStickDeadband = 0.10f;
float altitudeHoverThrust = 0.49f;
const float altitudeStickCenter = 0.50f;
const float automaticTakeoffHoverFeedForwardCap = 0.49f;
const float altitudeHoverFeedForwardSlew = 0.05f; // normalized thrust per second
float altitudeHoverFeedForwardEffective = 0.49f;

float getHoldAltitude() {
	return position.z;
}

bool altitudeVoltageCompensationEnabled() {
	return voltagePin >= 0 && voltageCompensationMax > 1.0f;
}

float altitudeHoverFeedForwardTarget() {
	return constrain(altitudeHoverThrust * voltageThrustCompensationFactor(),
		0.0f, 1.0f);
}

void updateAltitudeHoverFeedForward() {
	// Disabled/missing hardware is the proven legacy path. Keep it exact rather
	// than filtering it so a no-divider aircraft behaves exactly as before.
	if (!altitudeVoltageCompensationEnabled()) {
		altitudeHoverFeedForwardEffective = altitudeHoverThrust;
		return;
	}

	float target = altitudeHoverFeedForwardTarget();
	// The fitted low-voltage hover value is intentionally not exposed during
	// automatic launch. The existing 0.49 takeoff feed-forward is flight-proven;
	// after handover the effective value slews to the loaded-voltage target.
	if (!armed || assistedTakeoffGroundIdle || autoFlightPhase == AUTO_TAKEOFF) {
		target = min(target, automaticTakeoffHoverFeedForwardCap);
	}
	if (!armed) {
		altitudeHoverFeedForwardEffective = target;
		return;
	}

	float maximumStep = altitudeHoverFeedForwardSlew * max(dt, 0.0f);
	altitudeHoverFeedForwardEffective += constrain(
		target - altitudeHoverFeedForwardEffective,
		-maximumStep,
		maximumStep);
}

float altitudeHoverFeedForward() {
	return constrain(altitudeHoverFeedForwardEffective, 0.0f, 1.0f);
}

void updateAltitudeHoldControl() {
	bool manualHoldRequested = (mode == ALT_HOLD || mode == POS_HOLD) && !assistedTakeoffGroundIdle;
	bool holdRequested = manualHoldRequested || (mode == AUTO && (autoAltitudeActive() || offboardLocalActive));
	if (!armed || !holdRequested) {
		altitudeHoldEngaged = false;
		altitudeHoldCorrection = 0.0f;
		altitudeHoldLastSequence = 0;
		altitudeHoldRejectReason = 1;
		return;
	}
	altitudeHoldRejectReason = 0;
	if (mode == AUTO && autoLandingFlareCutActive()) {
		altitudeHoldEngaged = false;
		altitudeHoldCorrection = 0.0f;
		altitudeHoldRejectReason = 4;
		return;
	}

	bool takeoffGroundValid = mode == AUTO && autoFlightPhase == AUTO_TAKEOFF &&
		opticalFlowHeight > FLOW_SENSOR_MIN_HEIGHT;
	bool tofValid = tofHealthy && tofTimestamp != 0 && millis() - tofTimestamp <= 150 &&
		(takeoffGroundValid || position.z > 0.05f) && position.z < 6.0f;
	if (!altitudeHoldEngaged && tofValid) {
		altitudeHoldTarget = autoAltitudeActive() ? autoFlightTargetHeight :
			(offboardLocalActive && offboardUseAltitude ? offboardTargetZ : getHoldAltitude());
		altitudeHoldEngaged = true;
		altitudeHoldIntegral = 0.0f;
		altitudeHoldCorrection = 0.0f;
		altitudeHoldLastSequence = tofSequence;
		print("Alt hold engaged, target: %.2fm\n", altitudeHoldTarget);
	}

	// Preserve the target through a short ToF dropout and fade the last correction.
	if (!tofValid) {
		altitudeHoldRejectReason = 3;
		altitudeHoldCorrection *= expf(-max(dt, 0.0f) / 0.50f);
		if (abs(altitudeHoldCorrection) < 0.002f) altitudeHoldCorrection = 0.0f;
		thrustTarget = constrain(thrustTarget + altitudeHoldCorrection, 0.0f, 1.0f);
		return;
	}

	// Advance the controller only on a new ToF sample and hold output in between.
	float error = altitudeHoldTarget - getHoldAltitude();
	if (tofSequence != altitudeHoldLastSequence) {
		altitudeHoldLastSequence = tofSequence;
		float dtAlt = constrain(tofSampleDt, 0.01f, 0.20f);
		if (autoAltitudeActive()) {
			altitudeHoldTarget = autoFlightTargetHeight;
		} else if (offboardLocalActive) {
			if (offboardUseAltitude) altitudeHoldTarget = offboardTargetZ;
			else if (offboardUseVerticalSpeed) altitudeHoldTarget = constrain(
				altitudeHoldTarget + offboardTargetVZ * dtAlt, 0.05f, 5.80f);
		} else if (pilotControlFresh()) {
			float lowerEdge = altitudeStickCenter - altitudeStickDeadband;
			float upperEdge = altitudeStickCenter + altitudeStickDeadband;
			float command = 0.0f;
			if (controlThrottle < lowerEdge) command = -(lowerEdge - controlThrottle) / max(lowerEdge, 0.05f);
			if (controlThrottle > upperEdge) command = (controlThrottle - upperEdge) / max(1.0f - upperEdge, 0.05f);
			altitudeHoldTarget += constrain(command, -1.0f, 1.0f) * altitudeHoldMaxClimbRate * dtAlt;
		}
		error = altitudeHoldTarget - getHoldAltitude();
		altitudeHoldIntegral = constrain(
			altitudeHoldIntegral + error * dtAlt,
			-altitudeHoldIntegralLimit,
			altitudeHoldIntegralLimit);
		float derivative = -velocity.z;
		altitudeHoldCorrection = constrain(
			error * altitudeHoldP + altitudeHoldIntegral * altitudeHoldI + derivative * altitudeHoldD,
			-altitudeHoldMaxCorrection,
			altitudeHoldMaxCorrection);
	}

	float verticalThrust = altitudeHoverFeedForward() + altitudeHoldCorrection;
	float verticalFactor = constrain(abs(attitudeBodyUp.z), 0.85f, 1.0f);
	thrustTarget = verticalThrust / verticalFactor;
	if (mode == AUTO && autoFlightPhase == AUTO_TAKEOFF) {
		thrustTarget = min(thrustTarget, autoTakeoffThrustLimit);
	}
	thrustTarget = constrain(thrustTarget, 0.0f, 1.0f);
}
