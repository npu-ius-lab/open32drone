// Automatic takeoff and landing lifecycle.
// This controller owns vertical motion only; live pilot attitude remains available.

bool beginAutomaticTakeoff(float targetHeight, bool pilotTriggered, int returnMode) {
	automaticFlightFailure = "none";
	if (autoFlightActive()) {
		automaticFlightFailure = "automatic flight already active";
		return false;
	}
	// Takeoff is a management action, not a continuous offboard setpoint. Do not
	// silently steal a live ROS/controller lease; the caller can stop offboard or
	// select a pilot mode first, then retry.
	if (offboardActive || offboardLocalActive || offboardSetpointPending()) {
		automaticFlightFailure = "offboard control active";
		print("Takeoff rejected: %s\n", automaticFlightFailure);
		return false;
	}
	if (!tofGroundReady()) {
		automaticFlightFailure = "ToF unavailable";
		print("Takeoff rejected: ToF unavailable\n");
		return false;
	}
	if (!armed && !requestArm(pilotTriggered ? "automatic takeoff" : "MAVLink takeoff", pilotTriggered)) {
		automaticFlightFailure = preArmFailure;
		return false;
	}
	// Once a takeoff or airborne session clears ground-idle, it cannot be started
	// again before disarm. Do not depend on the instantaneous airborne detector,
	// which deliberately clears near the floor and during some sensor failures.
	if (armed && !assistedTakeoffGroundIdle) {
		automaticFlightFailure = "aircraft already airborne";
		return false;
	}
	clearOffboardLocalControl();
	resetAutomaticLandingFlare();
	autoFlightGroundHeight = position.z;
	// A blind-zone takeoff is valid, but it must not inherit a launch-surface
	// range from an earlier arm cycle. That stale range would move the landing
	// contact/flare thresholds on the next flight.
	autoFlightGroundRange = tofRangeFresh() && opticalFlowHeight > FLOW_SENSOR_MIN_HEIGHT ?
		opticalFlowHeight : 0.0f;
	autoFlightGoalHeight = min(autoFlightGroundHeight + max(targetHeight, 0.20f), 5.80f);
	// One controller owns the whole takeoff. Its target starts at the measured
	// launch surface and advances continuously toward the requested height.
	autoFlightTargetHeight = autoFlightGroundHeight;
	autoTakeoffThrustLimit = 0.20f;
	altitudeHoldEngaged = false;
	altitudeHoldIntegral = 0.0f;
	autoFlightYaw = attitudeEuler.z;
	autoFlightPhaseStart = t;
	autoFlightPhase = AUTO_TAKEOFF;
	autoFlightPilotTriggered = pilotTriggered;
	autoFlightSource = pilotTriggered ? AUTO_SOURCE_PILOT : AUTO_SOURCE_MAVLINK;
	autoFlightReturnMode = returnMode == POS_HOLD ? POS_HOLD : ALT_HOLD;
	autoTakeoffTargetX = position.x;
	autoTakeoffTargetY = position.y;
	autoTakeoffTargetValid = autoFlightReturnMode == POS_HOLD;
	autoLandingTargetValid = false;
	autoTakeoffControlTimeAtStart = controlTime;
	assistedTakeoffGroundIdle = false;
	mode = AUTO;
	externalModeControlValue = controlMode;
	actuatorOwner = ACTUATOR_AUTONOMOUS;
	print("Automatic takeoff goal: %.2fm (ground %.2fm, thrust cap %.0f%%)\n",
		autoFlightGoalHeight, autoFlightGroundHeight, altitudeTakeoffMaxThrust * 100.0f);
	return true;
}

bool startAutomaticTakeoff(float targetHeight) {
	// MAVLink TAKEOFF is the one-key Position takeoff used by Android and ROS.
	// Its result must not depend on whichever standby mode happened to be shown
	// before the command (for example ALT_HOLD after returning from Wi-Fi setup).
	return beginAutomaticTakeoff(targetHeight, false, POS_HOLD);
}

bool startPilotAssistedTakeoff(int returnMode) {
	if (!armed || !assistedTakeoffGroundIdle) return false;
	return beginAutomaticTakeoff(altitudeTakeoffHeight, true, returnMode);
}

bool beginAutomaticLanding(int source) {
	automaticFlightFailure = "none";
	if (!armed) {
		automaticFlightFailure = "aircraft not armed";
		return false;
	}
	// LAND is idempotent. A safety monitor may repeat the command while descent
	// is already active; restarting here would recapture the current XY point and
	// height, producing a visible pause/bounce and abandoning the original
	// landing point.
	if (autoFlightPhase == AUTO_LAND_DESCEND || autoFlightPhase == AUTO_LAND_FLARE) {
		print("Automatic landing already active\n");
		return true;
	}
	// When an altitude controller already owns vertical motion, preserve its
	// lower target and integral across the LAND ownership change. Re-capturing
	// position.z here raised the target after a pilot-commanded descent and
	// produced a visible collective step. A stale target from STAB/direct
	// attitude control must not be reused.
	bool preserveAltitudeControl = altitudeHoldEngaged &&
		(mode == ALT_HOLD || mode == POS_HOLD ||
			(mode == AUTO && (autoAltitudeActive() || offboardLocalActive)));
	float landingTargetHeight = max(position.z - 0.02f, 0.05f);
	if (preserveAltitudeControl && isfinite(altitudeHoldTarget)) {
		landingTargetHeight = min(landingTargetHeight,
			constrain(altitudeHoldTarget, 0.05f, 5.80f));
	}
	// Landing is safety-critical and is allowed to override an active offboard
	// stream. Release the complete lease so ROS can reconnect cleanly afterwards.
	releaseOffboardControl();
	resetAutomaticLandingFlare();
	if (mode == STAB || mode == ALT_HOLD || mode == POS_HOLD) autoFlightReturnMode = mode;
	if (!preserveAltitudeControl) {
		altitudeHoldEngaged = false;
		altitudeHoldIntegral = 0.0f;
	}
	autoFlightTargetHeight = landingTargetHeight;
	altitudeHoldTarget = landingTargetHeight;
	autoFlightYaw = attitudeEuler.z;
	autoFlightPhaseStart = t;
	autoFlightPhase = AUTO_LAND_DESCEND;
	autoFlightPilotTriggered = false;
	autoFlightSource = (AutoFlightSource)source;
	autoLandingRelockPending = autoFlightReturnMode == POS_HOLD;
	autoLandingTargetX = position.x;
	autoLandingTargetY = position.y;
	autoLandingTargetValid = autoLandingRelockPending;
	autoTakeoffTargetValid = false;
	mode = AUTO;
	externalModeControlValue = controlMode;
	actuatorOwner = ACTUATOR_AUTONOMOUS;
	print("Automatic landing started at %.2fm\n", position.z);
	return true;
}

bool startAutomaticLanding() {
	return beginAutomaticLanding(AUTO_SOURCE_MAVLINK);
}

bool startPilotAutomaticLanding() {
	return beginAutomaticLanding(AUTO_SOURCE_PILOT);
}

void cancelAutomaticFlight(const char *reason) {
	if (!autoFlightActive()) return;
	int returnMode = autoFlightReturnMode;
	resetAutomaticLandingFlare();
	autoFlightPhase = AUTO_FLIGHT_IDLE;
	autoFlightPilotTriggered = false;
	autoFlightSource = AUTO_SOURCE_NONE;
	autoTakeoffTargetValid = false;
	autoLandingTargetValid = false;
	altitudeHoldEngaged = false;
	mode = returnMode;
	actuatorOwner = ACTUATOR_PILOT;
	print("Automatic flight cancelled: %s\n", reason);
}

void handOverAssistedTakeoff(const char *reason, bool preserveTakeoffGoal) {
	float handoverAltitude = preserveTakeoffGoal ? autoFlightGoalHeight : position.z;
	bool pilotTriggered = autoFlightPilotTriggered;
	resetAutomaticLandingFlare();
	autoFlightPhase = AUTO_FLIGHT_IDLE;
	autoFlightPilotTriggered = false;
	autoFlightSource = AUTO_SOURCE_NONE;
	autoTakeoffTargetValid = false;
	mode = autoFlightReturnMode;
	// A MAVLink one-key takeoff must remain in its requested assisted mode after
	// handover. Only a takeoff initiated by the physical pilot immediately follows
	// the RC mode switch. A newly active RC still releases this override through
	// the normal deliberate-takeover path in interpretControls().
	externalModeOverride = !pilotTriggered;
	externalModeControlValue = rcPilotActive && isfinite(controlMode) ? controlMode : NAN;
	// A completed automatic takeoff must keep the requested relative height.
	// Capturing the instantaneous height here made a fresh battery's residual
	// climb become the new hold target.  Pilot protection and landing cancel
	// still deliberately capture the current height. Preserve the controller's
	// established integral on successful handover as well: clearing it caused
	// an immediate thrust step down and a repeatable 10--13 cm altitude loss.
	altitudeHoldTarget = handoverAltitude;
	if (!preserveTakeoffGoal) altitudeHoldIntegral = 0.0f;
	actuatorOwner = ACTUATOR_PILOT;
	print("Assisted takeoff handover: %s at %.2fm\n", reason, position.z);
}

void completeAutomaticLanding() {
	int completedMode = autoFlightReturnMode;
	forceDisarm("automatic landing complete");
	// Preserve the assisted mode selected before landing. Capture the current RC
	// switch as the override reference so an unchanged low switch cannot silently
	// rewrite the displayed/selected mode to STAB after touchdown.
	mode = completedMode;
	externalModeOverride = true;
	externalModeControlValue = rcPilotActive && isfinite(controlMode) ? controlMode : NAN;
}

void updateAutoFlightControl() {
	static Delay touchdownDelay(0.06f);
	static Delay takeoffOverspeedDelay(0.25f);
	static Delay takeoffSettledDelay(0.20f);
	static double tofLossStart = 0.0;
	if (!autoFlightActive()) {
		touchdownDelay.update(false);
		takeoffOverspeedDelay.update(false);
		takeoffSettledDelay.update(false);
		resetAutomaticLandingFlare();
		tofLossStart = 0.0f;
		autoTakeoffControlTimeAtStart = 0.0;
		return;
	}
	if (!armed) {
		resetAutomaticLandingFlare();
		autoFlightPhase = AUTO_FLIGHT_IDLE;
		autoTakeoffTargetValid = false;
		autoLandingTargetValid = false;
		return;
	}
	if (rcPilotActive && isfinite(controlMode) && isfinite(externalModeControlValue) &&
		abs(controlMode - externalModeControlValue) > 0.15f) {
		cancelAutomaticFlight("RC mode switch takeover");
		return;
	}
	mode = AUTO;
	actuatorOwner = ACTUATOR_AUTONOMOUS;
	// Automatic takeoff/landing own vertical motion only. A live pilot retains
	// immediate roll/pitch/yaw authority throughout both sequences. In POS_HOLD,
	// the optical-flow controller may replace roll/pitch later in control() while
	// its gate is healthy; it converts the same sticks into horizontal velocity.
	bool automaticPilotAttitude = pilotControlFresh();
	if (automaticPilotAttitude) {
		float pilotRoll = abs(controlRoll) < 0.05f ? 0.0f : controlRoll;
		float pilotPitch = abs(controlPitch) < 0.05f ? 0.0f : controlPitch;
		float pilotYaw = abs(controlYaw) < 0.10f ? 0.0f : controlYaw;
		if (pilotYaw != 0.0f) autoFlightYaw = attitudeEuler.z;
		attitudeTarget = Quaternion::fromEuler(Vector(
			pilotRoll * tiltMax,
			pilotPitch * tiltMax,
			autoFlightYaw));
		ratesExtra = Vector(0, 0, -pilotYaw * maxRate.z);
	} else {
		attitudeTarget = Quaternion::fromEuler(Vector(0, 0, autoFlightYaw));
		ratesExtra = Vector();
	}
	float elapsed = (float)(t - autoFlightPhaseStart);
	bool tofFresh = tofRangeFresh();
	bool groundBlindFresh = !flightWasAirborne && tofPacketFresh() && tofRangeInBlindZone;
	// MAVLink pre-arm intentionally requires a fresh zero-throttle stream. Do not
	// reinterpret that pre-command sample as an abort on the very next control
	// iteration. Any genuinely new pilot sample after this takeoff starts may still
	// abort immediately when it commands minimum throttle.
	bool takeoffPilotSampleAfterStart = pilotControlFresh() &&
		controlTime > autoTakeoffControlTimeAtStart;
	if (autoFlightPhase == AUTO_TAKEOFF && takeoffPilotSampleAfterStart &&
		controlThrottle < 0.05f) {
		if (position.z < autoFlightGroundHeight + 0.12f) forceDisarm("assisted takeoff aborted");
		else startPilotAutomaticLanding();
		return;
	}
	if ((autoFlightPhase == AUTO_LAND_DESCEND || autoFlightPhase == AUTO_LAND_FLARE) &&
		pilotControlFresh() && controlThrottle > 0.60f) {
		handOverAssistedTakeoff("landing cancelled by throttle", false);
		return;
	}
	float roll = attitudeEuler.x;
	float pitch = attitudeEuler.y;
	if (abs(roll) > radians(25.0f) || abs(pitch) > radians(25.0f)) {
		if (position.z < 0.15f) forceDisarm("automatic takeoff tilt limit");
		else if (rcPilotActive) cancelAutomaticFlight("tilt limit, pilot takeover required");
		else { autoFlightPhase = AUTO_LAND_DESCEND; autoFlightPhaseStart = t; }
		return;
	}
	if (tofFresh || (autoFlightPhase == AUTO_TAKEOFF && groundBlindFresh)) tofLossStart = 0.0f;
	else if (tofLossStart == 0.0f) tofLossStart = t;
	if (autoFlightPhase == AUTO_TAKEOFF && tofLossStart > 0.0f &&
		t - tofLossStart > 0.30f) {
		if (position.z < autoFlightGroundHeight + 0.15f) forceDisarm("takeoff ToF lost");
		else {
			autoFlightTargetHeight = position.z;
			autoFlightPhase = AUTO_LAND_DESCEND;
			autoFlightPhaseStart = t;
		}
		return;
	}
	if (autoFlightPhase == AUTO_TAKEOFF) {
		// When the module is physically inside its documented 20 mm blind zone,
		// lift smoothly on a bounded feed-forward until the first usable range.
		// Freeze the height reference during this bootstrap so range acquisition
		// cannot create a stored error and a second acceleration step.
		if (!tofFresh && groundBlindFresh) {
			autoFlightTargetHeight = autoFlightGroundHeight;
			autoTakeoffThrustLimit = min(altitudeTakeoffMaxThrust,
				autoTakeoffThrustLimit + altitudeTakeoffThrustSlew * max(dt, 0.0f));
			thrustTarget = min(altitudeHoverFeedForward() + 0.08f, autoTakeoffThrustLimit);
			if (elapsed >= 4.0f) forceDisarm("automatic takeoff failed to clear ToF blind zone");
			return;
		}
		// A single height controller owns the entire takeoff. The reference advances
		// at 0.40 m/s but never leads the aircraft by more than 12 cm, so waiting for
		// liftoff cannot build a large error and launch the aircraft abruptly.
		float velocityLead = constrain(max(velocity.z, 0.0f) * 0.25f, 0.05f,
			altitudeTakeoffTargetLead);
		float nextTarget = max(
			autoFlightTargetHeight + altitudeTakeoffClimbRate * max(dt, 0.0f),
			position.z + velocityLead);
		autoFlightTargetHeight = min(autoFlightGoalHeight,
			min(nextTarget, position.z + altitudeTakeoffTargetLead));
		autoTakeoffThrustLimit = min(altitudeTakeoffMaxThrust,
			autoTakeoffThrustLimit + altitudeTakeoffThrustSlew * max(dt, 0.0f));
		if (tofFresh && position.z > autoFlightGroundHeight + 0.08f) flightWasAirborne = true;
		if (!flightWasAirborne && elapsed >= 4.0f) {
			forceDisarm("automatic takeoff failed to lift");
			return;
		}
		bool runaway = tofFresh && position.z > autoFlightGroundHeight + 0.20f &&
			(velocity.z > 1.0f || position.z > autoFlightGoalHeight + 0.20f);
		if (takeoffOverspeedDelay.update(runaway)) {
			autoFlightTargetHeight = position.z;
			autoFlightPhase = AUTO_LAND_DESCEND;
			autoFlightPhaseStart = t;
			altitudeHoldIntegral = 0.0f;
			print("Takeoff aborted: climb %.2fm/s height %.2fm\n", velocity.z, position.z);
			return;
		}
		bool settled = autoFlightTargetHeight >= autoFlightGoalHeight &&
			abs(autoFlightGoalHeight - position.z) < 0.06f && abs(velocity.z) < 0.12f;
		if (takeoffSettledDelay.update(settled)) {
			handOverAssistedTakeoff("automatic takeoff complete", true);
		}
		return;
	}
	takeoffSettledDelay.update(false);
	if (autoFlightPhase == AUTO_LAND_DESCEND || autoFlightPhase == AUTO_LAND_FLARE) {
		// LAND may arrive while the aircraft is still carrying horizontal velocity
		// from an Offboard/RC ownership handover. Pause only while real braking is
		// needed; a settled hover must start descending immediately rather than hop.
		float landingHorizontalSpeed = sqrt(velocity.x * velocity.x + velocity.y * velocity.y);
		if (autoFlightPhase == AUTO_LAND_DESCEND && position.z > 0.15f &&
			landingHorizontalSpeed > 0.10f && elapsed < 0.30f) {
			return;
		}
		float descentRate = position.z >= 0.30f ? 0.45f : 0.28f;
		if (autoLandingFlareLatched) autoFlightPhase = AUTO_LAND_FLARE;
		else autoFlightPhase = position.z < 0.30f ? AUTO_LAND_FLARE : AUTO_LAND_DESCEND;
		if (tofFresh) {
			// Keep the descending target close enough to the measured height that
			// the position error cannot build up into a fast dive followed by a
			// derivative-driven thrust spike near the floor.
			float targetLead = position.z >= 0.30f ? 0.12f : 0.07f;
			float nextTarget = max(0.0f,
				autoFlightTargetHeight - descentRate * max(dt, 0.0f));
			float minimumTarget = max(0.0f, position.z - targetLead);
			autoFlightTargetHeight = min(autoFlightTargetHeight,
				max(nextTarget, minimumTarget));
		}
		else thrustTarget = max(0.12f, thrustTarget - max(dt, 0.0f) * 0.02f);
		if (!tofFresh && tofLossStart > 0.0f && t - tofLossStart > 12.0f) {
			forceDisarm("landing sensor-loss timeout");
			return;
		}
		float contactRange = autoFlightGroundRange > FLOW_SENSOR_MIN_HEIGHT ?
			autoFlightGroundRange + 0.012f : 0.06f;
		float groundRange = autoFlightGroundRange > FLOW_SENSOR_MIN_HEIGHT ?
			autoFlightGroundRange : 0.05f;
		float landingClearance = max(0.0f, opticalFlowHeight - groundRange);
		bool nearGround = tofFresh && autoFlightPhase == AUTO_LAND_FLARE &&
			landingClearance <= automaticLandingFlareClearance;
		if (!autoLandingFlareLatched && nearGround) {
			autoLandingFlareLatched = true;
			autoLandingFlareStart = t;
			print("Landing flare latched at %.2fm clearance\n", landingClearance);
		}
		if (autoLandingFlareLatched) {
			// This latch is intentionally one-way. ToF ground effect or a rebound
			// cannot re-enable altitude PID and add thrust before touchdown.
			thrustTarget = max(0.10f, thrustTarget - max(dt, 0.0f) * 0.50f);
		}
		bool kinematicTouchdown = abs(velocity.z) < 0.15f && abs(acc.norm() - ONE_G) < ONE_G * 0.20f;
		// Once the commanded height is on the floor, the launch-surface range is
		// a stronger contact signal than the vibration-sensitive accelerometer.
		// Require non-upward motion so a single short ToF packet cannot cut motors
		// while the aircraft is bouncing away from the surface.
		bool rangeTouchdown = tofFresh && autoFlightTargetHeight <= 0.02f &&
			opticalFlowHeight <= contactRange && velocity.z <= 0.12f;
		float hoverFeedForward = altitudeHoverFeedForward();
		bool softTouchdown = kinematicTouchdown && thrustTarget < hoverFeedForward * 0.80f;
		bool reducedToIdle = thrustTarget <= max(0.12f, hoverFeedForward * 0.45f);
		bool flareTimeout = autoLandingFlareLatched &&
			t - autoLandingFlareStart >= 0.45f;
		bool touchdown = rangeTouchdown || (autoLandingFlareLatched &&
			(softTouchdown || reducedToIdle || flareTimeout));
		if (touchdownDelay.update(touchdown)) {
			completeAutomaticLanding();
		}
	}
}
