#include "base_mqtt.h"
#include "base_config.h"
#include "project_version.h"
#include "project_config.h"
#include "base_web.h"
#include "base_wifi.h"
#include "base_diagnostics.h"
#include <WiFi.h>
#include <Preferences.h>

WiFiClient espClient;
PubSubClient mqttClient(espClient);

String mqtt_broker = "";
uint16_t mqtt_port = 1883;
String mqtt_user = "";
String mqtt_pass = "";
String mqtt_prefix = MQTT_DEFAULT_PREFIX;
String mqtt_client_id = "";
uint8_t mqtt_qos = 0;
bool mqtt_enabled = false;

unsigned long lastReconnectAttempt = 0;
int reconnectDelay = MQTT_INITIAL_RECONNECT_DELAY;

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String topicStr = String(topic);
  String payloadStr = "";
  for (unsigned int i = 0; i < length; i++) {
    payloadStr += (char)payload[i];
  }

#ifdef DEBUG_SERIAL
  Serial.printf("MQTT message received on %s: %s\n", topic, payloadStr.c_str());
#endif

  onMqttMessage(topicStr, payloadStr);
}

void mqttCoreInit() {
  Preferences prefs;
  prefs.begin("mqtt", true);
  mqtt_broker = prefs.getString("broker", "");
  mqtt_port = prefs.getUShort("port", 1883);
  mqtt_user = prefs.getString("user", "");
  mqtt_pass = prefs.getString("pass", "");
  mqtt_prefix = prefs.getString("prefix", MQTT_DEFAULT_PREFIX);
  mqtt_qos = prefs.getUChar("qos", 0);

  uint64_t mac = ESP.getEfuseMac();
  String defaultClientId = String(MQTT_DEFAULT_CLIENT_PREFIX) + "-" + String((uint32_t)(mac & 0xFFFFFF), HEX);
  mqtt_client_id = prefs.getString("client_id", defaultClientId);

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

#if ENABLE_MQTT_FILTER_UI
  loadMqttFilterSettings();
#endif

  server.on("/mqtt", HTTP_GET, handleMQTTConfig);
  server.on("/mqtt", HTTP_POST, handleMQTTSave);
}

void mqttCoreLoop() {
  if (!mqtt_enabled) return;

  if (!mqttClient.connected()) {
    reconnectMQTT();
  } else {
    mqttClient.loop();
  }
}

void reconnectMQTT() {
  if (millis() - lastReconnectAttempt < (unsigned long)reconnectDelay) return;

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
    onMqttConnected();

    reconnectDelay = MQTT_INITIAL_RECONNECT_DELAY;
  } else {
#ifdef DEBUG_SERIAL
    Serial.printf("MQTT connection failed, rc=%d, retrying later\n", mqttClient.state());
#endif

    reconnectDelay = min(reconnectDelay * 2, MQTT_MAX_RECONNECT_DELAY);
  }

  lastReconnectAttempt = millis();
}

void mqttSubscribe(const String& topic) {
  if (!mqtt_enabled || !mqttClient.connected()) return;
  mqttClient.subscribe(topic.c_str(), mqtt_qos);
#ifdef DEBUG_SERIAL
  Serial.printf("MQTT subscribed to: %s\n", topic.c_str());
#endif
}

void mqttPublish(const String& topic, const String& payload, bool retained) {
  if (!mqtt_enabled || !mqttClient.connected()) return;
  mqttClient.publish(topic.c_str(), payload.c_str(), retained);
#ifdef DEBUG_SERIAL
  Serial.printf("MQTT published to %s: %s\n", topic.c_str(), payload.c_str());
#endif
}

void publishStatus() {
  if (!mqtt_enabled || !mqttClient.connected()) return;

  String topic = mqtt_prefix + "status";
  String payload = getStatusJSON();

  mqttClient.publish(topic.c_str(), payload.c_str(), true);
}

void publishDiagnostics() {
  if (!mqtt_enabled || !mqttClient.connected()) return;
  if (!diagnostics_mqtt_enabled) return;

  String topic = mqtt_prefix + "diagnostics";
  String payload = getDiagnosticsJSON();

  mqttClient.publish(topic.c_str(), payload.c_str(), false);
}

String getStatusJSON() {
  String json = "{";
  json += "\"status\":\"online\",";
  json += "\"project\":\"" + String(PROJECT_NAME) + "\",";
  json += "\"version\":\"" + String(FIRMWARE_VERSION) + "\",";
  json += "\"uptime\":" + String(millis() / 1000) + ",";
  json += "\"ip\":\"" + (wifiConnected ? WiFi.localIP().toString() : WiFi.softAPIP().toString()) + "\"";
  json += "}";
  return json;
}

String getMqttStatusHTML() {
  bool mqttConfigured = (mqtt_broker.length() > 0);
  bool mqttConnected = mqttConfigured && mqttClient.connected();

  String html = "<div class=\"status\">";
  html += "<div class=\"dot " + String(mqttConnected ? "green" : (mqttConfigured ? "yellow" : "red")) + "\"></div>";
  html += "<span>MQTT: " + String(mqttConnected ? "Connected" : (mqttConfigured ? "Configured (not connected)" : "Not configured")) + "</span>";
  html += "</div>";

  if (mqttConnected) {
    html += "<div style=\"margin-top:20px;padding:16px;background:var(--bg-secondary);border-radius:8px;\">";
    html += "<h3 style=\"margin:0 0 12px 0;font-size:16px;cursor:pointer;\" onclick=\"document.getElementById('mqtt-topics').style.display=document.getElementById('mqtt-topics').style.display==='none'?'block':'none'\">📡 MQTT Topics ▼</h3>";
    html += "<div id=\"mqtt-topics\" style=\"display:none;font-size:13px;\">";
    html += getMqttTopicsHTML();
    html += "</div></div>";
  }

  return html;
}

String getMqttStatusJSON() {
  bool mqttConfigured = (mqtt_broker.length() > 0);
  bool mqttConnected = mqttConfigured && mqttClient.connected();

  String json = "{";
  json += "\"configured\":" + String(mqttConfigured ? "true" : "false") + ",";
  json += "\"connected\":" + String(mqttConnected ? "true" : "false") + ",";
  json += "\"broker\":\"" + mqtt_broker + "\",";
  json += "\"port\":" + String(mqtt_port) + ",";
  json += "\"prefix\":\"" + mqtt_prefix + "\"";
  json += "}";
  return json;
}

void handleMQTTConfig() {
  if (!checkAuth()) return;

  Preferences prefs;
  prefs.begin("mqtt", true);
  String broker = prefs.getString("broker", "");
  uint16_t port = prefs.getUShort("port", 1883);
  String user = prefs.getString("user", "");
  String pass = prefs.getString("pass", "");
  String prefix = prefs.getString("prefix", MQTT_DEFAULT_PREFIX);
  uint8_t qos = prefs.getUChar("qos", 0);

  uint64_t mac = ESP.getEfuseMac();
  String defaultClientId = String(MQTT_DEFAULT_CLIENT_PREFIX) + "-" + String((uint32_t)(mac & 0xFFFFFF), HEX);
  String clientId = prefs.getString("client_id", defaultClientId);

  prefs.end();

  String html = buildHTMLHeader("MQTT Configuration");
  html += "<div class=\"card\"><h1>MQTT Settings</h1>";
  html += "<p class=\"subtitle\">Configure MQTT broker connection</p>";
  html += "<form action=\"/mqtt\" method=\"POST\">";
  html += "<label for=\"broker\">MQTT Broker Host</label>";
  html += "<input type=\"text\" id=\"broker\" name=\"broker\" value=\"" + broker + "\" placeholder=\"mqtt.example.com or 192.168.1.100\">";
  html += "<label for=\"port\">MQTT Port</label>";
  html += "<input type=\"text\" id=\"port\" name=\"port\" value=\"" + String(port) + "\" placeholder=\"1883\">";
  html += "<label for=\"user\">Username (optional)</label>";
  html += "<input type=\"text\" id=\"user\" name=\"user\" value=\"" + user + "\" placeholder=\"Leave empty if not required\">";
  html += "<label for=\"pass\">Password (optional)</label>";
  html += "<div class=\"input-wrapper\">";
  html += "<input type=\"password\" id=\"pass\" name=\"pass\" value=\"" + pass + "\" placeholder=\"Leave empty if not required\">";
  html += "<span class=\"eye-icon\" id=\"pass-eye\" onclick=\"togglePassword('pass')\"></span>";
  html += "</div>";
  html += "<label for=\"prefix\">Topic Prefix</label>";
  html += "<input type=\"text\" id=\"prefix\" name=\"prefix\" value=\"" + prefix + "\" placeholder=\"" + String(MQTT_DEFAULT_PREFIX) + "\">";
  html += "<label for=\"client_id\">Client ID</label>";
  html += "<input type=\"text\" id=\"client_id\" name=\"client_id\" value=\"" + clientId + "\" placeholder=\"" + defaultClientId + "\">";
  html += "<label for=\"qos\">QoS Level</label>";
  html += "<select id=\"qos\" name=\"qos\" style=\"width:100%;padding:12px;border:2px solid var(--border-color);border-radius:8px;font-size:16px;margin-bottom:16px;background:var(--bg-primary);color:var(--text-primary);\">";
  html += "<option value=\"0\"" + String(qos == 0 ? " selected" : "") + ">0 - At most once</option>";
  html += "<option value=\"1\"" + String(qos == 1 ? " selected" : "") + ">1 - At least once</option>";
  html += "<option value=\"2\"" + String(qos == 2 ? " selected" : "") + ">2 - Exactly once</option>";
  html += "</select>";

#if ENABLE_MQTT_FILTER_UI
  html += getMqttFilterConfigHTML();
#endif

  html += "<button type=\"submit\" style=\"margin-top:24px;\">Save MQTT Settings</button>";
  html += "</form></div>";
  html += buildHTMLFooter();
  server.send(200, "text/html", html);
}

void handleMQTTSave() {
  String broker = server.arg("broker");
  uint16_t port = server.arg("port").toInt();
  String user = server.arg("user");
  String pass = server.arg("pass");
  String prefix = server.arg("prefix");
  String clientId = server.arg("client_id");
  uint8_t qos = server.arg("qos").toInt();

  if (port == 0) port = 1883;
  if (prefix.length() == 0) prefix = MQTT_DEFAULT_PREFIX;
  if (!prefix.endsWith("/")) prefix += "/";

  Preferences prefs;
  prefs.begin("mqtt", false);
  prefs.putString("broker", broker);
  prefs.putUShort("port", port);
  prefs.putString("user", user);
  prefs.putString("pass", pass);
  prefs.putString("prefix", prefix);
  prefs.putString("client_id", clientId);
  prefs.putUChar("qos", qos);
  prefs.end();

#if ENABLE_MQTT_FILTER_UI
  saveMqttFilterSettings();
#endif

  String html = buildHTMLHeader("MQTT Settings Saved");
  html += "<div class=\"card\">";
  html += "<h1 style=\"color:var(--accent-green);\">MQTT Settings Saved</h1>";
  html += "<p>MQTT configuration has been updated. The device will now reboot to apply the new settings.</p>";
  html += "<p style=\"margin-top:20px;color:var(--text-secondary);\">Rebooting in <span id=\"countdown\">3</span> seconds...</p>";
  html += "</div>";
  html += "<script>";
  html += "let count=3;";
  html += "const interval=setInterval(function(){";
  html += "count--;";
  html += "document.getElementById('countdown').textContent=count;";
  html += "if(count<=0){clearInterval(interval);fetch('/api/reboot',{method:'POST'});}";
  html += "},1000);";
  html += "</script>";
  html += buildHTMLFooter();
  server.send(200, "text/html", html);
}
