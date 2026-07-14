#ifndef MQTT_EXTENSIONS_H
#define MQTT_EXTENSIONS_H

#include <Arduino.h>
#include "project_ballast_state.h"

// The app* MQTT hooks (appMqttConnected/appMqttMessage/appMqttTopicsHTML) are
// declared in base_api.h; this header only carries the ballast's own publishers.
// The appMqttFilter* hooks are NOT implemented here — ENABLE_MQTT_FILTER_UI is 0
// for the ballast, so the base's weak defaults cover them.
void publishBallastState();
void publishBallastCommand(const BallastMessage& msg);
void publishBallastConfig();

#endif
