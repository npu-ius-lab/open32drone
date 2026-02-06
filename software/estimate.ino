// Copyright (c) 2023 Open32Drone Project
// Repository: https://github.com/okalachev/flix

#include "quaternion.h"
#include "vector.h"
#include "lpf.h"
#include "util.h"
#include "kalman_angle.h"

#ifndef ONE_G
#define ONE_G 9.80665f
#endif

// ============================================================================
//                               参数配置区域
// ============================================================================

// --- 1. 姿态解算参数 (Kalman Filter) ---
#define RATES_LFP_ALPHA 0.2f        // 角速度低通滤波系数
#define BOARD_ALIGN_PITCH 0.01f     // 板载安装误差修正 (Pitch)
#define BOARD_ALIGN_ROLL -0.035f    // 板载安装误差修正 (Roll)

// --- 2. 定高参数 (Z轴融合) ---
#define ACC_Z_DEADBAND 1.5f         // 垂直加速度死区 (忽略微小震动)
#define VEL_Z_DAMPING 0.99f         // 垂直速度阻尼 (模拟空气阻力)
#define POS_Z_CORRECTION_GAIN 0.05f // 高度观测校正增益 (Sonar/Baro -> PosZ)
#define VEL_Z_CORRECTION_GAIN 0.10f // 垂直速度校正增益 (Sonar/Baro -> VelZ)
#define TERRAIN_JUMP_THRESHOLD 0.3f // 地形突变判定阈值 (米)

// --- 3. 水平位置参数 (XY轴融合) ---
#define FLOW_SMOOTHING 0.4f         // 光流速度平滑系数 (1.0=最灵敏, 0.1=最平滑)
#define TILT_SAFE_LIMIT 0.82f       // 倾角安全限制 (cos(35deg) ≈ 0.82)
#define VEL_DAMPING_XY 0.95f        // 无光流时的水平速度阻尼

// ============================================================================
//                               全局对象与变量
// ============================================================================

// --- 滤波器实例 ---
KalmanAngle kalmanRoll;
KalmanAngle kalmanPitch;
static LowPassFilter<Vector> ratesFilter(RATES_LFP_ALPHA);

// --- 状态变量 ---
// Yaw 轴积分 (无磁力计模式)
float yaw_accumulated = 0;

// --- 外部引用 ---
extern Vector gyro;
extern Vector acc;
extern Vector rates;       // 滤波后的角速度
extern Quaternion attitude;
extern float dt;
extern bool landed;
extern Vector velocity;    // 机体坐标系速度 (XY), 世界系 (Z)
extern Vector position;    // 全局位置 (NED世界坐标系)

// --- 外部传感器数据 ---
extern bool opticalFlowHealthy;
extern float opticalFlowVelocityX; // 像素速度或物理速度 (m/s)
extern float opticalFlowVelocityY; // 像素速度或物理速度 (m/s)
extern float opticalFlowHeight;    // 对地高度
extern bool armed;
extern float controlThrottle;      // 用于判断是否在地面

// --- 外部函数 ---
extern bool motorsActive();
extern void sendDebugVect(const char* name, float x, float y, float z);

// --- 本地函数声明 ---
void estimateAttitude();
void estimateHeight();
void estimatePositionXY();

// ============================================================================
//                               主处理函数
// ============================================================================

void estimate() {
    // 1. 姿态解算 (Acc + Gyro -> Roll/Pitch/Yaw)
    estimateAttitude();

    // 2. 落地状态检测
    float accNorm = acc.norm();
    landed = !motorsActive() && abs(accNorm - ONE_G) < ONE_G * 0.1f;

    // 3. 位置与速度估计
    estimateHeight();           
    estimatePositionXY();
}

// ============================================================================
//                               子模块实现
// ============================================================================

// ----------------------------------------------------------------------------
// 1. 姿态解算
// ----------------------------------------------------------------------------
void estimateAttitude() {
    // A. 预处理角速度
    rates = ratesFilter.update(gyro);

    // B. 计算加速度计观测角度 (Measurement)
    // Roll: atan2(acc.y, acc.z)
    float accRollAngle = atan2(acc.y, acc.z);
    // Pitch: atan2(-acc.x, sqrt(y^2 + z^2))
    float accPitchAngle = atan2(-acc.x, sqrt(acc.y * acc.y + acc.z * acc.z));

    // C. 应用板载安装误差修正
    accRollAngle += BOARD_ALIGN_ROLL;
    accPitchAngle += BOARD_ALIGN_PITCH;

    // D. 卡尔曼滤波更新
    // getAngle(观测角度, 角速度, dt)
    float estimatedRoll = kalmanRoll.getAngle(accRollAngle, rates.x, dt);
    float estimatedPitch = kalmanPitch.getAngle(accPitchAngle, rates.y, dt);

    // E. Yaw轴处理 (纯积分)
    yaw_accumulated += rates.z * dt;
    // 归一化到 [-PI, PI]
    if (yaw_accumulated > PI) yaw_accumulated -= 2 * PI;
    if (yaw_accumulated < -PI) yaw_accumulated += 2 * PI;

    // F. 更新全局姿态四元数
    attitude = Quaternion::fromEuler(Vector(estimatedRoll, estimatedPitch, yaw_accumulated));
}

// ----------------------------------------------------------------------------
// 2. 高度估计
// ----------------------------------------------------------------------------
void estimateHeight() {
    if (dt <= 0) return;

    // A. 计算世界坐标系下的垂直加速度
    Vector accWorld = Quaternion::rotateVector(acc, attitude);
    float acc_diff = accWorld.z - ONE_G;

    // 死区处理，消除静止时的噪声漂移
    float accZ_linear = 0;
    if (abs(acc_diff) > ACC_Z_DEADBAND) {
        accZ_linear = acc_diff;
    }

    // B. 惯性导航预测 (Predict)
    // s = s + v*t + 0.5*a*t^2
    position.z += velocity.z * dt + 0.5f * accZ_linear * dt * dt;
    velocity.z += accZ_linear * dt;
    
    // 阻尼 (由于气压计/超声波滞后，纯积分容易发散，这里加一点阻尼稳住)
    velocity.z *= VEL_Z_DAMPING; 

    // C. 观测融合
    // 注意：这里假设 opticalFlowHeight 是准确的对地高度 (来自超声波或激光)
    if (opticalFlowHeight > 0.05f) {
        float posError = opticalFlowHeight - position.z;
        
        // 地形突变检测 (Step Change Detection)
        // 场景：飞机平飞越过桌子边缘，高度读数突变，但加速度没变。
        // 此时不应该认为是飞机瞬间掉下去了，而应该认为是地形变了。
        bool terrain_jump = abs(posError) > TERRAIN_JUMP_THRESHOLD && abs(velocity.z) < 1.0f;

        if (terrain_jump) {
            // 缓慢跟随地形变化 (低增益)
            position.z += posError * 0.02f; 
        } else {
            // 正常校正 (高增益)
            position.z += posError * POS_Z_CORRECTION_GAIN;
            velocity.z += posError * VEL_Z_CORRECTION_GAIN;
        }
    }

    // D. 地面约束
    if (position.z < 0) {
        position.z = 0;
        if (velocity.z < 0) velocity.z = 0;
    }
}

// ----------------------------------------------------------------------------
// 3. 水平位置估计 
// ----------------------------------------------------------------------------
void estimatePositionXY() {
    if (dt <= 0) return;

    // A. 落地检查
    // 如果电机未解锁，或油门很低且高度很低，认为在地面 -> 速度归零
    bool on_ground = !armed || (position.z < 0.15f && controlThrottle < 0.05f);
    if (on_ground) {
        velocity.x = 0;
        velocity.y = 0;
        return; 
    }
    
    // B. 倾角检查 (安全保护)
    // 如果倾角过大，光流大概率失效，不再信任
    Vector up = Quaternion::rotateVector(Vector(0, 0, 1), attitude);
    bool is_tilted_safe = abs(up.z) > TILT_SAFE_LIMIT; 

    // C. 光流数据融合
    bool flow_is_valid = opticalFlowHealthy && opticalFlowHeight > 0.05f && is_tilted_safe;

    if (flow_is_valid) {
        // 需根据实际传感器调整符号    
        float flow_meas_x = -opticalFlowVelocityY; 
        float flow_meas_y = opticalFlowVelocityX;  

        // 平滑逼近
        velocity.x += (flow_meas_x - velocity.x) * FLOW_SMOOTHING;
        velocity.y += (flow_meas_y - velocity.y) * FLOW_SMOOTHING;

        // 零速锁定 =
        // 当光流读数极小时，强制衰减速度，增强悬停稳定性
        if (abs(flow_meas_x) < 0.05f) velocity.x *= 0.8f; 
        if (abs(flow_meas_y) < 0.05f) velocity.y *= 0.8f;

    } else {
        // 无光流信号时，水平速度阻尼，避免漂移
        velocity.x *= VEL_DAMPING_XY; 
        velocity.y *= VEL_DAMPING_XY;
    }

    // D. 位置积分 (机体系速度 -> 世界系位置)
    Vector bodyVel(velocity.x, velocity.y, 0);
    Vector worldVel = Quaternion::rotateVector(bodyVel, attitude);
    
    position.x += worldVel.x * dt;
    position.y += worldVel.y * dt;
}