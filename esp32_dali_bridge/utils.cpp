#include "utils.h"
#include "version.h"
#include "diagnostics.h"

String bytesToHex(uint8_t* bytes, uint8_t length) {
  String hex = "";
  for (uint8_t i = 0; i < length; i++) {
    if (bytes[i] < 0x10) hex += "0";
    hex += String(bytes[i], HEX);
  }
  hex.toUpperCase();
  return hex;
}

String daliMessageToJSON(const DaliMessage& msg) {
  String json = "{";
  json += "\"timestamp\":" + String(msg.timestamp) + ",";
  json += "\"direction\":\"" + String(msg.is_tx ? "tx" : "rx") + "\",";
  json += "\"source\":\"" + msg.source + "\",";
  json += "\"raw\":\"" + bytesToHex((uint8_t*)msg.raw_bytes, msg.length) + "\",";
  if (msg.raw_bits.length() > 0) {
    json += "\"raw_bits\":\"" + msg.raw_bits + "\",";
  }
  json += "\"parsed\":{";
  json += "\"type\":\"" + msg.parsed.type + "\",";
  json += "\"address\":" + String(msg.parsed.address) + ",";
  json += "\"address_type\":\"" + msg.parsed.address_type + "\",";
  json += "\"level\":" + String(msg.parsed.level) + ",";
  json += "\"level_percent\":" + String(msg.parsed.level_percent, 1) + ",";
  json += "\"description\":\"" + msg.description + "\"";
  json += "}}";
  return json;
}

String daliScanResultToJSON(const DaliScanResult& result) {
  String json = "{";
  json += "\"scan_timestamp\":" + String(result.scan_timestamp) + ",";
  json += "\"devices\":[";

  for (size_t i = 0; i < result.devices.size(); i++) {
    if (i > 0) json += ",";
    const DaliDevice& dev = result.devices[i];
    json += "{";
    json += "\"address\":" + String(dev.address) + ",";
    json += "\"type\":\"" + dev.type + "\",";
    json += "\"status\":\"" + dev.status + "\",";
    json += "\"lamp_failure\":" + String(dev.lamp_failure ? "true" : "false") + ",";
    json += "\"min_level\":" + String(dev.min_level) + ",";
    json += "\"max_level\":" + String(dev.max_level);
    json += "}";
  }

  json += "],";
  json += "\"total_found\":" + String(result.total_found);
  json += "}";

  return json;
}

DaliCommand parseCommandJSON(const String& json) {
  DaliCommand cmd;
  cmd.command_type = "";
  cmd.address = 0;
  cmd.address_type = "short";
  cmd.level = 0;
  cmd.scene = 0;
  cmd.group = 0;
  cmd.fade_time = 0;
  cmd.fade_rate = 0;
  cmd.force = false;
  cmd.queued_at = millis();
  cmd.priority = 1;
  cmd.retry_count = 0;

  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, json);

  if (error) {
#ifdef DEBUG_SERIAL
    Serial.print("JSON parse error: ");
    Serial.println(error.c_str());
#endif
    return cmd;
  }

  if (doc.containsKey("command")) {
    cmd.command_type = doc["command"].as<String>();
  }

  if (doc.containsKey("address")) {
    cmd.address = doc["address"].as<uint8_t>();
  }

  if (doc.containsKey("address_type")) {
    cmd.address_type = doc["address_type"].as<String>();
  }

  if (doc.containsKey("level")) {
    cmd.level = doc["level"].as<uint8_t>();
  }

  if (doc.containsKey("level_percent")) {
    float percent = doc["level_percent"].as<float>();
    cmd.level = (uint8_t)((percent / 100.0) * 254.0);
  }

  if (doc.containsKey("scene")) {
    cmd.scene = doc["scene"].as<uint8_t>();
  }

  if (doc.containsKey("group")) {
    cmd.group = doc["group"].as<uint8_t>();
  }

  if (doc.containsKey("fade_time")) {
    cmd.fade_time = doc["fade_time"].as<uint8_t>();
  }

  if (doc.containsKey("fade_rate")) {
    cmd.fade_rate = doc["fade_rate"].as<uint8_t>();
  }

  if (doc.containsKey("force")) {
    cmd.force = doc["force"].as<bool>();
  }

  if (doc.containsKey("priority")) {
    cmd.priority = doc["priority"].as<uint8_t>();
  }

  // DT8 Color control fields
  if (doc.containsKey("color_r")) {
    cmd.color_r = doc["color_r"].as<uint8_t>();
  }

  if (doc.containsKey("color_g")) {
    cmd.color_g = doc["color_g"].as<uint8_t>();
  }

  if (doc.containsKey("color_b")) {
    cmd.color_b = doc["color_b"].as<uint8_t>();
  }

  if (doc.containsKey("color_w")) {
    cmd.color_w = doc["color_w"].as<uint8_t>();
  }

  if (doc.containsKey("color_temp_kelvin")) {
    cmd.color_temp_kelvin = doc["color_temp_kelvin"].as<uint16_t>();
  }

  return cmd;
}

String getStatusJSON() {
  updateDiagnostics();

  String json = "{";
  json += "\"status\":\"online\",";
  json += "\"version\":\"" + String(FIRMWARE_VERSION) + "\",";
  json += "\"uptime\":" + String(stats.uptime_seconds) + ",";
  json += "\"free_heap_kb\":" + String(stats.free_heap_kb, 1);
  json += "}";

  return json;
}

String commissioningProgressToJSON(const CommissioningProgress& progress) {
  String stateStr;
  switch (progress.state) {
    case COMM_IDLE: stateStr = "idle"; break;
    case COMM_INITIALIZING: stateStr = "initializing"; break;
    case COMM_SEARCHING: stateStr = "searching"; break;
    case COMM_PROGRAMMING: stateStr = "programming"; break;
    case COMM_VERIFYING: stateStr = "verifying"; break;
    case COMM_COMPLETE: stateStr = "complete"; break;
    case COMM_ERROR: stateStr = "error"; break;
    default: stateStr = "unknown"; break;
  }

  String json = "{";
  json += "\"state\":\"" + stateStr + "\",";
  json += "\"start_timestamp\":" + String(progress.start_timestamp) + ",";
  json += "\"devices_found\":" + String(progress.devices_found) + ",";
  json += "\"devices_programmed\":" + String(progress.devices_programmed) + ",";
  json += "\"current_address\":" + String(progress.current_address) + ",";
  json += "\"next_free_address\":" + String(progress.next_free_address) + ",";
  json += "\"current_random_address\":\"0x" + String(progress.current_random_address, HEX) + "\",";
  json += "\"status_message\":\"" + progress.status_message + "\",";
  json += "\"progress_percent\":" + String(progress.progress_percent);
  json += "}";

  return json;
}
