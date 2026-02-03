#include "project_diagnostics.h"
#include "project_function.h"
#include "project_config.h"
#include "project_dali_handler.h"
#include "base_mqtt.h"

std::vector<DiagnosticSection> getFunctionDiagnosticSections() {
  std::vector<DiagnosticSection> sections;

  uint8_t queueSize = (queueTail >= queueHead) ? (queueTail - queueHead) : (COMMAND_QUEUE_SIZE - queueHead + queueTail);

  DiagnosticSection daliSection;
  daliSection.title = "DALI Diagnostics";
  daliSection.items.push_back({"Bus State", busIsIdle ? "Idle" : "Active"});
  daliSection.items.push_back({"Command Queue", String(queueSize) + " / " + String(COMMAND_QUEUE_SIZE)});
  daliSection.items.push_back({"Passive Devices", String(getPassiveDeviceCount())});
  daliSection.items.push_back({"Last Activity", String((millis() - lastBusActivityTime) / 1000) + "s ago"});
  sections.push_back(daliSection);

  DiagnosticSection mqttSection;
  mqttSection.title = "MQTT Diagnostics";
  mqttSection.items.push_back({"MQTT Enabled", mqtt_enabled ? "Yes" : "No"});
  mqttSection.items.push_back({"MQTT Connected", mqttClient.connected() ? "Yes" : "No"});
  mqttSection.items.push_back({"MQTT Broker", mqtt_broker});
  mqttSection.items.push_back({"MQTT Prefix", mqtt_prefix});
  sections.push_back(mqttSection);

  return sections;
}

String getFunctionDiagnosticsJSON() {
  uint8_t queueSize = (queueTail >= queueHead) ? (queueTail - queueHead) : (COMMAND_QUEUE_SIZE - queueHead + queueTail);

  String json = "{";
  json += "\"dali\":{";
  json += "\"bus_idle\":" + String(busIsIdle ? "true" : "false") + ",";
  json += "\"queue_size\":" + String(queueSize) + ",";
  json += "\"passive_devices\":" + String(getPassiveDeviceCount()) + ",";
  json += "\"last_activity_ms\":" + String(millis() - lastBusActivityTime);
  json += "},";
  json += "\"mqtt\":{";
  json += "\"enabled\":" + String(mqtt_enabled ? "true" : "false") + ",";
  json += "\"connected\":" + String(mqttClient.connected() ? "true" : "false") + ",";
  json += "\"broker\":\"" + mqtt_broker + "\",";
  json += "\"prefix\":\"" + mqtt_prefix + "\"";
  json += "}";
  json += "}";
  return json;
}
