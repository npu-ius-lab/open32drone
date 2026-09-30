// Offboard setpoint staging, activation, and shared ToF readiness gates.

void clearOffboardLocalControl() {
	offboardLocalActive = false;
	offboardUsePositionXY = false;
	offboardUseVelocityXY = false;
	offboardUseAltitude = false;
	offboardUseVerticalSpeed = false;
}

void clearOffboardSetpointStaging() {
	stagedOffboardKind = OFFBOARD_SETPOINT_NONE;
	stagedOffboardStart = 0.0f;
	stagedOffboardLast = 0.0f;
	stagedOffboardSamples = 0;
	stagedOffboardThrust = 0.0f;
	stagedOffboardAttitudeIgnored = false;
	stagedOffboardUsePositionXY = false;
	stagedOffboardUseVelocityXY = false;
	stagedOffboardUseAltitude = false;
	stagedOffboardUseVerticalSpeed = false;
	stagedOffboardUseYaw = false;
	stagedOffboardYawRate = 0.0f;
}

void recordOffboardSetpoint(int kind) {
	bool restart = stagedOffboardKind != kind || stagedOffboardLast <= 0.0f ||
		t - stagedOffboardLast > offboardWarmupMaxGap;
	if (restart) {
		clearOffboardSetpointStaging();
		stagedOffboardKind = kind;
		stagedOffboardStart = t;
		stagedOffboardSamples = 1;
	} else if (stagedOffboardSamples < UINT16_MAX) {
		stagedOffboardSamples++;
	}
	stagedOffboardLast = t;
}

bool offboardSetpointPending() {
	return stagedOffboardKind != OFFBOARD_SETPOINT_NONE && stagedOffboardLast > 0.0f &&
		t - stagedOffboardLast <= offboardWarmupMaxGap;
}

bool offboardSetpointStreamReady() {
	float duration = (float)(t - stagedOffboardStart);
	float rate = duration > 0.0f && stagedOffboardSamples > 1 ?
		(stagedOffboardSamples - 1) / duration : 0.0f;
	return offboardSetpointPending() && duration >= offboardWarmupTime &&
		stagedOffboardSamples >= 5 && rate >= offboardWarmupMinRate;
}

bool tofRangeFresh() {
	return tofHealthy && tofTimestamp != 0 && millis() - tofTimestamp <= 150;
}

bool tofPacketFresh() {
	return tofPacketHealthy && tofPacketTimestamp != 0 &&
		millis() - tofPacketTimestamp <= 150;
}

bool tofGroundReady() {
	return tofRangeFresh() || (assistedTakeoffGroundIdle && !flightWasAirborne &&
		tofPacketFresh() && tofRangeInBlindZone);
}

bool offboardLocalSensorsReady() {
	return tofRangeFresh() &&
		opticalFlowHealthy && flowCtrlUsingFlow && flowBiasReady &&
		flowPositionGateOpen && flowAirborne;
}

void applyStagedOffboardSetpoint() {
	if (stagedOffboardKind == OFFBOARD_SETPOINT_ATTITUDE) {
		clearOffboardLocalControl();
		ratesTarget = stagedOffboardRates;
		attitudeTarget = stagedOffboardAttitude;
		if (stagedOffboardAttitudeIgnored) attitudeTarget.invalidate();
		thrustTarget = stagedOffboardThrust;
		ratesExtra = Vector(0, 0, 0);
		actuatorOwner = ACTUATOR_OFFBOARD_ATTITUDE;
	} else if (stagedOffboardKind == OFFBOARD_SETPOINT_LOCAL) {
		offboardUsePositionXY = stagedOffboardUsePositionXY;
		offboardUseVelocityXY = stagedOffboardUseVelocityXY;
		offboardUseAltitude = stagedOffboardUseAltitude;
		offboardUseVerticalSpeed = stagedOffboardUseVerticalSpeed;
		offboardTargetX = stagedOffboardTargetX;
		offboardTargetY = stagedOffboardTargetY;
		offboardTargetZ = stagedOffboardTargetZ;
		offboardTargetVX = stagedOffboardTargetVX;
		offboardTargetVY = stagedOffboardTargetVY;
		offboardTargetVZ = stagedOffboardTargetVZ;
		if (stagedOffboardUseYaw) {
			attitudeTarget = Quaternion::fromEuler(Vector(0, 0, stagedOffboardYaw));
		} else if (abs(stagedOffboardYawRate) > 0.001f) {
			// A pure yaw-rate command must not fight the captured heading through
			// yawPID. Refresh the reference on every setpoint so ratesExtra remains
			// the commanded rate; once the rate returns to zero, the last heading is
			// retained normally.
			attitudeTarget = Quaternion::fromEuler(Vector(0, 0, attitudeEuler.z));
		}
		ratesExtra = Vector(0, 0, stagedOffboardYawRate);
		offboardLocalActive = true;
		actuatorOwner = ACTUATOR_OFFBOARD_LOCAL;
	}
}

bool activateStagedOffboardControl() {
	if (!offboardSetpointStreamReady()) return false;
	if (stagedOffboardKind == OFFBOARD_SETPOINT_ATTITUDE && stagedOffboardThrust < 0.10f) return false;
	if (stagedOffboardKind == OFFBOARD_SETPOINT_LOCAL && !offboardLocalSensorsReady()) return false;
	applyStagedOffboardSetpoint();
	offboardControlTime = t;
	offboardActive = true;
	offboardFailsafeActive = false;
	return true;
}
