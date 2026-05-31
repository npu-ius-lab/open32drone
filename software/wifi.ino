// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Wi-Fi support

#if WIFI_ENABLED

#include <WiFi.h>
#include <WiFiAP.h>
#include <WiFiUdp.h>
#include <Preferences.h>

#define WIFI_SSID "flix"
#define WIFI_PASSWORD "flixwifi"
#define WIFI_UDP_PORT 14550
#define WIFI_UDP_REMOTE_PORT 14550
#define WIFI_UDP_REMOTE_ADDR "255.255.255.255"
#ifndef WIFI_STA_SSID
#define WIFI_STA_SSID "209-2_4"
#endif
#ifndef WIFI_STA_PASSWORD
#define WIFI_STA_PASSWORD "nwpu209209"
#endif
#define WIFI_STA_CONNECT_TIMEOUT_MS 15000

extern Preferences storage;
WiFiUDP udp;

bool connectWiFi(const char *ssid, const char *password, uint32_t timeoutMs);
bool configureWiFi(const char *ssid, const char *password);
void clearSavedWiFi();
void scanWiFi();
const char *authModeName(wifi_auth_mode_t authMode);
void startAccessPoint();

void setupWiFi() {
	print("Setup Wi-Fi\n");
	WiFi.persistent(false);
	WiFi.setSleep(false);
	WiFi.setTxPower(WIFI_POWER_19_5dBm);
	bool staConnected = false;
	bool hasSavedWiFi = storage.isKey("WIFI_SSID");
	String savedSsid = storage.getString("WIFI_SSID", "");
	String savedPassword = storage.getString("WIFI_PASS", "");

	if (hasSavedWiFi && savedSsid.length() > 0) {
		print("Trying saved STA credentials\n");
		staConnected = connectWiFi(savedSsid.c_str(), savedPassword.c_str(), WIFI_STA_CONNECT_TIMEOUT_MS);
	}
	if (!staConnected && strlen(WIFI_STA_SSID) > 0 &&
		(strcmp(savedSsid.c_str(), WIFI_STA_SSID) != 0 || strcmp(savedPassword.c_str(), WIFI_STA_PASSWORD) != 0)) {
		print("Trying built-in STA credentials\n");
		staConnected = connectWiFi(WIFI_STA_SSID, WIFI_STA_PASSWORD, WIFI_STA_CONNECT_TIMEOUT_MS);
		if (staConnected) {
			storage.putString("WIFI_SSID", WIFI_STA_SSID);
			storage.putString("WIFI_PASS", WIFI_STA_PASSWORD);
		}
	}
	if (!staConnected) {
		print("STA unavailable, falling back to AP mode\n");
		startAccessPoint();
	}
	udp.begin(WIFI_UDP_PORT);
}

void startAccessPoint() {
	print("Starting Wi-Fi AP: %s\n", WIFI_SSID);
	WiFi.mode(WIFI_AP);
	WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);
}

bool connectWiFi(const char *ssid, const char *password, uint32_t timeoutMs) {
	if (ssid == NULL || strlen(ssid) == 0) {
		print("STA Wi-Fi SSID is empty\n");
		return false;
	}

	print("Connecting Wi-Fi STA: %s\n", ssid);
	WiFi.disconnect(true, false);
	delay(300);
	WiFi.mode(WIFI_STA);
	WiFi.setSleep(false);
	WiFi.setHostname("flix-drone");
	WiFi.begin(ssid, password);

	uint32_t start = millis();
	while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
		delay(250);
	}

	if (WiFi.status() == WL_CONNECTED) {
		print("STA connected, IP: %s\n", WiFi.localIP().toString().c_str());
		return true;
	}

	print("STA connect failed, status: %d, SSID: %s\n", WiFi.status(), ssid);
	return false;
}

bool configureWiFi(const char *ssid, const char *password) {
	if (!connectWiFi(ssid, password, WIFI_STA_CONNECT_TIMEOUT_MS)) {
		startAccessPoint();
		return false;
	}
	storage.putString("WIFI_SSID", ssid);
	storage.putString("WIFI_PASS", password);
	print("STA credentials saved\n");
	return true;
}

void clearSavedWiFi() {
	storage.remove("WIFI_SSID");
	storage.remove("WIFI_PASS");
	print("Saved STA credentials cleared\n");
}

void scanWiFi() {
	print("Scanning Wi-Fi networks...\n");
	WiFi.mode(WIFI_STA);
	WiFi.disconnect(false, false);
	delay(200);
	int count = WiFi.scanNetworks(false, true);
	if (count <= 0) {
		print("No Wi-Fi networks found: %d\n", count);
		return;
	}
	for (int i = 0; i < count; i++) {
		print("%2d: %s RSSI:%d ch:%d auth:%s\n",
			i + 1,
			WiFi.SSID(i).c_str(),
			WiFi.RSSI(i),
			WiFi.channel(i),
			authModeName(WiFi.encryptionType(i)));
	}
	WiFi.scanDelete();
}

const char *authModeName(wifi_auth_mode_t authMode) {
	switch (authMode) {
		case WIFI_AUTH_OPEN: return "OPEN";
		case WIFI_AUTH_WEP: return "WEP";
		case WIFI_AUTH_WPA_PSK: return "WPA";
		case WIFI_AUTH_WPA2_PSK: return "WPA2";
		case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/WPA2";
		case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-ENT";
		case WIFI_AUTH_WPA3_PSK: return "WPA3";
		case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA2/WPA3";
		default: return "UNKNOWN";
	}
}

void sendWiFi(const uint8_t *buf, int len) {
	if (WiFi.softAPIP() == IPAddress(0, 0, 0, 0) && WiFi.status() != WL_CONNECTED) return;
	udp.beginPacket(udp.remoteIP() ? udp.remoteIP() : WIFI_UDP_REMOTE_ADDR, WIFI_UDP_REMOTE_PORT);
	udp.write(buf, len);
	udp.endPacket();
}

int receiveWiFi(uint8_t *buf, int len) {
	udp.parsePacket();
	return udp.read(buf, len);
}

void printWiFiInfo() {
	print("Wi-Fi mode: %d\n", WiFi.getMode());
	print("AP MAC: %s\n", WiFi.softAPmacAddress().c_str());
	print("AP SSID: %s\n", WiFi.softAPSSID().c_str());
	print("AP Password: %s\n", WIFI_PASSWORD);
	print("AP Clients: %d\n", WiFi.softAPgetStationNum());
	print("AP IP: %s\n", WiFi.softAPIP().toString().c_str());
	print("STA MAC: %s\n", WiFi.macAddress().c_str());
	print("STA SSID: %s\n", WiFi.SSID().c_str());
	print("STA Status: %d\n", WiFi.status());
	print("STA IP: %s\n", WiFi.localIP().toString().c_str());
	print("Remote IP: %s\n", udp.remoteIP().toString().c_str());
	print("MAVLink connected: %d\n", mavlinkConnected);
}

#endif
