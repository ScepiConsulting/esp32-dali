#include "mqtt_handler.h"
#include "config.h"
#include "ballast_handler.h"
#include "diagnostics.h"
#include "utils.h"
#include <PubSubClient.h>
#include <WiFi.h>
#include <Preferences.h>

#define MQTT_MAX_PACKET_SIZE 512

WiFiClient espClient;
PubSubClient mqttClient(espClient);

String mqtt_broker = "";
uint16_t mqtt_port = 1883;
String mqtt_user = "";
String mqtt_pass = "";
String mqtt_prefix = "home/dali/ballast/";
String mqtt_client_id = "";
uint8_t mqtt_qos = 0;
bool mqtt_enabled = false;

unsigned long lastReconnectAttempt = 0;
int reconnectDelay = 5000;

void mqttInit() {
  Preferences prefs;
  prefs.begin("mqtt", true);
  mqtt_broker = prefs.getString("broker", "");
  mqtt_port = prefs.getUShort("port", 1883);
  mqtt_user = prefs.getString("user", "");
  mqtt_pass = prefs.getString("pass", "");
  mqtt_prefix = prefs.getString("prefix", "home/dali/ballast/");
  mqtt_qos = prefs.getUChar("qos", 0);

  uint64_t mac = ESP.getEfuseMac();
  String defaultClientId = "dali-ballast-" + String((uint32_t)(mac & 0xFFFFFF), HEX);
  mqtt_client_id = prefs.getString("client_id", defaultClientId);

  prefs.end();

  mqttClient.setBufferSize(MQTT_MAX_PACKET_SIZE);

  mqtt_enabled = (mqtt_broker.length() > 0);

  if (mqtt_enabled) {
#ifdef DEBUG_SERIAL
    Serial.printf("MQTT enabled: %s:%d\n", mqtt_broker.c_str(), mqtt_port);
#endif
    mqttClient.setServer(mqtt_broker.c_str(), mqtt_port);
  } else {
#ifdef DEBUG_SERIAL
    Serial.println("MQTT not configured");
#endif
  }
}

void mqttLoop() {
  if (!mqtt_enabled) return;

  if (!mqttClient.connected()) {
    reconnectMQTT();
  } else {
    mqttClient.loop();
  }
}

void reconnectMQTT() {
  if (millis() - lastReconnectAttempt < reconnectDelay) return;

#ifdef DEBUG_SERIAL
  Serial.println("Attempting MQTT connection...");
#endif
  String statusTopic = mqtt_prefix + "status";
  String willMessage = "{\"status\":\"offline\"}";

  bool connected = mqttClient.connect(
    mqtt_client_id.c_str(),
    mqtt_user.c_str(),
    mqtt_pass.c_str(),
    statusTopic.c_str(),
    mqtt_qos,
    true,
    willMessage.c_str()
  );

  if (connected) {
#ifdef DEBUG_SERIAL
    Serial.println("MQTT connected");
#endif

    publishStatus();
    publishBallastConfig();
    publishBallastState();

    reconnectDelay = 5000;
  } else {
#ifdef DEBUG_SERIAL
    Serial.printf("MQTT connection failed, rc=%d, retrying later\n", mqttClient.state());
#endif

    reconnectDelay = min(reconnectDelay * 2, 60000);
  }

  lastReconnectAttempt = millis();
}

void publishBallastState() {
  if (!mqtt_enabled || !mqttClient.connected()) return;

  String topic = mqtt_prefix + "ballast_status";

  String payload = "{";
  payload += "\"timestamp\":" + String(millis()) + ",";
  payload += "\"address\":" + String(ballastState.short_address) + ",";
  payload += "\"actual_level\":" + String(ballastState.actual_level) + ",";
  payload += "\"level_percent\":" + String((ballastState.actual_level / 254.0) * 100.0, 1) + ",";
  payload += "\"target_level\":" + String(ballastState.target_level) + ",";
  payload += "\"fade_running\":" + String(ballastState.fade_running ? "true" : "false") + ",";
  payload += "\"lamp_arc_power_on\":" + String(ballastState.lamp_arc_power_on ? "true" : "false") + ",";
  payload += "\"lamp_failure\":" + String(ballastState.lamp_failure ? "true" : "false");
  payload += "}";

#ifdef DEBUG_SERIAL
  Serial.printf("[MQTT] Publishing state: %s\n", payload.c_str());
#endif

  mqttClient.publish(topic.c_str(), payload.c_str(), false);
}

void publishBallastCommand(const BallastMessage& msg) {
  if (!mqtt_enabled || !mqttClient.connected()) return;

  String topic = mqtt_prefix + "command";

  String payload = "{";
  payload += "\"timestamp\":" + String(msg.timestamp) + ",";
  payload += "\"source\":\"" + msg.source + "\",";
  payload += "\"command_type\":\"" + msg.command_type + "\",";
  payload += "\"address\":" + String(msg.address) + ",";

  if (msg.is_query_response) {
    payload += "\"is_query_response\":true,";
    payload += "\"response\":\"0x" + String(msg.response_byte, HEX) + "\",";
  }

  if (msg.value > 0 || msg.command_type == "set_brightness") {
    payload += "\"value\":" + String(msg.value) + ",";
    if (msg.value_percent > 0) {
      payload += "\"value_percent\":" + String(msg.value_percent, 1) + ",";
    }
  }

  payload += "\"raw\":\"";
  payload += String(msg.raw_bytes[0], HEX);
  if (msg.raw_bytes[0] < 0x10) payload = payload.substring(0, payload.length() - 1) + "0" + payload.substring(payload.length() - 1);
  payload += String(msg.raw_bytes[1], HEX);
  if (msg.raw_bytes[1] < 0x10) payload = payload.substring(0, payload.length() - 1) + "0" + payload.substring(payload.length() - 1);
  payload += "\",";

  payload += "\"description\":\"" + msg.description + "\"";
  payload += "}";

  mqttClient.publish(topic.c_str(), payload.c_str(), false);
}

void publishBallastConfig() {
  if (!mqtt_enabled || !mqttClient.connected()) return;

  String topic = mqtt_prefix + "config";

  String payload = "{";
  payload += "\"short_address\":" + String(ballastState.short_address) + ",";
  payload += "\"min_level\":" + String(ballastState.min_level) + ",";
  payload += "\"max_level\":" + String(ballastState.max_level) + ",";
  payload += "\"power_on_level\":" + String(ballastState.power_on_level) + ",";
  payload += "\"fade_time\":" + String(ballastState.fade_time) + ",";
  payload += "\"fade_rate\":" + String(ballastState.fade_rate);
  payload += "}";

  mqttClient.publish(topic.c_str(), payload.c_str(), true);
}

void publishStatus() {
  if (!mqtt_enabled || !mqttClient.connected()) return;

  String topic = mqtt_prefix + "status";
  String payload = getStatusJSON();

  mqttClient.publish(topic.c_str(), payload.c_str(), true);
}

void publishDiagnostics() {
  if (!mqtt_enabled || !mqttClient.connected()) return;
  if (!diagnostics_enabled) return;

  String topic = mqtt_prefix + "diagnostics";
  String payload = getDiagnosticsJSON();

  mqttClient.publish(topic.c_str(), payload.c_str(), false);
}
