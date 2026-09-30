// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// MAVLink communication
// mavlinkSysId + telemetry rates parameterized, battery status added

#if WIFI_ENABLED

#include <MAVLink.h>
#include <stdarg.h>

#include "util.h"

extern double controlTime;
extern float voltage; // from power.ino
bool voltageAvailable();
extern bool offboardActive;
extern const char *externalModeFailure;
extern const char *automaticFlightFailure;
extern double offboardControlTime;
extern bool imuHealthy, rcLinkHealthy, rcReceiverFailsafe, rcPilotActive;
extern uint8_t offboardOwnerSysId, offboardOwnerCompId;
extern bool tofPacketHealthy;
extern uint32_t tofPacketTimestamp;
extern uint16_t opticalFlowTofDistanceMm;

int mavlinkSysId = 1;
Rate telemetrySlow(2); // 2 Hz slow telemetry
Rate telemetryFast(10); // 10 Hz fast telemetry
Rate mavlinkParameterStreamRate(20); // management traffic is bounded outside the control path
int mavlinkParameterStreamIndex = -1;
bool mavlinkTxUsedThisLoop = false;
uint8_t mavlinkSlowTelemetryPending = 0;
uint8_t mavlinkFastTelemetryPending = 0;
bool mavlinkLogStreamActive = false;
uint32_t mavlinkLogStreamOffset = 0;
uint32_t mavlinkLogStreamEnd = 0;

enum MavlinkSlowTelemetrySlot {
	MAV_SLOW_HEARTBEAT,
	MAV_SLOW_EXTENDED_STATE,
	MAV_SLOW_CURRENT_MODE,
	MAV_SLOW_AVAILABLE_MODES,
	MAV_SLOW_SYSTEM_STATUS,
	MAV_SLOW_BATTERY,
	MAV_SLOW_SLOT_COUNT
};

enum MavlinkFastTelemetrySlot {
	MAV_FAST_ATTITUDE,
	MAV_FAST_RC,
	MAV_FAST_ACTUATORS,
	MAV_FAST_IMU,
	MAV_FAST_LOCAL_POSITION,
	MAV_FAST_DISTANCE,
	MAV_FAST_SLOT_COUNT
};

int16_t mavlinkAccelMg(float acceleration) {
	if (!isfinite(acceleration)) return 0;
	float milligravity = acceleration * (1000.0f / ONE_G);
	if (milligravity >= INT16_MAX) return INT16_MAX;
	if (milligravity <= INT16_MIN) return INT16_MIN;
	return (int16_t)roundf(milligravity);
}

bool mavlinkConnected = false;
uint32_t mavlinkLastRxMs = 0;
bool mavlinkManualControlActive = false;
uint32_t mavlinkManualControlLastMs = 0;
String mavlinkPrintBuffer;
extern bool mavlinkArmSession;

// A GCS may explicitly hand control from finite MANUAL_CONTROL commands to an
// assisted hold before warming up Offboard. Release only that MAVLink lease;
// physical SBUS ownership is untouched. Clearing the sample timestamp prevents
// stale low/high throttle from being interpreted after the mode transition.
void releaseMavlinkManualControlForMode(int requestedMode) {
	if (requestedMode != ALT_HOLD && requestedMode != POS_HOLD) return;
	mavlinkManualControlActive = false;
	mavlinkManualControlLastMs = 0;
	if (!rcPilotActive) {
		controlRoll = 0.0f;
		controlPitch = 0.0f;
		controlYaw = 0.0f;
		controlThrottle = 0.5f;
		controlMode = NAN;
		controlTime = 0.0;
	}
}

struct MavlinkFlightModeInfo {
	uint8_t customMode;
	uint8_t standardMode;
	uint32_t properties;
	const char *name;
};

const MavlinkFlightModeInfo mavlinkFlightModes[] = {
	{STAB, MAV_STANDARD_MODE_NON_STANDARD, 0, "Stabilize"},
	{ALT_HOLD, MAV_STANDARD_MODE_ALTITUDE_HOLD, 0, "Altitude Hold"},
	{POS_HOLD, MAV_STANDARD_MODE_POSITION_HOLD, 0, "Position Hold"},
	{AUTO, MAV_STANDARD_MODE_NON_STANDARD, MAV_MODE_PROPERTY_NOT_USER_SELECTABLE | MAV_MODE_PROPERTY_AUTO_MODE, "Automatic"},
	{6, MAV_STANDARD_MODE_TAKEOFF, MAV_MODE_PROPERTY_NOT_USER_SELECTABLE | MAV_MODE_PROPERTY_AUTO_MODE, "Takeoff"},
	{7, MAV_STANDARD_MODE_LAND, MAV_MODE_PROPERTY_NOT_USER_SELECTABLE | MAV_MODE_PROPERTY_AUTO_MODE, "Land"},
};
const uint8_t mavlinkFlightModeCount = sizeof(mavlinkFlightModes) / sizeof(mavlinkFlightModes[0]);
const uint8_t mavlinkAvailableModesSequence = 1;

uint8_t mavlinkStandardModeFor(int requestedMode) {
	if (requestedMode == ALT_HOLD) return MAV_STANDARD_MODE_ALTITUDE_HOLD;
	if (requestedMode == POS_HOLD) return MAV_STANDARD_MODE_POSITION_HOLD;
	if (requestedMode == AUTO && autoFlightPhase == AUTO_TAKEOFF) {
		return MAV_STANDARD_MODE_TAKEOFF;
	}
	if (requestedMode == AUTO && (autoFlightPhase == AUTO_LAND_DESCEND ||
		autoFlightPhase == AUTO_LAND_FLARE)) return MAV_STANDARD_MODE_LAND;
	return MAV_STANDARD_MODE_NON_STANDARD;
}

uint8_t mavlinkBaseMode() {
	uint8_t flags = MAV_MODE_FLAG_CUSTOM_MODE_ENABLED;
	if (armed) flags |= MAV_MODE_FLAG_SAFETY_ARMED;
	if (mode == AUTO) flags |= MAV_MODE_FLAG_AUTO_ENABLED;
	else {
		flags |= MAV_MODE_FLAG_MANUAL_INPUT_ENABLED;
		if (mode == STAB || mode == ALT_HOLD || mode == POS_HOLD) flags |= MAV_MODE_FLAG_STABILIZE_ENABLED;
		if (mode == ALT_HOLD || mode == POS_HOLD) flags |= MAV_MODE_FLAG_GUIDED_ENABLED;
	}
	return flags;
}

void sendMavlinkStatusText(uint8_t severity, const char *text) {
	if (!mavlinkConnected || !text) return;
	char bounded[51] = {};
	strlcpy(bounded, text, sizeof(bounded));
	mavlink_message_t msg;
	mavlink_msg_statustext_pack(mavlinkSysId, MAV_COMP_ID_AUTOPILOT1, &msg, severity, bounded, 0, 0);
	sendMessage(&msg);
}

void sendMavlinkStatusTextf(uint8_t severity, const char *format, ...) {
	char text[51] = {};
	va_list args;
	va_start(args, format);
	vsnprintf(text, sizeof(text), format, args);
	va_end(args);
	sendMavlinkStatusText(severity, text);
}

void reportOffboardConflict(uint8_t sysid, uint8_t compid) {
	static uint32_t lastReportMs = UINT32_MAX;
	if (lastReportMs != UINT32_MAX && millis() - lastReportMs < 1000) return;
	lastReportMs = millis();
	sendMavlinkStatusTextf(MAV_SEVERITY_WARNING,
		"Offboard denied: controller %u/%u owns control", offboardOwnerSysId, offboardOwnerCompId);
	print("Offboard command from %u/%u denied: owner %u/%u\n",
		sysid, compid, offboardOwnerSysId, offboardOwnerCompId);
}

bool claimMavlinkOffboardControl(uint8_t sysid, uint8_t compid) {
	if (claimOffboardControl(sysid, compid)) return true;
	reportOffboardConflict(sysid, compid);
	return false;
}

bool sendAvailableMode(uint8_t modeIndex) {
	if (modeIndex < 1 || modeIndex > mavlinkFlightModeCount) return false;
	const MavlinkFlightModeInfo& available = mavlinkFlightModes[modeIndex - 1];
	mavlink_message_t msg;
	mavlink_msg_available_modes_pack(mavlinkSysId, MAV_COMP_ID_AUTOPILOT1, &msg,
		mavlinkFlightModeCount, modeIndex, available.standardMode, available.customMode,
		available.properties, available.name);
	sendMessage(&msg);
	return true;
}

void sendCurrentMode() {
	mavlink_message_t msg;
	mavlink_msg_current_mode_pack(mavlinkSysId, MAV_COMP_ID_AUTOPILOT1, &msg,
		mavlinkStandardModeFor(mode), mode, mode);
	sendMessage(&msg);
}

void sendAutopilotVersion() {
	mavlink_message_t msg;
	const uint8_t flightCustomVersion[8] = {'s', 'a', 'f', 'e', 't', 'y', '2', 0};
	const uint8_t emptyVersion[8] = {};
	const uint8_t emptyUid[18] = {};
	uint64_t capabilities = MAV_PROTOCOL_CAPABILITY_PARAM_FLOAT |
		MAV_PROTOCOL_CAPABILITY_SET_ATTITUDE_TARGET |
		MAV_PROTOCOL_CAPABILITY_SET_POSITION_TARGET_LOCAL_NED |
		MAV_PROTOCOL_CAPABILITY_MAVLINK2;
	mavlink_msg_autopilot_version_pack(mavlinkSysId, MAV_COMP_ID_AUTOPILOT1, &msg,
		capabilities, OPEN32DRONE_FLIGHT_SW_VERSION, 0, 0, 1,
		flightCustomVersion, emptyVersion, emptyVersion, 0, 0, 0, emptyUid);
	sendMessage(&msg);
}

void sendSystemStatus(uint16_t measuredVoltage, int8_t remaining) {
	uint32_t present = MAV_SYS_STATUS_SENSOR_3D_GYRO | MAV_SYS_STATUS_SENSOR_3D_ACCEL |
		MAV_SYS_STATUS_SENSOR_ANGULAR_RATE_CONTROL | MAV_SYS_STATUS_SENSOR_ATTITUDE_STABILIZATION |
		MAV_SYS_STATUS_SENSOR_MOTOR_OUTPUTS | MAV_SYS_STATUS_SENSOR_RC_RECEIVER |
		MAV_SYS_STATUS_SENSOR_LASER_POSITION | MAV_SYS_STATUS_SENSOR_Z_ALTITUDE_CONTROL |
		MAV_SYS_STATUS_SENSOR_XY_POSITION_CONTROL | MAV_SYS_STATUS_PREARM_CHECK;
	// RC is an optional control source for a GCS-armed session. Advertising a
	// powered-off receiver as enabled-but-unhealthy makes a client show "Not Ready"
	// even when every mandatory flight sensor and the GCS link are healthy.
	uint32_t enabled = present & ~(MAV_SYS_STATUS_SENSOR_RC_RECEIVER |
		MAV_SYS_STATUS_SENSOR_XY_POSITION_CONTROL);
	if (rcPilotActive || (armed && !mavlinkArmSession)) enabled |= MAV_SYS_STATUS_SENSOR_RC_RECEIVER;
	// Ground XY flow is optional. Advertise the controller as enabled only after
	// valid flow is available; once airborne, keep it enabled so a later loss is
	// reported as unhealthy instead of being hidden.
	if (opticalFlowHealthy || flightWasAirborne) enabled |= MAV_SYS_STATUS_SENSOR_XY_POSITION_CONTROL;
	uint32_t health = MAV_SYS_STATUS_SENSOR_MOTOR_OUTPUTS;
	if (imuHealthy) health |= MAV_SYS_STATUS_SENSOR_3D_GYRO | MAV_SYS_STATUS_SENSOR_3D_ACCEL;
	if (imuHealthy && attitude.valid() && rates.valid()) {
		health |= MAV_SYS_STATUS_SENSOR_ANGULAR_RATE_CONTROL | MAV_SYS_STATUS_SENSOR_ATTITUDE_STABILIZATION;
	}
	if (rcPilotActive && rcLinkHealthy && !rcReceiverFailsafe) health |= MAV_SYS_STATUS_SENSOR_RC_RECEIVER;
	// A fresh TF-0850 blind-zone packet is valid ground evidence for assisted
	// takeoff even though it cannot provide a numeric range yet.
	if (tofGroundReady()) {
		health |= MAV_SYS_STATUS_SENSOR_LASER_POSITION | MAV_SYS_STATUS_SENSOR_Z_ALTITUDE_CONTROL;
	}
	if (opticalFlowHealthy) health |= MAV_SYS_STATUS_SENSOR_XY_POSITION_CONTROL;
	if (armed || preArmCheck(false)) health |= MAV_SYS_STATUS_PREARM_CHECK;
	mavlink_message_t msg;
	mavlink_msg_sys_status_pack(mavlinkSysId, MAV_COMP_ID_AUTOPILOT1, &msg,
		present, enabled, health, 0, measuredVoltage, -1, remaining,
		0, 0, 0, 0, 0, 0, 0, 0, 0);
	sendMessage(&msg);
}

void processMavlink() {
	mavlinkTxUsedThisLoop = false;
	sendMavlink();
	receiveMavlink();
	if (!mavlinkTxUsedThisLoop) sendPendingMavlinkParameter();
	if (!mavlinkTxUsedThisLoop) sendPendingMavlinkLogData();
}

void sendPendingMavlinkParameter() {
	if (!mavlinkConnected) {
		mavlinkParameterStreamIndex = -1;
		return;
	}
	if (mavlinkParameterStreamIndex < 0 || !mavlinkParameterStreamRate) return;
	if (mavlinkParameterStreamIndex >= parametersCount()) {
		mavlinkParameterStreamIndex = -1;
		return;
	}
	mavlink_message_t msg;
	int index = mavlinkParameterStreamIndex++;
	mavlink_msg_param_value_pack(mavlinkSysId, MAV_COMP_ID_AUTOPILOT1, &msg,
		getParameterName(index), getParameter(index), MAV_PARAM_TYPE_REAL32,
		parametersCount(), index);
	sendMessage(&msg);
}

void sendPendingMavlinkLogData() {
	if (!mavlinkConnected || armed) {
		mavlinkLogStreamActive = false;
		return;
	}
	if (!mavlinkLogStreamActive) return;
	if (mavlinkLogStreamOffset >= mavlinkLogStreamEnd) {
		mavlinkLogStreamActive = false;
		return;
	}
	uint8_t data[MAVLINK_MSG_LOG_DATA_FIELD_DATA_LEN] = {};
	uint8_t requested = (uint8_t)min((uint32_t)sizeof(data),
		mavlinkLogStreamEnd - mavlinkLogStreamOffset);
	uint8_t count = copyLogDataBytes(mavlinkLogStreamOffset, data, requested);
	if (count == 0) {
		mavlinkLogStreamActive = false;
		return;
	}
	mavlink_message_t msg;
	mavlink_msg_log_data_pack(mavlinkSysId, MAV_COMP_ID_AUTOPILOT1, &msg,
		0, mavlinkLogStreamOffset, count, data);
	mavlinkLogStreamOffset += count;
	if (mavlinkLogStreamOffset >= mavlinkLogStreamEnd) mavlinkLogStreamActive = false;
	sendMessage(&msg);
}

void copyMavlinkParameterId(const char *source, size_t sourceLength,
	char *destination, size_t destinationLength) {
	if (!destination || destinationLength == 0) return;
	size_t count = min(sourceLength, destinationLength - 1);
	memcpy(destination, source, count);
	destination[count] = '\0';
}

bool mavlinkTargetsThisAutopilot(uint8_t targetSystem, uint8_t targetComponent) {
	return (targetSystem == 0 || targetSystem == mavlinkSysId) &&
		(targetComponent == 0 || targetComponent == MAV_COMP_ID_AUTOPILOT1);
}

bool sendNextSlowTelemetry(uint32_t time) {
	mavlink_message_t msg;
	for (int slot = 0; slot < MAV_SLOW_SLOT_COUNT; slot++) {
		uint8_t bit = (uint8_t)(1U << slot);
		if (!(mavlinkSlowTelemetryPending & bit)) continue;
		mavlinkSlowTelemetryPending &= ~bit;
		if (slot != MAV_SLOW_HEARTBEAT && !mavlinkConnected) continue;
		switch ((MavlinkSlowTelemetrySlot)slot) {
			case MAV_SLOW_HEARTBEAT:
				mavlink_msg_heartbeat_pack(mavlinkSysId, MAV_COMP_ID_AUTOPILOT1, &msg,
					MAV_TYPE_QUADROTOR, MAV_AUTOPILOT_GENERIC, mavlinkBaseMode(), mode,
					armed ? MAV_STATE_ACTIVE : MAV_STATE_STANDBY);
				sendMessage(&msg);
				break;
			case MAV_SLOW_EXTENDED_STATE:
				mavlink_msg_extended_sys_state_pack(mavlinkSysId, MAV_COMP_ID_AUTOPILOT1, &msg,
					MAV_VTOL_STATE_UNDEFINED,
					landed ? MAV_LANDED_STATE_ON_GROUND : MAV_LANDED_STATE_IN_AIR);
				sendMessage(&msg);
				break;
			case MAV_SLOW_CURRENT_MODE:
				sendCurrentMode();
				break;
			case MAV_SLOW_AVAILABLE_MODES:
				mavlink_msg_available_modes_monitor_pack(mavlinkSysId,
					MAV_COMP_ID_AUTOPILOT1, &msg, mavlinkAvailableModesSequence);
				sendMessage(&msg);
				break;
			case MAV_SLOW_SYSTEM_STATUS: {
				bool available = voltageAvailable();
				uint16_t measured = available ?
					(uint16_t)constrain((int)roundf(voltage * 1000.0f), 0, 65534) : UINT16_MAX;
				sendSystemStatus(measured, -1);
				break;
			}
			case MAV_SLOW_BATTERY: {
				bool available = voltageAvailable();
				uint16_t measured = available ?
					(uint16_t)constrain((int)roundf(voltage * 1000.0f), 0, 65534) : UINT16_MAX;
				uint16_t voltages[] = {measured, UINT16_MAX, UINT16_MAX, UINT16_MAX,
					UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX};
				uint16_t voltagesExt[] = {0, 0, 0, 0};
				mavlink_msg_battery_status_pack(mavlinkSysId, MAV_COMP_ID_AUTOPILOT1, &msg,
					0, MAV_BATTERY_FUNCTION_ALL, MAV_BATTERY_TYPE_LIPO, INT16_MAX, voltages,
					-1, -1, -1, -1, 0, MAV_BATTERY_CHARGE_STATE_UNDEFINED,
					voltagesExt, 0, 0);
				sendMessage(&msg);
				break;
			}
			default: break;
		}
		return mavlinkTxUsedThisLoop;
	}
	return false;
}

bool sendNextFastTelemetry(uint32_t time) {
	mavlink_message_t msg;
	for (int slot = 0; slot < MAV_FAST_SLOT_COUNT; slot++) {
		uint8_t bit = (uint8_t)(1U << slot);
		if (!(mavlinkFastTelemetryPending & bit)) continue;
		mavlinkFastTelemetryPending &= ~bit;
		if (!mavlinkConnected) continue;
		switch ((MavlinkFastTelemetrySlot)slot) {
			case MAV_FAST_ATTITUDE: {
				const float zeroQuat[] = {0, 0, 0, 0};
				mavlink_msg_attitude_quaternion_pack(mavlinkSysId, MAV_COMP_ID_AUTOPILOT1,
					&msg, time, attitude.w, attitude.x, -attitude.y, -attitude.z,
					rates.x, -rates.y, -rates.z, zeroQuat);
				sendMessage(&msg);
				break;
			}
			case MAV_FAST_RC: {
				if (channels[0] == 0) continue;
				uint32_t controlTimeMs = controlTime > 0.0 ?
					(uint32_t)((uint64_t)(controlTime * 1000.0)) : 0;
				mavlink_msg_rc_channels_raw_pack(mavlinkSysId, MAV_COMP_ID_AUTOPILOT1,
					&msg, controlTimeMs, 0, channels[0], channels[1], channels[2], channels[3],
					channels[4], channels[5], channels[6], channels[7], UINT8_MAX);
				sendMessage(&msg);
				break;
			}
			case MAV_FAST_ACTUATORS: {
				float controls[8] = {};
				memcpy(controls, motors, sizeof(motors));
				mavlink_msg_actuator_control_target_pack(mavlinkSysId,
					MAV_COMP_ID_AUTOPILOT1, &msg, time, 0, controls);
				sendMessage(&msg);
				break;
			}
			case MAV_FAST_IMU:
				mavlink_msg_scaled_imu_pack(mavlinkSysId, MAV_COMP_ID_AUTOPILOT1, &msg, time,
					mavlinkAccelMg(acc.x), mavlinkAccelMg(-acc.y), mavlinkAccelMg(-acc.z),
					gyro.x * 1000, -gyro.y * 1000, -gyro.z * 1000, 0, 0, 0, 0);
				sendMessage(&msg);
				break;
			case MAV_FAST_LOCAL_POSITION: {
				Vector worldVelocity = worldVelocityEstimate();
				mavlink_msg_local_position_ned_pack(mavlinkSysId, MAV_COMP_ID_AUTOPILOT1,
					&msg, time, position.x, -position.y, -position.z,
					worldVelocity.x, -worldVelocity.y, -worldVelocity.z);
				sendMessage(&msg);
				break;
			}
			case MAV_FAST_DISTANCE: {
				if (!tofPacketHealthy || tofPacketTimestamp == 0 ||
					millis() - tofPacketTimestamp > 150) continue;
				const float sensorQuaternion[4] = {0, 0, 0, 0};
				mavlink_msg_distance_sensor_pack(mavlinkSysId, MAV_COMP_ID_AUTOPILOT1, &msg,
					time, 2, 600,
					constrain((int)roundf(opticalFlowTofDistanceMm / 10.0f), 0, 600),
					MAV_DISTANCE_SENSOR_LASER, 0, MAV_SENSOR_ROTATION_PITCH_270, UINT8_MAX,
					0.0f, 0.0f, sensorQuaternion, 0);
				sendMessage(&msg);
				break;
			}
			default: break;
		}
		return mavlinkTxUsedThisLoop;
	}
	return false;
}

void sendMavlink() {
	if (mavlinkConnected && millis() - mavlinkLastRxMs > 3000) {
		mavlinkConnected = false;
		mavlinkParameterStreamIndex = -1;
		mavlinkLogStreamActive = false;
		mavlinkFastTelemetryPending = 0;
	}
	sendMavlinkPrint();
	if (mavlinkTxUsedThisLoop) return;
	if (telemetrySlow) {
		mavlinkSlowTelemetryPending |= (uint8_t)(1U << MAV_SLOW_HEARTBEAT);
		if (mavlinkConnected) {
			mavlinkSlowTelemetryPending |= (uint8_t)((1U << MAV_SLOW_SLOT_COUNT) - 1U);
		}
	}
	if (telemetryFast && mavlinkConnected) {
		mavlinkFastTelemetryPending |= (uint8_t)((1U << MAV_FAST_SLOT_COUNT) - 1U);
	}
	uint32_t time = (uint32_t)(monotonicTimeUs / 1000ULL);
	if (sendNextSlowTelemetry(time)) return;
	sendNextFastTelemetry(time);
}

void sendMessage(const void *msg) {
	uint8_t buf[MAVLINK_MAX_PACKET_LEN];
	int len = mavlink_msg_to_send_buffer(buf, (mavlink_message_t *)msg);
	sendWiFi(buf, len);
	mavlinkTxUsedThisLoop = true;
}

void receiveMavlink() {
	uint8_t buf[MAVLINK_MAX_PACKET_LEN];
	int len = receiveWiFi(buf, MAVLINK_MAX_PACKET_LEN);

	// New packet, parse it
	mavlink_message_t msg;
	mavlink_status_t status;
	for (int i = 0; i < len; i++) {
		if (mavlink_parse_char(MAVLINK_COMM_0, buf[i], &msg, &status)) {
			mavlinkConnected = true;
			mavlinkLastRxMs = millis();
			handleMavlink(&msg);
		}
	}
}

void handleMavlink(const void *_msg) {
	const mavlink_message_t& msg = *(mavlink_message_t *)_msg;

	if (msg.msgid == MAVLINK_MSG_ID_SET_MODE) {
		mavlink_set_mode_t m;
		mavlink_msg_set_mode_decode(&msg, &m);
		if (m.target_system && m.target_system != mavlinkSysId) return;
		bool success = m.custom_mode <= POS_HOLD && requestExternalMode((int)m.custom_mode);
		if (!success) {
			const char *reason = m.custom_mode > POS_HOLD ? "unsupported mode" : externalModeFailure;
			sendMavlinkStatusTextf(MAV_SEVERITY_WARNING, "Mode denied: %s", reason);
		} else {
			releaseMavlinkManualControlForMode((int)m.custom_mode);
			sendMavlinkStatusTextf(MAV_SEVERITY_INFO, "Mode: %s", getModeName());
			sendCurrentMode();
		}
	}

	if (msg.msgid == MAVLINK_MSG_ID_MANUAL_CONTROL) {
		mavlink_manual_control_t m;
		mavlink_msg_manual_control_decode(&msg, &m);
		if (m.target && m.target != mavlinkSysId) return; // 0 is broadcast
		if (m.z < 0 || m.z > 1000 || abs(m.x) > 1000 || abs(m.y) > 1000 || abs(m.r) > 1000) return;
		// A physical transmitter has priority while airborne. On the ground, some
		// receivers keep transmitting the last non-zero channels after their radio
		// is switched off. A continuous neutral GCS stream may release that stale
		// ground lease only after the observed SBUS channels have remained unchanged.
		// Any subsequent physical stick movement immediately claims RC again.
		if (rcPilotActive) {
			bool neutralGroundRequest = !armed && m.z <= 50 &&
				abs(m.x) <= 50 && abs(m.y) <= 50 && abs(m.r) <= 50;
			if (neutralGroundRequest && rcGroundReleaseAllowed()) {
				releaseRCGroundControl();
				sendMavlinkStatusText(MAV_SEVERITY_INFO, "RC ground lease released to GCS");
			} else return;
		}
		if (!claimMavlinkOffboardControl(msg.sysid, msg.compid)) return;
		controlThrottle = constrain(m.z / 1000.0f, 0.0f, 1.0f);
		controlPitch = constrain(m.x / 1000.0f, -1.0f, 1.0f);
		controlRoll = constrain(m.y / 1000.0f, -1.0f, 1.0f);
		controlYaw = constrain(m.r / 1000.0f, -1.0f, 1.0f);
		controlMode = NAN;
		controlTime = t;
		mavlinkManualControlLastMs = millis();
		mavlinkManualControlActive = true;
		offboardActive = false;
		clearOffboardLocalControl();
		clearOffboardSetpointStaging();
		if (armed) actuatorOwner = ACTUATOR_PILOT;
	}

	if (msg.msgid == MAVLINK_MSG_ID_PARAM_REQUEST_LIST) {
		mavlink_param_request_list_t m;
		mavlink_msg_param_request_list_decode(&msg, &m);
		if (!mavlinkTargetsThisAutopilot(m.target_system, m.target_component)) return;

		// Stream one item at a bounded rate instead of sending the complete list
		// synchronously inside the flight loop.
		mavlinkParameterStreamIndex = 0;
	}

	if (msg.msgid == MAVLINK_MSG_ID_PARAM_REQUEST_READ) {
		mavlink_param_request_read_t m;
		mavlink_msg_param_request_read_decode(&msg, &m);
		if (!mavlinkTargetsThisAutopilot(m.target_system, m.target_component)) return;

		char requestedName[MAVLINK_MSG_PARAM_REQUEST_READ_FIELD_PARAM_ID_LEN + 1] = {};
		copyMavlinkParameterId(m.param_id,
			MAVLINK_MSG_PARAM_REQUEST_READ_FIELD_PARAM_ID_LEN,
			requestedName, sizeof(requestedName));
		int index = m.param_index >= 0 ? m.param_index : findParameterIndex(requestedName);
		if (index < 0 || index >= parametersCount()) {
			sendMavlinkStatusText(MAV_SEVERITY_WARNING, "Parameter read denied: unknown parameter");
			return;
		}
		mavlink_message_t msg;
		mavlink_msg_param_value_pack(mavlinkSysId, MAV_COMP_ID_AUTOPILOT1, &msg,
			getParameterName(index), getParameter(index), MAV_PARAM_TYPE_REAL32,
			parametersCount(), index);
		sendMessage(&msg);
	}

	if (msg.msgid == MAVLINK_MSG_ID_PARAM_SET) {
		mavlink_param_set_t m;
		mavlink_msg_param_set_decode(&msg, &m);
		if (!mavlinkTargetsThisAutopilot(m.target_system, m.target_component)) return;

		char name[MAVLINK_MSG_PARAM_SET_FIELD_PARAM_ID_LEN + 1] = {};
		copyMavlinkParameterId(m.param_id, MAVLINK_MSG_PARAM_SET_FIELD_PARAM_ID_LEN,
			name, sizeof(name));
		int index = findParameterIndex(name);
		if (index < 0) {
			sendMavlinkStatusText(MAV_SEVERITY_WARNING, "Parameter write denied: unknown parameter");
			return;
		}
		bool accepted = setParameter(name, m.param_value);
		float actual = getParameter(index);
		// PARAM_VALUE is both the successful acknowledgement and QGC's way to
		// reconcile a rejected write. Always return the authoritative current value.
		mavlink_message_t msg;
		mavlink_msg_param_value_pack(mavlinkSysId, MAV_COMP_ID_AUTOPILOT1, &msg,
			getParameterName(index), actual, MAV_PARAM_TYPE_REAL32, parametersCount(), index);
		sendMessage(&msg);
		if (!accepted) {
			sendMavlinkStatusText(MAV_SEVERITY_WARNING,
				armed ? "Parameter write denied while armed" : "Parameter write denied: invalid value");
		}
	}

	if (msg.msgid == MAVLINK_MSG_ID_SET_ATTITUDE_TARGET) {
		mavlink_set_attitude_target_t m;
		mavlink_msg_set_attitude_target_decode(&msg, &m);
		if (m.target_system && m.target_system != mavlinkSysId) return;
		bool stagingMode = mode == ALT_HOLD || mode == POS_HOLD;
		if (!armed || autoFlightActive() || offboardFailsafeActive ||
			(mode != AUTO && !stagingMode) || (mode == AUTO && !offboardActive)) return;
		if (!isfinite(m.thrust) || m.thrust < 0.0f || m.thrust > 1.0f) return;
		if (!(m.type_mask & ATTITUDE_TARGET_TYPEMASK_BODY_ROLL_RATE_IGNORE) && !isfinite(m.body_roll_rate)) return;
		if (!(m.type_mask & ATTITUDE_TARGET_TYPEMASK_BODY_PITCH_RATE_IGNORE) && !isfinite(m.body_pitch_rate)) return;
		if (!(m.type_mask & ATTITUDE_TARGET_TYPEMASK_BODY_YAW_RATE_IGNORE) && !isfinite(m.body_yaw_rate)) return;

		Quaternion requestedAttitude(m.q[0], m.q[1], -m.q[2], -m.q[3]);
		if (!(m.type_mask & ATTITUDE_TARGET_TYPEMASK_ATTITUDE_IGNORE)) {
			float targetNorm = requestedAttitude.norm();
			if (!isfinite(targetNorm) || targetNorm < 0.5f || targetNorm > 1.5f) return;
			requestedAttitude.normalize();
		}
		if (!claimMavlinkOffboardControl(msg.sysid, msg.compid)) return;

		// In ALT/POS the validated command is staged but never applied. AUTO can
		// only be selected after this stream has remained continuous long enough;
		// activation then copies the latest complete sample atomically.
		recordOffboardSetpoint(OFFBOARD_SETPOINT_ATTITUDE);
		stagedOffboardRates.x = (m.type_mask & ATTITUDE_TARGET_TYPEMASK_BODY_ROLL_RATE_IGNORE) ? 0.0f : m.body_roll_rate;
		stagedOffboardRates.y = (m.type_mask & ATTITUDE_TARGET_TYPEMASK_BODY_PITCH_RATE_IGNORE) ? 0.0f : -m.body_pitch_rate;
		stagedOffboardRates.z = (m.type_mask & ATTITUDE_TARGET_TYPEMASK_BODY_YAW_RATE_IGNORE) ? 0.0f : -m.body_yaw_rate;
		stagedOffboardAttitude = requestedAttitude;
		stagedOffboardAttitudeIgnored = m.type_mask & ATTITUDE_TARGET_TYPEMASK_ATTITUDE_IGNORE;
		stagedOffboardThrust = m.thrust;
		if (mode == AUTO && offboardActive) {
			applyStagedOffboardSetpoint();
			offboardControlTime = t;
		}
	}

	if (msg.msgid == MAVLINK_MSG_ID_SET_POSITION_TARGET_LOCAL_NED) {
		mavlink_set_position_target_local_ned_t m;
		mavlink_msg_set_position_target_local_ned_decode(&msg, &m);
		if (m.target_system && m.target_system != mavlinkSysId) return;
		if (!armed || autoFlightActive() || offboardFailsafeActive ||
			(mode != AUTO && mode != POS_HOLD) || (mode == AUTO && !offboardActive)) return;
		if (m.coordinate_frame != MAV_FRAME_LOCAL_NED) return;

		bool usePositionXY = !(m.type_mask & POSITION_TARGET_TYPEMASK_X_IGNORE) &&
			!(m.type_mask & POSITION_TARGET_TYPEMASK_Y_IGNORE);
		bool useVelocityXY = !(m.type_mask & POSITION_TARGET_TYPEMASK_VX_IGNORE) &&
			!(m.type_mask & POSITION_TARGET_TYPEMASK_VY_IGNORE);
		bool useAltitude = !(m.type_mask & POSITION_TARGET_TYPEMASK_Z_IGNORE);
		bool useVerticalSpeed = !(m.type_mask & POSITION_TARGET_TYPEMASK_VZ_IGNORE);
		bool partialXY = ((m.type_mask & POSITION_TARGET_TYPEMASK_X_IGNORE) != 0) !=
			((m.type_mask & POSITION_TARGET_TYPEMASK_Y_IGNORE) != 0);
		bool partialVXY = ((m.type_mask & POSITION_TARGET_TYPEMASK_VX_IGNORE) != 0) !=
			((m.type_mask & POSITION_TARGET_TYPEMASK_VY_IGNORE) != 0);
		bool accelerationRequested = !(m.type_mask & POSITION_TARGET_TYPEMASK_AX_IGNORE) ||
			!(m.type_mask & POSITION_TARGET_TYPEMASK_AY_IGNORE) ||
			!(m.type_mask & POSITION_TARGET_TYPEMASK_AZ_IGNORE);
		bool yawRequested = !(m.type_mask & POSITION_TARGET_TYPEMASK_YAW_IGNORE);
		bool yawRateRequested = !(m.type_mask & POSITION_TARGET_TYPEMASK_YAW_RATE_IGNORE);
		if (partialXY || partialVXY || accelerationRequested ||
			(!usePositionXY && !useVelocityXY) || (!useAltitude && !useVerticalSpeed)) return;
		if ((usePositionXY && (!isfinite(m.x) || !isfinite(m.y))) ||
			(useVelocityXY && (!isfinite(m.vx) || !isfinite(m.vy))) ||
			(useAltitude && !isfinite(m.z)) || (useVerticalSpeed && !isfinite(m.vz)) ||
			(yawRequested && !isfinite(m.yaw)) || (yawRateRequested && !isfinite(m.yaw_rate))) return;
		if (!claimMavlinkOffboardControl(msg.sysid, msg.compid)) return;

		recordOffboardSetpoint(OFFBOARD_SETPOINT_LOCAL);
		stagedOffboardUsePositionXY = usePositionXY;
		stagedOffboardUseVelocityXY = useVelocityXY;
		stagedOffboardUseAltitude = useAltitude;
		stagedOffboardUseVerticalSpeed = useVerticalSpeed;
		if (usePositionXY) {
			stagedOffboardTargetX = constrain(m.x, position.x - 1.0f, position.x + 1.0f);
			stagedOffboardTargetY = constrain(-m.y, position.y - 1.0f, position.y + 1.0f);
		}
		if (useVelocityXY) {
			stagedOffboardTargetVX = m.vx;
			stagedOffboardTargetVY = -m.vy;
			limitHorizontalSpeedCommand(
				stagedOffboardTargetVX, stagedOffboardTargetVY);
		}
		if (useAltitude) stagedOffboardTargetZ = constrain(-m.z, 0.05f, 5.80f);
		if (useVerticalSpeed) stagedOffboardTargetVZ = constrain(-m.vz, -0.45f, 0.45f);
		stagedOffboardUseYaw = yawRequested;
		if (stagedOffboardUseYaw) stagedOffboardYaw = -m.yaw;
		stagedOffboardYawRate = yawRateRequested ? -m.yaw_rate : 0.0f;
		if (mode == AUTO && offboardActive) {
			applyStagedOffboardSetpoint();
			offboardControlTime = t;
		}
	}

	if (msg.msgid == MAVLINK_MSG_ID_LOG_REQUEST_LIST) {
		mavlink_log_request_list_t m;
		mavlink_msg_log_request_list_decode(&msg, &m);
		if (!mavlinkTargetsThisAutopilot(m.target_system, m.target_component)) return;
		uint32_t size = logDataSizeBytes();
		bool logAvailable = size > 0 && m.start == 0;
		mavlink_message_t response;
		mavlink_msg_log_entry_pack(mavlinkSysId, MAV_COMP_ID_AUTOPILOT1, &response,
			0, logAvailable ? 1 : 0, 0, 0, logAvailable ? size : 0);
		sendMessage(&response);
	}

	if (msg.msgid == MAVLINK_MSG_ID_LOG_REQUEST_DATA) {
		mavlink_log_request_data_t m;
		mavlink_msg_log_request_data_decode(&msg, &m);
		if (!mavlinkTargetsThisAutopilot(m.target_system, m.target_component)) return;
		if (armed || m.id != 0) {
			sendMavlinkStatusText(MAV_SEVERITY_WARNING,
				armed ? "Log download denied while armed" : "Log download denied: invalid id");
			return;
		}
		uint32_t total = logDataSizeBytes();
		mavlinkLogStreamOffset = min(m.ofs, total);
		uint32_t available = total - mavlinkLogStreamOffset;
		uint32_t requested = m.count == UINT32_MAX ? available : min(m.count, available);
		mavlinkLogStreamEnd = mavlinkLogStreamOffset + requested;
		mavlinkLogStreamActive = mavlinkLogStreamOffset < mavlinkLogStreamEnd;
	}

	if (msg.msgid == MAVLINK_MSG_ID_LOG_REQUEST_END) {
		mavlink_log_request_end_t m;
		mavlink_msg_log_request_end_decode(&msg, &m);
		if (!mavlinkTargetsThisAutopilot(m.target_system, m.target_component)) return;
		mavlinkLogStreamActive = false;
	}

	// Handle commands
	if (msg.msgid == MAVLINK_MSG_ID_COMMAND_LONG) {
		mavlink_command_long_t m;
		mavlink_msg_command_long_decode(&msg, &m);
		if (m.target_system && m.target_system != mavlinkSysId) return;
		if (m.target_component && m.target_component != MAV_COMP_ID_AUTOPILOT1) return;
		uint8_t result = MAV_RESULT_UNSUPPORTED;
		uint8_t deferredStatusSeverity = MAV_SEVERITY_INFO;
		char deferredStatus[51] = {};

		if (m.command == MAV_CMD_REQUEST_MESSAGE) {
			uint32_t requestedMessage = isfinite(m.param1) ? (uint32_t)roundf(m.param1) : UINT32_MAX;
			if (requestedMessage == MAVLINK_MSG_ID_AUTOPILOT_VERSION) {
				sendAutopilotVersion();
				result = MAV_RESULT_ACCEPTED;
			} else if (requestedMessage == MAVLINK_MSG_ID_AVAILABLE_MODES) {
				uint8_t modeIndex = isfinite(m.param2) ? (uint8_t)roundf(m.param2) : 0;
				result = sendAvailableMode(modeIndex) ? MAV_RESULT_ACCEPTED : MAV_RESULT_DENIED;
			} else if (requestedMessage == MAVLINK_MSG_ID_CURRENT_MODE) {
				sendCurrentMode();
				result = MAV_RESULT_ACCEPTED;
			} else if (requestedMessage == MAVLINK_MSG_ID_AVAILABLE_MODES_MONITOR) {
				mavlink_message_t response;
				mavlink_msg_available_modes_monitor_pack(mavlinkSysId, MAV_COMP_ID_AUTOPILOT1,
					&response, mavlinkAvailableModesSequence);
				sendMessage(&response);
				result = MAV_RESULT_ACCEPTED;
			}
		}

		// MAVROS 2.12 requests the legacy capabilities command during link
		// discovery instead of MAV_CMD_REQUEST_MESSAGE(AUTOPILOT_VERSION).
		// Both commands have the same response and are part of the MAVLink
		// compatibility surface; this does not grant any control authority.
		if (m.command == MAV_CMD_REQUEST_AUTOPILOT_CAPABILITIES) {
			if (m.param1 == 1.0f) {
				sendAutopilotVersion();
				result = MAV_RESULT_ACCEPTED;
			} else result = MAV_RESULT_DENIED;
		}

		if (m.command == MAV_CMD_COMPONENT_ARM_DISARM) {
			bool success = false;
			if (m.param1 == 1.0f) success = requestArm("MAVLink", false);
			else if (m.param1 == 0.0f) { forceDisarm("MAVLink"); success = true; }
			result = success ? MAV_RESULT_ACCEPTED : MAV_RESULT_TEMPORARILY_REJECTED;
		}

		if (m.command == MAV_CMD_DO_SET_MODE) {
			bool validMode = isfinite(m.param2) && m.param2 == truncf(m.param2) &&
				(m.param2 == STAB || m.param2 == AUTO ||
				 m.param2 == ALT_HOLD || m.param2 == POS_HOLD);
			bool success = validMode && requestExternalMode((int)m.param2);
			result = success ? MAV_RESULT_ACCEPTED : MAV_RESULT_DENIED;
			if (success) {
				releaseMavlinkManualControlForMode((int)m.param2);
				sendMavlinkStatusTextf(MAV_SEVERITY_INFO, "Mode: %s", getModeName());
				sendCurrentMode();
			} else sendMavlinkStatusTextf(MAV_SEVERITY_WARNING, "Mode denied: %s",
				validMode ? externalModeFailure : "unsupported mode");
		}

		if (m.command == MAV_CMD_DO_SET_STANDARD_MODE) {
			int standardMode = isfinite(m.param1) ? (int)roundf(m.param1) : -1;
			bool success = false;
			const char *failure = "unsupported standard mode";
			if (standardMode == MAV_STANDARD_MODE_ALTITUDE_HOLD) {
				success = requestExternalMode(ALT_HOLD);
				if (success) releaseMavlinkManualControlForMode(ALT_HOLD);
				failure = externalModeFailure;
			} else if (standardMode == MAV_STANDARD_MODE_POSITION_HOLD) {
				success = requestExternalMode(POS_HOLD);
				if (success) releaseMavlinkManualControlForMode(POS_HOLD);
				failure = externalModeFailure;
			} else if (standardMode == MAV_STANDARD_MODE_TAKEOFF) {
				success = startAutomaticTakeoff(altitudeTakeoffHeight);
				failure = automaticFlightFailure;
			} else if (standardMode == MAV_STANDARD_MODE_LAND) {
				success = startAutomaticLanding();
				failure = automaticFlightFailure;
			}
			result = success ? MAV_RESULT_ACCEPTED : MAV_RESULT_DENIED;
			if (success) sendCurrentMode();
			else sendMavlinkStatusTextf(MAV_SEVERITY_WARNING, "Standard mode denied: %s", failure);
		}

		if (m.command == MAV_CMD_NAV_TAKEOFF) {
			bool heightUnspecified = !isfinite(m.param7) || abs(m.param7) < 0.001f;
			bool heightValid = heightUnspecified || (m.param7 >= 0.20f && m.param7 <= 5.80f);
			float height = heightUnspecified ? altitudeTakeoffHeight : m.param7;
			if (!heightValid) {
				result = MAV_RESULT_DENIED;
				deferredStatusSeverity = MAV_SEVERITY_WARNING;
				strlcpy(deferredStatus, "Takeoff denied: height must be 0.20-5.80m",
					sizeof(deferredStatus));
			} else {
				// COMMAND_LONG confirmation is non-zero on an Android retry. If the
				// first request started this same takeoff but only its UDP ACK was lost,
				// acknowledge the retry without restarting targets, integrators, or mode.
				float requestedGoal = min(autoFlightGroundHeight + max(height, 0.20f), 5.80f);
				bool replay = m.confirmation > 0 && autoFlightPhase == AUTO_TAKEOFF &&
					autoFlightSource == AUTO_SOURCE_MAVLINK &&
					abs(requestedGoal - autoFlightGoalHeight) < 0.01f;
				result = (replay || startAutomaticTakeoff(height)) ?
					MAV_RESULT_ACCEPTED : MAV_RESULT_DENIED;
				if (!replay) {
					deferredStatusSeverity = result == MAV_RESULT_ACCEPTED ?
						MAV_SEVERITY_INFO : MAV_SEVERITY_WARNING;
					if (result == MAV_RESULT_ACCEPTED) snprintf(deferredStatus,
						sizeof(deferredStatus), "Takeoff accepted: %.2fm", height);
					else snprintf(deferredStatus, sizeof(deferredStatus),
						"Takeoff denied: %s", automaticFlightFailure);
				}
			}
		}

		if (m.command == MAV_CMD_NAV_LAND) {
			result = startAutomaticLanding() ? MAV_RESULT_ACCEPTED : MAV_RESULT_DENIED;
			deferredStatusSeverity = result == MAV_RESULT_ACCEPTED ?
				MAV_SEVERITY_INFO : MAV_SEVERITY_WARNING;
			if (result == MAV_RESULT_ACCEPTED) strlcpy(
				deferredStatus, "Landing accepted", sizeof(deferredStatus));
			else snprintf(deferredStatus, sizeof(deferredStatus),
				"Landing denied: %s", automaticFlightFailure);
		}

		// ACK precedes the optional human-readable text. Under Wi-Fi contention the
		// protocol result is more important than a duplicate status description.
		mavlink_message_t ack;
		mavlink_msg_command_ack_pack(mavlinkSysId, MAV_COMP_ID_AUTOPILOT1, &ack, m.command, result, UINT8_MAX, 0, msg.sysid, msg.compid);
		sendMessage(&ack);
		if (deferredStatus[0]) sendMavlinkStatusText(deferredStatusSeverity, deferredStatus);
	}
}

// Send shell output to GCS
void mavlinkPrint(const char* str) {
	if (!str) return;
	// Serial remains the complete diagnostic transport. The MAVLink shell copy is
	// best-effort and bounded so a verbose ground command cannot consume the Wi-Fi
	// heap or later become a multi-packet burst during flight.
	if (!mavlinkConnected || mavlinkPrintBuffer.length() >= 512) return;
	size_t room = 512 - mavlinkPrintBuffer.length();
	mavlinkPrintBuffer.concat(str, min(room, strlen(str)));
}

void sendMavlinkPrint() {
	if (mavlinkPrintBuffer.isEmpty() || !mavlinkConnected) return;
	uint8_t data[MAVLINK_MSG_SERIAL_CONTROL_FIELD_DATA_LEN] = {};
	size_t count = min((size_t)MAVLINK_MSG_SERIAL_CONTROL_FIELD_DATA_LEN,
		mavlinkPrintBuffer.length());
	memcpy(data, mavlinkPrintBuffer.c_str(), count);
	mavlink_message_t msg;
	mavlink_msg_serial_control_pack(mavlinkSysId, MAV_COMP_ID_AUTOPILOT1, &msg,
		SERIAL_CONTROL_DEV_SHELL,
		count < mavlinkPrintBuffer.length() ? SERIAL_CONTROL_FLAG_MULTI : 0,
		0, 0, count, data, 0, 0);
	sendMessage(&msg);
	mavlinkPrintBuffer.remove(0, count);
}

#endif
