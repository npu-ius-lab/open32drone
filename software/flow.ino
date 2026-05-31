// Optical flow sensor driver implementation

#ifdef OPTICAL_FLOW_ENABLED

#include <Arduino.h>

enum ParserState { WAIT_HEADER, RECEIVE_DATA };
ParserState state = WAIT_HEADER;
byte packetBuffer[19];
int byteCount = 0;
unsigned long lastByteTime = 0;
const unsigned long SERIAL1_TIMEOUT_MS = 50;

const float FLOW_SCALE_FACTOR = 1.0f / 10000.0f;
const float MIN_VALID_HEIGHT = 0.3f;
const float MAX_VALID_HEIGHT = 6.0f;
const unsigned long OPTICAL_FLOW_DATA_TIMEOUT_MS = 120;
const uint16_t MIN_INTEGRATION_TIME_US = 5000;
const uint16_t MAX_INTEGRATION_TIME_US = 100000;

struct OpticalFlowRawData {
	int16_t flowX;
	int16_t flowY;
	uint16_t tofDistance;
	uint16_t integrationTime;
	bool dataValid;
	unsigned long timestamp;
};

OpticalFlowRawData opticalFlowRawData = {0};

void processOpticalFlowPacket();
void processOpticalFlowData();
void checkOpticalFlowHealth();

bool opticalFlowFrameFresh() {
	return opticalFlowRawData.timestamp != 0 &&
		millis() - opticalFlowRawData.timestamp <= OPTICAL_FLOW_DATA_TIMEOUT_MS;
}

void setupOpticalFlow() {
	Serial1.begin(115200, SERIAL_8N1, 8, 7);
	memset(&opticalFlowRawData, 0, sizeof(opticalFlowRawData));
	opticalFlowHealthy = false;
}

void readOpticalFlow() {
	if (state == RECEIVE_DATA && millis() - lastByteTime > SERIAL1_TIMEOUT_MS) {
		state = WAIT_HEADER;
		byteCount = 0;
	}

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
	if (packetBuffer[1] != 0x15 || packetBuffer[2] != 0x00 ||
		packetBuffer[3] != 0x55 || packetBuffer[5] != 0x0C) {
		return;
	}

	uint16_t calcChecksum = 0;
	for (int i = 0; i < 18; i++) calcChecksum += packetBuffer[i];
	calcChecksum &= 0xFF;
	if (calcChecksum != packetBuffer[18]) return;

	opticalFlowRawData.tofDistance = (packetBuffer[7] << 8) | packetBuffer[6];
	opticalFlowRawData.flowX = (int16_t)((packetBuffer[11] << 8) | packetBuffer[10]);
	opticalFlowRawData.flowY = (int16_t)((packetBuffer[13] << 8) | packetBuffer[12]);
	opticalFlowRawData.integrationTime = (packetBuffer[15] << 8) | packetBuffer[14];
	opticalFlowRawData.dataValid = (packetBuffer[16] == 245);
	opticalFlowRawData.timestamp = millis();
}

void processOpticalFlowData() {
	float measuredHeight = opticalFlowRawData.tofDistance / 1000.0f;
	bool fresh = opticalFlowFrameFresh();

	if (fresh &&
		opticalFlowRawData.dataValid &&
		measuredHeight > MIN_VALID_HEIGHT &&
		measuredHeight < MAX_VALID_HEIGHT &&
		opticalFlowRawData.integrationTime >= MIN_INTEGRATION_TIME_US &&
		opticalFlowRawData.integrationTime <= MAX_INTEGRATION_TIME_US) {

		opticalFlowHeight = measuredHeight;
		float flowDt = opticalFlowRawData.integrationTime / 1000000.0f;
		opticalFlowVelocityX = (opticalFlowRawData.flowX * FLOW_SCALE_FACTOR * opticalFlowHeight) / flowDt;
		opticalFlowVelocityY = (opticalFlowRawData.flowY * FLOW_SCALE_FACTOR * opticalFlowHeight) / flowDt;
		opticalFlowHealthy = true;
	} else {
		opticalFlowHeight = 0.0f;
		opticalFlowVelocityX = 0.0f;
		opticalFlowVelocityY = 0.0f;
		opticalFlowHealthy = false;
	}
}

void checkOpticalFlowHealth() {
	if (!opticalFlowFrameFresh()) {
		opticalFlowHeight = 0.0f;
		opticalFlowVelocityX = 0.0f;
		opticalFlowVelocityY = 0.0f;
		opticalFlowHealthy = false;
	}
}

#endif
