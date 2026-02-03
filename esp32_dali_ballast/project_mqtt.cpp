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
