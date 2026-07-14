#include "base_api.h"
#include "base_i18n.h"
#include "project_function.h"
#include "project_config.h"
#include "project_ballast_handler.h"
#include "base_mqtt.h"

std::vector<DiagnosticSection> appDiagnosticSections() {
  std::vector<DiagnosticSection> sections;

  DiagnosticSection ballastSection;
  ballastSection.title = tr("Előtét diagnosztika", "Ballast Diagnostics");
  ballastSection.items.push_back({tr("Rövid cím", "Short Address"), String(ballastState.short_address)});
  ballastSection.items.push_back({tr("Eszköztípus", "Device Type"), "DT" + String(ballastState.device_type)});
  ballastSection.items.push_back({tr("Aktuális szint", "Actual Level"), String(ballastState.actual_level) + " (" + String((ballastState.actual_level / 254.0) * 100.0, 1) + "%)"});
  ballastSection.items.push_back({tr("Lámpa állapota", "Lamp Status"), ballastState.lamp_arc_power_on ? tr("BE", "ON") : tr("KI", "OFF")});
  ballastSection.items.push_back({tr("Fényváltás fut", "Fade Running"), ballastState.fade_running ? tr("Igen", "Yes") : tr("Nem", "No")});
  ballastSection.items.push_back({tr("Busz üresjáratban", "Bus Idle"), busIsIdle ? tr("Igen", "Yes") : tr("Nem", "No")});
  sections.push_back(ballastSection);

  DiagnosticSection mqttSection;
  mqttSection.title = tr("MQTT diagnosztika", "MQTT Diagnostics");
  mqttSection.items.push_back({tr("MQTT engedélyezve", "MQTT Enabled"), mqtt_enabled ? tr("Igen", "Yes") : tr("Nem", "No")});
  mqttSection.items.push_back({tr("MQTT kapcsolódva", "MQTT Connected"), mqttClient.connected() ? tr("Igen", "Yes") : tr("Nem", "No")});
  mqttSection.items.push_back({tr("MQTT bróker", "MQTT Broker"), mqtt_broker});
  mqttSection.items.push_back({tr("MQTT előtag", "MQTT Prefix"), mqtt_prefix});
  sections.push_back(mqttSection);

  return sections;
}

String appDiagnosticsJSON() {
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
