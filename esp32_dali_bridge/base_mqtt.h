#ifndef MQTT_CORE_H
#define MQTT_CORE_H

#include <Arduino.h>
#include <PubSubClient.h>

extern PubSubClient mqttClient;
extern String mqtt_broker;
extern uint16_t mqtt_port;
extern String mqtt_user;
extern String mqtt_pass;
extern String mqtt_prefix;
extern String mqtt_client_id;
extern uint8_t mqtt_qos;
extern bool mqtt_enabled;

void mqttCoreInit();
void mqttCoreLoop();
void reconnectMQTT();
void publishStatus();
void publishDiagnostics();

String getMqttStatusHTML();
String getMqttStatusJSON();
String getStatusJSON();

void handleMQTTConfig();
void handleMQTTSave();

void mqttSubscribe(const String& topic);
void mqttPublish(const String& topic, const String& payload, bool retained = false);

extern void onMqttConnected();
extern void onMqttMessage(const String& topic, const String& payload);

#endif
