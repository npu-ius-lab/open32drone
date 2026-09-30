// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Wi-Fi transport for MAVLink, configuration, and ground-only OTA.

#if WIFI_ENABLED

#include <WiFi.h>
#include <WiFiAP.h>
#include <WiFiUdp.h>
#include <Preferences.h>
#include "esp_partition.h"

extern Preferences storage; // from parameters.ino
extern bool parameterStorageHealthy;

// Configured modes: 0=disabled, 1=AP, 2=STA(client). Runtime mode can be AP
// even when STA is configured, because a failed boot connection opens the
// recovery AP without overwriting the saved STA selection.
const int W_AP = 1, W_STA = 2;

// Private builds may inject router defaults with build.extra_flags=-include...
// The tracked/open build remains AP-first and contains no real credentials.
#ifndef OPEN32DRONE_WIFI_BOOT_MODE
#define OPEN32DRONE_WIFI_BOOT_MODE W_AP
#endif
#ifndef OPEN32DRONE_WIFI_STA_SSID
#define OPEN32DRONE_WIFI_STA_SSID ""
#endif
#ifndef OPEN32DRONE_WIFI_STA_PASS
#define OPEN32DRONE_WIFI_STA_PASS ""
#endif
static_assert(OPEN32DRONE_WIFI_BOOT_MODE >= 0 && OPEN32DRONE_WIFI_BOOT_MODE <= W_STA,
	"invalid compiled Wi-Fi mode");
static_assert(OPEN32DRONE_WIFI_BOOT_MODE != W_STA ||
	(sizeof(OPEN32DRONE_WIFI_STA_SSID) > 1 && sizeof(OPEN32DRONE_WIFI_STA_SSID) <= 33),
	"compiled STA SSID must be 1..32 characters");
static_assert(OPEN32DRONE_WIFI_BOOT_MODE != W_STA ||
	(sizeof(OPEN32DRONE_WIFI_STA_PASS) >= 9 && sizeof(OPEN32DRONE_WIFI_STA_PASS) <= 64),
	"compiled STA password must be 8..63 characters");

int wifiMode = OPEN32DRONE_WIFI_BOOT_MODE;
int wifiRuntimeMode = 0;
int udpLocalPort = 14550;
int udpRemotePort = 14550;
IPAddress udpRemoteIP = "255.255.255.255";
uint16_t udpPeerPort = 14550; // learned runtime endpoint; never persisted as WIFI_PORT_REM
uint32_t udpRxPackets = 0;
uint32_t udpTxPackets = 0;
bool udpBound = false;
bool wifiStaFallbackActive = false;

const uint32_t WIFI_STA_CONNECT_TIMEOUT_MS = 8000;

// Higher TX power improves range but increases peak current. If the 3.3 V rail
// sags, the softAP can fail to broadcast intermittently.
#define WIFI_TX_POWER WIFI_POWER_11dBm

WiFiUDP udp;

bool startWiFiAccessPoint(bool staFallback) {
	WiFi.disconnect(true, false);
	WiFi.mode(WIFI_AP);
	String ssid = storage.getString("WIFI_AP_SSID", "open32drone");
	String pass = storage.getString("WIFI_AP_PASS", "12345678");
	bool apOk = false;
	for (int attempt = 0; attempt < 3 && !apOk; attempt++) {
		if (attempt > 0) delay(300);
		apOk = WiFi.softAP(ssid.c_str(), pass.c_str());
	}
	if (!apOk) {
		wifiRuntimeMode = 0;
		print("softAP started: NO\n");
		return false;
	}

	wifiRuntimeMode = W_AP;
	wifiStaFallbackActive = staFallback;
	udpRemoteIP = WiFi.softAPBroadcastIP();
	WiFi.setSleep(false);
	bool txOk = WiFi.setTxPower(WIFI_TX_POWER);
	print("softAP started: yes (IP %s%s)\n", WiFi.softAPIP().toString().c_str(),
		staFallback ? ", STA fallback" : "");
	print("TX power: %s\n", txOk ? "set" : "failed, using default");
	return true;
}

bool startWiFiStation() {
	// Explicitly saved NVS credentials take precedence. On a fully erased board,
	// a private build may fall back to locally injected defaults.
	String ssid = storage.getString("WIFI_STA_SSID", OPEN32DRONE_WIFI_STA_SSID);
	String pass = storage.getString("WIFI_STA_PASS", OPEN32DRONE_WIFI_STA_PASS);
	if (ssid.isEmpty()) {
		print("WiFi STA not configured: SSID is empty\n");
		return false;
	}

	WiFi.mode(WIFI_STA);
	WiFi.setSleep(false);
	WiFi.setAutoReconnect(true);
	WiFi.begin(ssid.c_str(), pass.c_str());
	print("WiFi STA connecting to: %s", ssid.c_str());
	uint32_t startedAt = millis();
	uint32_t nextDotAt = startedAt + 1000;
	while (!WiFi.isConnected() && millis() - startedAt < WIFI_STA_CONNECT_TIMEOUT_MS) {
		delay(100);
		if ((int32_t)(millis() - nextDotAt) >= 0) {
			print(".");
			nextDotAt += 1000;
		}
	}
	print("\n");
	if (!WiFi.isConnected()) {
		print("WiFi STA connection failed (status %d); starting recovery AP\n", WiFi.status());
		return false;
	}

	wifiRuntimeMode = W_STA;
	wifiStaFallbackActive = false;
	udpRemoteIP = IPAddress(255, 255, 255, 255);
	print("WiFi STA connected: IP %s RSSI %d dBm\n",
		WiFi.localIP().toString().c_str(), WiFi.RSSI());
	return true;
}

void setupWiFi() {
	print("Setup Wi-Fi\n");
	// NVS space is shared by Wi-Fi PHY calibration and parameters. Print the
	// partition size because a full NVS partition prevents networking startup.
	const esp_partition_t *nvsPart = esp_partition_find_first(
		ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS, NULL);
	if (nvsPart != NULL) print("NVS partition size: %u KB\n", nvsPart->size / 1024);

	wifiRuntimeMode = 0;
	wifiStaFallbackActive = false;
	if (wifiMode == W_AP) {
		startWiFiAccessPoint(false);
	} else if (wifiMode == W_STA) {
		if (!startWiFiStation()) startWiFiAccessPoint(true);
	} else {
		print("WiFi disabled\n");
		return;
	}
	if (wifiRuntimeMode == 0) return;

	udpPeerPort = udpRemotePort;
	udpBound = udp.begin(udpLocalPort) == 1;
	print("MAVLink UDP: bind %s local %d initial peer %s:%d\n", udpBound ? "OK" : "FAILED",
		udpLocalPort, udpRemoteIP.toString().c_str(), udpPeerPort);
	setupOtaHttpServer();
}

bool wifiNetworkReady() {
	if (wifiRuntimeMode == W_AP) return WiFi.softAPgetStationNum() > 0;
	if (wifiRuntimeMode == W_STA) return WiFi.isConnected();
	return false;
}

void sendWiFi(const uint8_t *buf, int len) {
	if (!udpBound || !wifiNetworkReady()) return;
	if (!udp.beginPacket(udpRemoteIP, udpPeerPort)) return;
	udp.write(buf, len);
	if (udp.endPacket()) udpTxPackets++;
}

int receiveWiFi(uint8_t *buf, int len) {
	if (!udpBound || !wifiNetworkReady()) return 0;
	int packetSize = udp.parsePacket();
	if (packetSize <= 0) return 0;
	// Reply to the actual source endpoint. MAVROS Router and GCS clients do not
	// necessarily transmit from the configured destination port.
	if (udp.remoteIP()) udpRemoteIP = udp.remoteIP();
	if (udp.remotePort() != 0) udpPeerPort = udp.remotePort();
	int received = udp.read(buf, min(len, packetSize));
	if (received > 0) udpRxPackets++;
	return received;
}

bool wifiTransportHealthy() {
	if (!udpBound) return false;
	if (wifiRuntimeMode == W_AP) return WiFi.getMode() == WIFI_MODE_AP &&
		WiFi.softAPIP() != IPAddress(0, 0, 0, 0);
	if (wifiRuntimeMode == W_STA) return WiFi.isConnected();
	return false;
}

void printWiFiInfo() {
	print("Configured mode: %s (%d)\n",
		wifiMode == W_AP ? "AP" : wifiMode == W_STA ? "STA" : "disabled", wifiMode);
	if (wifiRuntimeMode == W_AP) {
		print("Mode: Access Point (AP)%s\n", wifiStaFallbackActive ? " - STA fallback" : "");
		print("MAC: %s\n", WiFi.softAPmacAddress().c_str());
		print("SSID: %s\n", WiFi.softAPSSID().c_str());
		print("Password: [redacted]\n");
		print("Clients: %d\n", WiFi.softAPgetStationNum());
		print("IP: %s\n", WiFi.softAPIP().toString().c_str());
	} else if (wifiRuntimeMode == W_STA) {
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
	print("MAVLink UDP: bound %d local %d peer %s:%d tx %u rx %u\n", udpBound,
		udpLocalPort, udpRemoteIP.toString().c_str(), udpPeerPort, udpTxPackets, udpRxPackets);
	print("MAVLink connected: %d\n", mavlinkConnected);
}

// Store credentials and the selected boot mode. The active network is never
// switched while running; reboot is required so a configuration command cannot
// interrupt an established control link.
bool configWiFi(bool ap, const char *ssid, const char *password) {
	if (armed) {
		print("Wi-Fi configuration rejected while armed\n");
		return false;
	}
	if (!parameterStorageHealthy) {
		print("Wi-Fi configuration rejected: parameter storage unavailable\n");
		return false;
	}
	String requestedSsid = ssid ? String(ssid) : String();
	String requestedPass = password ? String(password) : String();
	if (requestedSsid.isEmpty() || requestedSsid.length() > 32) {
		print("Wi-Fi configuration rejected: SSID must be 1..32 characters\n");
		return false;
	}
	// The two-argument serial parser cannot represent an empty password. Keep the
	// accepted range identical for AP and STA so configuration is unambiguous.
	if (requestedPass.length() < 8 || requestedPass.length() > 63) {
		print("Wi-Fi configuration rejected: password must be 8..63 characters\n");
		return false;
	}

	const char *ssidKey = ap ? "WIFI_AP_SSID" : "WIFI_STA_SSID";
	const char *passKey = ap ? "WIFI_AP_PASS" : "WIFI_STA_PASS";
	int requestedMode = ap ? W_AP : W_STA;
	storage.putString(ssidKey, requestedSsid);
	storage.putString(passKey, requestedPass);
	storage.putFloat("WIFI_MODE", requestedMode);
	bool stored = storage.getString(ssidKey, "") == requestedSsid &&
		storage.getString(passKey, "") == requestedPass &&
		(int)storage.getFloat("WIFI_MODE", 0) == requestedMode;
	if (!stored) {
		parameterStorageHealthy = false;
		print("Wi-Fi configuration failed: NVS write/readback error\n");
		return false;
	}

	wifiMode = requestedMode;
	print("Wi-Fi %s configured for '%s'; reboot to apply\n",
		ap ? "AP" : "STA", requestedSsid.c_str());
	return true;
}

#endif
