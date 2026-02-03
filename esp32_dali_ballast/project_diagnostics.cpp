#include "project_diagnostics.h"
#include "project_function.h"
#include "project_config.h"
#include "project_ballast_handler.h"
#include "base_mqtt.h"

std::vector<DiagnosticSection> getFunctionDiagnosticSections() {
  std::vector<DiagnosticSection> sections;

  DiagnosticSection ballastSection;
  ballastSection.title = "Ballast Diagnostics";
  ballastSection.items.push_back({"Short Address", String(ballastState.short_address)});
  ballastSection.items.push_back({"Device Type", "DT" + String(ballastState.device_type)});
  ballastSection.items.push_back({"Actual Level", String(ballastState.actual_level) + " (" + String((ballastState.actual_level / 254.0) * 100.0, 1) + "%)"});
  ballastSection.items.push_back({"Lamp Status", ballastState.lamp_arc_power_on ? "ON" : "OFF"});
  ballastSection.items.push_back({"Fade Running", ballastState.fade_running ? "Yes" : "No"});
  ballastSection.items.push_back({"Bus Idle", busIsIdle ? "Yes" : "No"});
  sections.push_back(ballastSection);

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
  String json = "{";
  json += "\"ballast\":{";
  json += "\"short_address\":" + String(ballastState.short_address) + ",";
  json += "\"device_type\":" + String(ballastState.device_type) + ",";
  json += "\"actual_level\":" + String(ballastState.actual_level) + ",";
  json += "\"lamp_on\":" + String(ballastState.lamp_arc_power_on ? "true" : "false") + ",";
  json += "\"fade_running\":" + String(ballastState.fade_running ? "true" : "false") + ",";
  json += "\"bus_idle\":" + String(busIsIdle ? "true" : "false");
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
