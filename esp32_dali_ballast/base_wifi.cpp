#include "base_wifi.h"
#include "base_config.h"
#include "project_config.h"
#include <WiFi.h>
#include <Preferences.h>
#include <DNSServer.h>

String storedSSID = "";
String storedPassword = "";
bool wifiConnected = false;
bool apMode = true;

DNSServer dnsServer;

void wifiInit() {
  Preferences prefs;
  prefs.begin("wifi", true);
  storedSSID = prefs.getString("ssid", "");
  storedPassword = prefs.getString("password", "");
  prefs.end();

#ifdef DEBUG_SERIAL
  Serial.printf("Stored SSID: %s\n", storedSSID.c_str());
#endif

  if (storedSSID.length() > 0) {
#ifdef DEBUG_SERIAL
    Serial.println("Attempting to connect to stored WiFi...");
#endif
    WiFi.mode(WIFI_STA);
    WiFi.begin(storedSSID.c_str(), storedPassword.c_str());

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < WIFI_CONNECT_ATTEMPTS) {
      delay(500);
#ifdef DEBUG_SERIAL
      Serial.print(".");
#endif
      attempts++;
    }
#ifdef DEBUG_SERIAL
    Serial.println();
#endif

    if (WiFi.status() == WL_CONNECTED) {
      wifiConnected = true;
      apMode = false;
#ifdef DEBUG_SERIAL
      Serial.printf("Connected! IP: %s\n", WiFi.localIP().toString().c_str());
#endif
    } else {
#ifdef DEBUG_SERIAL
      Serial.println("Failed to connect, starting AP mode...");
#endif
    }
  }

  if (!wifiConnected) {
    startAPMode();
  }
}

void startAPMode() {
  apMode = true;
  WiFi.mode(WIFI_AP);

  uint64_t mac = ESP.getEfuseMac();
  String apSSID = String(AP_SSID_PREFIX) + "-" + String((uint32_t)(mac & 0xFFFFFF), HEX);
  apSSID.toUpperCase();

  WiFi.softAP(apSSID.c_str(), AP_PASSWORD);

  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

#ifdef DEBUG_SERIAL
  Serial.printf("AP Mode started. SSID: %s, Password: %s\n", apSSID.c_str(), AP_PASSWORD);
  Serial.printf("Connect to http://%s\n", WiFi.softAPIP().toString().c_str());
#endif
}

void wifiLoop() {
  if (apMode) {
    dnsServer.processNextRequest();
  }
}

String getWiFiStatusHTML() {
  String html = "<div class=\"status\">";
  html += "<div class=\"dot " + String(wifiConnected ? "green" : "yellow") + "\"></div>";
  html += "<span>WiFi: " + String(wifiConnected ? "Connected (" + WiFi.localIP().toString() + ")" : "AP Mode") + "</span>";
  html += "</div>";
  return html;
}

String getWiFiStatusJSON() {
  String json = "{";
  json += "\"connected\":" + String(wifiConnected ? "true" : "false") + ",";
  json += "\"ap_mode\":" + String(apMode ? "true" : "false") + ",";
  json += "\"ip\":\"" + (wifiConnected ? WiFi.localIP().toString() : WiFi.softAPIP().toString()) + "\",";
  json += "\"ssid\":\"" + (wifiConnected ? WiFi.SSID() : String(AP_SSID_PREFIX)) + "\"";
  json += "}";
  return json;
}
