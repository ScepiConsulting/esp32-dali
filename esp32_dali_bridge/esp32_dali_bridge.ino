#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <DNSServer.h>
#include <SPIFFS.h>
#include <Update.h>
#include "esp_task_wdt.h"

#include "project_version.h"
#include "base_config.h"
#include "project_config.h"

#include "base_wifi.h"
#include "base_web.h"
#include "base_mqtt.h"
#include "base_diagnostics.h"
#include "base_ota.h"

#include "project_function.h"
#include "project_home.h"
#include "project_diagnostics.h"
#include "project_mqtt.h"

unsigned long lastStatusPublish = 0;

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("\n\n=== " + String(PROJECT_NAME) + " Starting ===");
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

  diagnosticsCoreInit();

  wifiInit();

  mqttCoreInit();

  webCoreInit();

  otaInit();

  functionInit();

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
    updateCoreDiagnostics();
    Serial.printf("[STATUS] Uptime: %lus | Heap: %.1fKB\n",
                  coreStats.uptime_seconds, coreStats.free_heap_kb);
#endif
    lastStatusLog = millis();
  }

  wifiLoop();

  webCoreLoop();

  mqttCoreLoop();

  functionLoop();

  unsigned long now = millis();
  if (now - lastStatusPublish >= STATUS_PUBLISH_INTERVAL) {
    publishStatus();
    publishDiagnostics();
    lastStatusPublish = now;
  }

  delay(10);
}
