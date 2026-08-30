// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Main firmware file

#include "vector.h"
#include "quaternion.h"
#include "util.h"
#include "esp_http_server.h" // required before Arduino-generated OTA handler prototypes

#define WIFI_ENABLED 1
const char OPEN32DRONE_BUILD_ID[] = "minimal";
const uint32_t OPEN32DRONE_FLIGHT_SW_VERSION = (0U << 24) | (1U << 16) | (0U << 8) | 255U;
#define OPTICAL_FLOW_ENABLED 1
#define FLOW_SENSOR_MIN_HEIGHT 0.015f // near-ground packets: launch readiness and ground-bias learning
Vector position; // estimated position in world frame, m
// Horizontal velocity is body FLU; vertical velocity is world-up. Use
// worldVelocityEstimate() at telemetry/world-position boundaries.
Vector velocity;

// OTA is a hard flight-control exclusion: uploads are accepted only while
// disarmed and landed, and arming remains blocked for the whole flash write.
#if WIFI_ENABLED
extern volatile bool otaUpdateActive;
#endif

bool tofHealthy = false;
bool opticalFlowHealthy = false;
float opticalFlowVelocityX = 0;
float opticalFlowVelocityY = 0;
float opticalFlowHeight = 0;

extern double t;
extern float dt;
extern float controlRoll, controlPitch, controlYaw, controlThrottle, controlMode;
extern Vector gyro, acc;
extern Vector rates;
extern Quaternion attitude;
extern Vector attitudeEuler, attitudeBodyUp;
extern bool landed;
extern float motors[4];

// Explicit prototypes are required because Arduino's sketch preprocessor does
// not reliably generate declarations for functions with default arguments.
bool preArmCheck(bool requireRC, const char **reason = nullptr);
bool requestArm(const char *source, bool requireRC = true);
bool motorsActive();
bool autoFlightActive();
bool wifiTransportHealthy();
void limitHorizontalSpeedCommand(float &x, float &y);
void setupCamera();
void setupCameraStream();

// The profiler samples one loop in sixteen so its own timer reads do not
// become a meaningful control-loop load. Stage names describe ownership, not
// independent tasks: every stage still runs serially in loopTask.
enum PerformanceStage {
	PERF_IMU,
	PERF_INPUTS,
	PERF_ESTIMATE,
	PERF_CONTROL,
	PERF_CLI,
	PERF_MAVLINK,
	PERF_HOUSEKEEPING,
	PERF_STAGE_COUNT
};
void beginPerformanceCycle();
void markPerformanceStage(PerformanceStage stage);
void endPerformanceCycle();

void setup() {
	Serial.begin(115200);
	print("Initializing flix\n");
	setupPower();
	setupParameters();
	setupLED();
	setupMotors();
	setLED(true);
	// Allocate camera DMA and PSRAM frame buffers before Wi-Fi and the OTA HTTP
	// server consume runtime heap. The stream itself starts only after Wi-Fi.
	setupCamera();
#if WIFI_ENABLED
	setupWiFi();
	setupCameraStream();
#endif
	setupIMU();
	setupRC();
#if OPTICAL_FLOW_ENABLED
	setupOpticalFlow();
#endif
#if WIFI_ENABLED
	setupOtaBootValidation();
#endif
	setLED(false);
	print("Initializing complete\n");

	// Keep the Arduino loop above background networking tasks.
	TaskHandle_t tsk = xTaskGetHandle("loopTask");
	if (tsk != NULL) {
		vTaskPrioritySet(tsk, 3);
	}
}

void loop() {
	waitForControlLoopTick();
	beginPerformanceCycle();
	readIMU();
	markPerformanceStage(PERF_IMU);
	step();
	readRC();
#if OPTICAL_FLOW_ENABLED
	readOpticalFlow();
#endif
	markPerformanceStage(PERF_INPUTS);
	estimate();
	estimateHeight();
	estimateHorizontalVelocity();
	markPerformanceStage(PERF_ESTIMATE);
	control();
	sendMotors();
	markPerformanceStage(PERF_CONTROL);
	if (controlLoopEvery(3)) handleInput(); // 100 Hz serial service
	markPerformanceStage(PERF_CLI);
#if WIFI_ENABLED
	if (controlLoopEvery(2)) processMavlink(); // 150 Hz command/telemetry service
	if (controlLoopEvery(6)) updateOtaBootValidation(); // 50 Hz, pending boots only
#endif
	markPerformanceStage(PERF_MAVLINK);
	readVoltage();
	updateLED();
	logData();
	syncParameters();
	markPerformanceStage(PERF_HOUSEKEEPING);
	endPerformanceCycle();
}
