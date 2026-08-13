// Optical flow sensor driver implementation
// File: optical_flow.cpp
#ifdef OPTICAL_FLOW_ENABLED

#include <Arduino.h>

// 光流解析状态机
enum ParserState { WAIT_HEADER, RECEIVE_DATA };
ParserState state = WAIT_HEADER;
byte packetBuffer[19];
int byteCount = 0;
unsigned long lastByteTime = 0;
const unsigned long SERIAL1_TIMEOUT_MS = 50; // 20HZ
const unsigned long FLOW_STALE_TIMEOUT_MS = 150;


// 光流参数
const float FLOW_SCALE_FACTOR = 1.0f / 10000.0f;
const float MIN_VALID_HEIGHT = 0.05f; // sensor validity only; not a flight-height restriction
const float MAX_VALID_HEIGHT = 6.0f;


// Optical flow raw data structure
struct OpticalFlowRawData {
    int16_t flowX;           // 光流原始X值
    int16_t flowY;           // 光流原始Y值
    uint16_t tofDistance;    // TOF距离 (mm)
    uint16_t integrationTime;// 积分时间 (us)
    bool dataValid;          // 数据有效标志
    unsigned long timestamp; // 时间戳
};

OpticalFlowRawData opticalFlowRawData = {0};
uint32_t opticalFlowSequence = 0;
uint32_t opticalFlowTimestamp = 0;
float opticalFlowSampleDt = 0.05f;
uint32_t opticalFlowValidPackets = 0;
uint32_t opticalFlowInvalidPackets = 0;

void setupOpticalFlow() {
    // osrbot PIN_MAP 
    Serial1.begin(115200, SERIAL_8N1, 8, 7); // Rx Tx
    memset(&opticalFlowRawData, 0, sizeof(opticalFlowRawData));
    opticalFlowHealthy = false;
}

void readOpticalFlow() {
    // 检查超时
    if (state == RECEIVE_DATA && millis() - lastByteTime > SERIAL1_TIMEOUT_MS) {
        state = WAIT_HEADER;
        byteCount = 0;
    }

    // 读取串口数据
    while (Serial1.available() > 0) {
        byte inByte = Serial1.read();
        lastByteTime = millis();

        switch (state) {
            case WAIT_HEADER:
                if (inByte == 0xDF) {
                    packetBuffer[0] = inByte;
                    byteCount = 1;
                    state = RECEIVE_DATA;
                }
                break;

            case RECEIVE_DATA:
                if (byteCount < 19) {
                    packetBuffer[byteCount] = inByte;
                    byteCount++;
                }
                
                if (byteCount == 19) {
                    processOpticalFlowPacket();
                    state = WAIT_HEADER;
                    byteCount = 0;
                }
                break;
        }
    }

    checkOpticalFlowHealth();
}

void processOpticalFlowPacket() {
    // 检查包头和设备ID
    if (packetBuffer[1] != 0x15 || packetBuffer[2] != 0x00 || 
        packetBuffer[3] != 0x55 || packetBuffer[5] != 0x0C) {
        opticalFlowInvalidPackets++;
        return;
    }

    // 校验和检查
    uint16_t calcChecksum = 0;
    for (int i = 0; i < 18; i++) {
        calcChecksum += packetBuffer[i];
    }
    calcChecksum &= 0xFF;

    if (calcChecksum != packetBuffer[18]) {
        opticalFlowInvalidPackets++;
        return;
    }

    // 解析数据。只有校验通过且测量有效的包才推进序号，控制器据此按 20Hz 更新。
    opticalFlowRawData.tofDistance = (packetBuffer[7] << 8) | packetBuffer[6];
    opticalFlowRawData.flowX = (int16_t)((packetBuffer[11] << 8) | packetBuffer[10]);
    opticalFlowRawData.flowY = (int16_t)((packetBuffer[13] << 8) | packetBuffer[12]);
    opticalFlowRawData.integrationTime = (packetBuffer[15] << 8) | packetBuffer[14];
    opticalFlowRawData.dataValid = (packetBuffer[16] == 245);
    unsigned long now = millis();
    opticalFlowRawData.timestamp = now;

    float sampleHeight = opticalFlowRawData.tofDistance / 1000.0f;
    if (!opticalFlowRawData.dataValid ||
        sampleHeight <= MIN_VALID_HEIGHT ||
        sampleHeight >= MAX_VALID_HEIGHT ||
        opticalFlowRawData.integrationTime == 0) {
        // Keep the last valid measurement until the stale timeout. One bad
        // packet must not disengage altitude/position hold for a single frame.
        opticalFlowInvalidPackets++;
        return;
    }

    if (opticalFlowTimestamp != 0) {
        float packetDt = (now - opticalFlowTimestamp) / 1000.0f;
        if (packetDt >= 0.01f && packetDt <= 0.20f) {
            opticalFlowSampleDt = packetDt;
        }
    }

    float integrationDt = opticalFlowRawData.integrationTime / 1000000.0f;
    opticalFlowHeight = sampleHeight;
    opticalFlowVelocityX = (opticalFlowRawData.flowX * FLOW_SCALE_FACTOR * sampleHeight) / integrationDt;
    opticalFlowVelocityY = (opticalFlowRawData.flowY * FLOW_SCALE_FACTOR * sampleHeight) / integrationDt;
    opticalFlowTimestamp = now;
    opticalFlowSequence++;
    if (opticalFlowSequence == 0) opticalFlowSequence = 1; // reserve 0 for no sample yet
    opticalFlowValidPackets++;
    opticalFlowHealthy = true;
}

void checkOpticalFlowHealth() {
    if (opticalFlowTimestamp == 0 || millis() - opticalFlowTimestamp > FLOW_STALE_TIMEOUT_MS) {
        opticalFlowHealthy = false;
        opticalFlowVelocityX = 0;
        opticalFlowVelocityY = 0;
        opticalFlowHeight = 0;
    }
}

#endif
