#include "project_diagnostics.h"
#include "project_function.h"
#include "project_config.h"
#include "project_dali_handler.h"

String getFunctionDiagnosticsHTML() {
  String html = "";

  html += "<div style=\"margin-top:20px;padding-top:20px;border-top:1px solid var(--border-color);\">";
  html += "<h2>DALI Diagnostics</h2>";

  uint8_t queueSize = (queueTail >= queueHead) ? (queueTail - queueHead) : (COMMAND_QUEUE_SIZE - queueHead + queueTail);

  html += "<div style=\"display:grid;grid-template-columns:1fr 1fr;gap:8px;font-size:14px;\">";
  html += "<div style=\"color:var(--text-secondary);\">Bus State:</div>";
  html += "<div>" + String(busIsIdle ? "Idle" : "Active") + "</div>";
  html += "<div style=\"color:var(--text-secondary);\">Command Queue:</div>";
  html += "<div>" + String(queueSize) + " / " + String(COMMAND_QUEUE_SIZE) + "</div>";
  html += "<div style=\"color:var(--text-secondary);\">Passive Devices:</div>";
  html += "<div>" + String(getPassiveDeviceCount()) + "</div>";
  html += "<div style=\"color:var(--text-secondary);\">Last Activity:</div>";
  html += "<div>" + String((millis() - lastBusActivityTime) / 1000) + "s ago</div>";
  html += "</div>";

  html += "</div>";

  return html;
}

String getFunctionDiagnosticsJSON() {
  uint8_t queueSize = (queueTail >= queueHead) ? (queueTail - queueHead) : (COMMAND_QUEUE_SIZE - queueHead + queueTail);

  String json = "";
  json += "\"bus_idle\":" + String(busIsIdle ? "true" : "false") + ",";
  json += "\"queue_size\":" + String(queueSize) + ",";
  json += "\"passive_devices\":" + String(getPassiveDeviceCount()) + ",";
  json += "\"last_activity_ms\":" + String(millis() - lastBusActivityTime);
  return json;
}
