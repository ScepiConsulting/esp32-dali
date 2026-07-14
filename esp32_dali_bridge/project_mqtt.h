#ifndef MQTT_EXTENSIONS_H
#define MQTT_EXTENSIONS_H

#include <Arduino.h>
#include "project_config.h"
#include "project_dali_protocol.h"

// The app* MQTT hooks (appMqttConnected/appMqttMessage/appMqttTopicsHTML and the
// appMqttFilter* trio) are declared in base_api.h; this header only carries the
// bridge's own MQTT publishers.
extern MonitorFilter monitorFilter;

void publishMonitor(const DaliMessage& msg);
void publishScanResult(const DaliScanResult& result);
void publishCommissioningProgress(const CommissioningProgress& progress);

#endif
