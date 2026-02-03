#ifndef MQTT_EXTENSIONS_H
#define MQTT_EXTENSIONS_H

#include <Arduino.h>
#include "project_ballast_state.h"

void onMqttConnected();
void onMqttMessage(const String& topic, const String& payload);
void publishBallastState();
void publishBallastCommand(const BallastMessage& msg);
void publishBallastConfig();

#endif
