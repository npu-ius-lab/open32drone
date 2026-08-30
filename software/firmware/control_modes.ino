// Pilot mode interpretation, control ownership, and assisted-stick actions.

void interpretControls() {
	// Physical-RC emergency stop is an independent, highest-priority path.  It
	// must remain available while AUTO or Offboard owns the ordinary controls.
	if (armed && rcEmergencyDisarmRequested()) {
		forceDisarm("RC emergency gesture");
		return;
	}
	// A powered or noisy receiver is not sufficient to steal a GCS-armed flight.
	// Transfer ordinary ownership only after a deliberate, sustained stick input.
	if (armed && mavlinkArmSession && rcArmedTakeoverRequested()) mavlinkArmSession = false;
	if ((offboardActive || autoFlightActive()) && mode == AUTO) return;
	if (externalModeOverride && rcPilotActive && isfinite(controlMode)) {
		// A mode selected while RC was absent yields no reference switch value.
		// Let a newly connected physical transmitter take over immediately.
		if (!isfinite(externalModeControlValue) ||
			abs(controlMode - externalModeControlValue) > 0.15f) externalModeOverride = false;
	}
	// Only a live physical receiver may map its switch into a flight mode. A
	// powered-off receiver can leave a stale zero in controlMode; applying that
	// value after a GCS takeoff silently rewrites POS_HOLD to STAB.
	if (!externalModeOverride && rcPilotActive && isfinite(controlMode)) {
		if (controlMode < 0.25) mode = flightModes[0];
		else if (controlMode <= 0.75) mode = flightModes[1];
		else if (controlMode > 0.75) mode = flightModes[2];
	}

	if (mode == AUTO) return; // pilot is not effective in AUTO mode

	if (controlThrottle < 0.05 && controlYaw > 0.95 && !armed) requestArm("RC gesture");
	if (armed) actuatorOwner = ACTUATOR_PILOT;
	if (armed && flowAirborne) {
		assistedTakeoffGroundIdle = false;
		flightWasAirborne = true;
	}
	// Direct-throttle STAB flight has no automatic takeoff transition. Once the
	// pilot commands meaningful thrust, stop advertising the armed aircraft as
	// assisted-ground-idle; the independent airborne latch is still set only by
	// measured flight evidence.
	if (armed && mode == STAB && controlThrottle > 0.12f) {
		assistedTakeoffGroundIdle = false;
	}
	updatePilotVerticalFlightCommands();
	if (autoFlightActive() && mode == AUTO) return;

	if (abs(controlYaw) < 0.1) controlYaw = 0; // yaw dead zone
	if (abs(controlRoll) < 0.05) controlRoll = 0; // roll dead zone（消除起飞微偏漂移）
	if (abs(controlPitch) < 0.05) controlPitch = 0; // pitch dead zone（消除起飞微偏漂移）

	// STAB keeps direct throttle. Assisted modes keep idle on the ground until
	// the takeoff trigger, then use hover thrust plus the altitude controller.
	if (mode == STAB) thrustTarget = controlThrottle;
	if (mode == ALT_HOLD || mode == POS_HOLD) {
		thrustTarget = assistedTakeoffGroundIdle ? 0.0f : altitudeHoverFeedForward();
	}

	if (mode == STAB || mode == ALT_HOLD || mode == POS_HOLD) {
		float yawTarget = attitudeTarget.getYaw();
		if (!armed || invalid(yawTarget) || controlYaw != 0) yawTarget = attitudeEuler.z; // reset yaw target
		attitudeTarget = Quaternion::fromEuler(Vector(
			controlRoll * tiltMax,
			controlPitch * tiltMax,
			yawTarget));
		ratesExtra = Vector(0, 0, -controlYaw * maxRate.z); // positive yaw stick means clockwise rotation in FLU
	}
}

void updatePilotVerticalFlightCommands() {
	static Delay takeoffTriggerDelay(0.20f);
	static Delay landingTriggerDelay(0.30f);
	static uint32_t lastTakeoffBlockedPrintMs = UINT32_MAX;
	bool assistedMode = mode == ALT_HOLD || mode == POS_HOLD;
	// Only a live pilot input may turn stick positions into takeoff/landing
	// commands. A GCS-only MAVLink arm deliberately seeds throttle to zero; once
	// an automatic takeoff hands back to POS_HOLD, that neutral placeholder must
	// not be mistaken for a pilot requesting an immediate landing.
	bool commandEligible = armed && assistedMode && !offboardActive &&
		!autoFlightActive() && pilotControlFresh();

	if (!commandEligible) {
		takeoffTriggerDelay.update(false);
		landingTriggerDelay.update(false);
		return;
	}

	bool groundTofReady = tofGroundReady();
	if (assistedTakeoffGroundIdle) {
		landingTriggerDelay.update(false);
		bool throttleRequested = controlThrottle >= altitudeTakeoffTrigger;
		if (throttleRequested && !groundTofReady &&
			(lastTakeoffBlockedPrintMs == UINT32_MAX || millis() - lastTakeoffBlockedPrintMs >= 1000)) {
			lastTakeoffBlockedPrintMs = millis();
			print("Takeoff blocked: no fresh ToF packet (use STAB or check flow)\n");
		}
		bool trigger = groundTofReady && throttleRequested;
		if (takeoffTriggerDelay.update(trigger)) {
			startPilotAssistedTakeoff(mode);
		}
		return;
	}

	takeoffTriggerDelay.update(false);
	// flowAirborne intentionally clears near the floor, so it cannot qualify a
	// landing request. Use the per-arm airborne latch to preserve that evidence.
	bool landRequest = flightWasAirborne && controlThrottle < 0.05f;
	if (landingTriggerDelay.update(landRequest)) {
		autoFlightReturnMode = assistedMode ? mode : ALT_HOLD;
		startPilotAutomaticLanding();
	}
}

bool requestExternalMode(int requestedMode) {
	externalModeFailure = "none";
	if (requestedMode != STAB && requestedMode != AUTO &&
		requestedMode != ALT_HOLD && requestedMode != POS_HOLD) {
		externalModeFailure = "unsupported mode";
		return false;
	}
	if (requestedMode == AUTO && !autoFlightActive() && !offboardActive) {
		if (!offboardSetpointPending()) {
			externalModeFailure = "offboard stream missing";
			return false;
		}
		if (!offboardSetpointStreamReady()) {
			externalModeFailure = "offboard warmup incomplete";
			return false;
		}
		if (stagedOffboardKind == OFFBOARD_SETPOINT_ATTITUDE && stagedOffboardThrust < 0.10f) {
			externalModeFailure = "offboard thrust unsafe";
			return false;
		}
		if (stagedOffboardKind == OFFBOARD_SETPOINT_LOCAL && !offboardLocalSensorsReady()) {
			externalModeFailure = "position control unavailable";
			return false;
		}
		if (!activateStagedOffboardControl()) {
			externalModeFailure = "offboard activation failed";
			return false;
		}
	}
	bool assistedMode = requestedMode == ALT_HOLD || requestedMode == POS_HOLD;
	bool tofFresh = tofRangeFresh();
	bool assistedGroundReady = assistedTakeoffGroundIdle && tofGroundReady();
	if (armed && assistedMode && !tofFresh && !assistedGroundReady) {
		externalModeFailure = "ToF unavailable";
		return false;
	}
	if (requestedMode != AUTO && autoFlightActive()) {
		resetAutomaticLandingFlare();
		autoFlightPhase = AUTO_FLIGHT_IDLE;
		autoFlightPilotTriggered = false;
		autoFlightSource = AUTO_SOURCE_NONE;
		autoTakeoffTargetValid = false;
		autoLandingTargetValid = false;
		altitudeHoldEngaged = false;
	}
	if (requestedMode != AUTO && (offboardActive || offboardLocalActive || offboardSetpointPending())) {
		releaseOffboardControl();
		actuatorOwner = ACTUATOR_PILOT;
	}
	mode = requestedMode;
	externalModeOverride = true;
	externalModeControlValue = controlMode;
	return true;
}

const char* getModeName() {
	switch (mode) {
		case STAB: return "STAB";
		case ALT_HOLD: return "ALT_HOLD";
		case POS_HOLD: return "POS_HOLD";
		case AUTO: return "AUTO";
		default: return "UNKNOWN";
	}
}
