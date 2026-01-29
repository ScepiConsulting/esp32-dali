#ifndef UTILS_H
#define UTILS_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "dali_protocol.h"

String daliMessageToJSON(const DaliMessage& msg);
String daliScanResultToJSON(const DaliScanResult& result);
DaliCommand parseCommandJSON(const String& json);
String bytesToHex(uint8_t* bytes, uint8_t length);
String getStatusJSON();
String commissioningProgressToJSON(const CommissioningProgress& progress);

#endif
