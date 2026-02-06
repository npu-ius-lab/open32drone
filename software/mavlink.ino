// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// MAVLink communication

#if WIFI_ENABLED

#include <MAVLink.h>
#include "util.h"

#define SYSTEM_ID 1
#define MAVLINK_RATE_SLOW 1
#define MAVLINK_RATE_FAST 10
#define MAVLINK_RATE_OPTICAL_FLOW 10  // 光流数据发送频率
#define MAVLINK_CONTROL_YAW_DEAD_ZONE 0.1f

bool mavlinkConnected = false;
String mavlinkPrintBuffer;

extern float controlTime;
extern float controlRoll, controlPitch, controlThrottle, controlYaw, controlMode;

extern float velIntegralX; // 引用控制代码里的积分项
extern float velIntegralY;
extern Vector position;    // 引用位置
extern Vector velocity;    // 引用速度
extern Quaternion attitudeTarget; // 引用目标姿态

// 声明外部光流变量
#ifdef OPTICAL_FLOW_ENABLED
extern bool opticalFlowHealthy;
extern float opticalFlowVelocityX;
extern float opticalFlowVelocityY;
extern float opticalFlowHeight;
extern OpticalFlowRawData opticalFlowRawData;
extern const float MIN_VALID_HEIGHT;
extern const float MAX_VALID_HEIGHT;
#endif

#ifdef OPTICAL_FLOW_ENABLED
	static Rate opticalFlowRate(MAVLINK_RATE_OPTICAL_FLOW);
#endif

void processMavlink() {
	sendMavlink();
	receiveMavlink();
}

void sendMavlink() {
	sendMavlinkPrint();

	mavlink_message_t msg;
	uint32_t time = t * 1000;

	static Rate slow(MAVLINK_RATE_SLOW), fast(MAVLINK_RATE_FAST);

	if (slow) {
		mavlink_msg_heartbeat_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &msg, MAV_TYPE_QUADROTOR, MAV_AUTOPILOT_GENERIC,
			(armed ? MAV_MODE_FLAG_SAFETY_ARMED : 0) |
			((mode == STAB) ? MAV_MODE_FLAG_STABILIZE_ENABLED : 0) |
			((mode == AUTO) ? MAV_MODE_FLAG_AUTO_ENABLED : MAV_MODE_FLAG_MANUAL_INPUT_ENABLED),
			mode, MAV_STATE_STANDBY);
		sendMessage(&msg);

		if (!mavlinkConnected) return; // send only heartbeat until connected

		mavlink_msg_extended_sys_state_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &msg,
			MAV_VTOL_STATE_UNDEFINED, landed ? MAV_LANDED_STATE_ON_GROUND : MAV_LANDED_STATE_IN_AIR);
		sendMessage(&msg);
	}

	if (fast && mavlinkConnected) {
		const float zeroQuat[] = {0, 0, 0, 0};
		mavlink_msg_attitude_quaternion_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &msg,
			time, attitude.w, attitude.x, -attitude.y, -attitude.z, rates.x, -rates.y, -rates.z, zeroQuat); // convert to frd
		sendMessage(&msg);

		mavlink_msg_rc_channels_raw_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &msg, controlTime * 1000, 0,
			channels[0], channels[1], channels[2], channels[3], channels[4], channels[5], channels[6], channels[7], UINT8_MAX);
		if (channels[0] != 0) sendMessage(&msg); // 0 means no RC input

		float controls[8];
		memcpy(controls, motors, sizeof(motors));
		mavlink_msg_actuator_control_target_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &msg, time, 0, controls);
		sendMessage(&msg);

		mavlink_msg_scaled_imu_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &msg, time,
			acc.x * 1000, -acc.y * 1000, -acc.z * 1000, // convert to frd
			gyro.x * 1000, -gyro.y * 1000, -gyro.z * 1000,
			0, 0, 0, 0);
		sendMessage(&msg);
	}

#ifdef OPTICAL_FLOW_ENABLED
	// 发送光流数据
	if (opticalFlowRate && mavlinkConnected && opticalFlowHealthy) {
		// 计算传感器ID
		uint8_t sensor_id = 0;
		
		// 计算质量指标 (0-255)
		uint8_t quality = 0;
		if (opticalFlowRawData.dataValid) {
			// 基础质量基于积分时间
			quality = constrain(opticalFlowRawData.integrationTime / 100, 0, 255);
			// 根据高度调整质量
			if (opticalFlowHeight > 0.5f && opticalFlowHeight < 3.0f) {
				quality = constrain(quality + 50, 0, 255);
			}
		}
		
		// 计算地面距离（米）
		float distance = opticalFlowHeight;
		
		// 计算角速度（rad/s）
		// 注意坐标系转换：光流传感器安装在底部，坐标系通常为：
		// X轴：向右（飞行器右侧）
		// Y轴：向前（飞行器前方）
		
		// 从FLU转换到光流传感器坐标系：
		// 光流传感器X轴（向右）对应FLU的负滚转角速度
		// 光流传感器Y轴（向前）对应FLU的负俯仰角速度
		
		float flow_rate_x = -gyro.x;  // 光流传感器X轴角速度（rad/s）
		float flow_rate_y = -gyro.y;  // 光流传感器Y轴角速度（rad/s）
		
		// 计算积分时间（秒）
		float integration_time_s = opticalFlowRawData.integrationTime / 1000000.0f;
		
		// 计算积分后的角位移（弧度）
		// integrated_xgyro = 角速度 * 积分时间
		float integrated_xgyro = flow_rate_x * integration_time_s;
		float integrated_ygyro = flow_rate_y * integration_time_s;
		float integrated_zgyro = 0.0f;  // Z轴角速度积分（通常为0）
		
		// 温度数据（℃ * 100）
		int16_t temperature = 0;  // 如果没有温度传感器，设为0
		
		// 距离测量时间差（微秒）
		uint32_t time_delta_distance_us = 0;  // 可以设为0，表示未知
		
		// 计算积分后的光流位移（弧度）
		// 根据驱动代码，FLOW_SCALE_FACTOR = 1.0f / 10000.0f
		// 这个因子将原始像素值转换为弧度
		const float FLOW_SCALE_FACTOR = 1.0f / 10000.0f;
		float integrated_x = opticalFlowRawData.flowX * FLOW_SCALE_FACTOR;
		float integrated_y = opticalFlowRawData.flowY * FLOW_SCALE_FACTOR;
		
		// 发送OPTICAL_FLOW_RAD消息
		mavlink_msg_optical_flow_rad_pack(
			SYSTEM_ID,
			MAV_COMP_ID_AUTOPILOT1,
			&msg,
			micros(),                     // 时间戳 (微秒)
			sensor_id,                    // 传感器ID
			opticalFlowRawData.integrationTime, // 积分时间 (微秒)
			integrated_x,                 // 积分后的X方向光流（弧度）
			integrated_y,                 // 积分后的Y方向光流（弧度）
			integrated_xgyro,             // 积分后的X角速度（弧度）
			integrated_ygyro,             // 积分后的Y角速度（弧度）
			integrated_zgyro,             // 积分后的Z角速度（弧度）
			temperature,                  // 温度（℃ * 100）
			quality,                      // 质量 (0-255)
			time_delta_distance_us,       // 距离测量时间差（微秒）
			distance                      // 到地面的距离 (米)
		);
		sendMessage(&msg);
		
		// 发送OPTICAL_FLOW消息（兼容旧版本）
		mavlink_msg_optical_flow_pack(
			SYSTEM_ID,
			MAV_COMP_ID_AUTOPILOT1,
			&msg,
			micros(),                     // 时间戳 (微秒)
			sensor_id,                    // 传感器ID
			opticalFlowRawData.flowX,     // 原始光流X (像素)
			opticalFlowRawData.flowY,     // 原始光流Y (像素)
			opticalFlowVelocityX,         // 补偿后光流X速度 (m/s)
			opticalFlowVelocityY,         // 补偿后光流Y速度 (m/s)
			quality,                      // 质量 (0-255)
			distance,                     // 地面距离 (米)
			flow_rate_x,                  // X轴角速度 (rad/s)
			flow_rate_y                   // Y轴角速度 (rad/s)
		);
		sendMessage(&msg);
		
		// 发送距离传感器数据
		if (distance > 0.1f) {
			// 为距离传感器消息准备额外参数
			float zero_quaternion[4] = {1.0f, 0.0f, 0.0f, 0.0f}; // 无旋转的四元数
			uint8_t signal_quality = 0;
			if (opticalFlowRawData.dataValid) {
				// 根据TOF距离和积分时间计算信号质量 (0-100)
				signal_quality = constrain((opticalFlowHeight - MIN_VALID_HEIGHT) / (MAX_VALID_HEIGHT - MIN_VALID_HEIGHT) * 100, 0, 100);
			}
			
			mavlink_msg_distance_sensor_pack(
				SYSTEM_ID,
				MAV_COMP_ID_AUTOPILOT1,
				&msg,
				micros() / 1000,          // 时间戳 (毫秒)
				MIN_VALID_HEIGHT * 100,   // 最小距离 (cm)
				MAX_VALID_HEIGHT * 100,   // 最大距离 (cm)
				distance * 100,           // 当前距离 (cm)
				MAV_DISTANCE_SENSOR_LASER,// 传感器类型
				sensor_id,                // 传感器ID
				MAV_SENSOR_ROTATION_PITCH_270, // 朝向 (向下)
				0,                        // 协方差 (UINT8_MAX表示未知)
				0.0f,                     // 水平视场角 (rad) - 未知
				0.0f,                     // 垂直视场角 (rad) - 未知
				zero_quaternion,          // 传感器四元数 (默认无旋转)
				signal_quality            // 信号质量 (0-100)
			);
			sendMessage(&msg);
		}
		
		// 发送地面速度消息，用于导航
		mavlink_msg_vision_speed_estimate_pack(
			SYSTEM_ID,
			MAV_COMP_ID_AUTOPILOT1,
			&msg,
			micros() / 1000,             // 时间戳 (毫秒)
			opticalFlowVelocityX,        // X速度 (m/s)
			opticalFlowVelocityY,        // Y速度 (m/s)
			0.0f,                        // Z速度 (m/s) - 光流通常不提供垂直速度
			0,                           // 协方差 (未知)
			sensor_id                    // 估计源
		);
		sendMessage(&msg);
	}
#endif
}

void sendMessage(const void *msg) {
	uint8_t buf[MAVLINK_MAX_PACKET_LEN];
	int len = mavlink_msg_to_send_buffer(buf, (mavlink_message_t *)msg);
	sendWiFi(buf, len);
}

void receiveMavlink() {
	uint8_t buf[MAVLINK_MAX_PACKET_LEN];
	int len = receiveWiFi(buf, MAVLINK_MAX_PACKET_LEN);
	if (len) mavlinkConnected = true;

	// New packet, parse it
	mavlink_message_t msg;
	mavlink_status_t status;
	for (int i = 0; i < len; i++) {
		if (mavlink_parse_char(MAVLINK_COMM_0, buf[i], &msg, &status)) {
			handleMavlink(&msg);
		}
	}
}

void handleMavlink(const void *_msg) {
	const mavlink_message_t& msg = *(mavlink_message_t *)_msg;

	if (msg.msgid == MAVLINK_MSG_ID_MANUAL_CONTROL) {
		mavlink_manual_control_t m;
		mavlink_msg_manual_control_decode(&msg, &m);
		if (m.target && m.target != SYSTEM_ID) return; // 0 is broadcast

		controlThrottle = m.z / 1000.0f;
		controlPitch = m.x / 1000.0f;
		controlRoll = m.y / 1000.0f;
		controlYaw = m.r / 1000.0f;
		controlMode = NAN;
		controlTime = t;

		if (abs(controlYaw) < MAVLINK_CONTROL_YAW_DEAD_ZONE) controlYaw = 0;
	}

	if (msg.msgid == MAVLINK_MSG_ID_PARAM_REQUEST_LIST) {
		mavlink_param_request_list_t m;
		mavlink_msg_param_request_list_decode(&msg, &m);
		if (m.target_system && m.target_system != SYSTEM_ID) return;

		mavlink_message_t msg;
		for (int i = 0; i < parametersCount(); i++) {
			mavlink_msg_param_value_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &msg,
				getParameterName(i), getParameter(i), MAV_PARAM_TYPE_REAL32, parametersCount(), i);
			sendMessage(&msg);
			delay(1); // [修复] 增加延时防止 UDP 缓冲溢出导致丢包
		}
	}

	if (msg.msgid == MAVLINK_MSG_ID_PARAM_REQUEST_READ) {
		mavlink_param_request_read_t m;
		mavlink_msg_param_request_read_decode(&msg, &m);
		if (m.target_system && m.target_system != SYSTEM_ID) return;

		char name[MAVLINK_MSG_PARAM_REQUEST_READ_FIELD_PARAM_ID_LEN + 1];
		strlcpy(name, m.param_id, sizeof(name)); // param_id might be not null-terminated
		float value = strlen(name) == 0 ? getParameter(m.param_index) : getParameter(name);
		if (m.param_index != -1) {
			memcpy(name, getParameterName(m.param_index), 16);
		}
		mavlink_message_t msg;
		mavlink_msg_param_value_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &msg,
			name, value, MAV_PARAM_TYPE_REAL32, parametersCount(), m.param_index);
		sendMessage(&msg);
	}

	if (msg.msgid == MAVLINK_MSG_ID_PARAM_SET) {
		mavlink_param_set_t m;
		mavlink_msg_param_set_decode(&msg, &m);
		if (m.target_system && m.target_system != SYSTEM_ID) return;

		char name[MAVLINK_MSG_PARAM_SET_FIELD_PARAM_ID_LEN + 1];
		strlcpy(name, m.param_id, sizeof(name)); // param_id might be not null-terminated
		setParameter(name, m.param_value);
		// send ack
		mavlink_message_t msg;
		mavlink_msg_param_value_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &msg,
			m.param_id, m.param_value, MAV_PARAM_TYPE_REAL32, parametersCount(), 0); // index is unknown
		sendMessage(&msg);
	}

	if (msg.msgid == MAVLINK_MSG_ID_MISSION_REQUEST_LIST) { // handle to make qgc happy
		mavlink_mission_request_list_t m;
		mavlink_msg_mission_request_list_decode(&msg, &m);
		if (m.target_system && m.target_system != SYSTEM_ID) return;

		mavlink_message_t msg;
		mavlink_msg_mission_count_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &msg, 0, 0, 0, MAV_MISSION_TYPE_MISSION, 0);
		sendMessage(&msg);
	}

	if (msg.msgid == MAVLINK_MSG_ID_SERIAL_CONTROL) {
		mavlink_serial_control_t m;
		mavlink_msg_serial_control_decode(&msg, &m);
		if (m.target_system && m.target_system != SYSTEM_ID) return;

		char data[MAVLINK_MSG_SERIAL_CONTROL_FIELD_DATA_LEN + 1];
		strlcpy(data, (const char *)m.data, m.count); // data might be not null-terminated
		doCommand(data, true);
	}

	if (msg.msgid == MAVLINK_MSG_ID_SET_ATTITUDE_TARGET) {
		if (mode != AUTO) return;

		mavlink_set_attitude_target_t m;
		mavlink_msg_set_attitude_target_decode(&msg, &m);
		if (m.target_system && m.target_system != SYSTEM_ID) return;

		// copy attitude, rates and thrust targets
		ratesTarget.x = m.body_roll_rate;
		ratesTarget.y = -m.body_pitch_rate; // convert to flu
		ratesTarget.z = -m.body_yaw_rate;
		attitudeTarget.w = m.q[0];
		attitudeTarget.x = m.q[1];
		attitudeTarget.y = -m.q[2];
		attitudeTarget.z = -m.q[3];
		thrustTarget = m.thrust;
		ratesExtra = Vector(0, 0, 0);

		if (m.type_mask & ATTITUDE_TARGET_TYPEMASK_ATTITUDE_IGNORE) attitudeTarget.invalidate();
		armed = m.thrust > 0;
	}

	if (msg.msgid == MAVLINK_MSG_ID_SET_ACTUATOR_CONTROL_TARGET) {
		if (mode != AUTO) return;

		mavlink_set_actuator_control_target_t m;
		mavlink_msg_set_actuator_control_target_decode(&msg, &m);
		if (m.target_system && m.target_system != SYSTEM_ID) return;

		attitudeTarget.invalidate();
		ratesTarget.invalidate();
		torqueTarget.invalidate();
		memcpy(motors, m.controls, sizeof(motors)); // copy motor thrusts
		armed = motors[0] > 0 || motors[1] > 0 || motors[2] > 0 || motors[3] > 0;
	}

	if (msg.msgid == MAVLINK_MSG_ID_LOG_REQUEST_DATA) {
		mavlink_log_request_data_t m;
		mavlink_msg_log_request_data_decode(&msg, &m);
		if (m.target_system && m.target_system != SYSTEM_ID) return;

		// Send all log records
		for (int i = 0; i < sizeof(logBuffer) / sizeof(logBuffer[0]); i++) {
			mavlink_message_t msg;
			mavlink_msg_log_data_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &msg, 0, i,
				sizeof(logBuffer[0]), (uint8_t *)logBuffer[i]);
			sendMessage(&msg);
		}
	}

	// Handle commands
	if (msg.msgid == MAVLINK_MSG_ID_COMMAND_LONG) {
		mavlink_command_long_t m;
		mavlink_msg_command_long_decode(&msg, &m);
		if (m.target_system && m.target_system != SYSTEM_ID) return;
		mavlink_message_t response;
		bool accepted = false;

		if (m.command == MAV_CMD_REQUEST_MESSAGE && m.param1 == MAVLINK_MSG_ID_AUTOPILOT_VERSION) {
			accepted = true;
			mavlink_msg_autopilot_version_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &response,
				MAV_PROTOCOL_CAPABILITY_PARAM_FLOAT | MAV_PROTOCOL_CAPABILITY_MAVLINK2, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0);
			sendMessage(&response);
		}

		if (m.command == MAV_CMD_COMPONENT_ARM_DISARM) {
			if (m.param1 && controlThrottle > 0.05) return; // don't arm if throttle is not low
			accepted = true;
			armed = m.param1 == 1;
		}

		if (m.command == MAV_CMD_DO_SET_MODE) {
			if (m.param2 < 0 || m.param2 > AUTO) return; // incorrect mode
			accepted = true;
			mode = m.param2;
		}

		// send command ack
		mavlink_message_t ack;
		mavlink_msg_command_ack_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &ack, m.command, accepted ? MAV_RESULT_ACCEPTED : MAV_RESULT_UNSUPPORTED, UINT8_MAX, 0, msg.sysid, msg.compid);
		sendMessage(&ack);
	}
}

// Send shell output to GCS
void mavlinkPrint(const char* str) {
	mavlinkPrintBuffer += str;
}

void sendMavlinkPrint() {
	// Send mavlink print data in chunks
	const char *str = mavlinkPrintBuffer.c_str();
	for (int i = 0; i < strlen(str); i += MAVLINK_MSG_SERIAL_CONTROL_FIELD_DATA_LEN) {
		char data[MAVLINK_MSG_SERIAL_CONTROL_FIELD_DATA_LEN + 1];
		strlcpy(data, str + i, sizeof(data));
		mavlink_message_t msg;
		mavlink_msg_serial_control_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &msg,
			SERIAL_CONTROL_DEV_SHELL,
			i + MAVLINK_MSG_SERIAL_CONTROL_FIELD_DATA_LEN < strlen(str) ? SERIAL_CONTROL_FLAG_MULTI : 0, // more chunks to go
			0, 0, strlen(data), (uint8_t *)data, 0, 0);
		sendMessage(&msg);
	}
	mavlinkPrintBuffer.clear();
}

void sendDebugVect(const char* name, float x, float y, float z) {
    mavlink_message_t msg;
    // msgid 250: DEBUG_VECT
    mavlink_msg_debug_vect_pack(SYSTEM_ID, MAV_COMP_ID_AUTOPILOT1, &msg, 
                                name, 
                                micros(), // 时间戳
                                x, y, z); // 三个数据
    sendMessage(&msg);
}

#endif