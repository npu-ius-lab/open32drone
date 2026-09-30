// Safe A/B firmware update support for the Open32Drone ESP32-S3 target.

#if WIFI_ENABLED

#include "esp_app_format.h"
#include "esp_http_server.h"
#include "esp_ota_ops.h"
#include "mbedtls/sha256.h"

extern bool parameterStorageHealthy;
extern bool imuHealthy, gyroCalibrated;
extern uint32_t imuLastSampleMs;
extern float loopRate;
extern bool udpBound;
extern bool offboardActive;
extern float thrustTarget;
extern bool tofPacketHealthy;
extern uint32_t tofPacketTimestamp;

bool motorsActive();
bool autoFlightActive();
bool wifiTransportHealthy();

volatile bool otaUpdateActive = false;

static httpd_handle_t otaHttpd = NULL;
static bool otaBootPendingVerification = false;
static uint32_t otaBootVerificationStartMs = 0;
static uint32_t otaBootHealthySinceMs = 0;

static const uint16_t OTA_HTTP_PORT = 8080;
static const uint32_t OTA_BOOT_HEALTH_TIMEOUT_MS = 30000;
static const uint32_t OTA_BOOT_HEALTH_STABLE_MS = 3000;

// Arduino-ESP32 declares verifyRollbackLater() weak, so defining that same C
// name in a sketch inherits the weak attribute and loses to the core archive.
// Export a different C++ function under the exact strong linker symbol instead.
// This defers image acceptance until flight sensors, storage and Wi-Fi are live.
bool open32VerifyRollbackLater() asm("verifyRollbackLater");
bool open32VerifyRollbackLater() {
	return true;
}

static const char *otaPartitionName(const esp_partition_t *partition) {
	return partition == NULL ? "none" : partition->label;
}

static const char *otaSafetyFailure() {
	if (otaUpdateActive) return "update already in progress";
	if (otaBootPendingVerification) return "current firmware is pending boot validation";
	if (armed) return "aircraft is armed";
	if (!landed) return "aircraft is not landed";
	if (autoFlightActive()) return "automatic flight is active";
	if (offboardActive) return "offboard control is active";
	if (motorsActive() || (isfinite(thrustTarget) && thrustTarget > 0.01f)) return "motors are active";
	if (esp_ota_get_app_partition_count() < 2) return "A/B OTA partition table is not installed";
	if (esp_ota_get_next_update_partition(NULL) == NULL) return "inactive OTA partition unavailable";
	return NULL;
}

static void sendOtaJson(httpd_req_t *req, const char *status, const char *body) {
	httpd_resp_set_status(req, status);
	httpd_resp_set_type(req, "application/json");
	httpd_resp_set_hdr(req, "Cache-Control", "no-store");
	httpd_resp_sendstr(req, body);
}

static esp_err_t otaStatusHandler(httpd_req_t *req) {
	const esp_partition_t *running = esp_ota_get_running_partition();
	const esp_partition_t *next = esp_ota_get_next_update_partition(NULL);
	const char *failure = otaSafetyFailure();
	char response[512];
	snprintf(response, sizeof(response),
		"{\"ready\":%s,\"reason\":\"%s\",\"auth_required\":false,\"active_slot\":\"%s\","
		"\"next_slot\":\"%s\",\"partition_count\":%d,\"pending_verify\":%s,"
		"\"update_active\":%s,\"max_image_bytes\":%lu}",
		failure == NULL ? "true" : "false", failure == NULL ? "none" : failure,
		otaPartitionName(running), otaPartitionName(next), esp_ota_get_app_partition_count(),
		otaBootPendingVerification ? "true" : "false", otaUpdateActive ? "true" : "false",
		(unsigned long)(next == NULL ? 0 : next->size));
	sendOtaJson(req, "200 OK", response);
	return ESP_OK;
}

static int hexDigit(char value) {
	if (value >= '0' && value <= '9') return value - '0';
	if (value >= 'a' && value <= 'f') return value - 'a' + 10;
	if (value >= 'A' && value <= 'F') return value - 'A' + 10;
	return -1;
}

static bool readExpectedSha256(httpd_req_t *req, uint8_t expected[32]) {
	size_t length = httpd_req_get_hdr_value_len(req, "X-Firmware-SHA256");
	if (length != 64) return false;
	char text[65];
	if (httpd_req_get_hdr_value_str(req, "X-Firmware-SHA256", text, sizeof(text)) != ESP_OK) return false;
	for (size_t i = 0; i < 32; i++) {
		int high = hexDigit(text[i * 2]);
		int low = hexDigit(text[i * 2 + 1]);
		if (high < 0 || low < 0) return false;
		expected[i] = (uint8_t)((high << 4) | low);
	}
	return true;
}

static esp_err_t otaUpdateHandler(httpd_req_t *req) {
	const char *failure = otaSafetyFailure();
	if (failure != NULL) {
		char response[160];
		snprintf(response, sizeof(response), "{\"ok\":false,\"error\":\"%s\"}", failure);
		sendOtaJson(req, "409 Conflict", response);
		return ESP_OK;
	}
	uint8_t expectedSha256[32];
	if (!readExpectedSha256(req, expectedSha256)) {
		sendOtaJson(req, "400 Bad Request", "{\"ok\":false,\"error\":\"invalid or missing SHA-256 header\"}");
		return ESP_OK;
	}

	const esp_partition_t *updatePartition = esp_ota_get_next_update_partition(NULL);
	if (req->content_len < (int)sizeof(esp_image_header_t) || updatePartition == NULL ||
		req->content_len > (int)updatePartition->size) {
		sendOtaJson(req, "413 Payload Too Large", "{\"ok\":false,\"error\":\"firmware image size is invalid\"}");
		return ESP_OK;
	}

	otaUpdateActive = true;
	vTaskDelay(pdMS_TO_TICKS(20)); // let the control loop observe the OTA exclusion flag

	uint8_t *buffer = (uint8_t *)malloc(4096);
	if (buffer == NULL) {
		otaUpdateActive = false;
		sendOtaJson(req, "500 Internal Server Error", "{\"ok\":false,\"error\":\"insufficient memory\"}");
		return ESP_OK;
	}

	esp_ota_handle_t updateHandle = 0;
	bool updateBegun = false;
	mbedtls_sha256_context shaContext;
	mbedtls_sha256_init(&shaContext);
	bool hashStarted = mbedtls_sha256_starts(&shaContext, 0) == 0;
	size_t receivedTotal = 0;
	const char *error = NULL;
	int timeoutCount = 0;

	while (receivedTotal < (size_t)req->content_len && error == NULL) {
		if (armed || !landed || autoFlightActive() || offboardActive || motorsActive()) {
			error = "aircraft state changed during update";
			break;
		}
		size_t remaining = (size_t)req->content_len - receivedTotal;
		int received = httpd_req_recv(req, (char *)buffer, min(remaining, (size_t)4096));
		if (received == HTTPD_SOCK_ERR_TIMEOUT && timeoutCount++ < 3) continue;
		if (received <= 0) {
			error = "firmware upload interrupted";
			break;
		}
		timeoutCount = 0;

		if (receivedTotal == 0) {
			if (buffer[0] != ESP_IMAGE_HEADER_MAGIC) {
				error = "invalid ESP firmware image";
				break;
			}
			if (esp_ota_begin(updatePartition, req->content_len, &updateHandle) != ESP_OK) {
				error = "unable to erase inactive OTA slot";
				break;
			}
			updateBegun = true;
		}

		if (!hashStarted || mbedtls_sha256_update(&shaContext, buffer, received) != 0) {
			error = "SHA-256 calculation failed";
			break;
		}
		if (esp_ota_write(updateHandle, buffer, received) != ESP_OK) {
			error = "flash write failed";
			break;
		}
		receivedTotal += received;
	}

	uint8_t actualSha256[32];
	if (error == NULL && mbedtls_sha256_finish(&shaContext, actualSha256) != 0) {
		error = "SHA-256 finalization failed";
	}
	mbedtls_sha256_free(&shaContext);

	if (error == NULL && memcmp(actualSha256, expectedSha256, sizeof(actualSha256)) != 0) {
		error = "firmware SHA-256 mismatch";
	}
	if (error == NULL && esp_ota_end(updateHandle) != ESP_OK) {
		error = "firmware image validation failed";
		updateBegun = false; // esp_ota_end already invalidated the handle
	}
	if (error == NULL) updateBegun = false;
	esp_app_desc_t newAppDescription;
	if (error == NULL &&
		esp_ota_get_partition_description(updatePartition, &newAppDescription) != ESP_OK) {
		error = "firmware application metadata is invalid";
	}
	if (error == NULL && esp_ota_set_boot_partition(updatePartition) != ESP_OK) {
		error = "unable to select the new OTA slot";
	}

	if (error != NULL) {
		if (updateBegun) esp_ota_abort(updateHandle);
		free(buffer);
		otaUpdateActive = false;
		char response[180];
		snprintf(response, sizeof(response), "{\"ok\":false,\"error\":\"%s\"}", error);
		sendOtaJson(req, "400 Bad Request", response);
		return ESP_OK;
	}

	free(buffer);
	char response[192];
	snprintf(response, sizeof(response),
		"{\"ok\":true,\"bytes\":%u,\"next_slot\":\"%s\",\"rebooting\":true}",
		(unsigned)receivedTotal, otaPartitionName(updatePartition));
	sendOtaJson(req, "200 OK", response);
	Serial.printf("OTA image accepted: %u bytes -> %s; rebooting\n", (unsigned)receivedTotal, updatePartition->label);
	delay(300);
	ESP.restart();
	return ESP_OK;
}

void setupOtaHttpServer() {
	if (esp_ota_get_app_partition_count() < 2) {
		Serial.println("OTA disabled: install the A/B partition table once over USB");
		return;
	}

	httpd_config_t config = HTTPD_DEFAULT_CONFIG();
	config.server_port = OTA_HTTP_PORT;
	config.ctrl_port = 32769;
	config.max_open_sockets = 2;
	config.stack_size = 8192;
	config.task_priority = 1;
	config.recv_wait_timeout = 10;
	config.send_wait_timeout = 10;

	if (httpd_start(&otaHttpd, &config) != ESP_OK) {
		Serial.println("OTA HTTP server failed to start");
		return;
	}

	httpd_uri_t statusUri = {};
	statusUri.uri = "/api/ota/status";
	statusUri.method = HTTP_GET;
	statusUri.handler = otaStatusHandler;
	httpd_uri_t updateUri = {};
	updateUri.uri = "/api/ota/update";
	updateUri.method = HTTP_POST;
	updateUri.handler = otaUpdateHandler;
	httpd_register_uri_handler(otaHttpd, &statusUri);
	httpd_register_uri_handler(otaHttpd, &updateUri);
	Serial.printf("Safe A/B OTA server: http://device-ip:%u/api/ota/status\n", OTA_HTTP_PORT);
}

void setupOtaBootValidation() {
	const esp_partition_t *running = esp_ota_get_running_partition();
	esp_ota_img_states_t state;
	otaBootPendingVerification = running != NULL &&
		esp_ota_get_state_partition(running, &state) == ESP_OK &&
		state == ESP_OTA_IMG_PENDING_VERIFY;
	otaBootVerificationStartMs = millis();
	otaBootHealthySinceMs = 0;
	if (otaBootPendingVerification) {
		Serial.printf("OTA boot pending health validation on %s\n", running->label);
	}
}

void updateOtaBootValidation() {
	if (!otaBootPendingVerification) return;
	uint32_t now = millis();
	bool coreHealthy = parameterStorageHealthy && imuHealthy && gyroCalibrated &&
		imuLastSampleMs != 0 && now - imuLastSampleMs <= 50 &&
		attitude.valid() && rates.valid() && loopRate >= 200.0f &&
		tofPacketHealthy && tofPacketTimestamp != 0 && now - tofPacketTimestamp <= 250 &&
		udpBound && wifiTransportHealthy() && !armed && landed;

	if (coreHealthy) {
		if (otaBootHealthySinceMs == 0) otaBootHealthySinceMs = now;
		if (now - otaBootHealthySinceMs >= OTA_BOOT_HEALTH_STABLE_MS) {
			esp_err_t result = esp_ota_mark_app_valid_cancel_rollback();
			Serial.printf("OTA boot validation: %s\n", result == ESP_OK ? "PASSED" : esp_err_to_name(result));
			otaBootPendingVerification = false;
		}
	} else {
		otaBootHealthySinceMs = 0;
	}

	if (otaBootPendingVerification && now - otaBootVerificationStartMs >= OTA_BOOT_HEALTH_TIMEOUT_MS) {
		Serial.println("OTA boot validation failed; rolling back to the previous slot");
		delay(100);
		esp_ota_mark_app_invalid_rollback_and_reboot();
	}
}

void printOtaInfo() {
	const esp_partition_t *running = esp_ota_get_running_partition();
	const esp_partition_t *next = esp_ota_get_next_update_partition(NULL);
	const char *failure = otaSafetyFailure();
	print("OTA A/B slots: %d active: %s inactive: %s\n",
		esp_ota_get_app_partition_count(), otaPartitionName(running), otaPartitionName(next));
	print("OTA ready: %d reason: %s pending verification: %d\n",
		failure == NULL, failure == NULL ? "none" : failure, otaBootPendingVerification);
	Serial.printf("OTA URL: http://device-ip:%u/api/ota/update\n", OTA_HTTP_PORT);
	Serial.println("OTA authentication: disabled (private development build)");
}

#endif
