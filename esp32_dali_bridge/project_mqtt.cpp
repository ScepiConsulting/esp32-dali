#include "project_mqtt.h"
#include "project_config.h"
#include "project_function.h"
#include "base_mqtt.h"
#include "project_dali_handler.h"
#include <ArduinoJson.h>
#include <Preferences.h>

MonitorFilter monitorFilter = {
  .enable_dapc = true,
  .enable_control_cmds = true,
  .enable_queries = true,
  .enable_responses = true,
  .enable_commissioning = true,
  .enable_self_sent = true,
  .enable_bus_traffic = true
};

void loadMonitorFilter() {
  Preferences prefs;
  prefs.begin("mqtt", true);
  monitorFilter.enable_dapc = prefs.getBool("mon_dapc", true);
  monitorFilter.enable_control_cmds = prefs.getBool("mon_ctrl", true);
  monitorFilter.enable_queries = prefs.getBool("mon_query", true);
  monitorFilter.enable_responses = prefs.getBool("mon_resp", true);
  monitorFilter.enable_commissioning = prefs.getBool("mon_comm", true);
  monitorFilter.enable_self_sent = prefs.getBool("mon_self", true);
  monitorFilter.enable_bus_traffic = prefs.getBool("mon_bus", true);
  prefs.end();
}

void onMqttConnected() {
#ifdef DEBUG_SERIAL
  Serial.println("[MQTT] Connected - subscribing to DALI topics");
#endif

  loadMonitorFilter();

  mqttSubscribe(mqtt_prefix + "command");
  mqttSubscribe(mqtt_prefix + "scan/trigger");
  mqttSubscribe(mqtt_prefix + "commission/trigger");
}

void onMqttMessage(const String& topic, const String& payload) {
#ifdef DEBUG_SERIAL
  Serial.printf("[MQTT] Message on %s: %s\n", topic.c_str(), payload.c_str());
#endif

  if (topic == mqtt_prefix + "command") {
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
#ifdef DEBUG_SERIAL
      Serial.println("[MQTT] Invalid JSON in command");
#endif
      return;
    }

    DaliCommand cmd;
    cmd.command_type = doc["command"].as<String>();
    cmd.address = doc["address"] | 0;
    cmd.level = doc["level"] | 254;
    cmd.scene = doc["scene"] | 0;
    cmd.force = doc["force"] | false;
    cmd.queued_at = millis();
    cmd.priority = doc["priority"] | 1;
    cmd.retry_count = 0;

    if (doc.containsKey("level_percent")) {
      float percent = doc["level_percent"].as<float>();
      cmd.level = (uint8_t)((percent / 100.0) * 254.0);
    }

    if (validateDaliCommand(cmd) || cmd.force) {
      enqueueDaliCommand(cmd);
    }
  } else if (topic == mqtt_prefix + "scan/trigger") {
#ifdef DEBUG_SERIAL
    Serial.println("[MQTT] Scan triggered");
#endif
    DaliScanResult result = scanDaliDevices();
    publishScanResult(result);
  } else if (topic == mqtt_prefix + "commission/trigger") {
#ifdef DEBUG_SERIAL
    Serial.println("[MQTT] Commissioning triggered");
#endif
    uint8_t start_address = 0;
    if (payload.length() > 0) {
      start_address = payload.toInt();
      if (start_address > 63) start_address = 0;
    }
    commissionDevices(start_address);
  }
}

String bytesToHex(uint8_t* bytes, uint8_t length) {
  String hex = "";
  for (uint8_t i = 0; i < length; i++) {
    if (bytes[i] < 0x10) hex += "0";
    hex += String(bytes[i], HEX);
  }
  hex.toUpperCase();
  return hex;
}

void publishMonitor(const DaliMessage& msg) {
  if (!mqtt_enabled || !mqttClient.connected()) return;

  // Apply monitor filters
  if (msg.source == "self" && !monitorFilter.enable_self_sent) return;
  if (msg.source == "bus" && !monitorFilter.enable_bus_traffic) return;

  String type = msg.parsed.type;
  bool shouldPublish = false;

  if (type == "direct_arc_power") {
    shouldPublish = monitorFilter.enable_dapc;
  } else if (type == "response") {
    shouldPublish = monitorFilter.enable_responses;
  } else if (type == "off" || type == "up" || type == "down" ||
             type == "step_up" || type == "step_down" ||
             type == "recall_max" || type == "recall_min" ||
             type == "reset" || type == "go_to_scene") {
    shouldPublish = monitorFilter.enable_control_cmds;
  } else if (type.startsWith("query_")) {
    shouldPublish = monitorFilter.enable_queries;
  } else if (type == "initialise" || type == "randomise" ||
             type == "compare" || type == "withdraw" ||
             type.startsWith("searchaddr") || type == "program_short_address") {
    shouldPublish = monitorFilter.enable_commissioning;
  } else {
    shouldPublish = true;
  }

  if (!shouldPublish) return;

  String topic = mqtt_prefix + "monitor";
  String json = "{";
  json += "\"timestamp\":" + String(msg.timestamp) + ",";
  json += "\"direction\":\"" + String(msg.is_tx ? "tx" : "rx") + "\",";
  json += "\"source\":\"" + msg.source + "\",";
  json += "\"raw\":\"" + bytesToHex((uint8_t*)msg.raw_bytes, msg.length) + "\",";
  json += "\"parsed\":{";
  json += "\"type\":\"" + msg.parsed.type + "\",";
  json += "\"address\":" + String(msg.parsed.address) + ",";
  json += "\"address_type\":\"" + msg.parsed.address_type + "\",";
  json += "\"level\":" + String(msg.parsed.level) + ",";
  json += "\"level_percent\":" + String(msg.parsed.level_percent, 1) + ",";
  json += "\"description\":\"" + msg.description + "\"";
  json += "}}";

  mqttPublish(topic, json, false);
}

void publishScanResult(const DaliScanResult& result) {
  if (!mqtt_enabled || !mqttClient.connected()) return;

  String topic = mqtt_prefix + "scan/result";
  String json = "{\"scan_timestamp\":" + String(result.scan_timestamp) + ",\"devices\":[";

  for (size_t i = 0; i < result.devices.size(); i++) {
    if (i > 0) json += ",";
    const DaliDevice& dev = result.devices[i];
    json += "{\"address\":" + String(dev.address) + ",";
    json += "\"type\":\"" + dev.type + "\",";
    json += "\"status\":\"" + dev.status + "\",";
    json += "\"lamp_failure\":" + String(dev.lamp_failure ? "true" : "false") + ",";
    json += "\"min_level\":" + String(dev.min_level) + ",";
    json += "\"max_level\":" + String(dev.max_level) + "}";
  }

  json += "],\"total_found\":" + String(result.total_found) + "}";
  mqttPublish(topic, json, false);
}

void publishCommissioningProgress(const CommissioningProgress& progress) {
  if (!mqtt_enabled || !mqttClient.connected()) return;

  String stateStr;
  switch (progress.state) {
    case COMM_IDLE: stateStr = "idle"; break;
    case COMM_INITIALIZING: stateStr = "initializing"; break;
    case COMM_SEARCHING: stateStr = "searching"; break;
    case COMM_PROGRAMMING: stateStr = "programming"; break;
    case COMM_VERIFYING: stateStr = "verifying"; break;
    case COMM_COMPLETE: stateStr = "complete"; break;
    case COMM_ERROR: stateStr = "error"; break;
    default: stateStr = "unknown"; break;
  }

  String topic = mqtt_prefix + "commission/progress";
  String json = "{";
  json += "\"state\":\"" + stateStr + "\",";
  json += "\"start_timestamp\":" + String(progress.start_timestamp) + ",";
  json += "\"devices_found\":" + String(progress.devices_found) + ",";
  json += "\"devices_programmed\":" + String(progress.devices_programmed) + ",";
  json += "\"current_address\":" + String(progress.current_address) + ",";
  json += "\"next_free_address\":" + String(progress.next_free_address) + ",";
  json += "\"current_random_address\":\"0x" + String(progress.current_random_address, HEX) + "\",";
  json += "\"status_message\":\"" + progress.status_message + "\",";
  json += "\"progress_percent\":" + String(progress.progress_percent);
  json += "}";

  mqttPublish(topic, json, false);
}
