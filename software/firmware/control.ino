// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Shared flight-control state and the top-level controller pipeline.
// Read next: control_modes -> control_auto_flight/control_altitude/control_position -> control_stabilization.

#include "vector.h"
#include "quaternion.h"
#include "pid.h"
#include "lpf.h"
#include "util.h"

#define PITCHRATE_P 0.05
#define PITCHRATE_I 0.2
#define PITCHRATE_D 0.001
#define PITCHRATE_I_LIM 0.3
#define ROLLRATE_P PITCHRATE_P
#define ROLLRATE_I PITCHRATE_I
#define ROLLRATE_D PITCHRATE_D
#define ROLLRATE_I_LIM PITCHRATE_I_LIM
#define YAWRATE_P 0.3
#define YAWRATE_I 0.0
#define YAWRATE_D 0.0
#define YAWRATE_I_LIM 0.3
#define ROLL_P 4.47f
#define ROLL_I 0
#define ROLL_D 0
#define PITCH_P ROLL_P
#define PITCH_I ROLL_I
#define PITCH_D ROLL_D
#define YAW_P 3
#define PITCHRATE_MAX radians(360)
#define ROLLRATE_MAX radians(360)
#define YAWRATE_MAX radians(300)
#define TILT_MAX radians(30)
#define RATES_D_LPF_ALPHA 0.2 // cutoff frequency ~ 40 Hz

// Keep the published custom-mode values stable while exposing only the modes
// used by the minimal aircraft. AUTO is internal to takeoff, landing, and
// validated Offboard control.
const int STAB = 2, AUTO = 3, ALT_HOLD = 4, POS_HOLD = 5;
int mode = STAB;
bool armed = false;
bool externalModeOverride = false;
float externalModeControlValue = NAN;
enum ActuatorOwner { ACTUATOR_NONE, ACTUATOR_PILOT, ACTUATOR_OFFBOARD_ATTITUDE, ACTUATOR_FAILSAFE, ACTUATOR_AUTONOMOUS, ACTUATOR_OFFBOARD_LOCAL };
ActuatorOwner actuatorOwner = ACTUATOR_NONE;
extern bool offboardActive;
extern bool offboardFailsafeActive;
extern double offboardControlTime;
extern bool flowCtrlUsingFlow, flowAirborne, flowBiasReady, flowPositionGateOpen;
extern bool tofHealthy, opticalFlowHealthy;
extern uint32_t tofTimestamp;
extern bool tofPacketHealthy, tofRangeInBlindZone;
extern uint32_t tofPacketTimestamp;
extern bool landed;
extern bool flightWasAirborne;
extern bool assistedTakeoffGroundIdle;
bool offboardLocalActive = false;
bool offboardUsePositionXY = false;
bool offboardUseVelocityXY = false;
bool offboardUseAltitude = false;
bool offboardUseVerticalSpeed = false;
float offboardTargetX = 0.0f, offboardTargetY = 0.0f, offboardTargetZ = 0.0f;
float offboardTargetVX = 0.0f, offboardTargetVY = 0.0f, offboardTargetVZ = 0.0f;

enum OffboardSetpointKind { OFFBOARD_SETPOINT_NONE, OFFBOARD_SETPOINT_ATTITUDE, OFFBOARD_SETPOINT_LOCAL };
int stagedOffboardKind = OFFBOARD_SETPOINT_NONE;
double stagedOffboardStart = 0.0;
double stagedOffboardLast = 0.0;
uint16_t stagedOffboardSamples = 0;
const float offboardWarmupTime = 0.35f;
const float offboardWarmupMaxGap = 0.15f;
const float offboardWarmupMinRate = 10.0f;

Quaternion stagedOffboardAttitude;
Vector stagedOffboardRates;
float stagedOffboardThrust = 0.0f;
bool stagedOffboardAttitudeIgnored = false;
bool stagedOffboardUsePositionXY = false;
bool stagedOffboardUseVelocityXY = false;
bool stagedOffboardUseAltitude = false;
bool stagedOffboardUseVerticalSpeed = false;
float stagedOffboardTargetX = 0.0f, stagedOffboardTargetY = 0.0f, stagedOffboardTargetZ = 0.0f;
float stagedOffboardTargetVX = 0.0f, stagedOffboardTargetVY = 0.0f, stagedOffboardTargetVZ = 0.0f;
bool stagedOffboardUseYaw = false;
float stagedOffboardYaw = 0.0f;
float stagedOffboardYawRate = 0.0f;

PID rollRatePID(ROLLRATE_P, ROLLRATE_I, ROLLRATE_D, ROLLRATE_I_LIM, RATES_D_LPF_ALPHA);
PID pitchRatePID(PITCHRATE_P, PITCHRATE_I, PITCHRATE_D, PITCHRATE_I_LIM, RATES_D_LPF_ALPHA);
PID yawRatePID(YAWRATE_P, YAWRATE_I, YAWRATE_D);
PID rollPID(ROLL_P, ROLL_I, ROLL_D);
PID pitchPID(PITCH_P, PITCH_I, PITCH_D);
PID yawPID(YAW_P, 0, 0);
Vector maxRate(ROLLRATE_MAX, PITCHRATE_MAX, YAWRATE_MAX);
float tiltMax = TILT_MAX;
int flightModes[] = {STAB, ALT_HOLD, POS_HOLD}; // map for rc mode switch: 中档=定高, 高档=定点

Quaternion attitudeTarget;
Vector ratesTarget;
Vector ratesExtra; // feedforward rates
Vector torqueTarget;
float thrustTarget;
float mixerScale = 1.0f;

extern const int MOTOR_REAR_LEFT, MOTOR_REAR_RIGHT, MOTOR_FRONT_RIGHT, MOTOR_FRONT_LEFT;
extern float controlRoll, controlPitch, controlThrottle, controlYaw, controlMode;
extern double controlTime;
// Cross-module declarations: estimator state is defined in estimate.ino; position-control state in control_position.ino.
extern uint32_t opticalFlowSequence;
extern float opticalFlowSampleDt;
extern uint32_t tofSequence;
extern float tofSampleDt;
extern float posRollCmd;
extern float posPitchCmd;
extern bool usePosCmd;
extern float altitudeHoverThrust;
extern float altitudeHoldMaxCorrection;
extern float altitudeStickDeadband;
extern bool altitudeHoldEngaged;
extern float altitudeHoldTarget;
extern float altitudeHoldIntegral;
extern bool rcLinkHealthy, rcPilotActive;
extern bool mavlinkArmSession;
extern float voltage;
extern int voltagePin;
extern float voltageCompensationMax;
extern const char *preArmFailure;

enum AutoFlightPhase { AUTO_FLIGHT_IDLE, AUTO_TAKEOFF, AUTO_LAND_DESCEND, AUTO_LAND_FLARE };
enum AutoFlightSource { AUTO_SOURCE_NONE, AUTO_SOURCE_PILOT, AUTO_SOURCE_MAVLINK };
AutoFlightPhase autoFlightPhase = AUTO_FLIGHT_IDLE;
AutoFlightSource autoFlightSource = AUTO_SOURCE_NONE;
float autoFlightTargetHeight = 0.0f;
float autoFlightGoalHeight = 0.0f;
double autoFlightPhaseStart = 0.0;
float autoFlightYaw = 0.0f;
float autoFlightGroundHeight = 0.0f;
float autoFlightGroundRange = 0.0f;
float autoTakeoffThrustLimit = 0.20f; // start above the 8520 dead zone so the attitude loop has authority to level the airframe while still on the ground
float autoTakeoffTargetX = 0.0f;
float autoTakeoffTargetY = 0.0f;
float autoLandingTargetX = 0.0f;
float autoLandingTargetY = 0.0f;
bool autoTakeoffTargetValid = false;
bool autoLandingTargetValid = false;
double autoTakeoffControlTimeAtStart = 0.0;
int autoFlightReturnMode = ALT_HOLD;
bool autoFlightPilotTriggered = false;
bool assistedTakeoffGroundIdle = true;
bool flightWasAirborne = false; // latched per arm cycle; unlike flowAirborne it survives the flare
bool autoLandingRelockPending = false;
bool autoLandingFlareLatched = false;
double autoLandingFlareStart = 0.0;

float altitudeTakeoffHeight = 0.60f;
float altitudeTakeoffTrigger = 0.625f;
float altitudeTakeoffMaxThrust = 0.90f;
const float altitudeTakeoffClimbRate = 0.40f;
const float altitudeTakeoffTargetLead = 0.12f;
const float altitudeTakeoffThrustSlew = 0.45f; // slow enough that the attitude loop has ~0.8 s to
                                                // level an inclined airframe before liftoff; 0.66 was
                                                // smooth but lifted a tilted aircraft before correction
const float automaticLandingFlareClearance = 0.14f;
const char *externalModeFailure = "not checked";
const char *automaticFlightFailure = "not checked";

bool autoFlightActive() { return autoFlightPhase != AUTO_FLIGHT_IDLE; }
int getAutoFlightPhaseValue() { return (int)autoFlightPhase; }
bool autoAltitudeActive() {
	return autoFlightPhase == AUTO_TAKEOFF ||
		autoFlightPhase == AUTO_LAND_DESCEND || autoFlightPhase == AUTO_LAND_FLARE;
}

void resetAutomaticLandingFlare() {
	autoLandingFlareLatched = false;
	autoLandingFlareStart = 0.0f;
}

bool autoLandingFlareCutActive() {
	return autoFlightPhase == AUTO_LAND_FLARE && autoLandingFlareLatched;
}

bool pilotControlFresh() {
	return (rcPilotActive && rcLinkHealthy) || mavlinkManualControlHealthy();
}

void control() {
	updateAltitudeHoverFeedForward();
	interpretControls();
	failsafe();
	updateAutoFlightControl();
	updateAltitudeHoldControl();
	// Always update horizontal hold so leaving POS_HOLD clears its lock point.
	// Re-entry captures the current position after sensor qualification.
	updatePositionControlSplit(dt);
	// Apply horizontal correction only when POS_HOLD is the selected source.
	bool positionCorrectionRequested = mode == POS_HOLD ||
		(mode == AUTO && ((autoFlightActive() && autoFlightReturnMode == POS_HOLD) || offboardLocalActive));
	if (positionCorrectionRequested && usePosCmd) {
		attitudeTarget = Quaternion::fromEuler(Vector(
			posRollCmd,
			posPitchCmd,
			attitudeTarget.getYaw()));
	}
	controlAttitude();
	controlRates();
	controlTorque();
}
