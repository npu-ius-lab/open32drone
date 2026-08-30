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
const unsigned long SERIAL1_TIMEOUT_MS = 50; // 50 Hz packets; allow more than two packet periods
const unsigned long FLOW_STALE_TIMEOUT_MS = 150;
const unsigned long FLOW_UART_RECOVERY_MS = 1000;
const uint16_t TOF_SPEC_MIN_RANGE_MM = 20; // TF-0850 datasheet blind zone
unsigned long lastUartRecoveryTime = 0;
uint32_t opticalFlowUartRestarts = 0;


// 光流参数
const float FLOW_SCALE_FACTOR = 1.0f / 10000.0f;
const float MAX_VALID_HEIGHT = 6.0f;


// Optical flow raw data structure
struct OpticalFlowRawData {
    int16_t flowX;           // 光流原始X值
    int16_t flowY;           // 光流原始Y值
    uint16_t tofDistance;    // TOF距离 (mm)
    uint8_t tofStrength;     // TOF强度 (0-100), diagnostic only
    uint16_t integrationTime;// 积分时间 (us)
    uint8_t packetSequence;  // TF-0850 packet sequence (0-255)
    uint8_t moduleVersion;   // 光流模块版本号
    bool dataValid;          // 数据有效标志
    unsigned long timestamp; // 时间戳
};

OpticalFlowRawData opticalFlowRawData{};
uint32_t opticalFlowSequence = 0;
uint32_t opticalFlowTimestamp = 0;
float opticalFlowSampleDt = 0.02f;
uint32_t tofSequence = 0;
uint32_t tofTimestamp = 0;
float tofSampleDt = 0.02f;
uint32_t tofPacketTimestamp = 0;
bool tofPacketHealthy = false;
bool tofRangeInBlindZone = false;
uint16_t opticalFlowTofDistanceMm = 0;
uint32_t opticalFlowValidPackets = 0;
uint32_t opticalFlowInvalidPackets = 0;
uint32_t tofValidPackets = 0;
uint32_t tofInvalidPackets = 0;
uint8_t opticalFlowSensorPacketSequence = 0;
uint8_t opticalFlowTofStrength = 0;
uint8_t opticalFlowModuleVersion = 0;
uint16_t opticalFlowIntegrationTimeUs = 0;
uint16_t opticalFlowIntegrationTimeMinUs = 0;
uint16_t opticalFlowIntegrationTimeMaxUs = 0;
uint32_t opticalFlowPacketGapCount = 0;
uint32_t opticalFlowPacketDuplicateCount = 0;
uint32_t opticalFlowPacketOutOfOrderCount = 0;
bool opticalFlowSensorPacketSequenceSeen = false;

void setupOpticalFlow() {
    // osrbot PIN_MAP
    Serial1.begin(115200, SERIAL_8N1, 8, 7); // Rx Tx
    memset(&opticalFlowRawData, 0, sizeof(opticalFlowRawData));
    tofHealthy = false;
    opticalFlowHealthy = false;
}

void readOpticalFlow() {
	unsigned long now = millis();
	// A brownout/noise event can leave the ESP32 UART receive path silent even
	// after the external flow sensor resumes. Re-open only after a full second
	// without any byte; this is non-blocking and never reacts to merely-invalid
	// packets that are still arriving.
	if (now > FLOW_UART_RECOVERY_MS && now - lastByteTime > FLOW_UART_RECOVERY_MS &&
		now - lastUartRecoveryTime > FLOW_UART_RECOVERY_MS) {
		state = WAIT_HEADER;
		byteCount = 0;
		Serial1.end();
		Serial1.begin(115200, SERIAL_8N1, 8, 7);
		lastUartRecoveryTime = now;
		opticalFlowUartRestarts++;
	}

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
        tofInvalidPackets++;
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
        tofInvalidPackets++;
        return;
    }

    // ToF distance and XY optical flow share one UART packet, but byte 16 only
    // qualifies the optical-flow integration. Near the floor XY flow may be
    // invalid while the range measurement remains valid. Keep independent
    // health, sequence and timing state so assisted takeoff can start on the
    // ground and position hold waits for real XY samples after liftoff.
    opticalFlowRawData.tofDistance = (packetBuffer[7] << 8) | packetBuffer[6];
    opticalFlowRawData.tofStrength = packetBuffer[8];
    opticalFlowRawData.flowX = (int16_t)((packetBuffer[11] << 8) | packetBuffer[10]);
    opticalFlowRawData.flowY = (int16_t)((packetBuffer[13] << 8) | packetBuffer[12]);
    opticalFlowRawData.integrationTime = (packetBuffer[15] << 8) | packetBuffer[14];
    opticalFlowRawData.packetSequence = packetBuffer[4];
    opticalFlowRawData.moduleVersion = packetBuffer[17];
    opticalFlowRawData.dataValid = (packetBuffer[16] == 245);
    unsigned long now = millis();
    opticalFlowRawData.timestamp = now;
    // Packet transport and usable ranging are different states. On the launch
    // surface the TF-0850 may report inside its 20 mm blind zone while its UART
    // stream is completely healthy. Keep that link evidence so boot validation
    // and the takeoff bootstrap do not require the aircraft to be hand-lifted.
    opticalFlowTofDistanceMm = opticalFlowRawData.tofDistance;
    tofPacketTimestamp = now;
    tofPacketHealthy = true;
    tofRangeInBlindZone = opticalFlowTofDistanceMm < TOF_SPEC_MIN_RANGE_MM;

    // Keep the vendor-provided quality and transport fields observable without
    // changing any control gate. The TF-0850 datasheet does not define a
    // strength threshold, so strength remains diagnostic-only.
    uint8_t sensorSequence = opticalFlowRawData.packetSequence;
    if (opticalFlowSensorPacketSequenceSeen) {
        uint8_t sequenceDelta = (uint8_t)(sensorSequence - opticalFlowSensorPacketSequence);
        if (sequenceDelta == 0) {
            opticalFlowPacketDuplicateCount++;
        } else if (sequenceDelta < 128) {
            opticalFlowPacketGapCount += sequenceDelta - 1;
        } else {
            opticalFlowPacketOutOfOrderCount++;
        }
    }
    opticalFlowSensorPacketSequence = sensorSequence;
    opticalFlowSensorPacketSequenceSeen = true;
    opticalFlowTofStrength = opticalFlowRawData.tofStrength;
    opticalFlowModuleVersion = opticalFlowRawData.moduleVersion;
    opticalFlowIntegrationTimeUs = opticalFlowRawData.integrationTime;
    if (opticalFlowIntegrationTimeUs > 0) {
        if (opticalFlowIntegrationTimeMinUs == 0 ||
            opticalFlowIntegrationTimeUs < opticalFlowIntegrationTimeMinUs) {
            opticalFlowIntegrationTimeMinUs = opticalFlowIntegrationTimeUs;
        }
        if (opticalFlowIntegrationTimeUs > opticalFlowIntegrationTimeMaxUs) {
            opticalFlowIntegrationTimeMaxUs = opticalFlowIntegrationTimeUs;
        }
    }

    float sampleHeight = opticalFlowRawData.tofDistance / 1000.0f;
    bool tofSampleValid = opticalFlowTofDistanceMm >= TOF_SPEC_MIN_RANGE_MM &&
        sampleHeight < MAX_VALID_HEIGHT;
    if (tofSampleValid) {
        if (tofTimestamp != 0) {
            float packetDt = (now - tofTimestamp) / 1000.0f;
            if (packetDt >= 0.01f && packetDt <= 0.20f) tofSampleDt = packetDt;
        }
        opticalFlowHeight = sampleHeight;
        tofTimestamp = now;
        tofSequence++;
        if (tofSequence == 0) tofSequence = 1;
        tofValidPackets++;
        tofHealthy = true;
    } else {
        // A single rejected range sample does not drop altitude control. The
        // stale timeout below owns that transition.
        tofInvalidPackets++;
    }

    if (!opticalFlowRawData.dataValid || !tofSampleValid ||
        opticalFlowRawData.integrationTime == 0) {
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
    unsigned long now = millis();
    if (tofPacketTimestamp == 0 || now - tofPacketTimestamp > FLOW_STALE_TIMEOUT_MS) {
        tofPacketHealthy = false;
        tofRangeInBlindZone = false;
    }
    if (tofTimestamp == 0 || now - tofTimestamp > FLOW_STALE_TIMEOUT_MS) {
        tofHealthy = false;
        opticalFlowHeight = 0;
    }
    if (opticalFlowTimestamp == 0 || now - opticalFlowTimestamp > FLOW_STALE_TIMEOUT_MS) {
        opticalFlowHealthy = false;
        opticalFlowVelocityX = 0;
        opticalFlowVelocityY = 0;
    }
}

#endif
