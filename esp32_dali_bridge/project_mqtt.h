#ifndef MQTT_EXTENSIONS_H
#define MQTT_EXTENSIONS_H

#include <Arduino.h>
#include "project_config.h"
#include "project_dali_protocol.h"

extern MonitorFilter monitorFilter;

void onMqttConnected();
void onMqttMessage(const String& topic, const String& payload);
void publishMonitor(const DaliMessage& msg);
void publishScanResult(const DaliScanResult& result);
void publishCommissioningProgress(const CommissioningProgress& progress);

String getMqttTopicsHTML();
String getMqttFilterConfigHTML();
void loadMqttFilterSettings();
void saveMqttFilterSettings();

#endif
