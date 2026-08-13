// altitude_hold.h
// 定高与气压计头文件 — TOF 定高（叠加式）+ BMP280 驱动接口（驱动保留诊断备用）
// 驱动（altitude.cpp）命名仿照 optical flow 风格：setupBaro / updateBaro / isBaroReady
#ifndef ALTITUDE_HOLD_H
#define ALTITUDE_HOLD_H

// ==================== BMP280 驱动接口（altitude.ino 实现）====================
// 状态机枚举放在头文件：规避 Arduino .ino 原型生成器把 getSensorState()
// 原型提升到 enum 定义之前导致的 "BMP280State was not declared"。
enum BMP280State {
	BMP280_STATE_IDLE,
	BMP280_STATE_CALIBRATING,
	BMP280_STATE_NORMAL_OPERATION,
	BMP280_STATE_ERROR
};

bool isBaroReady();
float getCurrentAltitude();      // 相对高度 (m)
float getAbsoluteAltitude();     // 绝对高度 (m)
float getTemperature();          // 温度 (°C)
float getBaselineAltitude();     // 基准高度 (m)

void setupBaro();
void updateBaro();
void restartBaro();
void recalibrate();
void resetAltitudeBaseline();
void testBaro();
void quickBaroStatus();
void printSensorStatus();
String getSensorStatus();
String getBaroDiagnostics();
bool getRawSensorData(float &pressure, float &temperature); // 原始气压/温度（诊断用）

// ==================== 气压定高系统状态（control.ino 定义）====================
extern bool altitudeHoldEngaged;           // 定高是否激活
extern float altitudeHoldTarget;           // 定高目标高度 (m)
extern float altitudeHoldThrottleCorrection; // 油门修正量
extern float altitudeHoldBaseThrottle;     // 悬停基准油门（interpretControls 需前向声明）

// ==================== 定高 PID 参数（油门叠加式）====================
#define ALT_HOLD_P 0.4              // 比例增益
#define ALT_HOLD_I 0.1              // 积分增益
#define ALT_HOLD_D 0.05             // 微分增益（0.1→0.05：降低对气压噪声的放大）
#define ALT_HOLD_I_LIMIT 0.3        // 积分限幅
#define ALT_HOLD_MAX_CORRECTION 0.15 // 最大油门修正量（0.12→0.15：静态漂移补偿余量，变化率限幅保护突变）

// 定高目标高度下限：防止起飞瞬间 engage 把目标锁在地面(≈0m)，导致飞高后被硬拉坠地
#define ALT_MIN_HOLD_HEIGHT 0.5f    // 最小定高目标高度 (m)
// 气压高度变化率阈值 (m/s)：单帧突变超过此值则丢弃该帧（2.0→1.5，防假高度穿透）
#define ALT_MAX_CLIMB_RATE 1.5f
// 修正量平滑系数（时间常数 ~0.5s）：突变时油门不瞬间跳变，防正反馈
#define ALT_HOLD_CORR_LPF 2.0f
// 高度误差死区 (m)：旋翼气流小幅假高度波动不触发修正
#define ALT_HOLD_DEADBAND 0.03f
// 悬停基准油门自适应速率：修正持续顶格时每 loop 调整基准（1kHz → 0.5/s），
// 收敛到真实悬停油门
#define ALT_HOLD_BASE_ADAPT 0.0005f
// 悬停基准收敛期 (ms)：engage 后此段时间内 baseThrottle 持续跟随杆位
#define ALT_HOLD_BASE_SETTLE 1000
// engage 后目标软启动时间 (ms)：目标在此时段内平滑跟随读数，适应气流/油门变化
#define ALT_HOLD_SETTLE_TIME 2000

// ==================== TOF 高度融合（flow.ino 光流）====================
// TOF 直接测距，无气压气流/温漂/突变问题；有效范围内优先使用，超出回退气压计
#define ALT_TOF_ENABLED 1   // 0 = 禁用 TOF 融合（光流数据不可靠时改 0，回退纯气压计）
#define ALT_TOF_MIN 0.3f    // TOF 有效范围下限 (m)
#define ALT_TOF_MAX 6.0f    // TOF 有效范围上限 (m)
#define ALT_FUSION_MAX_STEP 0.15f // 融合高度单帧突变阈值 (m)：两源均防跳变
// 定高起飞阶段：当前高度低于目标此差值时，油门由杆位直接控制（并持续更新基准）
#define ALT_TAKEOFF_GAP 0.1f
float getFusedAltitude();   // 旧融合接口（未使用；定高现用 getHoldAltitude 纯 TOF）

// 油门杆控制（死区相对悬停基准 base，自动适应不同悬停油门）
#define ALT_STICK_DEADBAND_HALF 0.15f // 死区半宽：杆位偏离悬停基准 ±0.15 内保持高度
#define ALT_STICK_BOTTOM        0.05f // 油门拉到底阈值：低于此才退出定高并归零油门（降落手势）
#define ALT_CLIMB_RATE          0.8f  // 推杆上升速率 (m/s)
#define ALT_DESCEND_RATE        0.3f  // 拉低下降速率上限 (m/s，随拉低程度 0→上限渐进；小飞机)

#endif // ALTITUDE_HOLD_H
