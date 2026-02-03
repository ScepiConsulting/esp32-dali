#include "base_diagnostics.h"
#include "base_config.h"
#include "project_version.h"
#include "project_config.h"
#include "base_web.h"
#include <Preferences.h>
#include <SPIFFS.h>

CoreDiagnostics coreStats;
bool diagnostics_mqtt_enabled = false;

void diagnosticsCoreInit() {
  coreStats.uptime_seconds = 0;
  coreStats.free_heap_kb = 0;
  coreStats.chip_model = ESP.getChipModel();
  coreStats.chip_revision = ESP.getChipRevision();
  coreStats.flash_size = ESP.getFlashChipSize();
  coreStats.sketch_size = ESP.getSketchSize();
  coreStats.free_sketch_space = ESP.getFreeSketchSpace();

  Preferences prefs;
  prefs.begin("settings", true);
  diagnostics_mqtt_enabled = prefs.getBool("diag_en", false);
  prefs.end();

  server.on("/diagnostics", HTTP_GET, handleDiagnosticsPage);
  server.on("/diagnostics", HTTP_POST, handleDiagnosticsSave);
  server.on("/api/diagnostics", handleAPIDiagnostics);
}

void updateCoreDiagnostics() {
  coreStats.uptime_seconds = millis() / 1000;
  coreStats.free_heap_kb = ESP.getFreeHeap() / 1024.0;
}

String getDiagnosticsJSON() {
  updateCoreDiagnostics();

  String json = "{";
  json += "\"core\":{";
  json += "\"uptime_seconds\":" + String(coreStats.uptime_seconds) + ",";
  json += "\"free_heap_kb\":" + String(coreStats.free_heap_kb, 1) + ",";
  json += "\"chip_model\":\"" + coreStats.chip_model + "\",";
  json += "\"chip_revision\":" + String(coreStats.chip_revision) + ",";
  json += "\"flash_size\":" + String(coreStats.flash_size) + ",";
  json += "\"sketch_size\":" + String(coreStats.sketch_size) + ",";
  json += "\"free_sketch_space\":" + String(coreStats.free_sketch_space);
  json += "},";
  json += "\"function\":" + getFunctionDiagnosticsJSON();
  json += "}";

  return json;
}

String getCoreDiagnosticsHTML() {
  updateCoreDiagnostics();

  String html = "<div class=\"status-grid\">";

  html += "<div class=\"status-item\">";
  html += "<div class=\"status-label\">Firmware Version</div>";
  html += "<div class=\"status-value\">" + String(FIRMWARE_VERSION) + "</div>";
  html += "</div>";

  html += "<div class=\"status-item\">";
  html += "<div class=\"status-label\">Uptime</div>";
  html += "<div class=\"status-value\">";
  unsigned long secs = coreStats.uptime_seconds;
  unsigned long mins = secs / 60;
  unsigned long hours = mins / 60;
  unsigned long days = hours / 24;
  secs %= 60; mins %= 60; hours %= 24;
  if (days > 0) html += String(days) + "d ";
  html += String(hours) + "h " + String(mins) + "m";
  html += "</div></div>";

  html += "<div class=\"status-item\">";
  html += "<div class=\"status-label\">Free Heap</div>";
  html += "<div class=\"status-value\">" + String(coreStats.free_heap_kb, 1) + " KB</div>";
  html += "</div>";

  html += "<div class=\"status-item\">";
  html += "<div class=\"status-label\">Chip Model</div>";
  html += "<div class=\"status-value\">" + coreStats.chip_model + "</div>";
  html += "</div>";

  html += "</div>";
  return html;
}

void handleDiagnosticsPage() {
  if (!checkAuth()) return;

  updateCoreDiagnostics();

  String html = buildHTMLHeader("Diagnostics");

  html += "<div class=\"card\"><h1>System Information</h1>";
  html += getCoreDiagnosticsHTML();
  html += "</div>";

  html += "<div class=\"card\" style=\"margin-top:20px;\"><h1>Diagnostics</h1>";
  html += "<p class=\"subtitle\">Enable or disable diagnostics export via MQTT</p>";
  html += "<form action=\"/diagnostics\" method=\"POST\">";
  html += "<div style=\"display:flex;align-items:center;gap:12px;margin-bottom:16px;\">";
  html += "<input type=\"checkbox\" id=\"diag_en\" name=\"diag_en\" value=\"1\"" + String(diagnostics_mqtt_enabled ? " checked" : "") + " style=\"width:auto;margin:0;\">";
  html += "<label for=\"diag_en\" style=\"margin:0;\">Enable diagnostics publishing to MQTT</label>";
  html += "</div>";
  html += "<button type=\"submit\">Save Settings</button>";
  html += "</form>";

  std::vector<DiagnosticSection> sections = getFunctionDiagnosticSections();
  for (const auto& section : sections) {
    html += "<div style=\"margin-top:20px;padding-top:20px;border-top:1px solid var(--border-color);\">";
    if (section.title.length() > 0) {
      html += "<h2>" + section.title + "</h2>";
    }
    html += "<div style=\"display:grid;grid-template-columns:1fr 1fr;gap:8px;font-size:14px;\">";
    for (const auto& item : section.items) {
      html += "<div style=\"color:var(--text-secondary);\">" + item.label + ":</div>";
      html += "<div>" + item.value + "</div>";
    }
    html += "</div></div>";
  }

  html += "<div style=\"margin-top:20px;\">";
  html += "<button onclick=\"exportDiagnostics()\" style=\"background:var(--accent-green);\">Export Diagnostics JSON</button>";
  html += "</div>";

  html += "<div style=\"margin-top:20px;padding-top:20px;border-top:1px solid var(--border-color);\">";
  html += "<h2>System Actions</h2>";
  html += "<button onclick=\"if(confirm('Are you sure you want to reboot the device?')){fetch('/api/reboot',{method:'POST'}).then(()=>{alert('Device is rebooting...');setTimeout(()=>window.location.href='/',5000);});}\" style=\"background:#ef4444;color:white;\">🔄 Reboot Device</button>";
  html += "</div></div>";

  html += "<script>";
  html += "function exportDiagnostics(){";
  html += "fetch('/api/diagnostics').then(r=>r.json()).then(d=>{";
  html += "const blob=new Blob([JSON.stringify(d,null,2)],{type:'application/json'});";
  html += "const url=URL.createObjectURL(blob);";
  html += "const a=document.createElement('a');a.href=url;a.download='diagnostics.json';a.click();";
  html += "}).catch(e=>alert('Error: '+e));";
  html += "}";
  html += "</script>";

  html += buildHTMLFooter();
  server.send(200, "text/html", html);
}

void handleDiagnosticsSave() {
  bool diag_enabled = server.hasArg("diag_en");

  Preferences prefs;
  prefs.begin("settings", false);
  prefs.putBool("diag_en", diag_enabled);
  prefs.end();

  diagnostics_mqtt_enabled = diag_enabled;

  String html = "<script>";
  html += "sessionStorage.setItem('message',JSON.stringify({title:'✓ Diagnostics Saved',message:'Diagnostics publishing is now " + String(diag_enabled ? "enabled" : "disabled") + ".',success:true}));";
  html += "window.location.href='/diagnostics';";
  html += "</script>";
  server.send(200, "text/html", html);
}

void handleAPIDiagnostics() {
  if (!checkAuth()) return;
  String json = getDiagnosticsJSON();
  server.send(200, "application/json", json);
}
