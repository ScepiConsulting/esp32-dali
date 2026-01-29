#include "diagnostics.h"

Diagnostics stats;
bool diagnostics_enabled = false;

void diagnosticsInit() {
  stats.uptime_seconds = 0;
  stats.messages_rx = 0;
  stats.messages_tx = 0;
  stats.errors = 0;
  stats.bus_health = "ok";
  stats.last_activity = 0;
  stats.free_heap_kb = 0;
}

void updateDiagnostics() {
  stats.uptime_seconds = millis() / 1000;
  stats.free_heap_kb = ESP.getFreeHeap() / 1024.0;

  if (stats.errors > 10) {
    stats.bus_health = "error";
  } else if (stats.errors > 5) {
    stats.bus_health = "warning";
  } else {
    stats.bus_health = "ok";
  }
}

void incrementRxCount() {
  stats.messages_rx++;
}

void incrementTxCount() {
  stats.messages_tx++;
}

void incrementErrorCount() {
  stats.errors++;
}

void updateLastActivity() {
  stats.last_activity = millis() / 1000;
}

String getDiagnosticsJSON() {
  updateDiagnostics();

  String json = "{";
  json += "\"uptime_seconds\":" + String(stats.uptime_seconds) + ",";
  json += "\"messages_rx\":" + String(stats.messages_rx) + ",";
  json += "\"messages_tx\":" + String(stats.messages_tx) + ",";
  json += "\"errors\":" + String(stats.errors) + ",";
  json += "\"bus_health\":\"" + stats.bus_health + "\",";
  json += "\"last_activity\":" + String(stats.last_activity) + ",";
  json += "\"free_heap_kb\":" + String(stats.free_heap_kb, 1);
  json += "}";

  return json;
}
