// Open32Drone XIAO ESP32-S3 Sense camera and bounded MJPEG stream.
// Camera work stays in the low-priority HTTP task and never enters loop().

#include "esp_camera.h"
#include "esp_http_server.h"
#include "esp_timer.h"
#include <WiFi.h>

// XIAO ESP32-S3 Sense camera expansion-board pin map.
#define CAMERA_PWDN_PIN  -1
#define CAMERA_RESET_PIN -1
#define CAMERA_XCLK_PIN  10
#define CAMERA_SIOD_PIN  40
#define CAMERA_SIOC_PIN  39
#define CAMERA_Y9_PIN    48
#define CAMERA_Y8_PIN    11
#define CAMERA_Y7_PIN    12
#define CAMERA_Y6_PIN    14
#define CAMERA_Y5_PIN    16
#define CAMERA_Y4_PIN    18
#define CAMERA_Y3_PIN    17
#define CAMERA_Y2_PIN    15
#define CAMERA_VSYNC_PIN 38
#define CAMERA_HREF_PIN  47
#define CAMERA_PCLK_PIN  13

constexpr uint32_t CAMERA_XCLK_HZ = 20000000;
constexpr framesize_t CAMERA_FRAME_SIZE = FRAMESIZE_QVGA;
constexpr int CAMERA_JPEG_QUALITY = 10;
constexpr int CAMERA_FRAME_BUFFERS = 2;
// Keep video below the point where the softAP TCP stream can starve small,
// safety-critical MAVLink UDP packets. Quality remains 10; only frame cadence
// is reduced.
constexpr int CAMERA_STREAM_FPS = 10;

// Motors use Arduino LEDC channels 1..4 on timer 0. The camera deliberately
// owns channel 0 on timer 1 so its 20 MHz XCLK cannot retune 10 kHz motor PWM.
constexpr ledc_channel_t CAMERA_LEDC_CHANNEL = LEDC_CHANNEL_0;
constexpr ledc_timer_t CAMERA_LEDC_TIMER = LEDC_TIMER_1;

static const char CAMERA_STREAM_TYPE[] =
	"multipart/x-mixed-replace;boundary=open32drone-frame";
static const char CAMERA_STREAM_BOUNDARY[] =
	"\r\n--open32drone-frame\r\n";

static httpd_handle_t cameraHttpd = NULL;
static bool cameraInitialized = false;
static bool cameraStreamActive = false;
static portMUX_TYPE cameraStateMux = portMUX_INITIALIZER_UNLOCKED;

#if WIFI_ENABLED
extern volatile bool otaUpdateActive;
#endif

static bool claimCameraStream() {
	bool claimed = false;
	portENTER_CRITICAL(&cameraStateMux);
	if (!cameraStreamActive) {
		cameraStreamActive = true;
		claimed = true;
	}
	portEXIT_CRITICAL(&cameraStateMux);
	return claimed;
}

static void releaseCameraStream() {
	portENTER_CRITICAL(&cameraStateMux);
	cameraStreamActive = false;
	portEXIT_CRITICAL(&cameraStateMux);
}

static esp_err_t cameraIndexHandler(httpd_req_t *req) {
	static const char page[] =
		"<!doctype html><html><head><meta name='viewport' content='width=device-width'>"
		"<title>Open32Drone camera</title></head>"
		"<body style='margin:0;background:#000;display:grid;place-items:center'>"
		"<img src='/stream' style='width:100%;max-width:640px;height:auto'>"
		"</body></html>";
	httpd_resp_set_type(req, "text/html");
	return httpd_resp_send(req, page, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t cameraUnavailable(httpd_req_t *req, const char *message) {
	httpd_resp_set_status(req, "503 Service Unavailable");
	httpd_resp_set_type(req, "text/plain");
	return httpd_resp_sendstr(req, message);
}

static esp_err_t cameraStreamHandler(httpd_req_t *req) {
	if (!cameraInitialized) {
		return cameraUnavailable(req, "camera unavailable");
	}
	if (!claimCameraStream()) {
		return cameraUnavailable(req, "camera stream already in use");
	}

	esp_err_t result = httpd_resp_set_type(req, CAMERA_STREAM_TYPE);
	if (result == ESP_OK) {
		httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
		httpd_resp_set_hdr(req, "Cache-Control", "no-store");
		httpd_resp_set_hdr(req, "X-Framerate", "10");
	}

	const int64_t framePeriodUs = 1000000LL / CAMERA_STREAM_FPS;
	while (result == ESP_OK) {
#if WIFI_ENABLED
		if (otaUpdateActive) break;
#endif
		int64_t frameStartUs = esp_timer_get_time();
		camera_fb_t *frame = esp_camera_fb_get();
		if (frame == NULL) {
			result = ESP_FAIL;
			break;
		}

		char header[96];
		int headerLength = snprintf(
			header, sizeof(header),
			"Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n",
			(unsigned)frame->len);
		result = httpd_resp_send_chunk(
			req, CAMERA_STREAM_BOUNDARY, strlen(CAMERA_STREAM_BOUNDARY));
		if (result == ESP_OK) {
			result = httpd_resp_send_chunk(req, header, headerLength);
		}
		if (result == ESP_OK) {
			result = httpd_resp_send_chunk(
				req, reinterpret_cast<const char *>(frame->buf), frame->len);
		}
		esp_camera_fb_return(frame);

		int64_t remainingUs = framePeriodUs - (esp_timer_get_time() - frameStartUs);
		if (result == ESP_OK && remainingUs >= 1000) {
			vTaskDelay(pdMS_TO_TICKS((remainingUs + 999) / 1000));
		}
	}

	releaseCameraStream();
	return result;
}

static bool startCameraHttpServer() {
	httpd_config_t config = HTTPD_DEFAULT_CONFIG();
	config.server_port = 80;
	config.ctrl_port = 32768; // OTA owns 32769.
	config.max_open_sockets = 3;
	config.max_uri_handlers = 2;
	config.stack_size = 8192;
	config.task_priority = 1;
	config.core_id = 0;
	// Release a dead stream promptly so the Android retry does not spend several
	// seconds receiving "stream already in use" after a Wi-Fi interruption.
	config.send_wait_timeout = 1;

	if (httpd_start(&cameraHttpd, &config) != ESP_OK) return false;

	httpd_uri_t indexUri = {};
	indexUri.uri = "/";
	indexUri.method = HTTP_GET;
	indexUri.handler = cameraIndexHandler;
	httpd_uri_t streamUri = {};
	streamUri.uri = "/stream";
	streamUri.method = HTTP_GET;
	streamUri.handler = cameraStreamHandler;

	if (httpd_register_uri_handler(cameraHttpd, &indexUri) != ESP_OK ||
		httpd_register_uri_handler(cameraHttpd, &streamUri) != ESP_OK) {
		httpd_stop(cameraHttpd);
		cameraHttpd = NULL;
		return false;
	}
	return true;
}

void setupCamera() {
	Serial.println("Setup Camera");
	if (!psramFound()) {
		Serial.println("Camera disabled: PSRAM unavailable");
		return;
	}

	camera_config_t config = {};
	config.ledc_channel = CAMERA_LEDC_CHANNEL;
	config.ledc_timer = CAMERA_LEDC_TIMER;
	config.pin_d0 = CAMERA_Y2_PIN;
	config.pin_d1 = CAMERA_Y3_PIN;
	config.pin_d2 = CAMERA_Y4_PIN;
	config.pin_d3 = CAMERA_Y5_PIN;
	config.pin_d4 = CAMERA_Y6_PIN;
	config.pin_d5 = CAMERA_Y7_PIN;
	config.pin_d6 = CAMERA_Y8_PIN;
	config.pin_d7 = CAMERA_Y9_PIN;
	config.pin_xclk = CAMERA_XCLK_PIN;
	config.pin_pclk = CAMERA_PCLK_PIN;
	config.pin_vsync = CAMERA_VSYNC_PIN;
	config.pin_href = CAMERA_HREF_PIN;
	config.pin_sccb_sda = CAMERA_SIOD_PIN;
	config.pin_sccb_scl = CAMERA_SIOC_PIN;
	config.pin_pwdn = CAMERA_PWDN_PIN;
	config.pin_reset = CAMERA_RESET_PIN;
	config.xclk_freq_hz = CAMERA_XCLK_HZ;
	config.frame_size = CAMERA_FRAME_SIZE;
	config.pixel_format = PIXFORMAT_JPEG;
	config.grab_mode = CAMERA_GRAB_LATEST;
	config.fb_location = CAMERA_FB_IN_PSRAM;
	config.jpeg_quality = CAMERA_JPEG_QUALITY;
	config.fb_count = CAMERA_FRAME_BUFFERS;

	esp_err_t error = esp_camera_init(&config);
	if (error != ESP_OK) {
		Serial.printf("Camera disabled: init error 0x%x\n", error);
		return;
	}

	sensor_t *sensor = esp_camera_sensor_get();
	if (sensor != NULL) {
		sensor->set_brightness(sensor, 0);
		sensor->set_contrast(sensor, 0);
		sensor->set_saturation(sensor, 1);
		sensor->set_whitebal(sensor, 1);
		sensor->set_awb_gain(sensor, 1);
		sensor->set_exposure_ctrl(sensor, 1);
		sensor->set_gain_ctrl(sensor, 1);
		sensor->set_bpc(sensor, 1);
		sensor->set_wpc(sensor, 1);
		sensor->set_raw_gma(sensor, 1);
		sensor->set_lenc(sensor, 1);
		// Relative to dev (H=1, V=0), toggle both axes: H=0, V=1.
		sensor->set_hmirror(sensor, 0);
		sensor->set_vflip(sensor, 1);
		Serial.printf("Camera sensor PID: 0x%04x\n", sensor->id.PID);
	}

	cameraInitialized = true;
}

void setupCameraStream() {
	if (!cameraInitialized) {
		Serial.println("Camera stream disabled: camera initialization failed");
		return;
	}
	if (!wifiTransportHealthy()) {
		Serial.println("Camera ready; stream disabled because Wi-Fi transport is unavailable");
		return;
	}
	if (!startCameraHttpServer()) {
		Serial.println("Camera ready; HTTP stream failed to start");
		return;
	}

	IPAddress streamAddress = WiFi.getMode() == WIFI_MODE_AP
		? WiFi.softAPIP() : WiFi.localIP();
	String streamIp = streamAddress.toString();
	Serial.printf("Camera stream: http://%s/stream\n", streamIp.c_str());
	Serial.printf("Camera profile: QVGA quality %d, %d FPS, H=0 V=1\n",
		CAMERA_JPEG_QUALITY, CAMERA_STREAM_FPS);
}
