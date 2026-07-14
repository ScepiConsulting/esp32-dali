#include "base_api.h"
#include "base_i18n.h"
#include "project_function.h"
#include "project_config.h"
#include "project_dali_handler.h"
#include "base_mqtt.h"

std::vector<DiagnosticSection> appDiagnosticSections() {
  std::vector<DiagnosticSection> sections;

  uint8_t queueSize = (queueTail >= queueHead) ? (queueTail - queueHead) : (COMMAND_QUEUE_SIZE - queueHead + queueTail);

  DiagnosticSection daliSection;
  daliSection.title = tr("DALI diagnosztika", "DALI Diagnostics");
  daliSection.items.push_back({tr("Busz állapot", "Bus State"), busIsIdle ? tr("Üresjárat", "Idle") : tr("Aktív", "Active")});
  daliSection.items.push_back({tr("Parancssor", "Command Queue"), String(queueSize) + " / " + String(COMMAND_QUEUE_SIZE)});
  daliSection.items.push_back({tr("Passzív eszközök", "Passive Devices"), String(getPassiveDeviceCount())});
  daliSection.items.push_back({tr("Utolsó aktivitás", "Last Activity"), String((millis() - lastBusActivityTime) / 1000) + tr(" mp-e", "s ago")});
  sections.push_back(daliSection);

  DiagnosticSection mqttSection;
  mqttSection.title = tr("MQTT diagnosztika", "MQTT Diagnostics");
  mqttSection.items.push_back({tr("MQTT engedélyezve", "MQTT Enabled"), mqtt_enabled ? tr("Igen", "Yes") : tr("Nem", "No")});
  mqttSection.items.push_back({tr("MQTT kapcsolódva", "MQTT Connected"), mqttClient.connected() ? tr("Igen", "Yes") : tr("Nem", "No")});
  mqttSection.items.push_back({tr("MQTT broker", "MQTT Broker"), mqtt_broker});
  mqttSection.items.push_back({tr("MQTT előtag", "MQTT Prefix"), mqtt_prefix});
  sections.push_back(mqttSection);

  return sections;
}

String appDiagnosticsJSON() {
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
