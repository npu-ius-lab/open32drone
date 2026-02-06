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


// 光流参数
const float FLOW_SCALE_FACTOR = 1.0f / 10000.0f;
const float MIN_VALID_HEIGHT = 0.3f;
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
// bool opticalFlowHealthy = false;
// float opticalFlowVelocityX = 0;
// float opticalFlowVelocityY = 0;
// float opticalFlowHeight = 0;

void setupOpticalFlow() {
    Serial1.begin(115200, SERIAL_8N1, 2, 1); // 2-Rx 1-Tx
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

    processOpticalFlowData();
    checkOpticalFlowHealth();
}

void processOpticalFlowPacket() {
    // 检查包头和设备ID
    if (packetBuffer[1] != 0x15 || packetBuffer[2] != 0x00 || 
        packetBuffer[3] != 0x55 || packetBuffer[5] != 0x0C) {
        return;
    }

    // 校验和检查
    uint16_t calcChecksum = 0;
    for (int i = 0; i < 18; i++) {
        calcChecksum += packetBuffer[i];
    }
    calcChecksum &= 0xFF;

    if (calcChecksum != packetBuffer[18]) {
        return;
    }

    // 解析数据
    opticalFlowRawData.tofDistance = (packetBuffer[7] << 8) | packetBuffer[6];
    opticalFlowRawData.flowX = (int16_t)((packetBuffer[11] << 8) | packetBuffer[10]);
    opticalFlowRawData.flowY = (int16_t)((packetBuffer[13] << 8) | packetBuffer[12]);
    opticalFlowRawData.integrationTime = (packetBuffer[15] << 8) | packetBuffer[14];
    opticalFlowRawData.dataValid = (packetBuffer[16] == 245);
    opticalFlowRawData.timestamp = millis();
}

void processOpticalFlowData() {
    opticalFlowHeight = opticalFlowRawData.tofDistance / 1000.0f;
    
    // 检查数据有效性
    if (opticalFlowRawData.dataValid && 
        opticalFlowHeight > MIN_VALID_HEIGHT && 
        opticalFlowHeight < MAX_VALID_HEIGHT &&
        opticalFlowRawData.integrationTime > 0) {
        
        // 计算速度 (米/秒)
        float dt_flow = opticalFlowRawData.integrationTime / 1000000.0f;
        opticalFlowVelocityX = (opticalFlowRawData.flowX * FLOW_SCALE_FACTOR * opticalFlowHeight) / dt_flow;
        opticalFlowVelocityY = (opticalFlowRawData.flowY * FLOW_SCALE_FACTOR * opticalFlowHeight) / dt_flow;
        
        opticalFlowHealthy = true;
    } else {
        opticalFlowVelocityX = 0;
        opticalFlowVelocityY = 0;
        opticalFlowHealthy = false;
    }
}

void checkOpticalFlowHealth() {
    static unsigned long lastHealthyTime = 0;
    
    if (opticalFlowHealthy) {
        lastHealthyTime = millis();
    } else if ((millis() - lastHealthyTime) > 200) {
        opticalFlowHealthy = false;
    }
}

#endif
