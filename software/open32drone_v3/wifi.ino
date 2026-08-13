// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Wi-Fi support with camera streaming + AP/STA modes

#if WIFI_ENABLED

#include <WiFi.h>
#include <WiFiAP.h>
#include <WiFiUdp.h>
#include <Preferences.h>
#include "esp_camera.h"
#include "esp_http_server.h"
#include "esp_partition.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "wifi_stream.h"

extern Preferences storage; // from parameters.ino

// WiFi modes: 0=disabled, 1=AP, 2=STA(client)
const int W_DISABLED = 0, W_AP = 1, W_STA = 2;
int wifiMode = W_AP; // default AP: camera streaming target
int udpLocalPort = 14550;
int udpRemotePort = 14550;
IPAddress udpRemoteIP = "255.255.255.255";

// WiFi TX power. The pre-camera firmware used 8.5 dBm for RF stability on
// this board; higher values (11/13/15/19.5 dBm) improve range but increase
// peak current - if the 3.3V rail sags, the softAP can fail to broadcast
// intermittently. Start low and increase until the AP becomes unreliable.
#define WIFI_TX_POWER WIFI_POWER_11dBm

// Target stream frame rate (frames per second)
#define STREAM_TARGET_FPS 30

WiFiUDP udp;

// HTTP server handle
httpd_handle_t stream_httpd = NULL;

// Stream control variables
volatile bool isStreaming = false;
volatile uint32_t clientCount = 0;

// Frame-rate filter: ra_filter_t type + ra_filter_run() live in wifi_stream.h
// (a header, so the Arduino prototype generator cannot hoist the function
// above its typedef). Only the instance stays here.
static ra_filter_t ra_filter;

// Forward declarations
static esp_err_t stream_handler(httpd_req_t *req);
static esp_err_t index_handler(httpd_req_t *req);
void streamServerTask(void* parameter);

#define PART_BOUNDARY "123456789000000000000987654321"
static const char *_STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char *_STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char *_STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\nX-Timestamp: %d.%06d\r\n\r\n";

static esp_err_t stream_handler(httpd_req_t *req) {
	camera_fb_t *fb = NULL;
	struct timeval _timestamp;
	esp_err_t res = ESP_OK;
	size_t _jpg_buf_len = 0;
	uint8_t *_jpg_buf = NULL;
	char part_buf[128];

	// Per-connection frame pacing state (not shared across clients)
	int64_t last_frame = 0;
	const int64_t frame_interval_us = 1000000 / STREAM_TARGET_FPS;

	res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
	if (res != ESP_OK) {
		return res;
	}

	httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
	httpd_resp_set_hdr(req, "X-Framerate", "30");

#if CONFIG_LED_ILLUMINATOR_ENABLED
	isStreaming = true;
	enable_led(true);
#endif

	while (true) {
		int64_t frame_start = esp_timer_get_time();

		fb = esp_camera_fb_get();
		if (!fb) {
			log_e("Camera capture failed");
			res = ESP_FAIL;
		} else {
			_timestamp.tv_sec = fb->timestamp.tv_sec;
			_timestamp.tv_usec = fb->timestamp.tv_usec;
			if (fb->format != PIXFORMAT_JPEG) {
				bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
				esp_camera_fb_return(fb);
				fb = NULL;
				if (!jpeg_converted) {
					log_e("JPEG compression failed");
					res = ESP_FAIL;
				}
			} else {
				_jpg_buf_len = fb->len;
				_jpg_buf = fb->buf;
			}
		}
		if (res == ESP_OK) {
			res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
		}
		if (res == ESP_OK) {
			size_t hlen = snprintf((char *)part_buf, 128, _STREAM_PART, _jpg_buf_len, _timestamp.tv_sec, _timestamp.tv_usec);
			res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
		}
		if (res == ESP_OK) {
			res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
		}
		if (fb) {
			esp_camera_fb_return(fb);
			fb = NULL;
			_jpg_buf = NULL;
		} else if (_jpg_buf) {
			free(_jpg_buf);
			_jpg_buf = NULL;
		}
		if (res != ESP_OK) {
			log_e("Send frame failed");
			break;
		}

		// Pace frames to STREAM_TARGET_FPS: keeps bandwidth bounded and
		// avoids saturating the Wi-Fi link at close range.
		int64_t elapsed = esp_timer_get_time() - frame_start;
		int64_t wait_us = frame_interval_us - elapsed;
		if (wait_us > 1000) {
			vTaskDelay(wait_us / 1000 / portTICK_PERIOD_MS);
		}

		int64_t frame_time = (esp_timer_get_time() - last_frame) / 1000;
		last_frame = esp_timer_get_time();
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO
		uint32_t avg_frame_time = ra_filter_run(&ra_filter, frame_time);
#endif
		log_i(
			"MJPG: %uB %ums (%.1ffps), AVG: %ums (%.1ffps)", (uint32_t)(_jpg_buf_len), (uint32_t)frame_time, 1000.0 / (uint32_t)frame_time, avg_frame_time,
			1000.0 / avg_frame_time
		);
	}

#if CONFIG_LED_ILLUMINATOR_ENABLED
	isStreaming = false;
	enable_led(false);
#endif

	return res;
}

// HTTP handler for HTML page
static const char* html_page =
"<html>"
"<head>"
"<title>Open32Drone Camera Stream</title>"
"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
"<style>"
"body { margin: 0; background: #000; }"
"#stream-container { width: 100%; max-width: 320px; margin: 0 auto; }"
"img { width: 100%; image-rendering: pixelated; }"
".info { color: white; text-align: center; font-family: Arial; padding: 5px; font-size: 12px; }"
"</style>"
"</head>"
"<body>"
"<div id='stream-container'>"
"<div class='info'>Open32Drone Camera Stream - QVGA 320x240</div>"
"<img src='/stream' />"
"<div class='info'>Optimized for flight performance</div>"
"</div>"
"</body>"
"</html>";

static esp_err_t index_handler(httpd_req_t *req) {
	httpd_resp_set_type(req, "text/html");
	return httpd_resp_send(req, html_page, strlen(html_page));
}

// Start streaming server
void startStreamServer() {
	httpd_config_t config = HTTPD_DEFAULT_CONFIG();
	config.server_port = 80;
	config.ctrl_port = 80;
	config.max_open_sockets = 3; // Allow ROS2 client + browser debugging
	config.stack_size = 8192;    // Larger stack: stream handler does JPEG work
	config.task_priority = 1;    // Low priority task, flight control wins

	ra_filter.size = 8;

	if (httpd_start(&stream_httpd, &config) == ESP_OK) {
		httpd_uri_t index_uri = {
			.uri       = "/",
			.method    = HTTP_GET,
			.handler   = index_handler,
			.user_ctx  = NULL
		};

		httpd_uri_t stream_uri = {
			.uri       = "/stream",
			.method    = HTTP_GET,
			.handler   = stream_handler,
			.user_ctx  = NULL
		};

		httpd_register_uri_handler(stream_httpd, &index_uri);
		httpd_register_uri_handler(stream_httpd, &stream_uri);

		Serial.println("HTTP streaming server started on port 80 (low priority)");
	} else {
		Serial.println("Error starting HTTP streaming server");
	}
}

// Separate task for stream server
void streamServerTask(void* parameter) {
	startStreamServer();

	// Keep task alive
	while (true) {
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
}

void setupWiFi() {
	print("Setup Wi-Fi\n");
	// NVS space is shared by the WiFi phy calibration data and the parameter
	// Preferences - if it is full, esp_wifi_start fails and the AP never
	// broadcasts. Print the partition size for diagnostics.
	const esp_partition_t *nvs_part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS, NULL);
	if (nvs_part != NULL) {
		print("NVS partition size: %u KB\n", nvs_part->size / 1024);
	}

	if (wifiMode == W_AP) {
		String ssid = storage.getString("WIFI_AP_SSID", "open32drone");
		String pass = storage.getString("WIFI_AP_PASS", "12345678");
		// Start the AP with retries: transient RF issues often clear after a short delay.
		bool ap_ok = false;
		for (int attempt = 0; attempt < 3 && !ap_ok; attempt++) {
			if (attempt > 0) {
				delay(300);
			}
			ap_ok = WiFi.softAP(ssid.c_str(), pass.c_str());
		}
		print("softAP started: %s (IP %s)\n", ap_ok ? "yes" : "NO", WiFi.softAPIP().toString().c_str());
		if (ap_ok) {
			WiFi.setSleep(false); // Disable modem sleep: lower latency for video
			bool tx_ok = WiFi.setTxPower(WIFI_TX_POWER);
			print("TX power: %s\n", tx_ok ? "set" : "failed, using default");
		}
	} else if (wifiMode == W_STA) {
		String ssid = storage.getString("WIFI_STA_SSID", "");
		String pass = storage.getString("WIFI_STA_PASS", "");
		WiFi.begin(ssid.c_str(), pass.c_str());
		WiFi.setSleep(false);
		print("WiFi STA connecting to: %s\n", ssid.c_str());
	} else {
		print("WiFi disabled\n");
		return; // W_DISABLED: skip UDP + streaming server
	}

	udp.begin(udpLocalPort);

	// Start streaming server in separate task with low priority
#if CAMERA_ENABLED && CAMERA_STREAMING_ENABLED
	if (cameraInitialized) {
		xTaskCreatePinnedToCore(
			streamServerTask,
			"stream_task",
			4096,  // Small stack
			NULL,
			2,     // Priority 2; flight control (loopTask) runs at priority 3
			NULL,
			0      // Core 0
		);

		Serial.print("Camera Stream URL: http://");
		if (wifiMode == W_AP) {
			Serial.print(WiFi.softAPIP());
		} else {
			Serial.print(WiFi.localIP());
		}
		Serial.println("/");
		Serial.println("Stream optimized: QVGA 320x240, low priority");
	} else {
		Serial.println("Camera stream disabled: camera init failed");
	}
#endif
}

void sendWiFi(const uint8_t *buf, int len) {
	if (WiFi.softAPgetStationNum() == 0 && !WiFi.isConnected()) return;
	udp.beginPacket(udpRemoteIP, udpRemotePort);
	udp.write(buf, len);
	udp.endPacket();
}

int receiveWiFi(uint8_t *buf, int len) {
	if (WiFi.softAPgetStationNum() == 0 && !WiFi.isConnected()) return 0;
	udp.parsePacket();
	if (udp.remoteIP()) udpRemoteIP = udp.remoteIP(); // auto-learn remote IP
	return udp.read(buf, len);
}

void printWiFiInfo() {
	if (WiFi.getMode() == WIFI_MODE_AP) {
		print("Mode: Access Point (AP)\n");
		print("MAC: %s\n", WiFi.softAPmacAddress().c_str());
		print("SSID: %s\n", WiFi.softAPSSID().c_str());
		print("Password: [redacted]\n");
		print("Clients: %d\n", WiFi.softAPgetStationNum());
		print("IP: %s\n", WiFi.softAPIP().toString().c_str());
	} else if (WiFi.getMode() == WIFI_MODE_STA) {
		print("Mode: Client (STA)\n");
		print("Connected: %d\n", WiFi.isConnected());
		print("MAC: %s\n", WiFi.macAddress().c_str());
		print("SSID: %s\n", WiFi.SSID().c_str());
		print("IP: %s\n", WiFi.localIP().toString().c_str());
		print("RSSI: %d dBm\n", WiFi.RSSI());
	} else {
		print("Mode: Disabled\n");
		return;
	}
	print("Channel: %d\n", WiFi.channel());
	print("Remote IP: %s\n", udpRemoteIP.toString().c_str());
	print("MAVLink connected: %d\n", mavlinkConnected);
}

// Configure WiFi credentials, stored in Preferences; reboot to apply
void configWiFi(bool ap, const char *ssid, const char *password) {
	if (ap) {
		storage.putString("WIFI_AP_SSID", ssid);
		storage.putString("WIFI_AP_PASS", password);
	} else {
		storage.putString("WIFI_STA_SSID", ssid);
		storage.putString("WIFI_STA_PASS", password);
	}
	print("✓ Reboot to apply new settings\n");
}

#endif
