// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Main firmware file

#include "vector.h"
#include "quaternion.h"
#include "util.h"
#include "esp_camera.h"
#include "altitude_hold.h" // BMP280 驱动（保留诊断备用；定高已用 TOF）

#define WIFI_ENABLED 1
// osrbot start
#define OPTICAL_FLOW_ENABLED 1
// Position estimation variables
Vector position; // estimated position in world frame, m
Vector velocity; // estimated velocity in world frame, m/s
float height; // estimated height above ground, m
bool positionValid; // is position estimate valid

bool opticalFlowHealthy = false;
float opticalFlowVelocityX = 0;
float opticalFlowVelocityY = 0;
float opticalFlowHeight = 0;
// osrbot end

// ============================
// Camera Configuration
// Camera Configuration (XIAO ESP32S3 Sense + OV3660)
// ============================
#define CAMERA_ENABLED 1
#define CAMERA_STREAMING_ENABLED 1

// Camera pin configuration for ESP32-S3 (reference, disabled)
// #define PWDN_GPIO_NUM  -1
// #define RESET_GPIO_NUM -1
// #define XCLK_GPIO_NUM  15
// #define SIOD_GPIO_NUM  4
// #define SIOC_GPIO_NUM  5

// #define Y2_GPIO_NUM 11
// #define Y3_GPIO_NUM 9
// #define Y4_GPIO_NUM 8
// #define Y5_GPIO_NUM 10
// #define Y6_GPIO_NUM 12
// #define Y7_GPIO_NUM 18
// #define Y8_GPIO_NUM 17
// #define Y9_GPIO_NUM 16

// #define VSYNC_GPIO_NUM 6
// #define HREF_GPIO_NUM  7
// #define PCLK_GPIO_NUM  13

// Camera pin configuration for XIAO32S3
#define PWDN_GPIO_NUM  -1
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM  10
#define SIOD_GPIO_NUM  40
#define SIOC_GPIO_NUM  39

#define Y9_GPIO_NUM    48
#define Y8_GPIO_NUM    11
#define Y7_GPIO_NUM    12
#define Y6_GPIO_NUM    14
#define Y5_GPIO_NUM    16
#define Y4_GPIO_NUM    18
#define Y3_GPIO_NUM    17
#define Y2_GPIO_NUM    15

#define VSYNC_GPIO_NUM 38
#define HREF_GPIO_NUM  47
#define PCLK_GPIO_NUM  13

// Camera parameters: 10 MHz xclk matches flix_cam_x32v2 (avoids DMA
// frame-buffer overflows); QVGA 320x240 = 1:1 on the 320px stream page.
#define CAMERA_XCLK_FREQ   10000000 // 10 MHz: lower DMA load, no FB-OVF
#define CAMERA_FRAME_SIZE  FRAMESIZE_QVGA // 320x240: matches the 320px stream page
#define CAMERA_JPEG_QUALITY 30       // Same as flix_cam_x32v2
#define CAMERA_FB_COUNT    1         // Single buffer, lowest latency

// Camera variables
bool cameraInitialized = false;
TaskHandle_t streamServerTaskHandle = NULL;

extern float t, dt;
extern float controlRoll, controlPitch, controlYaw, controlThrottle, controlMode;
extern Vector gyro, acc;
extern Vector rates;
extern Quaternion attitude;
extern bool landed;
extern float motors[4];

void setup() {
	Serial.begin(115200);
	print("Initializing flix\n");
	setupPower();
	setupParameters();
	setupLED();
	setupMotors();
	setLED(true);
#if CAMERA_ENABLED
	cameraInitialized = setupCamera();
#endif
#if WIFI_ENABLED
	setupWiFi();
#endif
	setupIMU();
	setupBaro(); // BMP280 驱动初始化（诊断备用，定高用 TOF）
	setupRC();
#if OPTICAL_FLOW_ENABLED
	setupOpticalFlow();
#endif
	setLED(false);
	print("Initializing complete\n");

	// Prioritize flight control over camera streaming
	TaskHandle_t tsk = xTaskGetHandle("loopTask");
	if (tsk != NULL) {
		vTaskPrioritySet(tsk, 3);
	}
	tsk = xTaskGetHandle("stream_task");
	if (tsk != NULL) {
		vTaskPrioritySet(tsk, 2);
	}
}

void loop() {
	readIMU();
	step();
	readRC();
#if OPTICAL_FLOW_ENABLED
	readOpticalFlow();
#endif
	updateBaro(); // BMP280 10Hz 更新（诊断备用，定高用 TOF）
	estimate();
	estimateHeight(); // 纯 ToF 高度与相邻有效样本垂直速度
	estimateHorizontalVelocity(); // 光流水平速度估计（velocity.x/y，定点数据基础）
	control();
	sendMotors();
	handleInput();
#if WIFI_ENABLED
	processMavlink();
#endif
	readVoltage();
	logData();
	syncParameters();
}

// ============================
// Camera Initialization
// Camera Initialization
// ============================
#if CAMERA_ENABLED
bool setupCamera() {
	camera_config_t config;

	// Camera configuration for OV3660 (XIAO32S3 Sense) - optimized for low resource usage
	config.ledc_channel = LEDC_CHANNEL_0;
	config.ledc_timer = LEDC_TIMER_0;
	config.pin_d0 = Y2_GPIO_NUM;
	config.pin_d1 = Y3_GPIO_NUM;
	config.pin_d2 = Y4_GPIO_NUM;
	config.pin_d3 = Y5_GPIO_NUM;
	config.pin_d4 = Y6_GPIO_NUM;
	config.pin_d5 = Y7_GPIO_NUM;
	config.pin_d6 = Y8_GPIO_NUM;
	config.pin_d7 = Y9_GPIO_NUM;
	config.pin_xclk = XCLK_GPIO_NUM;
	config.pin_pclk = PCLK_GPIO_NUM;
	config.pin_vsync = VSYNC_GPIO_NUM;
	config.pin_href = HREF_GPIO_NUM;
	config.pin_sccb_sda = SIOD_GPIO_NUM;
	config.pin_sccb_scl = SIOC_GPIO_NUM;
	config.pin_pwdn = PWDN_GPIO_NUM;
	config.pin_reset = RESET_GPIO_NUM;
	config.xclk_freq_hz = CAMERA_XCLK_FREQ; // 10 MHz: stable DMA, same as flix_cam_x32v2
	config.frame_size = CAMERA_FRAME_SIZE; // 320x240: 1:1 on the 320px stream page
	config.pixel_format = PIXFORMAT_JPEG;
	config.grab_mode = CAMERA_GRAB_LATEST;
	config.fb_location = CAMERA_FB_IN_PSRAM; // requires PSRAM enabled in board config (XIAO ESP32S3: OPI PSRAM)
	config.jpeg_quality = CAMERA_JPEG_QUALITY; // Low quality for performance
	config.fb_count = CAMERA_FB_COUNT; // Only one buffer to save memory

	esp_err_t err = esp_camera_init(&config);
	if (err != ESP_OK) {
		Serial.printf("Camera init failed with error 0x%x\n", err);
		Serial.printf("PSRAM: %s, size: %u bytes\n", psramFound() ? "found" : "NOT FOUND", ESP.getPsramSize());
		Serial.printf("Heap: %u bytes free\n", ESP.getFreeHeap());
		return false;
	}

	// Optimized sensor settings for OV3660
	sensor_t *s = esp_camera_sensor_get();
	if (s != NULL) {
		// Basic image settings
		s->set_brightness(s, 0);     // Default brightness
		s->set_contrast(s, 0);       // Default contrast
		s->set_saturation(s, 1);     // Slightly reduced saturation
		s->set_special_effect(s, 0); // No effect

		// Color and white balance
		s->set_whitebal(s, 1);       // Auto white balance
		s->set_awb_gain(s, 1);       // Auto WB gain
		s->set_wb_mode(s, 0);        // Auto mode

		// Exposure and gain
		s->set_exposure_ctrl(s, 1);  // Auto exposure
		s->set_aec2(s, 0);           // No night mode
		s->set_ae_level(s, 1);       // Medium exposure level
		s->set_gain_ctrl(s, 1);      // Auto gain
		s->set_agc_gain(s, 1);       // Medium gain

		// Image correction
		s->set_bpc(s, 1);            // Bad pixel correction
		s->set_wpc(s, 1);            // White pixel correction
		s->set_raw_gma(s, 1);        // Gamma correction
		s->set_lenc(s, 1);           // Lens correction
		s->set_hmirror(s, 0);        // No horizontal mirror
		// s->set_vflip(s, 1);          // Vertical flip ON (备用：图像上下翻转)
		s->set_vflip(s, 0);          // Vertical flip OFF

		s->set_colorbar(s, 0);       // No color lines
	}

	Serial.println("OV3660 camera initialized with optimized low-res settings");
	return true;
}
#endif
