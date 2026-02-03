#include "project_mqtt.h"
#include "project_config.h"
#include "project_function.h"
#include "base_mqtt.h"
#include "project_ballast_handler.h"
#include "project_version.h"
#include "base_diagnostics.h"

void onMqttConnected() {
#ifdef DEBUG_SERIAL
  Serial.println("[MQTT] Connected - publishing ballast state");
#endif

  publishBallastState();
  publishBallastConfig();
}

void onMqttMessage(const String& topic, const String& payload) {
#ifdef DEBUG_SERIAL
  Serial.printf("[MQTT] Message on %s: %s\n", topic.c_str(), payload.c_str());
#endif
}

void publishBallastState() {
  if (!mqtt_enabled || !mqttClient.connected()) return;

  String topic = mqtt_prefix + "state";
  String json = "{";
  json += "\"timestamp\":" + String(millis()) + ",";
  json += "\"address\":" + String(ballastState.short_address) + ",";
  json += "\"actual_level\":" + String(ballastState.actual_level) + ",";
  json += "\"level_percent\":" + String((ballastState.actual_level / 254.0) * 100.0, 1) + ",";
  json += "\"target_level\":" + String(ballastState.target_level) + ",";
  json += "\"fade_running\":" + String(ballastState.fade_running ? "true" : "false") + ",";
  json += "\"lamp_arc_power_on\":" + String(ballastState.lamp_arc_power_on ? "true" : "false") + ",";
  json += "\"lamp_failure\":" + String(ballastState.lamp_failure ? "true" : "false");
  json += "}";

  mqttPublish(topic, json, false);
}

void publishBallastCommand(const BallastMessage& msg) {
  if (!mqtt_enabled || !mqttClient.connected()) return;

  String topic = mqtt_prefix + "command";
  String json = "{";
  json += "\"timestamp\":" + String(msg.timestamp) + ",";
  json += "\"source\":\"" + msg.source + "\",";
  json += "\"command_type\":\"" + msg.command_type + "\",";
  json += "\"address\":" + String(msg.address) + ",";

  if (msg.is_query_response) {
    json += "\"is_query_response\":true,";
    json += "\"response\":\"0x" + String(msg.response_byte, HEX) + "\",";
  }

  if (msg.value > 0 || msg.command_type == "set_brightness") {
    json += "\"value\":" + String(msg.value) + ",";
    if (msg.value_percent > 0) {
      json += "\"value_percent\":" + String(msg.value_percent, 1) + ",";
    }
  }

  json += "\"raw\":\"";
  if (msg.raw_bytes[0] < 0x10) json += "0";
  json += String(msg.raw_bytes[0], HEX);
  if (msg.raw_bytes[1] < 0x10) json += "0";
  json += String(msg.raw_bytes[1], HEX);
  json += "\",";
  json += "\"description\":\"" + msg.description + "\"";
  json += "}";

  mqttPublish(topic, json, false);
}

void publishBallastConfig() {
  if (!mqtt_enabled || !mqttClient.connected()) return;

  String topic = mqtt_prefix + "config";
  String json = "{";
  json += "\"short_address\":" + String(ballastState.short_address) + ",";
  json += "\"device_type\":" + String(ballastState.device_type) + ",";
  json += "\"min_level\":" + String(ballastState.min_level) + ",";
  json += "\"max_level\":" + String(ballastState.max_level) + ",";
  json += "\"power_on_level\":" + String(ballastState.power_on_level) + ",";
  json += "\"fade_time\":" + String(ballastState.fade_time) + ",";
  json += "\"fade_rate\":" + String(ballastState.fade_rate);
  json += "}";

  mqttPublish(topic, json, true);
}

String getMqttTopicsHTML() {
  String html = "";

  html += "<p style=\"margin:12px 0 4px 0;color:var(--text-secondary);\"><strong>Ballast Status:</strong> <code style=\"background:var(--bg-primary);padding:2px 6px;border-radius:3px;font-family:monospace;\">" + mqtt_prefix + "ballast_status</code></p>";
  html += "<p style=\"margin:0 0 8px 0;font-size:12px;color:var(--text-secondary);\">Published when ballast state changes (level, fade, etc.)</p>";
  html += "<details style=\"margin:8px 0;padding:8px;background:var(--bg-primary);border-radius:4px;\">";
  html += "<summary style=\"cursor:pointer;color:var(--accent-green);font-weight:500;\">▸ Example: Ballast Status</summary>";
  html += "<pre style=\"background:var(--bg-secondary);padding:12px;border-radius:6px;overflow-x:auto;margin:8px 0;font-size:12px;\">{<br>  \"timestamp\": 1234567890,<br>  \"address\": 0,<br>  \"actual_level\": 128,<br>  \"level_percent\": 50.4,<br>  \"target_level\": 128,<br>  \"fade_running\": false,<br>  \"lamp_arc_power_on\": true,<br>  \"lamp_failure\": false<br>}</pre></details>";

  html += "<p style=\"margin:12px 0 4px 0;color:var(--text-secondary);\"><strong>Commands Received:</strong> <code style=\"background:var(--bg-primary);padding:2px 6px;border-radius:3px;font-family:monospace;\">" + mqtt_prefix + "command</code></p>";
  html += "<p style=\"margin:0 0 8px 0;font-size:12px;color:var(--text-secondary);\">Published when ballast receives DALI commands from the bus</p>";
  html += "<details style=\"margin:8px 0;padding:8px;background:var(--bg-primary);border-radius:4px;\">";
  html += "<summary style=\"cursor:pointer;color:var(--accent-green);font-weight:500;\">▸ Example: Set Level Command</summary>";
  html += "<pre style=\"background:var(--bg-secondary);padding:12px;border-radius:6px;overflow-x:auto;margin:8px 0;font-size:12px;\">{<br>  \"timestamp\": 1234567890,<br>  \"source\": \"bus\",<br>  \"command_type\": \"set_brightness\",<br>  \"address\": 0,<br>  \"value\": 128,<br>  \"value_percent\": 50.4,<br>  \"raw\": \"0180\",<br>  \"description\": \"Set to 128 (50.4%)\"<br>}</pre></details>";
  html += "<details style=\"margin:8px 0;padding:8px;background:var(--bg-primary);border-radius:4px;\">";
  html += "<summary style=\"cursor:pointer;color:var(--accent-green);font-weight:500;\">▸ Example: Query Response</summary>";
  html += "<pre style=\"background:var(--bg-secondary);padding:12px;border-radius:6px;overflow-x:auto;margin:8px 0;font-size:12px;\">{<br>  \"timestamp\": 1234567891,<br>  \"source\": \"bus\",<br>  \"command_type\": \"query_actual_level\",<br>  \"address\": 0,<br>  \"is_query_response\": true,<br>  \"response\": \"0x80\",<br>  \"value\": 128,<br>  \"value_percent\": 50.4,<br>  \"raw\": \"01A0\",<br>  \"description\": \"Query actual level: 128 (50.4%)\"<br>}</pre></details>";

  html += "<p style=\"margin:12px 0 4px 0;color:var(--text-secondary);\"><strong>Configuration:</strong> <code style=\"background:var(--bg-primary);padding:2px 6px;border-radius:3px;font-family:monospace;\">" + mqtt_prefix + "config</code></p>";
  html += "<p style=\"margin:0 0 8px 0;font-size:12px;color:var(--text-secondary);\">Ballast configuration published on connect and when changed</p>";
  html += "<details style=\"margin:8px 0;padding:8px;background:var(--bg-primary);border-radius:4px;\">";
  html += "<summary style=\"cursor:pointer;color:var(--accent-green);font-weight:500;\">▸ Example: Configuration</summary>";
  html += "<pre style=\"background:var(--bg-secondary);padding:12px;border-radius:6px;overflow-x:auto;margin:8px 0;font-size:12px;\">{<br>  \"address\": 0,<br>  \"min_level\": 1,<br>  \"max_level\": 254,<br>  \"power_on_level\": 254,<br>  \"system_failure_level\": 254,<br>  \"fade_time\": 0,<br>  \"fade_rate\": 7<br>}</pre></details>";

  html += "<p style=\"margin:12px 0 4px 0;color:var(--text-secondary);\"><strong>Device Status:</strong> <code style=\"background:var(--bg-primary);padding:2px 6px;border-radius:3px;font-family:monospace;\">" + mqtt_prefix + "status</code></p>";
  html += "<p style=\"margin:0 0 8px 0;font-size:12px;color:var(--text-secondary);\">Device online/offline status with uptime and IP address</p>";
  html += "<details style=\"margin:8px 0;padding:8px;background:var(--bg-primary);border-radius:4px;\">";
  html += "<summary style=\"cursor:pointer;color:var(--accent-green);font-weight:500;\">▸ Example: Device Status</summary>";
  html += "<pre style=\"background:var(--bg-secondary);padding:12px;border-radius:6px;overflow-x:auto;margin:8px 0;font-size:12px;\">{<br>  \"status\": \"online\",<br>  \"uptime\": 12345,<br>  \"ip\": \"192.168.1.100\",<br>  \"client_id\": \"dali-ballast-AABBCC\"<br>}</pre></details>";

  html += "<p style=\"margin:12px 0 4px 0;color:var(--text-secondary);\"><strong>Diagnostics:</strong> <code style=\"background:var(--bg-primary);padding:2px 6px;border-radius:3px;font-family:monospace;\">" + mqtt_prefix + "diagnostics</code></p>";
  html += "<p style=\"margin:0 0 8px 0;font-size:12px;color:var(--text-secondary);\">Published periodically with message counters and error counts</p>";
  html += "<details style=\"margin:8px 0;padding:8px;background:var(--bg-primary);border-radius:4px;\">";
  html += "<summary style=\"cursor:pointer;color:var(--accent-green);font-weight:500;\">▸ Example: Diagnostics</summary>";
  html += "<pre style=\"background:var(--bg-secondary);padding:12px;border-radius:6px;overflow-x:auto;margin:8px 0;font-size:12px;\">{<br>  \"rx_count\": 42,<br>  \"tx_count\": 15,<br>  \"error_count\": 0,<br>  \"uptime\": 12345<br>}</pre></details>";

  return html;
}

String getMqttFilterConfigHTML() {
  return "";
}

void loadMqttFilterSettings() {
}

void saveMqttFilterSettings() {
}
