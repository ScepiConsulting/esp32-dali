#include "project_home.h"
#include "project_function.h"
#include "project_config.h"
#include "project_ballast_handler.h"
#include "base_diagnostics.h"

String getFunctionHomeHTML() {
  String html = "";

  html += "<div style=\"margin-top:20px;padding-top:20px;border-top:1px solid var(--border-color);\">";
  html += "<h2>Ballast Status</h2>";

  html += "<div class=\"status\">";
  html += "<div class=\"dot " + String(ballastState.lamp_arc_power_on ? "green" : "red") + "\"></div>";
  html += "<span>Lamp: " + String(ballastState.lamp_arc_power_on ? "ON" : "OFF") + "</span>";
  html += "</div>";

  html += "<div class=\"status\">";
  html += "<div class=\"dot " + String(ballastState.short_address < 64 ? "green" : "yellow") + "\"></div>";
  html += "<span>Address: " + String(ballastState.short_address < 64 ? String(ballastState.short_address) : "Unassigned") + "</span>";
  html += "</div>";

  html += "<div style=\"margin-top:12px;display:grid;grid-template-columns:1fr 1fr 1fr;gap:12px;\">";
  html += "<div style=\"padding:12px;background:var(--bg-secondary);border-radius:8px;text-align:center;\">";
  html += "<div style=\"font-size:24px;font-weight:bold;color:var(--accent-green);\">" + String(ballastState.actual_level) + "</div>";
  html += "<div style=\"font-size:12px;color:var(--text-secondary);\">Level</div>";
  html += "</div>";
  html += "<div style=\"padding:12px;background:var(--bg-secondary);border-radius:8px;text-align:center;\">";
  html += "<div style=\"font-size:24px;font-weight:bold;color:var(--accent-purple);\">" + String((ballastState.actual_level / 254.0) * 100.0, 1) + "%</div>";
  html += "<div style=\"font-size:12px;color:var(--text-secondary);\">Brightness</div>";
  html += "</div>";
  html += "<div style=\"padding:12px;background:var(--bg-secondary);border-radius:8px;text-align:center;\">";
  html += "<div style=\"font-size:24px;font-weight:bold;color:var(--accent-green);\">" + String(ballastRxCount) + "</div>";
  html += "<div style=\"font-size:12px;color:var(--text-secondary);\">Commands RX</div>";
  html += "</div>";
  html += "</div>";

  html += "</div>";

  return html;
}
