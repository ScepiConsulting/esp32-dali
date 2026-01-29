#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <DNSServer.h>
#include <SPIFFS.h>
#include <Update.h>
#include "esp_task_wdt.h"

#include "version.h"
#include "config.h"
#include "dali_protocol.h"
#include "dali_handler.h"
#include "mqtt_handler.h"
#include "web_interface.h"
#include "diagnostics.h"
#include "utils.h"

Preferences preferences;
DNSServer dnsServer;

String storedSSID = "";
String storedPassword = "";
bool wifiConnected = false;
bool apMode = true;

unsigned long lastStatusPublish = 0;
const unsigned long STATUS_PUBLISH_INTERVAL = 60000;

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("\n\n=== ESP32 DALI Bridge Starting ===");
  Serial.printf("Version: %s\n", FIRMWARE_VERSION);
  Serial.printf("Build: %s %s\n", BUILD_DATE, BUILD_TIME);
  Serial.printf("DEBUG_SERIAL is %s\n",
#ifdef DEBUG_SERIAL
    "ENABLED"
#else
    "DISABLED"
#endif
  );

#ifdef DEBUG_SERIAL
  Serial.println("Initializing SPIFFS...");
#endif
  if (!SPIFFS.begin(false)) {
#ifdef DEBUG_SERIAL
    Serial.println("SPIFFS mount failed, formatting... (this takes ~30s on first boot)");
#endif
    if (!SPIFFS.begin(true)) {
#ifdef DEBUG_SERIAL
      Serial.println("SPIFFS format failed!");
#endif
    } else {
#ifdef DEBUG_SERIAL
      Serial.println("SPIFFS formatted and mounted successfully");
#endif
    }
  } else {
#ifdef DEBUG_SERIAL
    Serial.println("SPIFFS mounted successfully");
#endif
  }

  diagnosticsInit();

  preferences.begin("wifi", true);
  storedSSID = preferences.getString("ssid", "");
  storedPassword = preferences.getString("password", "");
  preferences.end();

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
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
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

  daliInit();

  mqttInit();

  webInit();

  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = WDT_TIMEOUT * 1000,
    .idle_core_mask = 0,
    .trigger_panic = true
  };
  esp_task_wdt_init(&wdt_config);
  esp_task_wdt_add(NULL);

#ifdef DEBUG_SERIAL
  Serial.println("Setup complete!");
  Serial.println("==============================\n");
#endif
}

void loop() {
  esp_task_wdt_reset();

  static unsigned long lastStatusLog = 0;
  if (millis() - lastStatusLog > 60000) {
#ifdef DEBUG_SERIAL
    Serial.printf("[STATUS] Uptime: %lus | Heap: %.1fKB | RX: %lu | TX: %lu | Errors: %lu\n",
                  stats.uptime_seconds, stats.free_heap_kb, stats.messages_rx, stats.messages_tx, stats.errors);
#endif
    lastStatusLog = millis();
  }

  if (apMode) {
    dnsServer.processNextRequest();
  }

  webLoop();

  mqttLoop();

  monitorDaliBus();

  processCommandQueue();

  unsigned long now = millis();
  if (now - lastStatusPublish >= STATUS_PUBLISH_INTERVAL) {
    publishStatus();
    lastStatusPublish = now;
  }

  updateDiagnostics();

  delay(10);
}

void startAPMode() {
  apMode = true;
  WiFi.mode(WIFI_AP);

  // Generate AP SSID with MAC suffix
  uint64_t mac = ESP.getEfuseMac();
  String apSSID = String(AP_SSID) + "-" + String((uint32_t)(mac & 0xFFFFFF), HEX);
  apSSID.toUpperCase();

  WiFi.softAP(apSSID.c_str(), AP_PASSWORD);

  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

#ifdef DEBUG_SERIAL
  Serial.printf("AP Mode started. SSID: %s, Password: %s\n", apSSID.c_str(), AP_PASSWORD);
  Serial.printf("Connect to http://%s\n", WiFi.softAPIP().toString().c_str());
#endif
}
