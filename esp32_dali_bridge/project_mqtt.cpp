#include "project_mqtt.h"
#include "project_config.h"
#include "project_function.h"
#include "base_mqtt.h"
#include "base_web.h"
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

String getMqttTopicsHTML() {
  String html = "";

  html += "<p style=\"margin:12px 0 4px 0;color:var(--text-secondary);\"><strong>Command:</strong> <code style=\"background:var(--bg-primary);padding:2px 6px;border-radius:3px;font-family:monospace;\">" + mqtt_prefix + "command</code></p>";
  html += "<p style=\"margin:0 0 8px 0;font-size:12px;color:var(--text-secondary);\">Subscribe to this topic to send commands to DALI devices</p>";
  html += "<details style=\"margin:8px 0;padding:8px;background:var(--bg-primary);border-radius:4px;\">";
  html += "<summary style=\"cursor:pointer;color:var(--accent-green);font-weight:500;\">▸ Example: Command</summary>";
  html += "<pre style=\"background:var(--bg-secondary);padding:12px;border-radius:6px;overflow-x:auto;margin:8px 0;font-size:12px;\">{<br>  \"command\": \"set_brightness\",<br>  \"address\": 0,<br>  \"level\": 128<br>}</pre></details>";

  html += "<p style=\"margin:12px 0 4px 0;color:var(--text-secondary);\"><strong>Monitor:</strong> <code style=\"background:var(--bg-primary);padding:2px 6px;border-radius:3px;font-family:monospace;\">" + mqtt_prefix + "monitor</code></p>";
  html += "<p style=\"margin:0 0 8px 0;font-size:12px;color:var(--text-secondary);\">Publishes all DALI bus activity with source field (self/bus)</p>";
  html += "<details style=\"margin:8px 0;padding:8px;background:var(--bg-primary);border-radius:4px;\">";
  html += "<summary style=\"cursor:pointer;color:var(--accent-green);font-weight:500;\">▸ Example: Monitor Message</summary>";
  html += "<pre style=\"background:var(--bg-secondary);padding:12px;border-radius:6px;overflow-x:auto;margin:8px 0;font-size:12px;\">{<br>  \"timestamp\": 1234567890,<br>  \"direction\": \"tx\",<br>  \"source\": \"self\",<br>  \"raw\": \"01FE\",<br>  \"parsed\": {<br>    \"type\": \"direct_arc_power\",<br>    \"address\": 0,<br>    \"level\": 254<br>  }<br>}</pre></details>";

  html += "<p style=\"margin:12px 0 4px 0;color:var(--text-secondary);\"><strong>Status:</strong> <code style=\"background:var(--bg-primary);padding:2px 6px;border-radius:3px;font-family:monospace;\">" + mqtt_prefix + "status</code></p>";
  html += "<p style=\"margin:0 0 8px 0;font-size:12px;color:var(--text-secondary);\">Device status published on connect and periodically</p>";
  html += "<details style=\"margin:8px 0;padding:8px;background:var(--bg-primary);border-radius:4px;\">";
  html += "<summary style=\"cursor:pointer;color:var(--accent-green);font-weight:500;\">▸ Example: Status</summary>";
  html += "<pre style=\"background:var(--bg-secondary);padding:12px;border-radius:6px;overflow-x:auto;margin:8px 0;font-size:12px;\">{<br>  \"status\": \"online\",<br>  \"uptime\": 12345,<br>  \"ip\": \"192.168.1.100\",<br>  \"client_id\": \"dali-bridge-AABBCC\"<br>}</pre></details>";

  html += "<p style=\"margin:12px 0 4px 0;color:var(--text-secondary);\"><strong>Scan Trigger:</strong> <code style=\"background:var(--bg-primary);padding:2px 6px;border-radius:3px;font-family:monospace;\">" + mqtt_prefix + "scan/trigger</code></p>";
  html += "<p style=\"margin:0 0 8px 0;font-size:12px;color:var(--text-secondary);\">Publish to trigger a DALI bus scan</p>";
  html += "<details style=\"margin:8px 0;padding:8px;background:var(--bg-primary);border-radius:4px;\">";
  html += "<summary style=\"cursor:pointer;color:var(--accent-green);font-weight:500;\">▸ Example: Scan Trigger</summary>";
  html += "<pre style=\"background:var(--bg-secondary);padding:12px;border-radius:6px;overflow-x:auto;margin:8px 0;font-size:12px;\">{\"scan\": true}</pre></details>";

  html += "<p style=\"margin:12px 0 4px 0;color:var(--text-secondary);\"><strong>Scan Result:</strong> <code style=\"background:var(--bg-primary);padding:2px 6px;border-radius:3px;font-family:monospace;\">" + mqtt_prefix + "scan/result</code></p>";
  html += "<p style=\"margin:0 0 8px 0;font-size:12px;color:var(--text-secondary);\">Scan results published after completion</p>";
  html += "<details style=\"margin:8px 0;padding:8px;background:var(--bg-primary);border-radius:4px;\">";
  html += "<summary style=\"cursor:pointer;color:var(--accent-green);font-weight:500;\">▸ Example: Scan Result</summary>";
  html += "<pre style=\"background:var(--bg-secondary);padding:12px;border-radius:6px;overflow-x:auto;margin:8px 0;font-size:12px;\">{<br>  \"timestamp\": 1234567890,<br>  \"total_found\": 3,<br>  \"devices\": [<br>    {\"address\": 0, \"status\": \"responsive\"},<br>    {\"address\": 1, \"status\": \"responsive\"},<br>    {\"address\": 5, \"status\": \"responsive\"}<br>  ]<br>}</pre></details>";

  html += "<hr style=\"margin:20px 0;border:none;border-top:1px solid var(--border-color);\">";
  html += "<h3 style=\"margin:16px 0 8px 0;font-size:15px;color:var(--accent-purple);\">🔧 Commissioning</h3>";

  html += "<p style=\"margin:12px 0 4px 0;color:var(--text-secondary);\"><strong>Commission Trigger:</strong> <code style=\"background:var(--bg-primary);padding:2px 6px;border-radius:3px;font-family:monospace;\">" + mqtt_prefix + "commission/trigger</code></p>";
  html += "<p style=\"margin:0 0 8px 0;font-size:12px;color:var(--text-secondary);\">Publish starting address (0-63) to begin commissioning</p>";
  html += "<details style=\"margin:8px 0;padding:8px;background:var(--bg-primary);border-radius:4px;\">";
  html += "<summary style=\"cursor:pointer;color:var(--accent-green);font-weight:500;\">▸ Example: Commission Trigger</summary>";
  html += "<pre style=\"background:var(--bg-secondary);padding:12px;border-radius:6px;overflow-x:auto;margin:8px 0;font-size:12px;\">{\"start_address\": 0}</pre></details>";

  html += "<p style=\"margin:12px 0 4px 0;color:var(--text-secondary);\"><strong>Commission Progress:</strong> <code style=\"background:var(--bg-primary);padding:2px 6px;border-radius:3px;font-family:monospace;\">" + mqtt_prefix + "commission/progress</code></p>";
  html += "<p style=\"margin:0 0 8px 0;font-size:12px;color:var(--text-secondary);\">Real-time progress updates during commissioning</p>";
  html += "<details style=\"margin:8px 0;padding:8px;background:var(--bg-primary);border-radius:4px;\">";
  html += "<summary style=\"cursor:pointer;color:var(--accent-green);font-weight:500;\">▸ Example: Commission Progress</summary>";
  html += "<pre style=\"background:var(--bg-secondary);padding:12px;border-radius:6px;overflow-x:auto;margin:8px 0;font-size:12px;\">{<br>  \"state\": \"programming\",<br>  \"devices_found\": 2,<br>  \"devices_programmed\": 1,<br>  \"current_address\": 1,<br>  \"progress_percent\": 50,<br>  \"status_message\": \"Programming device...\"<br>}</pre></details>";

  return html;
}

String getMqttFilterConfigHTML() {
  Preferences prefs;
  prefs.begin("mqtt", true);
  bool mon_dapc = prefs.getBool("mon_dapc", true);
  bool mon_ctrl = prefs.getBool("mon_ctrl", true);
  bool mon_query = prefs.getBool("mon_query", true);
  bool mon_resp = prefs.getBool("mon_resp", true);
  bool mon_comm = prefs.getBool("mon_comm", true);
  bool mon_self = prefs.getBool("mon_self", true);
  bool mon_bus = prefs.getBool("mon_bus", true);
  prefs.end();

  String html = "";
  html += "<h2 style=\"margin-top:24px;margin-bottom:12px;font-size:18px;\">Monitor Topic Filters</h2>";
  html += "<p style=\"margin-bottom:16px;color:var(--text-secondary);font-size:14px;\">Select which message types to publish to the monitor topic</p>";
  html += "<div style=\"display:grid;gap:8px;\">";
  html += "<label style=\"display:flex;align-items:center;cursor:pointer;\">";
  html += "<input type=\"checkbox\" name=\"mon_dapc\" value=\"1\"" + String(mon_dapc ? " checked" : "") + " style=\"margin-right:8px;\">";
  html += "<span>DAPC (Direct Arc Power / Brightness)</span></label>";
  html += "<label style=\"display:flex;align-items:center;cursor:pointer;\">";
  html += "<input type=\"checkbox\" name=\"mon_ctrl\" value=\"1\"" + String(mon_ctrl ? " checked" : "") + " style=\"margin-right:8px;\">";
  html += "<span>Control Commands (Off, Up, Down, etc.)</span></label>";
  html += "<label style=\"display:flex;align-items:center;cursor:pointer;\">";
  html += "<input type=\"checkbox\" name=\"mon_query\" value=\"1\"" + String(mon_query ? " checked" : "") + " style=\"margin-right:8px;\">";
  html += "<span>Query Commands (Status, Level, etc.)</span></label>";
  html += "<label style=\"display:flex;align-items:center;cursor:pointer;\">";
  html += "<input type=\"checkbox\" name=\"mon_resp\" value=\"1\"" + String(mon_resp ? " checked" : "") + " style=\"margin-right:8px;\">";
  html += "<span>Responses (Backward frames from ballasts)</span></label>";
  html += "<label style=\"display:flex;align-items:center;cursor:pointer;\">";
  html += "<input type=\"checkbox\" name=\"mon_comm\" value=\"1\"" + String(mon_comm ? " checked" : "") + " style=\"margin-right:8px;\">";
  html += "<span>Commissioning Commands</span></label>";
  html += "<label style=\"display:flex;align-items:center;cursor:pointer;\">";
  html += "<input type=\"checkbox\" name=\"mon_self\" value=\"1\"" + String(mon_self ? " checked" : "") + " style=\"margin-right:8px;\">";
  html += "<span>Self-sent (Commands from this bridge)</span></label>";
  html += "<label style=\"display:flex;align-items:center;cursor:pointer;\">";
  html += "<input type=\"checkbox\" name=\"mon_bus\" value=\"1\"" + String(mon_bus ? " checked" : "") + " style=\"margin-right:8px;\">";
  html += "<span>Bus Traffic (Commands from other devices)</span></label>";
  html += "</div>";

  return html;
}

void loadMqttFilterSettings() {
  loadMonitorFilter();
}

void saveMqttFilterSettings() {
  Preferences prefs;
  prefs.begin("mqtt", false);
  prefs.putBool("mon_dapc", server.hasArg("mon_dapc"));
  prefs.putBool("mon_ctrl", server.hasArg("mon_ctrl"));
  prefs.putBool("mon_query", server.hasArg("mon_query"));
  prefs.putBool("mon_resp", server.hasArg("mon_resp"));
  prefs.putBool("mon_comm", server.hasArg("mon_comm"));
  prefs.putBool("mon_self", server.hasArg("mon_self"));
  prefs.putBool("mon_bus", server.hasArg("mon_bus"));
  prefs.end();
}
