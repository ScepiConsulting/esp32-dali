#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include <Arduino.h>
#include <PubSubClient.h>
#include "ballast_state.h"

extern PubSubClient mqttClient;
extern String mqtt_broker;
extern uint16_t mqtt_port;
extern String mqtt_user;
extern String mqtt_pass;
extern String mqtt_prefix;
extern String mqtt_client_id;
extern uint8_t mqtt_qos;
extern bool mqtt_enabled;

void mqttInit();
void mqttLoop();
void reconnectMQTT();
void publishBallastState();
void publishBallastCommand(const BallastMessage& msg);
void publishBallastConfig();
void publishStatus();
void publishDiagnostics();

#endif
