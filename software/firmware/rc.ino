// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Work with the RC receiver

#include <SBUS.h>
#include "util.h"

// osrbot PIN_MAP
SBUS rc(Serial2,44,9); // NOTE: Use RC(Serial2, Rx, Tx)

uint16_t channels[16]; // raw rc channels
double controlTime; // monotonic time of the last controls update
// Compiled defaults, used when the key is missing from NVS (see setupParameters).
// Values match the operator's RC calibration: ZERO_0/1/3 = 1023, ZERO_2/6 = 240,
// MAX_0/1/2/3/6 = 1807 (SBUS nominal range 172..1811).
float channelZero[16] = {1023.0f, 1023.0f, 240.0f, 1023.0f, 0, 0, 240.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0}; // calibration zero values
float channelMax[16] = {1807.0f, 1807.0f, 1807.0f, 1807.0f, 0, 0, 1807.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0}; // calibration max values

// Channels mapping (using float to store in parameters):
float rollChannel = 0, pitchChannel = 1, throttleChannel = 2, yawChannel = 3, modeChannel = 6;

float controlRoll, controlPitch, controlYaw, controlThrottle; // pilot's inputs, range [-1, 1]
float controlMode = NAN;
bool rcLinkHealthy = false;
bool rcReceiverFailsafe = false;
// A receiver may emit clean-looking, static SBUS frames before its transmitter
// has ever linked. Keep transport health separate from proven pilot intent so
// those frames cannot lock out a MAVLink controller at boot.
bool rcPilotActive = false;
uint8_t rcConsecutiveLostFrames = 0;
uint8_t rcRecoveryFrames = 0;
uint8_t rcIntentFrames = 0;
uint32_t rcLastFrameMs = 0;
bool rcIntentReferenceValid = false;
bool rcIntentNeutralReady = false;
uint32_t rcIntentNeutralSinceMs = 0;
float rcIntentReferenceRoll = 0.0f;
float rcIntentReferencePitch = 0.0f;
float rcIntentReferenceYaw = 0.0f;
float rcIntentReferenceThrottle = 0.0f;
float rcIntentReferenceMode = 0.0f;
bool rcObservedControlsValid = false;
float rcObservedRoll = 0.0f;
float rcObservedPitch = 0.0f;
float rcObservedYaw = 0.0f;
float rcObservedThrottle = 0.0f;
float rcObservedMode = 0.0f;
uint32_t rcLastObservedMotionMs = 0;
bool rcEmergencyReleaseSeen = false;
uint32_t rcEmergencyGestureStartMs = 0;

const uint32_t RC_FRAME_TIMEOUT_MS = 150;
const uint32_t RC_GROUND_RELEASE_QUIET_MS = 1000;
const uint32_t RC_INTENT_NEUTRAL_HOLD_MS = 200;
const uint32_t RC_EMERGENCY_HOLD_MS = 150;
const uint8_t RC_LOST_FRAME_FAILSAFE_COUNT = 3;

bool rcControlsNeutralForIntent() {
	return abs(controlRoll) < 0.15f && abs(controlPitch) < 0.15f &&
		abs(controlYaw) < 0.15f && controlThrottle < 0.10f;
}

void resetRCEmergencyGesture() {
	rcEmergencyReleaseSeen = false;
	rcEmergencyGestureStartMs = 0;
}

bool rcEmergencyDisarmRequested() {
	if (!rcPilotActive || !rcLinkHealthy || rcReceiverFailsafe) {
		resetRCEmergencyGesture();
		return false;
	}
	bool emergencyCorner = controlThrottle < 0.05f && controlYaw < -0.95f;
	if (!emergencyCorner) {
		rcEmergencyReleaseSeen = true;
		rcEmergencyGestureStartMs = 0;
		return false;
	}
	// A receiver may boot or fail into a static bottom-left value. It must first
	// publish a non-emergency position after becoming the active pilot, then enter
	// and hold the corner. This preserves a fast physical emergency stop without
	// allowing stale SBUS values to stop a MAVLink flight.
	if (!rcEmergencyReleaseSeen) return false;
	if (rcEmergencyGestureStartMs == 0) {
		rcEmergencyGestureStartMs = millis();
		return false;
	}
	return millis() - rcEmergencyGestureStartMs >= RC_EMERGENCY_HOLD_MS;
}

bool rcArmedTakeoverRequested() {
	if (!rcPilotActive || !rcLinkHealthy || rcReceiverFailsafe) return false;
	// Merely receiving SBUS frames must not steal a MAVLink-armed flight. Require
	// an unmistakable stick/throttle command; emergency disarm remains an
	// independent path and is checked before ordinary ownership transfer.
	return abs(controlRoll) > 0.20f || abs(controlPitch) > 0.20f ||
		abs(controlYaw) > 0.25f || controlThrottle > 0.15f;
}

void resetRCLink(bool receiverFailsafe) {
	rcLinkHealthy = false;
	rcReceiverFailsafe = receiverFailsafe;
	rcPilotActive = false;
	rcRecoveryFrames = 0;
	rcConsecutiveLostFrames = 0;
	rcIntentFrames = 0;
	rcIntentReferenceValid = false;
	rcIntentNeutralReady = false;
	rcIntentNeutralSinceMs = 0;
	rcObservedControlsValid = false;
	resetRCEmergencyGesture();
}

bool rcGroundReleaseAllowed() {
	return !armed && rcPilotActive && rcObservedControlsValid &&
		millis() - rcLastObservedMotionMs >= RC_GROUND_RELEASE_QUIET_MS;
}

void releaseRCGroundControl() {
	if (!rcGroundReleaseAllowed()) return;
	rcPilotActive = false;
	rcIntentFrames = 0;
	rcIntentReferenceValid = true;
	rcIntentNeutralReady = rcControlsNeutralForIntent();
	rcIntentNeutralSinceMs = rcIntentNeutralReady ? millis() : 0;
	rcIntentReferenceRoll = rcObservedRoll;
	rcIntentReferencePitch = rcObservedPitch;
	rcIntentReferenceYaw = rcObservedYaw;
	rcIntentReferenceThrottle = rcObservedThrottle;
	rcIntentReferenceMode = rcObservedMode;
	resetRCEmergencyGesture();
}

void setupRC() {
	print("Setup RC\n");
	rc.begin();
}

bool readRC() {
	if (rc.read()) {
		SBUSData data = rc.data();
		rcLastFrameMs = millis();
		if (data.failsafe) {
			resetRCLink(true);
			return false; // never refresh controlTime with receiver failsafe values
		}
		if (data.lost_frame) {
			if (rcConsecutiveLostFrames < UINT8_MAX) rcConsecutiveLostFrames++;
			// Hold the last known controls across an isolated lost-frame marker. The
			// receiver's explicit failsafe bit remains immediate; repeated lost frames
			// or the 150 ms transport timeout still fail closed.
			if (rcConsecutiveLostFrames >= RC_LOST_FRAME_FAILSAFE_COUNT) {
				resetRCLink(true);
			}
			return false;
		}
		rcConsecutiveLostFrames = 0;
		float previousRoll = controlRoll;
		float previousPitch = controlPitch;
		float previousYaw = controlYaw;
		float previousThrottle = controlThrottle;
		float previousMode = controlMode;
		for (int i = 0; i < 16; i++) channels[i] = data.ch[i]; // copy channels data
		normalizeRC();
		if (!validRCConfiguration() || !isfinite(controlRoll) || !isfinite(controlPitch) ||
			!isfinite(controlYaw) || !isfinite(controlThrottle) || !isfinite(controlMode)) {
			controlRoll = previousRoll;
			controlPitch = previousPitch;
			controlYaw = previousYaw;
			controlThrottle = previousThrottle;
			controlMode = previousMode;
			resetRCLink(true);
			return false;
		}
		if (rcRecoveryFrames < 5) rcRecoveryFrames++;
		rcLinkHealthy = rcRecoveryFrames >= 5;
		rcReceiverFailsafe = false;
		if (!rcLinkHealthy) {
			controlRoll = previousRoll;
			controlPitch = previousPitch;
			controlYaw = previousYaw;
			controlThrottle = previousThrottle;
			controlMode = previousMode;
			return false;
		}
		bool observedMotion = rcObservedControlsValid &&
			(abs(controlRoll - rcObservedRoll) > 0.02f ||
			 abs(controlPitch - rcObservedPitch) > 0.02f ||
			 abs(controlYaw - rcObservedYaw) > 0.02f ||
			 abs(controlThrottle - rcObservedThrottle) > 0.02f ||
			 abs(controlMode - rcObservedMode) > 0.05f);
		rcObservedRoll = controlRoll;
		rcObservedPitch = controlPitch;
		rcObservedYaw = controlYaw;
		rcObservedThrottle = controlThrottle;
		rcObservedMode = controlMode;
		if (!rcObservedControlsValid || observedMotion) rcLastObservedMotionMs = millis();
		rcObservedControlsValid = true;

		if (!rcPilotActive) {
			bool neutralControls = rcControlsNeutralForIntent();
			if (!rcIntentNeutralReady) {
				if (neutralControls) {
					if (rcIntentNeutralSinceMs == 0) rcIntentNeutralSinceMs = millis();
					if (millis() - rcIntentNeutralSinceMs >= RC_INTENT_NEUTRAL_HOLD_MS) {
						rcIntentNeutralReady = true;
						rcIntentReferenceRoll = controlRoll;
						rcIntentReferencePitch = controlPitch;
						rcIntentReferenceYaw = controlYaw;
						rcIntentReferenceThrottle = controlThrottle;
						rcIntentReferenceMode = controlMode;
						rcIntentReferenceValid = true;
					}
				} else {
					rcIntentNeutralSinceMs = 0;
				}
			}
			if (!rcIntentReferenceValid) {
				rcIntentReferenceRoll = controlRoll;
				rcIntentReferencePitch = controlPitch;
				rcIntentReferenceYaw = controlYaw;
				rcIntentReferenceThrottle = controlThrottle;
				rcIntentReferenceMode = controlMode;
				rcIntentReferenceValid = true;
			}
			bool emergencyCorner = controlThrottle < 0.05f && controlYaw < -0.95f;
			bool movedControls = rcIntentNeutralReady &&
				(abs(controlRoll - rcIntentReferenceRoll) > 0.08f ||
				 abs(controlPitch - rcIntentReferencePitch) > 0.08f ||
				 abs(controlYaw - rcIntentReferenceYaw) > 0.08f ||
				 abs(controlThrottle - rcIntentReferenceThrottle) > 0.05f);
			if (movedControls && !emergencyCorner) {
				if (rcIntentFrames < 3) rcIntentFrames++;
			} else {
				rcIntentFrames = 0;
			}
			bool activatePilot = rcIntentFrames >= 3;
			if (activatePilot) resetRCEmergencyGesture();
			rcPilotActive = activatePilot;
		}
		if (!rcPilotActive) {
			// Keep the active MAVLink/ground-safe controls untouched while SBUS is
			// merely present but has not demonstrated pilot intent.
			controlRoll = previousRoll;
			controlPitch = previousPitch;
			controlYaw = previousYaw;
			controlThrottle = previousThrottle;
			controlMode = previousMode;
			return false;
		}
		controlTime = t;
		return true;
	}
	if (rcLastFrameMs != 0 && millis() - rcLastFrameMs > RC_FRAME_TIMEOUT_MS) resetRCLink(true);
	return false;
}

bool validRCChannel(float channel) {
	return channel == -1.0f || (isfinite(channel) && channel >= 0.0f && channel <= 7.0f && channel == floorf(channel));
}

bool validRCConfiguration() {
	if (!validRCChannel(rollChannel) || !validRCChannel(pitchChannel) ||
		!validRCChannel(throttleChannel) || !validRCChannel(yawChannel) || !validRCChannel(modeChannel)) return false;
	const float mapped[] = {rollChannel, pitchChannel, throttleChannel, yawChannel, modeChannel};
	for (int i = 0; i < 5; i++) {
		float channel = mapped[i];
		if (channel < 0) continue;
		int index = (int)channel;
		if (!isfinite(channelZero[index]) || !isfinite(channelMax[index]) ||
			abs(channelMax[index] - channelZero[index]) < 20.0f) return false;
		for (int j = i + 1; j < 5; j++) {
			if (mapped[j] >= 0 && mapped[j] == channel) return false;
		}
	}
	return true;
}

void normalizeRC() {
	float controls[16];
	for (int i = 0; i < 16; i++) {
		float span = channelMax[i] - channelZero[i];
		controls[i] = abs(span) >= 20.0f ? mapf(channels[i], channelZero[i], channelMax[i], 0, 1) : NAN;
	}
	// Update control values
	controlRoll = rollChannel >= 0 ? constrain(controls[(int)rollChannel], -1.0f, 1.0f) : NAN;
	controlPitch = pitchChannel >= 0 ? constrain(controls[(int)pitchChannel], -1.0f, 1.0f) : NAN;
	controlYaw = yawChannel >= 0 ? constrain(controls[(int)yawChannel], -1.0f, 1.0f) : NAN;
	controlThrottle = throttleChannel >= 0 ? constrain(controls[(int)throttleChannel], 0.0f, 1.0f) : NAN;
	controlMode = modeChannel >= 0 ? constrain(controls[(int)modeChannel], 0.0f, 1.0f) : NAN;
}

void calibrateRC() {
	float oldZero[16], oldMax[16];
	memcpy(oldZero, channelZero, sizeof(oldZero));
	memcpy(oldMax, channelMax, sizeof(oldMax));
	float oldRoll = rollChannel;
	float oldPitch = pitchChannel;
	float oldThrottle = throttleChannel;
	float oldYaw = yawChannel;
	float oldMode = modeChannel;

	uint16_t zero[16];
	uint16_t center[16];
	uint16_t max[16];
	print("1/8 Calibrating RC: put all switches to default positions [3 sec]\n");
	pause(3);
	calibrateRCChannel(NULL, zero, zero, "2/8 Move sticks [3 sec]\n...     ...\n...     .o.\n.o.     ...\n");
	calibrateRCChannel(NULL, center, center, "3/8 Move sticks [3 sec]\n...     ...\n.o.     .o.\n...     ...\n");
	calibrateRCChannel(&throttleChannel, zero, max, "4/8 Move sticks [3 sec]\n.o.     ...\n...     .o.\n...     ...\n");
	calibrateRCChannel(&yawChannel, center, max, "5/8 Move sticks [3 sec]\n...     ...\n..o     .o.\n...     ...\n");
	calibrateRCChannel(&pitchChannel, zero, max, "6/8 Move sticks [3 sec]\n...     .o.\n...     ...\n.o.     ...\n");
	calibrateRCChannel(&rollChannel, zero, max, "7/8 Move sticks [3 sec]\n...     ...\n...     ..o\n.o.     ...\n");
	calibrateRCChannel(&modeChannel, zero, max, "8/8 Put mode switch to max [3 sec]\n");
	if (!validRCConfiguration()) {
		memcpy(channelZero, oldZero, sizeof(oldZero));
		memcpy(channelMax, oldMax, sizeof(oldMax));
		rollChannel = oldRoll;
		pitchChannel = oldPitch;
		throttleChannel = oldThrottle;
		yawChannel = oldYaw;
		modeChannel = oldMode;
		print("RC calibration rejected: incomplete or invalid; previous calibration restored\n");
		printRCCalibration();
		return;
	}
	print("RC calibration accepted; values will be saved while disarmed\n");
	printRCCalibration();
}

void calibrateRCChannel(float *channel, uint16_t in[16], uint16_t out[16], const char *str) {
	print("%s", str);
	pause(3);
	for (int i = 0; i < 30; i++) readRC(); // try update 30 times max
	memcpy(out, channels, sizeof(channels));

	if (channel == NULL) return; // no channel to calibrate

	// Find channel that changed the most between in and out
	int ch = -1, diff = 0;
	for (int i = 0; i < 8; i++) {
		if (abs(out[i] - in[i]) > diff) {
			ch = i;
			diff = abs(out[i] - in[i]);
		}
	}
	if (ch >= 0 && diff > 20) {
		*channel = ch;
		channelZero[ch] = in[ch];
		channelMax[ch] = out[ch];
	} else {
		*channel = NAN;
	}
}

void printRCCalibration() {
	print("Control   Ch     Zero   Max\n");
	print("Roll      %-7g%-7g%-7g\n", rollChannel, rollChannel >= 0 ? channelZero[(int)rollChannel] : NAN, rollChannel >= 0 ? channelMax[(int)rollChannel] : NAN);
	print("Pitch     %-7g%-7g%-7g\n", pitchChannel, pitchChannel >= 0 ? channelZero[(int)pitchChannel] : NAN, pitchChannel >= 0 ? channelMax[(int)pitchChannel] : NAN);
	print("Yaw       %-7g%-7g%-7g\n", yawChannel, yawChannel >= 0 ? channelZero[(int)yawChannel] : NAN, yawChannel >= 0 ? channelMax[(int)yawChannel] : NAN);
	print("Throttle  %-7g%-7g%-7g\n", throttleChannel, throttleChannel >= 0 ? channelZero[(int)throttleChannel] : NAN, throttleChannel >= 0 ? channelMax[(int)throttleChannel] : NAN);
	print("Mode      %-7g%-7g%-7g\n", modeChannel, modeChannel >= 0 ? channelZero[(int)modeChannel] : NAN, modeChannel >= 0 ? channelMax[(int)modeChannel] : NAN);
}
