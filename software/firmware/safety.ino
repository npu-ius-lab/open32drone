// Copyright (c) 2024 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Fail-safe functions

// Configurable timeouts (params SF_RC_LOSS_TIME / SF_DESCEND_TIME)
float rcLossTimeout = 1;
float descendTime = 5;
bool rcFailsafeActive = false;
float rcFailsafeThrust = 0.0f;
bool offboardActive = false;
bool offboardFailsafeActive = false;
double offboardControlTime = 0.0;
const float offboardTimeout = 0.30f;
// Minimal tip-over guard. Normal commanded tilt is limited to 30 degrees, so
// a sustained attitude beyond 70 degrees is outside the supported envelope.
// Do not depend on ToF or impact acceleration: both can be invalid in a crash.
const float tipOverCosLimit = 0.342f; // cos(70 degrees)
const float tipOverHoldTime = 0.25f;
Delay tipOverDelay(tipOverHoldTime);
bool mavlinkArmSession = false;
bool failsafeAttitudeCaptured = false;
float failsafeYaw = 0.0f;
const char *preArmFailure = "not checked";
const char *failsafeReason = "none";
uint8_t offboardOwnerSysId = 0;
uint8_t offboardOwnerCompId = 0;
double offboardOwnerLastCommand = 0.0;

extern double controlTime;
extern float controlRoll, controlPitch, controlThrottle, controlYaw;
extern bool rcLinkHealthy, rcReceiverFailsafe, rcPilotActive;
extern bool imuHealthy, gyroCalibrated, accelCalibrationActive;
extern bool parameterStorageHealthy;
extern uint32_t imuLastSampleMs;
extern float loopRate;
extern uint32_t tofTimestamp;
extern bool motorsInitialized;
#if WIFI_ENABLED
extern bool mavlinkConnected, mavlinkManualControlActive;
extern uint32_t mavlinkLastRxMs, mavlinkManualControlLastMs;
#endif

bool mavlinkLinkHealthy() {
#if WIFI_ENABLED
	return mavlinkConnected && mavlinkLastRxMs != 0 && millis() - mavlinkLastRxMs <= 3000;
#else
	return false;
#endif
}

bool mavlinkManualControlHealthy() {
#if WIFI_ENABLED
	return mavlinkManualControlActive && mavlinkManualControlLastMs != 0 &&
		millis() - mavlinkManualControlLastMs <= 500;
#else
	return false;
#endif
}

void resetFailsafeAttitude() {
	failsafeAttitudeCaptured = false;
	failsafeYaw = 0.0f;
}

void resetTipOverGuard() {
	tipOverDelay.update(false);
}

void releaseOffboardControl() {
	offboardActive = false;
	clearOffboardLocalControl();
	clearOffboardSetpointStaging();
	offboardOwnerSysId = 0;
	offboardOwnerCompId = 0;
	offboardOwnerLastCommand = 0.0f;
}

bool preArmCheck(bool requireRC, const char **reason) {
	const char *failure = nullptr;
	if (otaUpdateActive) failure = "firmware update active";
	else if (!parameterStorageHealthy) failure = "parameter storage unavailable";
	else if (accelCalibrationActive) failure = "accelerometer calibration active";
	else if (!motorsInitialized || !motorPwmHealthy()) failure = "motor PWM unavailable";
	else if (!imuHealthy || imuLastSampleMs == 0 || millis() - imuLastSampleMs > 50) failure = "IMU unavailable or stale";
	else if (!gyroCalibrated) failure = "gyro calibration incomplete";
	else if (!attitude.valid() || !rates.valid()) failure = "invalid attitude or rates";
	else if (requireRC && !validRCConfiguration()) failure = "invalid RC calibration/mapping";
	else if (requireRC && (!rcPilotActive || !rcLinkHealthy || rcReceiverFailsafe)) failure = "RC link unavailable";
	else if (!requireRC && !mavlinkLinkHealthy()) failure = "GCS link unavailable";
	else if ((requireRC || rcPilotActive || mavlinkManualControlHealthy()) &&
		(!isfinite(controlThrottle) || controlThrottle > 0.05f)) failure = "throttle not low";
	else if (loopRate != 0 && loopRate < 200.0f) failure = "control loop too slow";
	preArmFailure = failure ? failure : "none";
	if (reason) *reason = failure;
	return failure == nullptr;
}

bool requestArm(const char *source, bool requireRC) {
	static uint32_t lastArmRejectPrintMs = UINT32_MAX;
	if (armed) return true;
	const char *reason = nullptr;
	if (!preArmCheck(requireRC, &reason)) {
		if (lastArmRejectPrintMs == UINT32_MAX || millis() - lastArmRejectPrintMs >= 500) {
			lastArmRejectPrintMs = millis();
			print("Arm rejected (%s): %s\n", source, reason);
		#if WIFI_ENABLED
			sendMavlinkStatusTextf(MAV_SEVERITY_WARNING, "Arm denied: %s", reason);
		#endif
			if (!gyroCalibrated) printGyroCalibrationStatus();
		}
		return false;
	}
	// A stale receiver can retain its last stick values after the transmitter is
	// switched off. A GCS-only arm starts from explicit zero controls unless a
	// fresh physical or MAVLink joystick is actually supplying the sticks.
	if (!requireRC && !rcPilotActive && !mavlinkManualControlHealthy()) {
		controlRoll = 0.0f;
		controlPitch = 0.0f;
		controlYaw = 0.0f;
		controlThrottle = 0.0f;
		controlMode = NAN;
	}
	armed = true;
	resetRCEmergencyGesture();
	// Never let a zero-thrust failsafe latch survive into a new arm session.
	rcFailsafeActive = false;
	rcFailsafeThrust = 0.0f;
	offboardFailsafeActive = false;
	resetFailsafeAttitude();
	resetTipOverGuard();
	resetAutomaticLandingFlare();
	// If a physical pilot is already active, it owns the armed session even
	// when a MAVLink client sent the arm command. This keeps later RC loss fail-closed instead
	// of allowing a GCS heartbeat to mask stale physical stick values.
	mavlinkArmSession = !requireRC && !rcPilotActive;
	assistedTakeoffGroundIdle = true;
	flightWasAirborne = false;
	autoFlightGroundRange = tofHealthy && opticalFlowHeight > FLOW_SENSOR_MIN_HEIGHT ?
		opticalFlowHeight : 0.0f;
	autoLandingRelockPending = false;
	autoTakeoffTargetValid = false;
	autoLandingTargetValid = false;
	autoFlightSource = AUTO_SOURCE_NONE;
	clearOffboardLocalControl();
	actuatorOwner = ACTUATOR_PILOT;
	print("Armed by %s\n", source);
	#if WIFI_ENABLED
		sendMavlinkStatusTextf(MAV_SEVERITY_INFO, "Armed by %s", source);
	#endif
	return true;
}

void forceDisarm(const char *reason) {
	armed = false;
	resetRCEmergencyGesture();
	rcFailsafeActive = false;
	rcFailsafeThrust = 0.0f;
	offboardFailsafeActive = false;
	resetFailsafeAttitude();
	resetTipOverGuard();
	resetAutomaticLandingFlare();
	mavlinkArmSession = false;
#if WIFI_ENABLED
	mavlinkManualControlActive = false;
#endif
	assistedTakeoffGroundIdle = true;
	flightWasAirborne = false;
	autoLandingRelockPending = false;
	autoTakeoffTargetValid = false;
	autoLandingTargetValid = false;
	releaseOffboardControl();
	autoFlightPhase = AUTO_FLIGHT_IDLE;
	autoFlightSource = AUTO_SOURCE_NONE;
	actuatorOwner = ACTUATOR_NONE;
	thrustTarget = 0.0f;
	attitudeTarget.invalidate();
	ratesTarget.invalidate();
	torqueTarget.invalidate();
	memset(motors, 0, sizeof(motors));
	if (reason) print("Disarmed: %s\n", reason);
	#if WIFI_ENABLED
		if (reason) sendMavlinkStatusTextf(MAV_SEVERITY_INFO, "Disarmed: %s", reason);
	#endif
}

bool claimOffboardControl(uint8_t sysid, uint8_t compid) {
	bool leaseExpired = !offboardActive && !autoFlightActive() &&
		offboardOwnerLastCommand > 0.0f && t - offboardOwnerLastCommand > 1.0f;
	if (leaseExpired) {
		offboardOwnerSysId = 0;
		offboardOwnerCompId = 0;
	}
	if (offboardOwnerSysId == 0) {
		offboardOwnerSysId = sysid;
		offboardOwnerCompId = compid;
		offboardOwnerLastCommand = t;
		return true;
	}
	bool owner = offboardOwnerSysId == sysid && offboardOwnerCompId == compid;
	if (owner) offboardOwnerLastCommand = t;
	return owner;
}

void failsafe() {
	sensorFailsafe();
	tipOverFailsafe();
	rcLossFailsafe();
	offboardFailsafe();
	autoFailsafe();
}

void sensorFailsafe() {
	if (!armed) return;
	if (!imuHealthy || imuLastSampleMs == 0 || millis() - imuLastSampleMs > 50 ||
		!attitude.valid() || !rates.valid()) {
		failsafeReason = "IMU/estimator failure";
		forceDisarm(failsafeReason);
	}
}

void tipOverFailsafe() {
	if (!armed || !attitude.valid()) {
		resetTipOverGuard();
		return;
	}
	bool tipped = isfinite(attitudeBodyUp.z) && attitudeBodyUp.z < tipOverCosLimit;
	if (!tipOverDelay.update(tipped)) return;
	failsafeReason = "tip-over";
	forceDisarm(failsafeReason);
}

void offboardFailsafe() {
	if (!offboardActive) return;
	if (mode != AUTO || !armed) {
		releaseOffboardControl();
		if (armed) actuatorOwner = ACTUATOR_PILOT;
		return;
	}
	if (t - offboardControlTime <= offboardTimeout) return;
	failsafeReason = "offboard timeout";
	releaseOffboardControl();
	if (rcPilotActive && rcLinkHealthy && !rcReceiverFailsafe) {
		// A brief Wi-Fi interruption must not stop motors in flight. Return to an
		// RC mode with altitude assistance when ToF is available.
		bool tofFresh = tofHealthy && tofTimestamp != 0 &&
			millis() - tofTimestamp <= 150 && position.z > 0.05f;
		assistedTakeoffGroundIdle = false;
		requestExternalMode(tofFresh ? ALT_HOLD : STAB);
		actuatorOwner = ACTUATOR_PILOT;
		print("Offboard timeout: pilot takeover in %s\n", getModeName());
		return;
	}
	// RC-loss handling runs before this function and normally owns this path.
	// If it did not, use the existing controlled descent rather than disarming.
	offboardFailsafeActive = true;
	rcFailsafeActive = true;
	rcFailsafeThrust = constrain(thrustTarget, 0.0f, 1.0f);
	actuatorOwner = ACTUATOR_FAILSAFE;
}

// RC loss failsafe
void rcLossFailsafe() {
	if (!armed) {
		rcFailsafeActive = false;
		offboardFailsafeActive = false;
		resetFailsafeAttitude();
		return;
	}
	// A MAVROS heartbeat cannot replace a continuous flight-control stream.
	// Keep an offboard-timeout descent latched until disarm, even while the
	// management link continues to send healthy heartbeats.
	if (offboardFailsafeActive) {
		rcFailsafeActive = true;
		actuatorOwner = ACTUATOR_FAILSAFE;
		descend();
		return;
	}
	// Once a no-RC MAVLink flight loses its required manual stream, keep the
	// controlled descent latched until disarm. A returning Wi-Fi packet stream
	// must not clear the failsafe and leave the aircraft in direct-throttle STAB.
	// A real SBUS pilot can still take over because interpretControls() first
	// transfers mavlinkArmSession ownership to the physical receiver.
	if (rcFailsafeActive && mavlinkArmSession) {
		actuatorOwner = ACTUATOR_FAILSAFE;
		descend();
		return;
	}
	bool rcHealthy = rcPilotActive && rcLinkHealthy && !rcReceiverFailsafe && t - controlTime <= rcLossTimeout;
	bool manualStreamRequired = mavlinkArmSession && mavlinkManualControlActive && !autoFlightActive();
	bool gcsHealthy = mavlinkArmSession && mavlinkLinkHealthy() &&
		(!manualStreamRequired || mavlinkManualControlHealthy());
	if (rcHealthy || gcsHealthy) {
		rcFailsafeActive = false;
		resetFailsafeAttitude();
		return;
	}
	if (!mavlinkArmSession && controlTime == 0 && !rcReceiverFailsafe) return; // no RC at all
	if (!rcFailsafeActive) {
		rcFailsafeActive = true;
		failsafeReason = mavlinkArmSession ? "GCS/manual-control timeout" :
			(rcReceiverFailsafe ? "receiver failsafe" : "RC timeout");
		print("Failsafe descent: %s\n", failsafeReason);
	#if WIFI_ENABLED
		sendMavlinkStatusTextf(MAV_SEVERITY_CRITICAL,
			"Failsafe descent: %s", failsafeReason);
	#endif
		autoFlightPhase = AUTO_FLIGHT_IDLE;
		autoTakeoffTargetValid = false;
		autoLandingTargetValid = false;
		releaseOffboardControl();
		actuatorOwner = ACTUATOR_FAILSAFE;
		rcFailsafeThrust = constrain(thrustTarget, 0.0f, 1.0f);
	}
	descend();
}

// Smooth descend on RC lost
void descend() {
	mode = AUTO;
	if (!failsafeAttitudeCaptured) {
		failsafeYaw = attitude.valid() && isfinite(attitudeEuler.z) ? attitudeEuler.z : 0.0f;
		failsafeAttitudeCaptured = true;
		rollPID.reset();
		pitchPID.reset();
		yawPID.reset();
		rollRatePID.reset();
		pitchRatePID.reset();
		yawRatePID.reset();
	}
	attitudeTarget = Quaternion::fromEuler(Vector(0, 0, failsafeYaw));
	ratesExtra = Vector();
	float safeDescendTime = max(descendTime, 0.1f);
	rcFailsafeThrust -= dt / safeDescendTime;
	thrustTarget = max(rcFailsafeThrust, 0.0f);
	if (rcFailsafeThrust <= 0) {
		forceDisarm("RC failsafe descent complete");
		rcFailsafeActive = false;
	}
}

// Allow pilot to interrupt automatic flight
void autoFailsafe() {
	static float roll, pitch, yaw, throttle;
	if (rcFailsafeActive || autoFlightActive()) return; // automatic flight has explicit takeover handling
	if (roll != controlRoll || pitch != controlPitch || yaw != controlYaw || abs(throttle - controlThrottle) > 0.05) {
		// controls changed
		if (mode == AUTO) mode = STAB; // regain control by the pilot
	}
	roll = controlRoll;
	pitch = controlPitch;
	yaw = controlYaw;
	throttle = controlThrottle;
}
