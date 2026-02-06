// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

#include "vector.h"
#include "quaternion.h"
#include "pid.h"
#include "lpf.h"
#include "util.h"
#include "kalman_angle.h"

// ============================================================================
//                               参数配置区域
// ============================================================================

// --- 1. 角速度环PID ---
#define ROLLRATE_P 0.12f
#define ROLLRATE_I 0.8f
#define ROLLRATE_D 0.008f
#define ROLLRATE_I_LIM 1.0f

#define PITCHRATE_P ROLLRATE_P
#define PITCHRATE_I ROLLRATE_I
#define PITCHRATE_D ROLLRATE_D
#define PITCHRATE_I_LIM ROLLRATE_I_LIM

#define YAWRATE_P 0.3f
#define YAWRATE_I 0.0f
#define YAWRATE_D 0.0f
#define YAWRATE_I_LIM 0.3f

// --- 2. 角度环PID ---
#define ROLL_P 6.0f
#define ROLL_I 0.0f
#define ROLL_D 0.0f

#define PITCH_P ROLL_P
#define PITCH_I ROLL_I
#define PITCH_D ROLL_D

#define YAW_P 3.0f

// --- 3. 最大速率限制 ---
#define ROLLRATE_MAX radians(360)
#define PITCHRATE_MAX radians(360)
#define YAWRATE_MAX radians(300)
#define TILT_MAX radians(30)
#define RATES_D_LPF_ALPHA 0.2f 

// --- 4. 定高参数 ---
#define ALT_P 0.5f          // 高度误差 P
#define ALT_I 0.02f         // 高度积分 I (补油门)
#define ALT_I_MAX 0.15f     // 积分限幅
#define ALT_VEL_P 0.9f      // 垂直速度阻尼 (D项)
#define HOVER_THROTTLE 0.60f // 基础悬停油门

// --- 5. 光流定点参数 ---
// X轴 (前后/Pitch)
#define POS_HOLD_P_X 0.045f
#define POS_HOLD_I_X 0.050f
#define POS_HOLD_D_X 0.000f
#define POS_HOLD_TRIM_X 0.0f  // 物理重心修正

// Y轴 (左右/Roll)
#define POS_HOLD_P_Y 0.030f
#define POS_HOLD_I_Y 0.040f
#define POS_HOLD_D_Y 0.000f
#define POS_HOLD_TRIM_Y 0.0f  // 物理重心修正

// 通用定点参数
#define POS_HOLD_I_LIMIT 1.0f  // 速度环积分限幅
#define MAX_FLOW_ANGLE 0.2f    // 最大光流修正倾角 (约11度)
#define POS_HOLD_DEADBAND 0.05f // 速度死区 (m/s)

// ============================================================================
//                               全局变量定义
// ============================================================================

// 飞行模式
const int MANUAL = 0;
const int ACRO = 1;
const int STAB = 2;
const int AUTO = 3;
const int ALT_HOLD = 4;
const int POS_HOLD = 5;

int mode = STAB;
bool armed = false;

// 状态变量
float targetZ = 0;
bool altHoldEngaged = false;
bool posHoldLocked = false;
float targetPosX = 0;
float targetPosY = 0;
float velIntegralX = 0;
float velIntegralY = 0;

// 控制目标
Quaternion attitudeTarget;
Vector ratesTarget;
Vector ratesExtra; 
Vector torqueTarget;
float thrustTarget;

// PID 对象实例化
PID rollRatePID(ROLLRATE_P, ROLLRATE_I, ROLLRATE_D, ROLLRATE_I_LIM, RATES_D_LPF_ALPHA);
PID pitchRatePID(PITCHRATE_P, PITCHRATE_I, PITCHRATE_D, PITCHRATE_I_LIM, RATES_D_LPF_ALPHA);
PID yawRatePID(YAWRATE_P, YAWRATE_I, YAWRATE_D);
PID rollPID(ROLL_P, ROLL_I, ROLL_D);
PID pitchPID(PITCH_P, PITCH_I, PITCH_D);
PID yawPID(YAW_P, 0, 0);

Vector maxRate(ROLLRATE_MAX, PITCHRATE_MAX, YAWRATE_MAX);
float tiltMax = TILT_MAX;

float targetVelBodyX_debug = 0;
float targetVelBodyY_debug = 0;
float pos_hold_d_term_x_debug = 0; 
float pos_hold_d_term_y_debug = 0;

// 外部引用
extern const int MOTOR_REAR_LEFT, MOTOR_REAR_RIGHT, MOTOR_FRONT_RIGHT, MOTOR_FRONT_LEFT;
extern float controlRoll, controlPitch, controlThrottle, controlYaw, controlMode;
extern Quaternion attitude;
extern Vector rates;
extern float t, dt;
extern float motors[4];
extern Vector velocity;   
extern Vector position; 
extern bool opticalFlowHealthy;
extern KalmanAngle kalmanRoll, kalmanPitch; 
extern float debugMode; 

// 调试函数引用
extern void sendDebugVect(const char* name, float x, float y, float z);
void print(const char* format, ...);

// ============================================================================
//                               核心控制逻辑
// ============================================================================

void control() {
    // 1. 更新飞行模式与解锁状态
    updateFlightMode();

    // 2. 计算目标推力和姿态
    if (mode == ACRO) {
        runAcroControl();
    } else {
        runLevelControl();
    }

    // 3. 姿态控制 (Angle Loop)
    if (mode != ACRO) {
        controlAttitude();
    }

    // 4. 角速度控制 (Rate Loop)
    controlRates();

    // 5. 电机输出 (Mixer)
    controlTorque();

    // 6. 调试输出
    debugControl();
}

// ----------------------------------------------------------------------------
// 模式与状态管理
// ----------------------------------------------------------------------------
void updateFlightMode() {
    // 映射通道到模式
    if (controlMode < 0.33f) mode = STAB;
    else if (controlMode < 0.66f) mode = ALT_HOLD;
    else mode = POS_HOLD;

    // 解锁/上锁逻辑 (内八/外八或特定摇杆位)
    if (controlThrottle < 0.05f) {
        if (controlYaw > 0.95f) armed = true;
        if (controlYaw < -0.95f) armed = false;
    }

    // 状态重置
    if (!armed) {
        altHoldEngaged = false; 
        posHoldLocked = false;
        thrustTarget = 0;
        
        // 重置所有积分器
        velIntegralX = 0; velIntegralY = 0;
        rollRatePID.reset(); pitchRatePID.reset(); yawRatePID.reset();
        rollPID.reset(); pitchPID.reset();
        
        // 重置目标高度
        targetZ = 0;
    }
}

// ----------------------------------------------------------------------------
// ACRO (特技) 模式控制
// ----------------------------------------------------------------------------
void runAcroControl() {
    thrustTarget = controlThrottle;
    ratesTarget.x = controlRoll * maxRate.x;
    ratesTarget.y = controlPitch * maxRate.y;
    ratesTarget.z = controlYaw * maxRate.z;

    // 清除辅助模式状态
    altHoldEngaged = false;
    posHoldLocked = false;
    velIntegralX = 0;
    velIntegralY = 0;
}

// ----------------------------------------------------------------------------
// 自稳/定高/定点 模式主逻辑
// ----------------------------------------------------------------------------
void runLevelControl() {
    // A. 处理油门与高度
    if (mode == STAB) {
        // 自稳模式：直通油门
        thrustTarget = controlThrottle;
        altHoldEngaged = false;
        targetZ = position.z;
    } else {
        // 定高/定点模式：高度保持
        runAltitudeHold();
    }

    // B. 处理水平位置 (定点 vs 手动)
    if (mode == POS_HOLD && opticalFlowHealthy && position.z > 0.4f) {
        runPositionHoldControl();
    } else {
        // 普通自稳逻辑 (或盲飞保护)
        runManualLevelControl();
    }
}

// ----------------------------------------------------------------------------
// 定高逻辑 
// ----------------------------------------------------------------------------
void runAltitudeHold() {
    static float altIntegral = 0;
    float deadband_low = 0.20f, deadband_high = 0.80f;

    // 1. 激活检测
    if (!altHoldEngaged) { 
        if (controlThrottle > deadband_low) { 
            altHoldEngaged = true;
            targetZ = max(0.7f, position.z); // 初始锁定高度至少0.7m
        } else { 
            thrustTarget = 0;
            return;
        }
    }

    // 2. 更新目标高度
    if (controlThrottle > deadband_high) {
        targetZ += 0.8f * dt; // 上升
    } else if (controlThrottle < deadband_low) {
        if (controlThrottle < 0.05f) { 
            // 落地逻辑：接近地面时减慢下降
            targetZ -= (position.z < 0.30f) ? 0.20f * dt : 0.9f * dt;
        } else {
            targetZ -= 0.6f * dt; // 正常下降
        }
    }
    targetZ = constrain(targetZ, -0.2f, 3.5f);

    // 3. 高度 PID 控制
    if (thrustTarget > 0.001f || controlThrottle > 0.05f) {
        float altError = targetZ - position.z;
        
        // I项：补偿电池电压下降
        altIntegral = constrain(altIntegral + altError * dt * ALT_I, -ALT_I_MAX, ALT_I_MAX);
        if (position.z < 0.1f) altIntegral = 0; // 落地清积分

        // 输出混控
        thrustTarget = HOVER_THROTTLE 
                       + altIntegral 
                       + constrain(altError * ALT_P, -0.35f, 0.35f) 
                       - constrain(velocity.z * ALT_VEL_P, -0.2f, 0.2f);
                       
        thrustTarget = constrain(thrustTarget, 0.0f, 0.9f);
    }
}

// ----------------------------------------------------------------------------
// 光流定点逻辑
// ----------------------------------------------------------------------------
void runPositionHoldControl() {
    static float lastVelErrorX = 0;
    static float lastVelErrorY = 0;
    static uint32_t lastFlowGoodTime = 0;

    if (opticalFlowHealthy) lastFlowGoodTime = millis();
    bool flowIsStable = (millis() - lastFlowGoodTime < 300);

    // 1. 摇杆介入检测 (手动飞行优先)
    bool is_stick_moving = abs(controlRoll) > 0.05f || abs(controlPitch) > 0.05f;

    if (is_stick_moving || !flowIsStable) {
        posHoldLocked = false;
        // 动杆时清空 PID 状态
        velIntegralX = 0; 
        velIntegralY = 0;
        lastVelErrorX = 0; 
        lastVelErrorY = 0;
        
        // 切换回纯姿态控制
        runManualLevelControl();
        return;
    }

    // 2. 锁定位置
    if (!posHoldLocked) { 
        targetPosX = position.x;
        targetPosY = position.y; 
        posHoldLocked = true;
    }

    // 3. 位置环 -> 速度目标
    float errX = targetPosX - position.x;
    float errY = targetPosY - position.y;
    
    // 限制最大移动速度 (1.5m/s)
    float vWorldX = constrain(errX * 1.5f, -1.5f, 1.5f);
    float vWorldY = constrain(errY * 1.5f, -1.5f, 1.5f);

    // 转换到机体坐标系
    float yaw = attitude.getYaw(), c = cos(yaw), s = sin(yaw);
    float targetVelBodyX = vWorldX * c + vWorldY * s;
    float targetVelBodyY = -vWorldX * s + vWorldY * c;

    targetVelBodyX_debug = targetVelBodyX;
    targetVelBodyY_debug = targetVelBodyY;

    // 4. 速度环 PID
    float velErrorX = targetVelBodyX - velocity.x;
    float velErrorY = targetVelBodyY - velocity.y;

    // D项 (微分)
    float dTermX = (velErrorX - lastVelErrorX) / dt * POS_HOLD_D_X;
    float dTermY = (velErrorY - lastVelErrorY) / dt * POS_HOLD_D_Y;
    lastVelErrorX = velErrorX;
    lastVelErrorY = velErrorY;

    // I项 (积分 - 带死区)
    if (abs(velErrorX) > POS_HOLD_DEADBAND) {
        velIntegralX = constrain(velIntegralX + velErrorX * dt * POS_HOLD_I_X, -POS_HOLD_I_LIMIT, POS_HOLD_I_LIMIT);
    }
    if (abs(velErrorY) > POS_HOLD_DEADBAND) {
        velIntegralY = constrain(velIntegralY + velErrorY * dt * POS_HOLD_I_Y, -POS_HOLD_I_LIMIT, POS_HOLD_I_LIMIT);
    }

    // 5. 输出目标角度
    float flowPitch = constrain(velErrorX * POS_HOLD_P_X + velIntegralX + dTermX + POS_HOLD_TRIM_X, -MAX_FLOW_ANGLE, MAX_FLOW_ANGLE);
    float flowRoll  = constrain(velErrorY * POS_HOLD_P_Y + velIntegralY + dTermY + POS_HOLD_TRIM_Y, -MAX_FLOW_ANGLE, MAX_FLOW_ANGLE);

    // 应用目标姿态 
    setAttitudeTarget(flowRoll, flowPitch, controlYaw);
}

// ----------------------------------------------------------------------------
// 普通手动姿态逻辑
// ----------------------------------------------------------------------------
void runManualLevelControl() {
    // 低空或手动模式下，重置定点状态
    posHoldLocked = false;
    velIntegralX = 0; 
    velIntegralY = 0;

    // 目标角度 = 摇杆 * 最大倾角 + Trim
    float targetRoll  = controlRoll * tiltMax + POS_HOLD_TRIM_Y;
    float targetPitch = controlPitch * tiltMax + POS_HOLD_TRIM_X;

    setAttitudeTarget(targetRoll, targetPitch, controlYaw);
}

// 辅助函数：设置目标欧拉角
void setAttitudeTarget(float roll, float pitch, float yawRate) {
    float currentYaw = attitude.getYaw();
    float targetYaw = attitudeTarget.getYaw();
    
    // 如果 Yaw 目标无效或正在转动 Yaw，使用当前 Yaw
    if (invalid(targetYaw) || yawRate != 0) {
        targetYaw = currentYaw;
    }
    
    attitudeTarget = Quaternion::fromEuler(Vector(roll, pitch, targetYaw));
    ratesExtra = Vector(0, 0, -yawRate * maxRate.z);
}

// ----------------------------------------------------------------------------
// 姿态环控制器
// ----------------------------------------------------------------------------
void controlAttitude() {
    if (!armed || attitudeTarget.invalid() || thrustTarget < 0.01f) return;

    const Vector up(0, 0, 1);
    Vector upActual = Quaternion::rotateVector(up, attitude);
    Vector upTarget = Quaternion::rotateVector(up, attitudeTarget);
    Vector error = Vector::rotationVectorBetween(upTarget, upActual);

    ratesTarget.x = rollPID.update(error.x) + ratesExtra.x;
    ratesTarget.y = pitchPID.update(error.y) + ratesExtra.y;
    
    float yawError = wrapAngle(attitudeTarget.getYaw() - attitude.getYaw());
    ratesTarget.z = yawPID.update(yawError) + ratesExtra.z;
}

// ----------------------------------------------------------------------------
// 角速度环控制器
// ----------------------------------------------------------------------------
void controlRates() {
    if (!armed || ratesTarget.invalid() || thrustTarget < 0.01f) {
        torqueTarget.invalidate();
        return;
    }

    Vector error = ratesTarget - rates;
    torqueTarget.x = rollRatePID.update(error.x);
    torqueTarget.y = pitchRatePID.update(error.y);
    torqueTarget.z = yawRatePID.update(error.z);
}

// ----------------------------------------------------------------------------
// 混控器
// ----------------------------------------------------------------------------
void controlTorque() {
    if (!armed) { 
        memset(motors, 0, sizeof(motors)); 
        return;
    }
    
    // 怠速
    if (thrustTarget < 0.1f) {
        for(int i=0; i<4; i++) motors[i] = 0.05f;
        return;
    }

    // X字四轴混控
    motors[MOTOR_FRONT_LEFT]  = thrustTarget + torqueTarget.x - torqueTarget.y + torqueTarget.z;
    motors[MOTOR_FRONT_RIGHT] = thrustTarget - torqueTarget.x - torqueTarget.y - torqueTarget.z;
    motors[MOTOR_REAR_LEFT]   = thrustTarget + torqueTarget.x + torqueTarget.y - torqueTarget.z;
    motors[MOTOR_REAR_RIGHT]  = thrustTarget - torqueTarget.x + torqueTarget.y + torqueTarget.z;
    
    for(int i=0; i<4; i++) motors[i] = constrain(motors[i], 0.0f, 1.0f);
}

// ----------------------------------------------------------------------------
// 调试信息输出
// ----------------------------------------------------------------------------
void debugControl() {
    static uint32_t lastDbg = 0;
    if (millis() - lastDbg < 50) return; // 20Hz 发送频率，防止阻塞
    lastDbg = millis();

    int mode = (int)debugMode; // 获取当前的调试模式

    switch (mode) {
        // ==========================
        // 模式 1-4: 控制环路调试
        // ==========================
        case 1: // [姿态环] 目标角度 vs 实际角度 (单位: 度)
        {
            Vector attTgt = attitudeTarget.toEuler();
            Vector attAct = attitude.toEuler();
            // X: 目标Roll, Y: 实际Roll, Z: 目标Pitch
            sendDebugVect("ATT_CHK", degrees(attTgt.x), degrees(attAct.x), degrees(attTgt.y)); 
            break;
        }
        case 2: // [角速度环] PID 分项分析 (以 Roll 轴为例)
        {
            float error = ratesTarget.x - rates.x;
            // 反算 PID 分项
            float p = error * rollRatePID.p;
            float i = constrain(rollRatePID.integral * rollRatePID.i, -rollRatePID.windup, rollRatePID.windup);
            float d = rollRatePID.derivative * rollRatePID.d;
            // X: P项, Y: I项, Z: D项
            sendDebugVect("R_PID", p, i, d); 
            break;
        }
        case 3: // [速度环] 目标速度 vs 实际速度 (XY轴)
        {
            // 使用之前"偷"出来的全局变量
            // X: 目标速度X, Y: 实际速度X, Z: 速度环积分项X
            sendDebugVect("VEL_X", targetVelBodyX_debug, velocity.x, velIntegralX);
            break;
        }
        case 4: // [位置环/光流] 原始光流数据
        {
            extern float opticalFlowVelocityX, opticalFlowVelocityY, opticalFlowHeight; // 再次确保引用
            // X: 光流X速度, Y: 光流Y速度, Z: 光流高度
            sendDebugVect("FLOW", opticalFlowVelocityX, opticalFlowVelocityY, opticalFlowHeight);
            break;
        }

        // ==========================
        // 模式 5-8: 状态估计调试
        // ==========================
        case 5: // [卡尔曼滤波] 零偏估计 (Bias)
        {
            // 观察陀螺仪零偏是否收敛
            sendDebugVect("K_BIAS", kalmanRoll.getBias(), kalmanPitch.getBias(), 0);
            break;
        }
        case 6: // [姿态估计] 融合效果分析
        {
            extern Vector acc; // 确保引用
            // 重算加速度计角度用于对比
            float accRoll = atan2(acc.y, acc.z); 
            // X: 原始加速度计角度(噪声大), Y: 融合后角度(平滑), Z: 陀螺仪角速度
            sendDebugVect("EST_ROLL", degrees(accRoll), degrees(attitude.getRoll()), degrees(rates.x));
            break;
        }
        case 7: // [高度估计] 传感器 vs 融合结果
        {
            extern Vector acc; // 确保引用
            extern float opticalFlowHeight;
            // 计算垂直加速度(去除重力)
            Vector accWorld = Quaternion::rotateVector(acc, attitude);
            float accZ_linear = accWorld.z - 9.80665f;
            // X: 传感器高度, Y: 估计高度, Z: 垂直加速度
            sendDebugVect("EST_Z", opticalFlowHeight, position.z, accZ_linear);
            break;
        }
        case 8: // [水平速度估计] 光流测量 vs 融合速度
        {
            extern float opticalFlowVelocityY;
            float flow_meas_x = -opticalFlowVelocityY; // 根据 estimate.ino 逻辑
            // X: 光流测量速度(含噪声), Y: 融合后速度, Z: 两者误差
            sendDebugVect("EST_VX", flow_meas_x, velocity.x, flow_meas_x - velocity.x);
            break;
        }
        // ==========================
        // 模式 9-11: 基础状态查看
        // ==========================
        case 9: // [姿态] Roll/Pitch/Yaw
            sendDebugVect("ATT", degrees(attitude.getRoll()), degrees(attitude.getPitch()), degrees(attitude.getYaw()));
            break;
        
        case 10: // [位置] X/Y/Z
            sendDebugVect("POS", position.x, position.y, position.z);
            break;

        case 11: // [速度] VX/VY/VZ
            sendDebugVect("VEL", velocity.x, velocity.y, velocity.z);
            break;

        // ==========================
        // 默认模式 (Mode 0)
        // ==========================
        default: 
        {
            Vector attTgt = attitudeTarget.toEuler();
            // 默认显示前后速度调试信息
            sendDebugVect("TUN_X", velocity.x, velIntegralX, degrees(attTgt.y));
            break;
        }
    }
}

const char* getModeName() {
    switch(mode) {
        case STAB: return "STAB";
        case ALT_HOLD: return "ALT_HOLD";
        case POS_HOLD: return "POS_HOLD";
        case ACRO: return "ACRO";
        default: return "UNKNOWN";
    }
}