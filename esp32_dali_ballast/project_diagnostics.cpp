#include "project_diagnostics.h"
#include "project_function.h"
#include "project_config.h"
#include "project_ballast_handler.h"

String getFunctionDiagnosticsHTML() {
  String html = "";

  html += "<div style=\"margin-top:20px;padding-top:20px;border-top:1px solid var(--border-color);\">";
  html += "<h2>Ballast Diagnostics</h2>";

  html += "<div style=\"display:grid;grid-template-columns:1fr 1fr;gap:8px;font-size:14px;\">";
  html += "<div style=\"color:var(--text-secondary);\">Short Address:</div>";
  html += "<div>" + String(ballastState.short_address) + "</div>";
  html += "<div style=\"color:var(--text-secondary);\">Device Type:</div>";
  html += "<div>DT" + String(ballastState.device_type) + "</div>";
  html += "<div style=\"color:var(--text-secondary);\">Actual Level:</div>";
  html += "<div>" + String(ballastState.actual_level) + " (" + String((ballastState.actual_level / 254.0) * 100.0, 1) + "%)</div>";
  html += "<div style=\"color:var(--text-secondary);\">Lamp Status:</div>";
  html += "<div>" + String(ballastState.lamp_arc_power_on ? "ON" : "OFF") + "</div>";
  html += "<div style=\"color:var(--text-secondary);\">Fade Running:</div>";
  html += "<div>" + String(ballastState.fade_running ? "Yes" : "No") + "</div>";
  html += "<div style=\"color:var(--text-secondary);\">Bus Idle:</div>";
  html += "<div>" + String(busIsIdle ? "Yes" : "No") + "</div>";
  html += "</div>";

  html += "</div>";

  return html;
}

String getFunctionDiagnosticsJSON() {
  String json = "";
  json += "\"short_address\":" + String(ballastState.short_address) + ",";
  json += "\"device_type\":" + String(ballastState.device_type) + ",";
  json += "\"actual_level\":" + String(ballastState.actual_level) + ",";
  json += "\"lamp_on\":" + String(ballastState.lamp_arc_power_on ? "true" : "false") + ",";
  json += "\"fade_running\":" + String(ballastState.fade_running ? "true" : "false") + ",";
  json += "\"bus_idle\":" + String(busIsIdle ? "true" : "false");
  return json;
}
