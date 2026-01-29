#include "mqtt_handler.h"
#include "config.h"
#include "dali_handler.h"
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
String mqtt_prefix = "home/dali/";
String mqtt_client_id = "";
uint8_t mqtt_qos = 0;
bool mqtt_enabled = false;

MonitorFilter monitorFilter = {
  .enable_dapc = true,
  .enable_control_cmds = true,
  .enable_queries = true,
  .enable_responses = true,
  .enable_commissioning = true,
  .enable_self_sent = true,
  .enable_bus_traffic = true
};

unsigned long lastReconnectAttempt = 0;
int reconnectDelay = 5000;

void mqttInit() {
  Preferences prefs;
  prefs.begin("mqtt", true);
  mqtt_broker = prefs.getString("broker", "");
  mqtt_port = prefs.getUShort("port", 1883);
  mqtt_user = prefs.getString("user", "");
  mqtt_pass = prefs.getString("pass", "");
  mqtt_prefix = prefs.getString("prefix", "home/dali/");
  mqtt_qos = prefs.getUChar("qos", 0);

  uint64_t mac = ESP.getEfuseMac();
  String defaultClientId = "dali-bridge-" + String((uint32_t)(mac & 0xFFFFFF), HEX);
  mqtt_client_id = prefs.getString("client_id", defaultClientId);

  // Load monitor filter settings
  monitorFilter.enable_dapc = prefs.getBool("mon_dapc", true);
  monitorFilter.enable_control_cmds = prefs.getBool("mon_ctrl", true);
  monitorFilter.enable_queries = prefs.getBool("mon_query", true);
  monitorFilter.enable_responses = prefs.getBool("mon_resp", true);
  monitorFilter.enable_commissioning = prefs.getBool("mon_comm", true);
  monitorFilter.enable_self_sent = prefs.getBool("mon_self", true);
  monitorFilter.enable_bus_traffic = prefs.getBool("mon_bus", true);

  prefs.end();

  mqttClient.setBufferSize(MQTT_MAX_PACKET_SIZE);

  mqtt_enabled = (mqtt_broker.length() > 0);

  if (mqtt_enabled) {
#ifdef DEBUG_SERIAL
    Serial.printf("MQTT enabled: %s:%d\n", mqtt_broker.c_str(), mqtt_port);
#endif
    mqttClient.setServer(mqtt_broker.c_str(), mqtt_port);
    mqttClient.setCallback(mqttCallback);
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

    String commandTopic = mqtt_prefix + "command";
    String scanTriggerTopic = mqtt_prefix + "scan/trigger";
    String commissionTriggerTopic = mqtt_prefix + "commission/trigger";

    mqttClient.subscribe(commandTopic.c_str(), mqtt_qos);
    mqttClient.subscribe(scanTriggerTopic.c_str(), mqtt_qos);
    mqttClient.subscribe(commissionTriggerTopic.c_str(), mqtt_qos);

    publishStatus();

    reconnectDelay = 5000;
  } else {
#ifdef DEBUG_SERIAL
    Serial.printf("MQTT connection failed, rc=%d, retrying later\n", mqttClient.state());
#endif

    reconnectDelay = min(reconnectDelay * 2, 60000);
  }

  lastReconnectAttempt = millis();
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String topicStr = String(topic);
  String payloadStr = "";
  for (unsigned int i = 0; i < length; i++) {
    payloadStr += (char)payload[i];
  }

#ifdef DEBUG_SERIAL
  Serial.printf("MQTT message received on %s: %s\n", topic, payloadStr.c_str());
#endif

  if (topicStr == mqtt_prefix + "command") {
    DaliCommand cmd = parseCommandJSON(payloadStr);

    if (cmd.command_type.length() > 0) {
      bool valid = validateDaliCommand(cmd);

      if (valid || cmd.force) {
        if (enqueueDaliCommand(cmd)) {
#ifdef DEBUG_SERIAL
          Serial.println("[MQTT] Command enqueued");
#endif
        } else {
#ifdef DEBUG_SERIAL
          Serial.println("[MQTT] Failed to enqueue command");
#endif
        }
      } else {
#ifdef DEBUG_SERIAL
        Serial.println("[MQTT] Invalid command (use force flag to override)");
#endif
      }
    }
  } else if (topicStr == mqtt_prefix + "scan/trigger") {
#ifdef DEBUG_SERIAL
    Serial.println("[MQTT] Scan triggered");
#endif
    DaliScanResult result = scanDaliDevices();
    publishScanResult(result);
  } else if (topicStr == mqtt_prefix + "commission/trigger") {
#ifdef DEBUG_SERIAL
    Serial.println("[MQTT] Commissioning triggered");
#endif
    
    uint8_t start_address = 0;
    if (payloadStr.length() > 0) {
      start_address = payloadStr.toInt();
      if (start_address > 63) start_address = 0;
    }
    
#ifdef DEBUG_SERIAL
    Serial.printf("[MQTT] Starting commissioning from address %d\n", start_address);
#endif
    
    commissionDevices(start_address);
  }
}

void publishMonitor(const DaliMessage& msg) {
  if (!mqtt_enabled || !mqttClient.connected()) return;

  // Apply monitor filters
  bool shouldPublish = false;
  
  // Filter by source (self-sent vs bus traffic)
  if (msg.source == "self" && !monitorFilter.enable_self_sent) {
    return;
  }
  if (msg.source == "bus" && !monitorFilter.enable_bus_traffic) {
    return;
  }
  
  // Filter by message type
  String type = msg.parsed.type;
  
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
             type == "searchaddrh" || type == "searchaddrm" || 
             type == "searchaddrl" || type == "program_short_address" || 
             type == "verify_short_address" || type == "query_short_address") {
    shouldPublish = monitorFilter.enable_commissioning;
  } else {
    // Unknown types - publish if any filter is enabled
    shouldPublish = true;
  }
  
  if (!shouldPublish) return;

  String topic = mqtt_prefix + "monitor";
  String payload = daliMessageToJSON(msg);

#ifdef DEBUG_SERIAL
  Serial.printf("[MQTT] Publishing to %s: %s\n", topic.c_str(), msg.description.c_str());
  Serial.printf("[MQTT] Payload: %s\n", payload.c_str());
#endif
  bool result = mqttClient.publish(topic.c_str(), payload.c_str(), false);
#ifdef DEBUG_SERIAL
  Serial.printf("[MQTT] Publish result: %s\n", result ? "SUCCESS" : "FAILED");
#endif
}

void publishStatus() {
  if (!mqtt_enabled || !mqttClient.connected()) return;

  String topic = mqtt_prefix + "status";
  String payload = getStatusJSON();

  mqttClient.publish(topic.c_str(), payload.c_str(), true);
}

void publishScanResult(const DaliScanResult& result) {
  if (!mqtt_enabled || !mqttClient.connected()) return;

  String topic = mqtt_prefix + "scan/result";
  String payload = daliScanResultToJSON(result);

  mqttClient.publish(topic.c_str(), payload.c_str(), false);
}

void publishDiagnostics() {
  if (!mqtt_enabled || !mqttClient.connected()) return;
  if (!diagnostics_enabled) return;

  String topic = mqtt_prefix + "diagnostics";
  String payload = getDiagnosticsJSON();

  mqttClient.publish(topic.c_str(), payload.c_str(), false);
}

void publishCommissioningProgress(const CommissioningProgress& progress) {
  if (!mqtt_enabled || !mqttClient.connected()) return;

  String topic = mqtt_prefix + "commission/progress";
  String payload = commissioningProgressToJSON(progress);

#ifdef DEBUG_SERIAL
  Serial.printf("[MQTT] Publishing commissioning progress: %s\n", progress.status_message.c_str());
#endif

  mqttClient.publish(topic.c_str(), payload.c_str(), false);
}
