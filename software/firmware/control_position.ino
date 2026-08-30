// Optical-flow horizontal position and velocity control.

// ==================== Optical-flow position hold ====================
// Requires horizontal position/velocity estimates and active altitude hold.

// 参数
float positionHoldP = 0.85f;
float positionStickMaxSpeed = 0.70f;
float holdPX = 0.350f;
float holdIX = 0.04f;
float holdDX = 0.0f;
float holdPY = 0.350f;
float holdIY = 0.04f;
float holdDY = 0.0f;
float holdIntegralLimit = 0.08f;
float maxFlowAngleRate = 1.20f; // rad/s, prevents a step when POS_HOLD engages
const float positionTiltLimit = radians(12.0f);
float posStickDeadband = 0.10f;
const float yawUnlockEnterThreshold = 0.18f;
const float yawUnlockExitThreshold = 0.08f;

// 状态
bool posHoldLocked = false;
bool posHoldGateOpen = false;
float targetPosX = 0.0f;
float targetPosY = 0.0f;
float velIntegralX = 0.0f;
float velIntegralY = 0.0f;
float lastVelErrorX = 0.0f;
float lastVelErrorY = 0.0f;
float posRollCmd = 0.0f;
float posPitchCmd = 0.0f;
float posTargetVelBodyX = 0.0f;
float posTargetVelBodyY = 0.0f;
float posVelErrorX = 0.0f;
float posVelErrorY = 0.0f;
bool posControlSaturated = false;
bool usePosCmd = false;
bool posHoldFallbackActive = false;
int posHoldRejectReason = 0;
float landingHoldFallbackScale = 0.0f;

void limitHorizontalSpeedCommand(float &x, float &y) {
	float speedLimit = max(positionStickMaxSpeed, 0.0f);
	float speed = sqrt(x * x + y * y);
	if (speed <= speedLimit || speed <= 0.000001f) return;
	float scale = speedLimit / speed;
	x *= scale;
	y *= scale;
}

void resetHorizontalHoldTracking() {
	velIntegralX = 0.0f;
	velIntegralY = 0.0f;
	lastVelErrorX = 0.0f;
	lastVelErrorY = 0.0f;
	targetPosX = position.x;
	targetPosY = position.y;
	posHoldLocked = false;
	posHoldGateOpen = false;
	posTargetVelBodyX = 0.0f;
	posTargetVelBodyY = 0.0f;
	posVelErrorX = 0.0f;
	posVelErrorY = 0.0f;
	posControlSaturated = false;
}

void clearHorizontalHoldState() {
	resetHorizontalHoldTracking();
	posRollCmd = 0.0f;
	posPitchCmd = 0.0f;
	posHoldFallbackActive = false;
	usePosCmd = false;
}

void updateBoundedPositionFallback(float controlDt, bool allowPilotAttitude) {
	// A lost flow gate must not expose the wider 30-degree STAB envelope while
	// POS_HOLD is still selected. Slew toward a pilot request (or level for
	// Offboard/no live pilot) inside the same 12-degree position-control limit.
	float horizontalTiltLimit = min(tiltMax, positionTiltLimit);
	float desiredRoll = 0.0f;
	float desiredPitch = 0.0f;
	if (allowPilotAttitude && !offboardLocalActive && pilotControlFresh()) {
		float pilotRoll = abs(controlRoll) < 0.05f ? 0.0f : controlRoll;
		float pilotPitch = abs(controlPitch) < 0.05f ? 0.0f : controlPitch;
		desiredRoll = constrain(pilotRoll, -1.0f, 1.0f) * horizontalTiltLimit;
		desiredPitch = constrain(pilotPitch, -1.0f, 1.0f) * horizontalTiltLimit;
	}
	float boundedDt = constrain(controlDt, 0.001f, 0.02f);
	float maxCmdStep = maxFlowAngleRate * boundedDt;
	posRollCmd += constrain(desiredRoll - posRollCmd, -maxCmdStep, maxCmdStep);
	posPitchCmd += constrain(desiredPitch - posPitchCmd, -maxCmdStep, maxCmdStep);
	posTargetVelBodyX = 0.0f;
	posTargetVelBodyY = 0.0f;
	posVelErrorX = 0.0f;
	posVelErrorY = 0.0f;
	posControlSaturated = false;
	posHoldFallbackActive = true;
	usePosCmd = true;
}

void updatePositionControlSplit(float controlDt) {
	static Delay posGateDelay(0.30f);
	static uint32_t lastControlFlowSequence = 0;
	static bool yawBlocked = false;
	static float landingFallbackRollCmd = 0.0f;
	static float landingFallbackPitchCmd = 0.0f;
	static double landingFallbackTime = 0.0;

	bool automaticLandingActive = autoFlightPhase == AUTO_LAND_DESCEND ||
		autoFlightPhase == AUTO_LAND_FLARE;
	bool automaticTakeoff = autoTakeoffTargetValid &&
		mode == AUTO && autoFlightReturnMode == POS_HOLD &&
		autoFlightPhase == AUTO_TAKEOFF;
	bool positionHoldRequested = mode == POS_HOLD ||
		(mode == AUTO && ((autoFlightActive() && autoFlightReturnMode == POS_HOLD) || offboardLocalActive));
	if (!armed || !positionHoldRequested) {
		posHoldRejectReason = 1; // not armed or not in POS_HOLD
		posGateDelay.update(false);
		landingFallbackTime = 0.0;
		landingHoldFallbackScale = 0.0f;
		clearHorizontalHoldState();
		return;
	}

	if (yawBlocked) yawBlocked = abs(controlYaw) > yawUnlockExitThreshold;
	else yawBlocked = abs(controlYaw) > yawUnlockEnterThreshold;
	bool stickMoving = !offboardLocalActive &&
		(abs(controlRoll) > posStickDeadband || abs(controlPitch) > posStickDeadband);
	bool safeAltitude = tofHealthy && opticalFlowHeight > 0.05f && opticalFlowHeight < 6.0f;
	bool levelEnough = abs(attitudeEuler.x) < tiltMax && abs(attitudeEuler.y) < tiltMax;
	bool heightStable = altitudeHoldEngaged;
	bool velocityGateOpen = flowBiasReady && flowCtrlUsingFlow && flowPositionGateOpen && flowAirborne &&
		safeAltitude && levelEnough && heightStable && !yawBlocked;
	if (!flowBiasReady) posHoldRejectReason = 9;
	else if (!flowCtrlUsingFlow) posHoldRejectReason = 2;
	else if (!flowPositionGateOpen) posHoldRejectReason = 3;
	else if (!flowAirborne) posHoldRejectReason = 4;
	else if (!safeAltitude) posHoldRejectReason = 5;
	else if (!levelEnough) posHoldRejectReason = 6;
	else if (!heightStable) posHoldRejectReason = 7;
	else if (yawBlocked) posHoldRejectReason = 8;
	else posHoldRejectReason = 0;
	bool delayedGateOpen = posGateDelay.update(velocityGateOpen);
	// flowAirborne already requires a sustained 250 ms airborne sample window.
	// During automatic takeoff, do not add another 300 ms with no horizontal
	// correction after altitude hold has engaged. The angle-rate limiter and the
	// low-altitude tilt cap below keep this recovery bounded.
	posHoldGateOpen = delayedGateOpen ||
		(automaticTakeoff && velocityGateOpen && flowAirborne);

	if (!velocityGateOpen) {
		// Near the floor, optical flow can legitimately lose its horizontal gate.
		// A deliberate stick command remains available, but stays inside the same
		// bounded attitude envelope used by position control.
		bool landingPilotAttitudeOverride = automaticLandingActive &&
			pilotControlFresh() && stickMoving;
		if (landingPilotAttitudeOverride) {
			landingHoldFallbackScale = 0.0f;
			updateBoundedPositionFallback(controlDt, true);
			return;
		}
		// Optical flow normally drops its airborne qualification in the last few
		// centimetres. Abruptly clearing the last valid horizontal correction there
		// caused the airframe to slide during flare. Retain a small, time-bounded
		// correction and fade it to zero before motor cut so it cannot tip on contact.
		float landingClearance = max(0.0f, opticalFlowHeight - autoFlightGroundRange);
		bool landingFallbackFresh = automaticLandingActive && tofHealthy &&
			tofTimestamp != 0 && millis() - tofTimestamp <= 150 &&
			landingClearance > 0.005f && landingFallbackTime > 0.0f &&
			t - landingFallbackTime <= 0.75f;
		if (landingFallbackFresh) {
			// Fade against clearance above the measured launch surface because the
			// sensor is physically above the floor at touchdown.
			landingHoldFallbackScale = constrain(landingClearance / 0.06f, 0.0f, 1.0f);
			const float fallbackTiltLimit = radians(8.0f);
			posRollCmd = constrain(landingFallbackRollCmd,
				-fallbackTiltLimit, fallbackTiltLimit) * landingHoldFallbackScale;
			posPitchCmd = constrain(landingFallbackPitchCmd,
				-fallbackTiltLimit, fallbackTiltLimit) * landingHoldFallbackScale;
			posHoldFallbackActive = true;
			usePosCmd = true;
			return;
		}
		landingHoldFallbackScale = 0.0f;
		if (yawBlocked) {
			// Freeze the existing world target while yawing. Clearing it here made
			// small yaw-stick noise silently redefine the hold point.
			posGateDelay.update(false);
			posHoldGateOpen = false;
			updateBoundedPositionFallback(controlDt, true);
			return;
		}
		resetHorizontalHoldTracking();
		updateBoundedPositionFallback(controlDt, true);
		return;
	}
	if (!posHoldGateOpen) {
		// Keep pilot takeover available during the 300 ms qualification window,
		// but never jump from the position limit to the full STAB tilt envelope.
		updateBoundedPositionFallback(controlDt, true);
		return;
	}

	if (!posHoldLocked) {
		targetPosX = automaticTakeoff ? autoTakeoffTargetX : position.x;
		targetPosY = automaticTakeoff ? autoTakeoffTargetY : position.y;
		velIntegralX = 0.0f;
		velIntegralY = 0.0f;
		posHoldLocked = true;
	}
	if (offboardLocalActive && offboardUsePositionXY) {
		targetPosX = offboardTargetX;
		targetPosY = offboardTargetY;
	}
	if (automaticLandingActive && autoLandingRelockPending) {
		// Land at the point where LAND was requested, even if the horizontal gate
		// briefly closes before this relock executes. Preserve the bounded velocity
		// integrator when an existing position lock is healthy: it already contains
		// the airframe's hover-bias compensation, and clearing it exactly as descent
		// starts causes a repeatable lateral slide. A first-time lock still starts
		// from zero below.
		bool preserveLandingBias = posHoldLocked && posHoldGateOpen;
		targetPosX = autoLandingTargetValid ? autoLandingTargetX : position.x;
		targetPosY = autoLandingTargetValid ? autoLandingTargetY : position.y;
		if (!preserveLandingBias) {
			velIntegralX = 0.0f;
			velIntegralY = 0.0f;
		}
		autoLandingRelockPending = false;
		autoLandingTargetValid = false;
	}

	usePosCmd = true;
	posHoldFallbackActive = false;
	landingHoldFallbackScale = 0.0f;
	if (automaticLandingActive) {
		landingFallbackRollCmd = posRollCmd;
		landingFallbackPitchCmd = posPitchCmd;
		landingFallbackTime = t;
	}
	if (opticalFlowSequence == lastControlFlowSequence) return; // hold command between sensor samples
	lastControlFlowSequence = opticalFlowSequence;
	controlDt = constrain(opticalFlowSampleDt, 0.01f, 0.20f);
	// Optical-flow velocity becomes noisier in ground effect and close to the
	// floor. Preserve the stronger cruise-height hold, but progressively reduce
	// position authority below 30 cm so noise cannot grow into a lateral limit
	// cycle during takeoff or landing.
	float positionGroundRange = autoFlightGroundRange > FLOW_SENSOR_MIN_HEIGHT ?
		autoFlightGroundRange : FLOW_SENSOR_MIN_HEIGHT;
	float positionHeightAboveGround = max(0.0f, opticalFlowHeight - positionGroundRange);
	float lowAltitudeGainScale = constrain(positionHeightAboveGround / 0.30f, 0.40f, 1.0f);
	// Descent needs more braking authority than takeoff near the floor. Keep the
	// position pull bounded, but do not let it collapse far enough for a steady
	// drift to run away before touchdown.
	float horizontalAuthorityScale = automaticLandingActive ?
		max(0.80f, lowAltitudeGainScale) : lowAltitudeGainScale;

	float yaw = attitudeEuler.z;
	float c = cos(yaw);
	float s = sin(yaw);

	float stickVelBodyX = stickMoving ? controlPitch * positionStickMaxSpeed : 0.0f;
	// Positive roll tilts/thrusts to body-right (-Y in FLU), so the velocity
	// command must use the opposite sign from the body Y axis.
	float stickVelBodyY = stickMoving ? -controlRoll * positionStickMaxSpeed : 0.0f;
	limitHorizontalSpeedCommand(stickVelBodyX, stickVelBodyY);
	if (stickMoving && posHoldGateOpen && posHoldLocked) {
		float stickVelWorldX = stickVelBodyX * c - stickVelBodyY * s;
		float stickVelWorldY = stickVelBodyX * s + stickVelBodyY * c;
		targetPosX = constrain(targetPosX + stickVelWorldX * controlDt, position.x - 1.0f, position.x + 1.0f);
		targetPosY = constrain(targetPosY + stickVelWorldY * controlDt, position.y - 1.0f, position.y + 1.0f);
	}
	if (offboardLocalActive && offboardUseVelocityXY && posHoldGateOpen && posHoldLocked) {
		targetPosX = constrain(targetPosX + offboardTargetVX * controlDt, position.x - 1.0f, position.x + 1.0f);
		targetPosY = constrain(targetPosY + offboardTargetVY * controlDt, position.y - 1.0f, position.y + 1.0f);
	}

	float vWorldX = 0.0f;
	float vWorldY = 0.0f;
	if (posHoldGateOpen && posHoldLocked) {
		float errX = targetPosX - position.x;
		float errY = targetPosY - position.y;
		float positionGain = positionHoldP;
		positionGain *= horizontalAuthorityScale;
		vWorldX = errX * positionGain;
		vWorldY = errY * positionGain;
	}
	if (offboardLocalActive && offboardUseVelocityXY) {
		vWorldX += offboardTargetVX;
		vWorldY += offboardTargetVY;
	}

	posTargetVelBodyX = vWorldX * c + vWorldY * s + stickVelBodyX;
	posTargetVelBodyY = -vWorldX * s + vWorldY * c + stickVelBodyY;
	// POS_STICK_V is the single public horizontal command-speed limit. Position
	// feedback, pilot feed-forward, and Offboard feed-forward share this final
	// vector bound instead of adding into a larger hidden command.
	limitHorizontalSpeedCommand(posTargetVelBodyX, posTargetVelBodyY);

	posVelErrorX = posTargetVelBodyX - velocity.x;
	posVelErrorY = posTargetVelBodyY - velocity.y;
	float dTermX = 0.0f;
	float dTermY = 0.0f;
	if (controlDt > 0.0f && controlDt < 0.2f) {
		dTermX = (posVelErrorX - lastVelErrorX) / controlDt * holdDX;
		dTermY = (posVelErrorY - lastVelErrorY) / controlDt * holdDY;
	}
	lastVelErrorX = posVelErrorX;
	lastVelErrorY = posVelErrorY;

	// Keep the bounded velocity integral active for pilot and Offboard velocity
	// commands. Clearing it on every stick sample made low-speed commands unable
	// to overcome a persistent airframe/flow trim. Stop integrating further into
	// global tilt saturation, but always allow the error to unwind the integral.
	// Bound optical-flow corrections independently from manual STAB authority.
	float horizontalTiltLimit = min(tiltMax, positionTiltLimit);
	horizontalTiltLimit *= horizontalAuthorityScale;
	float takeoffVelocityGainX = holdPX;
	float takeoffVelocityGainY = holdPY;
	// Retain most velocity damping near the floor while reducing position pull.
	// This brakes drift without chasing noisy low-height position estimates.
	float lowAltitudeVelocityScale = automaticLandingActive ?
		1.0f : 0.70f + 0.30f * lowAltitudeGainScale;
	takeoffVelocityGainX *= lowAltitudeVelocityScale;
	takeoffVelocityGainY *= lowAltitudeVelocityScale;
	float activeIntegralLimit = holdIntegralLimit * horizontalAuthorityScale;
	velIntegralX = constrain(velIntegralX, -activeIntegralLimit, activeIntegralLimit);
	velIntegralY = constrain(velIntegralY, -activeIntegralLimit, activeIntegralLimit);
	float bodyDemandX = posVelErrorX * takeoffVelocityGainX + velIntegralX + dTermX;
	float bodyDemandY = posVelErrorY * takeoffVelocityGainY + velIntegralY + dTermY;
	if (abs(posVelErrorX) > 0.02f &&
		(abs(bodyDemandX) < horizontalTiltLimit || posVelErrorX * bodyDemandX < 0.0f)) {
		velIntegralX = constrain(velIntegralX + posVelErrorX * controlDt * holdIX, -activeIntegralLimit, activeIntegralLimit);
	}
	if (abs(posVelErrorY) > 0.02f &&
		(abs(bodyDemandY) < horizontalTiltLimit || posVelErrorY * bodyDemandY < 0.0f)) {
		velIntegralY = constrain(velIntegralY + posVelErrorY * controlDt * holdIY, -activeIntegralLimit, activeIntegralLimit);
	}

	// Position correction is independently bounded; manual STAB continues to use
	// the global attitude envelope.
	float pitchDemandRaw = posVelErrorX * takeoffVelocityGainX + velIntegralX + dTermX;
	// Positive roll tilts toward body -Y, so a positive body-Y velocity error
	// requires a negative roll command. Keep the conventional closed-loop sign;
	// reversing it caused a confirmed lateral runaway and flip in flight.
	float rollDemandRaw = -(posVelErrorY * takeoffVelocityGainY + velIntegralY + dTermY);
	posControlSaturated = abs(pitchDemandRaw) > horizontalTiltLimit ||
		abs(rollDemandRaw) > horizontalTiltLimit;
	float pitchDemand = constrain(pitchDemandRaw, -horizontalTiltLimit, horizontalTiltLimit);
	float rollDemand = constrain(rollDemandRaw, -horizontalTiltLimit, horizontalTiltLimit);
	float maxCmdStep = maxFlowAngleRate * controlDt;
	if (offboardLocalActive && offboardUsePositionXY) {
		// A waypoint correction is a two-dimensional tilt vector. Limit its total
		// rate so a noisy cross-axis flow sample cannot reverse one axis in a single
		// update while the other axis continues moving.
		maxCmdStep = min(maxCmdStep, 0.70f * controlDt);
		float pitchStep = pitchDemand - posPitchCmd;
		float rollStep = rollDemand - posRollCmd;
		float stepNorm = sqrt(pitchStep * pitchStep + rollStep * rollStep);
		if (stepNorm > maxCmdStep && stepNorm > 0.000001f) {
			float scale = maxCmdStep / stepNorm;
			pitchStep *= scale;
			rollStep *= scale;
		}
		posPitchCmd += pitchStep;
		posRollCmd += rollStep;
	} else {
		posPitchCmd += constrain(pitchDemand - posPitchCmd, -maxCmdStep, maxCmdStep);
		posRollCmd += constrain(rollDemand - posRollCmd, -maxCmdStep, maxCmdStep);
	}
}
