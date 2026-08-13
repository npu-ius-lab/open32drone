// BMP280 气压计驱动 — 基于 Adafruit_BMP280 库
// 注意：本文件为 .cpp（独立编译单元），避免 Adafruit_Sensor.h 的 sensor_t
// 与 esp32-camera 的 sensor_t 在同一翻译单元冲突（Arduino .ino 拼接会冲突）

#include <Wire.h>
#include <Adafruit_BMP280.h>
#include "altitude_hold.h"

// ==================== 传感器对象 ====================
// 与 IMU 共享同一 I2C 总线 (Wire: SDA=2, SCL=43)
// BMP280 地址 0x76/0x77，IMU 地址 0x68，无冲突
Adafruit_BMP280 pressureSensor;  // 默认使用 &Wire

// ==================== 工作模式 ====================
#define BMP280_OPERATING_MODE   Adafruit_BMP280::MODE_NORMAL
#define BMP280_TEMP_OVERSAMPLING Adafruit_BMP280::SAMPLING_X2
#define BMP280_PRESS_OVERSAMPLING Adafruit_BMP280::SAMPLING_X16
#define BMP280_IIR_FILTER        Adafruit_BMP280::FILTER_X8

// ==================== BMP280 状态机 ====================
// enum BMP280State 定义在 altitude_hold.h（规避 Arduino 原型生成器问题）

// ==================== 全局变量 ====================
float baselinePressure = 0.0;       // 基准压力 (Pa)
float baselineAltitude = 0.0;       // 基准海拔高度 (m)
float seaLevelPressure = 101325.0;  // 标准海平面压力 (Pa)

// 温度校准参数
float temperatureOffset = 0.0;
float temperatureCorrection = 0.0;
bool temperatureCalibrated = false;

// 校准状态
bool calibrationComplete = false;
bool calibrationInProgress = false;
unsigned long calibrationStartTime = 0;
unsigned long lastCalibrationSampleTime = 0;
const int CALIBRATION_TIME = 5000;  // 5秒校准
const int SAMPLE_COUNT = 10;
int sampleIndex = 0;
float pressureSamples[10];
float temperatureSamples[10];

// 滤波数据
float filteredPressure = 0.0;
float filteredTemperature = 0.0;
const float PRESSURE_ALPHA = 0.1;    // 压力低通滤波系数
const float TEMPERATURE_ALPHA = 0.05; // 温度低通滤波系数

// 温度稳定检测
float temperatureHistory[10];
int tempHistoryIndex = 0;
bool temperatureStable = false;
unsigned long lastTemperatureCheck = 0;
const float TEMP_STABILITY_THRESHOLD = 0.1;

// 实时数据
float currentAltitude = 0.0;     // 绝对高度
float relativeAltitude = 0.0;    // 相对基准高度
float currentTemperature = 0.0;  // 校准后温度
float rawTemperature = 0.0;      // 原始温度

// 状态标志
bool baroInitialized = false;
bool sensorReadError = false;
unsigned long lastReadTime = 0;
unsigned long lastErrorPrintTime = 0;
int consecutiveErrors = 0;

// 状态机
BMP280State baroState = BMP280_STATE_IDLE;

// 性能统计
unsigned long totalReads = 0;
unsigned long successfulReads = 0;
float minTemperature = 100.0;
float maxTemperature = -100.0;

// ==================== 函数声明 ====================
void setupBaro();
void initBaroCalibration();
void updateBaro();
void handleErrorState();
void updateCalibrationNonBlocking();
void displayCalibrationProgress();
void processCalibrationSamples();
void readSensorData();
float calculateAltitude(float pressure);
void restartBaro();
uint8_t getFilterCoefficient();
bool setFilterCoefficient(uint8_t coeff);
void recalibrate();
String getSensorStatus();
float getCurrentAltitude();
float getAbsoluteAltitude();
float getTemperature();
bool isBaroReady();
float getBaselineAltitude();
BMP280State getSensorState();
void printSensorStatus();
void setSeaLevelPressure(float pressure);
bool getRawSensorData(float &pressure, float &temperature);
void resetAltitudeBaseline();
void testBaro();
void quickBaroStatus();
String getBaroDiagnostics();

// 温度校准函数
void calibrateTemperatureWithReference(float knownTemperature);
void autoTemperatureCalibration();
float applyTemperatureCorrection(float rawTemp);
void updateTemperatureStabilityCheck(float temp);
bool isTemperatureStable();
void setTemperatureOffset(float offset);
float getTemperatureOffset();
void resetTemperatureStatistics();

// ==================== 初始化函数 ====================
void setupBaro() {
    Serial.println("===================================");
    Serial.println("   BMP280 气压计模块");
    Serial.println("     带温度校准功能");
    Serial.println("===================================");
    
    // Wire 总线已由 setupIMU() 初始化 (SDA=2, SCL=43, 400kHz)
    Serial.print("使用共享 I2C 总线 (Wire, 与 IMU 共用)...");
    
    // 初始化温度历史数组
    for (int i = 0; i < 10; i++) {
        temperatureHistory[i] = 25.0;
    }
    
    // === BMP280 初始化 ===
    // 尝试默认地址 0x77 和备用地址 0x76
    bool initSuccess = false;
    
    Serial.print("尝试地址 0x77... ");
    if (pressureSensor.begin(0x77, 0x58)) {  // BMP280 默认地址 + 芯片ID
        Serial.println("成功！");
        initSuccess = true;
    } else {
        Serial.println("失败");
        Serial.print("尝试地址 0x76... ");
        if (pressureSensor.begin(0x76, 0x58)) {
            Serial.println("成功！");
            initSuccess = true;
        } else {
            Serial.println("失败");
        }
    }
    
    // I2C 扫描回退
    if (!initSuccess) {
        Serial.println("开始I2C自动扫描 (Wire)...");
        byte error, address;
        for (address = 1; address < 127; address++) {
            Wire.beginTransmission(address);
            error = Wire.endTransmission();
            
            if (error == 0) {
                Serial.print("找到设备在地址: 0x");
                if (address < 16) Serial.print("0");
                Serial.print(address, HEX);
                
                if (pressureSensor.begin(address, 0x58)) {
                    Serial.println(" - BMP280 初始化成功！");
                    initSuccess = true;
                    break;
                }
                Serial.println(" (非BMP280)");
            }
        }
    }
    
    if (!initSuccess) {
        Serial.println("❌ BMP280 传感器连接失败！");
        baroState = BMP280_STATE_ERROR;
        return;
    }
    
    // === 配置传感器 ===
    pressureSensor.setSampling(BMP280_OPERATING_MODE,
                               BMP280_TEMP_OVERSAMPLING,
                               BMP280_PRESS_OVERSAMPLING,
                               BMP280_IIR_FILTER);
    
    Serial.println("BMP280 配置:");
    Serial.println("  模式: Normal (连续)");
    Serial.println("  温度过采样: x2");
    Serial.println("  压力过采样: x16");
    Serial.println("  IIR滤波: x8");
    
    baroInitialized = true;
    resetTemperatureStatistics();
    initBaroCalibration();
}

// ==================== 初始化校准 ====================
void initBaroCalibration() {
    if (!baroInitialized) return;
    
    Serial.println("\n开始传感器校准...");
    Serial.println("请保持设备静止 5 秒");
    Serial.println("正在采集基准数据...");
    
    calibrationComplete = false;
    calibrationInProgress = true;
    calibrationStartTime = millis();
    lastCalibrationSampleTime = millis();
    sampleIndex = 0;
    consecutiveErrors = 0;
    
    baroState = BMP280_STATE_CALIBRATING;
    
    for (int i = 0; i < SAMPLE_COUNT; i++) {
        pressureSamples[i] = 0;
        temperatureSamples[i] = 0;
    }
    
    displayCalibrationProgress();
}

// ==================== 更新函数（非阻塞式） ====================
void updateBaro() {
    switch (baroState) {
        case BMP280_STATE_ERROR:
            handleErrorState();
            break;
            
        case BMP280_STATE_CALIBRATING:
            updateCalibrationNonBlocking();
            break;
            
        case BMP280_STATE_NORMAL_OPERATION:
            if (millis() - lastReadTime >= 100) {  // 10Hz
                readSensorData();
            }
            
            if (millis() - lastTemperatureCheck >= 1000) {
                updateTemperatureStabilityCheck(currentTemperature);
                lastTemperatureCheck = millis();
            }
            break;
            
        case BMP280_STATE_IDLE:
        default:
            if (!baroInitialized) {
                static unsigned long lastInitAttempt = 0;
                if (millis() - lastInitAttempt > 5000) {
                    setupBaro();
                    lastInitAttempt = millis();
                }
            }
            break;
    }
}

// ==================== 处理错误状态 ====================
void handleErrorState() {
    static unsigned long lastReinitAttempt = 0;
    static int reinitAttemptCount = 0;
    
    unsigned long currentTime = millis();
    
    if (currentTime - lastReinitAttempt > 10000) {
        Serial.println("尝试重新初始化 BMP280...");
        baroInitialized = false;
        calibrationComplete = false;
        calibrationInProgress = false;
        
        // Wire 总线由 IMU 管理，不在此处重置
        
        setupBaro();
        
        lastReinitAttempt = currentTime;
        reinitAttemptCount++;
        
        if (reinitAttemptCount > 3) {
            Serial.println("多次重试失败，进入休眠状态");
            baroState = BMP280_STATE_IDLE;
            reinitAttemptCount = 0;
        }
    }
}

// ==================== 非阻塞式校准更新 ====================
void updateCalibrationNonBlocking() {
    if (!baroInitialized || !calibrationInProgress) {
        baroState = BMP280_STATE_ERROR;
        return;
    }
    
    unsigned long currentTime = millis();
    unsigned long elapsedTime = currentTime - calibrationStartTime;
    
    if (elapsedTime >= CALIBRATION_TIME) {
        processCalibrationSamples();
        calibrationInProgress = false;
        
        if (calibrationComplete) {
            baroState = BMP280_STATE_NORMAL_OPERATION;
            Serial.println("\n✅ 传感器校准完成！");
            autoTemperatureCalibration();
        } else {
            Serial.println("\n❌ 校准失败，将重新尝试...");
            baroState = BMP280_STATE_ERROR;
        }
        return;
    }
    
    if (currentTime - lastCalibrationSampleTime >= 200) {  // 5Hz
        lastCalibrationSampleTime = currentTime;
        
        float pressure = pressureSensor.readPressure();
        float temperature = pressureSensor.readTemperature();
        
        // 检查读数有效性
        if (pressure > 30000 && pressure < 120000 && temperature > -40 && temperature < 100) {
            consecutiveErrors = 0;
            
            if (sampleIndex < SAMPLE_COUNT) {
                pressureSamples[sampleIndex] = pressure;
                temperatureSamples[sampleIndex] = temperature;
                sampleIndex++;
            }
            
            static unsigned long lastProgressTime = 0;
            if (currentTime - lastProgressTime >= 5000) {
                displayCalibrationProgress();
                lastProgressTime = currentTime;
            }
        } else {
            consecutiveErrors++;
            if (consecutiveErrors > 5 && currentTime - lastErrorPrintTime > 2000) {
                Serial.println("⚠️ 校准期间传感器读取失败");
                lastErrorPrintTime = currentTime;
                
                if (consecutiveErrors > 20) {
                    Serial.println("❌ 连续错误过多，中止校准");
                    calibrationInProgress = false;
                    baroState = BMP280_STATE_ERROR;
                }
            }
        }
    }
}

// ==================== 显示校准进度 ====================
void displayCalibrationProgress() {
    if (!calibrationInProgress) return;
    
    int elapsedSeconds = (millis() - calibrationStartTime) / 1000;
    int totalSeconds = CALIBRATION_TIME / 1000;
    int percentComplete = (elapsedSeconds * 100) / totalSeconds;
    
    Serial.print("校准进度: ");
    Serial.print(elapsedSeconds);
    Serial.print("/");
    Serial.print(totalSeconds);
    Serial.print("秒 (");
    Serial.print(percentComplete);
    Serial.println("%)");
    
    if (sampleIndex > 0) {
        Serial.print("已采集样本: ");
        Serial.print(sampleIndex);
        Serial.print("/");
        Serial.println(SAMPLE_COUNT);
    }
}

// ==================== 处理校准样本 ====================
void processCalibrationSamples() {
    if (sampleIndex == 0) {
        Serial.println("❌ 校准失败：未采集到有效样本");
        calibrationComplete = false;
        return;
    }
    
    float sumPressure = 0.0;
    float sumTemperature = 0.0;
    int validSamples = 0;
    
    for (int i = 0; i < sampleIndex; i++) {
        if (pressureSamples[i] > 80000 && pressureSamples[i] < 120000) {
            sumPressure += pressureSamples[i];
            sumTemperature += temperatureSamples[i];
            validSamples++;
        }
    }
    
    Serial.print("总样本数: ");
    Serial.print(sampleIndex);
    Serial.print(", 有效样本: ");
    Serial.println(validSamples);
    
    if (validSamples < (SAMPLE_COUNT * 0.5)) {
        Serial.println("❌ 校准失败：有效样本太少");
        calibrationComplete = false;
        return;
    }
    
    baselinePressure = sumPressure / validSamples;
    float avgTemperature = sumTemperature / validSamples;
    
    // 使用标准气压高度公式计算基准高度
    baselineAltitude = calculateAltitude(baselinePressure);
    
    filteredPressure = baselinePressure;
    filteredTemperature = avgTemperature;
    
    rawTemperature = avgTemperature;
    currentTemperature = applyTemperatureCorrection(avgTemperature);
    
    currentAltitude = baselineAltitude;
    relativeAltitude = 0.0;
    
    calibrationComplete = true;
    
    Serial.print("基准压力: ");
    Serial.print(baselinePressure, 1);
    Serial.println(" Pa");
    Serial.print("基准高度: ");
    Serial.print(baselineAltitude, 2);
    Serial.println(" m");
    Serial.print("初始温度: ");
    Serial.print(avgTemperature, 1);
    Serial.println(" °C (原始)");
    Serial.print("校准温度: ");
    Serial.print(currentTemperature, 1);
    Serial.println(" °C");
}

// ==================== 读取传感器数据 ====================
void readSensorData() {
    if (!baroInitialized || !calibrationComplete) {
        return;
    }
    
    totalReads++;
    unsigned long currentTime = millis();
    
    float pressure = pressureSensor.readPressure();
    float temperature = pressureSensor.readTemperature();
    
    // 验证读数
    if (pressure > 30000 && pressure < 120000 && temperature > -40 && temperature < 100) {
        successfulReads++;
        sensorReadError = false;
        consecutiveErrors = 0;
        
        // 低通滤波
        filteredPressure = filteredPressure * (1 - PRESSURE_ALPHA) + pressure * PRESSURE_ALPHA;
        filteredTemperature = filteredTemperature * (1 - TEMPERATURE_ALPHA) + temperature * TEMPERATURE_ALPHA;
        
        rawTemperature = temperature;
        currentTemperature = applyTemperatureCorrection(filteredTemperature);
        
        if (currentTemperature < minTemperature) minTemperature = currentTemperature;
        if (currentTemperature > maxTemperature) maxTemperature = currentTemperature;
        
        // 计算高度
        currentAltitude = calculateAltitude(filteredPressure);
        relativeAltitude = currentAltitude - baselineAltitude;
        
        lastReadTime = currentTime;
        
    } else {
        sensorReadError = true;
        consecutiveErrors++;
        
        if (currentTime - lastErrorPrintTime > 2000) {
            Serial.print("BMP280 读取异常: P=");
            Serial.print(pressure, 0);
            Serial.print("Pa T=");
            Serial.print(temperature, 1);
            Serial.println("°C");
            
            if (consecutiveErrors > 10) {
                Serial.println("⚠️ BMP280 传感器可能已断开");
                baroState = BMP280_STATE_ERROR;
            }
            
            lastErrorPrintTime = currentTime;
        }
    }
}

// ==================== 温度校准函数 ====================
void calibrateTemperatureWithReference(float knownTemperature) {
    if (!baroInitialized || !calibrationComplete) {
        Serial.println("传感器未就绪，无法进行温度校准");
        return;
    }
    
    if (!temperatureStable) {
        Serial.println("温度未稳定，请稍后重试");
        return;
    }
    
    temperatureOffset = knownTemperature - currentTemperature;
    temperatureCalibrated = true;
    
    Serial.print("✅ 温度校准完成！");
    Serial.print(" 参考温度: ");
    Serial.print(knownTemperature, 1);
    Serial.println(" °C");
    Serial.print("测量温度: ");
    Serial.print(currentTemperature, 1);
    Serial.println(" °C");
    Serial.print("偏移量: ");
    Serial.print(temperatureOffset, 2);
    Serial.println(" °C");
    
    currentTemperature = applyTemperatureCorrection(rawTemperature);
}

void autoTemperatureCalibration() {
    if (!baroInitialized || !calibrationComplete) return;
    
    if (currentTemperature < 15.0 || currentTemperature > 40.0) {
        float expectedTemp = 25.0;
        temperatureCorrection = expectedTemp - currentTemperature;
        Serial.print("⚠️ 自动温度校正: ");
        Serial.print(temperatureCorrection, 1);
        Serial.println(" °C");
        currentTemperature = applyTemperatureCorrection(rawTemperature);
    }
}

float applyTemperatureCorrection(float rawTemp) {
    float correctedTemp = rawTemp + temperatureOffset + temperatureCorrection;
    correctedTemp = constrain(correctedTemp, -50.0, 100.0);
    return correctedTemp;
}

void updateTemperatureStabilityCheck(float temp) {
    temperatureHistory[tempHistoryIndex] = temp;
    tempHistoryIndex = (tempHistoryIndex + 1) % 10;
    
    float sum = 0, sumSq = 0;
    int count = 0;
    
    for (int i = 0; i < 10; i++) {
        if (temperatureHistory[i] != 0) {
            sum += temperatureHistory[i];
            sumSq += temperatureHistory[i] * temperatureHistory[i];
            count++;
        }
    }
    
    if (count >= 5) {
        float mean = sum / count;
        float variance = (sumSq / count) - (mean * mean);
        temperatureStable = (variance < TEMP_STABILITY_THRESHOLD * TEMP_STABILITY_THRESHOLD);
    }
}

bool isTemperatureStable() {
    return temperatureStable;
}

void setTemperatureOffset(float offset) {
    temperatureOffset = offset;
    temperatureCalibrated = true;
    Serial.print("温度偏移设置为: ");
    Serial.print(temperatureOffset, 2);
    Serial.println(" °C");
    if (calibrationComplete) {
        currentTemperature = applyTemperatureCorrection(rawTemperature);
    }
}

float getTemperatureOffset() {
    return temperatureOffset;
}

void resetTemperatureStatistics() {
    minTemperature = 100.0;
    maxTemperature = -100.0;
    totalReads = 0;
    successfulReads = 0;
}

// ==================== 计算高度 (标准气压公式) ====================
float calculateAltitude(float pressure) {
    if (pressure <= 0) return 0.0;
    float ratio = pressure / seaLevelPressure;
    if (ratio < 0.1 || ratio > 10.0) return 0.0;
    return 44330.8 * (1.0 - pow(ratio, 0.190284));
}

// ==================== 公共接口函数 ====================

void restartBaro() {
    Serial.println("正在重启 BMP280 传感器...");
    baroState = BMP280_STATE_IDLE;
    baroInitialized = false;
    calibrationComplete = false;
    calibrationInProgress = false;
    sensorReadError = false;
    consecutiveErrors = 0;
    delay(100);
    setupBaro();
}

// BMP280 不支持动态 IIR 滤波系数切换，这里返回过采样映射值
uint8_t getFilterCoefficient() {
    // 返回当前过采样级别: 0=skip, 1=x1, 2=x2, 4=x4, 8=x8, 16=x16
    return (uint8_t)BMP280_PRESS_OVERSAMPLING;
}

bool setFilterCoefficient(uint8_t coeff) {
    // BMP280 通过 setSampling 配置，支持的过采样: 0, 1, 2, 4, 8, 16
    Adafruit_BMP280::sensor_sampling sampling;
    switch (coeff) {
        case 0:  sampling = Adafruit_BMP280::SAMPLING_NONE; break;
        case 1:  sampling = Adafruit_BMP280::SAMPLING_X1;   break;
        case 2:  sampling = Adafruit_BMP280::SAMPLING_X2;   break;
        case 4:  sampling = Adafruit_BMP280::SAMPLING_X4;   break;
        case 8:  sampling = Adafruit_BMP280::SAMPLING_X8;   break;
        case 16: sampling = Adafruit_BMP280::SAMPLING_X16;  break;
        default: return false;
    }
    
    pressureSensor.setSampling(BMP280_OPERATING_MODE,
                               BMP280_TEMP_OVERSAMPLING,
                               sampling,
                               BMP280_IIR_FILTER);
    
    Serial.print("压力过采样设置为: x");
    Serial.println(coeff);
    return true;
}

void recalibrate() {
    if (!baroInitialized) {
        Serial.println("传感器未初始化，无法重新校准");
        return;
    }
    Serial.println("\n开始手动重新校准...");
    initBaroCalibration();
}

String getSensorStatus() {
    String status = "";
    status += "BMP280 状态: ";
    status += baroInitialized ? "已初始化" : "未初始化";
    status += "\n";
    
    status += "运行状态: ";
    switch (baroState) {
        case BMP280_STATE_IDLE:             status += "空闲"; break;
        case BMP280_STATE_CALIBRATING:      status += "校准中"; break;
        case BMP280_STATE_NORMAL_OPERATION: status += "正常运行"; break;
        case BMP280_STATE_ERROR:            status += "错误"; break;
        default: status += "未知"; break;
    }
    status += "\n";
    
    if (baroInitialized && calibrationComplete) {
        status += "当前高度: " + String(currentAltitude, 2) + " m\n";
        status += "相对高度: " + String(relativeAltitude, 3) + " m\n";
        status += "温度: " + String(currentTemperature, 2) + " °C\n";
        status += "读取成功率: ";
        if (totalReads > 0) {
            float successRate = (successfulReads * 100.0) / totalReads;
            status += String(successRate, 1) + "%\n";
        } else {
            status += "N/A\n";
        }
    }
    
    return status;
}

float getCurrentAltitude() {
    // 单帧突变丢弃：高度变化超过阈值（ALT_MAX_CLIMB_RATE @10Hz）则忽略该帧，
    // 防止旋翼气流/传感器偶发突变产生假高度
    static float lastAlt = 0;
    static bool first = true;
    float alt = relativeAltitude;
    if (first) {
        lastAlt = alt;
        first = false;
        return alt;
    }
    float maxStep = ALT_MAX_CLIMB_RATE / 10.0f;
    float delta = alt - lastAlt;
    if (delta > maxStep || delta < -maxStep) {
        return lastAlt; // 突变帧丢弃，保持上一帧
    }
    lastAlt = alt;
    return alt;
}

float getAbsoluteAltitude() {
    return currentAltitude;
}

float getTemperature() {
    return currentTemperature;
}

bool isBaroReady() {
    return baroInitialized && calibrationComplete && !sensorReadError &&
           baroState == BMP280_STATE_NORMAL_OPERATION;
}

float getBaselineAltitude() {
    return baselineAltitude;
}

BMP280State getSensorState() {
    return baroState;
}

void printSensorStatus() {
    if (!baroInitialized) {
        Serial.println("❌ BMP280 未初始化");
        return;
    }
    
    Serial.println("\n=== BMP280 传感器状态 ===");
    Serial.print("初始化状态: ");
    Serial.println(baroInitialized ? "✅ 成功" : "❌ 失败");
    
    Serial.print("运行状态: ");
    switch (baroState) {
        case BMP280_STATE_IDLE:             Serial.println("🔄 空闲"); break;
        case BMP280_STATE_CALIBRATING:      Serial.println("🔄 校准中"); break;
        case BMP280_STATE_NORMAL_OPERATION: Serial.println("✅ 正常运行"); break;
        case BMP280_STATE_ERROR:            Serial.println("❌ 错误"); break;
        default: Serial.println("❓ 未知"); break;
    }
    
    if (calibrationComplete) {
        Serial.print("基准高度: ");
        Serial.print(baselineAltitude, 2);
        Serial.println(" m");
        Serial.print("当前高度: ");
        Serial.print(currentAltitude, 2);
        Serial.println(" m (绝对)");
        Serial.print("相对高度: ");
        Serial.print(relativeAltitude, 3);
        Serial.println(" m");
        Serial.print("温度: ");
        Serial.print(currentTemperature, 2);
        Serial.println(" °C");
        Serial.print("压力过采样: x");
        Serial.println((int)BMP280_PRESS_OVERSAMPLING);
    }
    
    Serial.print("最后读取: ");
    if (lastReadTime > 0) {
        Serial.print((millis() - lastReadTime) / 1000.0, 1);
        Serial.println(" 秒前");
    } else {
        Serial.println("从未读取");
    }
    
    Serial.println("========================\n");
}

void setSeaLevelPressure(float pressure) {
    if (pressure > 80000 && pressure < 120000) {
        seaLevelPressure = pressure;
        if (calibrationComplete) {
            baselineAltitude = calculateAltitude(baselinePressure);
            currentAltitude = calculateAltitude(filteredPressure);
            relativeAltitude = currentAltitude - baselineAltitude;
        }
    }
}

bool getRawSensorData(float &pressure, float &temperature) {
    if (!baroInitialized) return false;
    
    pressure = pressureSensor.readPressure();
    temperature = pressureSensor.readTemperature();
    
    return (pressure > 30000 && pressure < 120000);
}

void resetAltitudeBaseline() {
    if (!baroInitialized || !calibrationComplete) {
        Serial.println("传感器未就绪，无法重置高度基准");
        return;
    }
    
    baselinePressure = filteredPressure;
    baselineAltitude = calculateAltitude(baselinePressure);
    relativeAltitude = 0.0;
    Serial.println("✅ 高度基准已重置");
}

void testBaro() {
    Serial.println("\n=== BMP280 测试 ===");
    
    if (!baroInitialized) {
        Serial.println("传感器未初始化");
        return;
    }
    
    float pressure = pressureSensor.readPressure();
    float temperature = pressureSensor.readTemperature();
    
    Serial.print("压力: ");
    Serial.print(pressure, 1);
    Serial.println(" Pa");
    Serial.print("温度: ");
    Serial.print(temperature, 1);
    Serial.println(" °C");
    Serial.print("校准温度: ");
    Serial.print(applyTemperatureCorrection(temperature), 1);
    Serial.println(" °C");
    
    float altitude = calculateAltitude(pressure);
    Serial.print("计算高度: ");
    Serial.print(altitude, 2);
    Serial.println(" m");
    
    if (pressure > 30000 && pressure < 120000) {
        Serial.println("✅ 传感器读取正常");
    } else {
        Serial.println("⚠️ 读数异常");
    }
    
    Serial.println("==================\n");
}

void quickBaroStatus() {
    if (!baroInitialized) {
        Serial.println("BMP280: 未初始化");
        return;
    }
    
    if (calibrationInProgress) {
        int elapsed = (millis() - calibrationStartTime) / 1000;
        int total = CALIBRATION_TIME / 1000;
        Serial.print("BMP280: 校准中 ");
        Serial.print(elapsed);
        Serial.print("/");
        Serial.print(total);
        Serial.println("秒");
    } else if (calibrationComplete) {
        Serial.print("BMP280: 高度=");
        Serial.print(relativeAltitude, 2);
        Serial.print("m, 温度=");
        Serial.print(currentTemperature, 1);
        Serial.println("°C");
    } else {
        Serial.println("BMP280: 校准未完成");
    }
}

String getBaroDiagnostics() {
    String diag = "=== BMP280 诊断信息 ===\n";
    diag += "初始化状态: " + String(baroInitialized ? "成功" : "失败") + "\n";
    diag += "当前状态: ";
    
    switch (baroState) {
        case BMP280_STATE_IDLE:             diag += "空闲"; break;
        case BMP280_STATE_CALIBRATING:      diag += "校准中"; break;
        case BMP280_STATE_NORMAL_OPERATION: diag += "正常运行"; break;
        case BMP280_STATE_ERROR:            diag += "错误"; break;
        default: diag += "未知"; break;
    }
    diag += "\n";
    
    diag += "校准状态: " + String(calibrationComplete ? "完成" : (calibrationInProgress ? "进行中" : "未开始")) + "\n";
    diag += "温度稳定性: " + String(temperatureStable ? "稳定" : "波动") + "\n";
    diag += "连续错误: " + String(consecutiveErrors) + "\n";
    diag += "读取成功率: " + String(totalReads > 0 ? String((successfulReads * 100.0) / totalReads, 1) + "%" : "N/A") + "\n";
    
    if (calibrationComplete) {
        diag += "基准压力: " + String(baselinePressure, 1) + " Pa\n";
        diag += "相对高度: " + String(relativeAltitude, 3) + " m\n";
        diag += "校准温度: " + String(currentTemperature, 2) + " °C\n";
    }
    
    diag += "=====================\n";
    return diag;
}
