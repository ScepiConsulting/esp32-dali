#include "project_function.h"
#include "project_config.h"
#include "base_web.h"
#include "base_mqtt.h"
#include "project_ballast_handler.h"

unsigned long lastFadeUpdate = 0;
const unsigned long FADE_UPDATE_INTERVAL = 50;

void functionInit() {
  ballastInit();

  server.on(NAV_FUNCTION_URL, HTTP_GET, handleFunctionPage);
  server.on("/ballast/save", HTTP_POST, handleBallastSave);
  server.on("/ballast/control", HTTP_POST, handleBallastControl);
  server.on("/api/recent", handleAPIRecent);
}

void functionLoop() {
  monitorDaliBus();

  unsigned long now = millis();
  if (now - lastFadeUpdate >= FADE_UPDATE_INTERVAL) {
    updateFade();
    lastFadeUpdate = now;
  }
}

void handleFunctionPage() {
  if (!checkAuth()) return;

  String html = buildHTMLHeader(String(NAV_FUNCTION_NAME));

  html += "<div class=\"card\">";
  html += "<h1>Ballast Configuration</h1>";
  html += "<p class=\"subtitle\">Configure DALI ballast emulator settings</p>";

  html += "<form id=\"ballast-form\" onsubmit=\"saveBallast(event)\">";

  html += "<div style=\"margin-top:20px;\">";
  html += "<h2>Address Settings</h2>";
  html += "<div style=\"display:grid;grid-template-columns:1fr 1fr;gap:12px;\">";
  html += "<div><label for=\"address\">Short Address (0-63, 255=unaddressed)</label>";
  html += "<input type=\"number\" id=\"address\" name=\"address\" min=\"0\" max=\"255\" value=\"" + String(ballastState.short_address) + "\"></div>";
  html += "<div><label for=\"device_type\">Device Type</label>";
  html += "<select id=\"device_type\" name=\"device_type\" style=\"width:100%;padding:12px;border:2px solid var(--border-color);border-radius:8px;\">";
  html += "<option value=\"0\"" + String(ballastState.device_type == 0 ? " selected" : "") + ">DT0 - Normal</option>";
  html += "<option value=\"6\"" + String(ballastState.device_type == 6 ? " selected" : "") + ">DT6 - LED</option>";
  html += "<option value=\"8\"" + String(ballastState.device_type == 8 ? " selected" : "") + ">DT8 - Color</option>";
  html += "</select></div>";
  html += "</div>";
  html += "</div>";

  html += "<div style=\"margin-top:20px;\">";
  html += "<h2>Level Settings</h2>";
  html += "<div style=\"display:grid;grid-template-columns:1fr 1fr 1fr 1fr;gap:12px;\">";
  html += "<div><label for=\"min_level\">Min Level</label>";
  html += "<input type=\"number\" id=\"min_level\" name=\"min_level\" min=\"1\" max=\"254\" value=\"" + String(ballastState.min_level) + "\"></div>";
  html += "<div><label for=\"max_level\">Max Level</label>";
  html += "<input type=\"number\" id=\"max_level\" name=\"max_level\" min=\"1\" max=\"254\" value=\"" + String(ballastState.max_level) + "\"></div>";
  html += "<div><label for=\"power_on_level\">Power-On Level</label>";
  html += "<input type=\"number\" id=\"power_on_level\" name=\"power_on_level\" min=\"0\" max=\"254\" value=\"" + String(ballastState.power_on_level) + "\"></div>";
  html += "<div><label for=\"fade_time\">Fade Time (0-15)</label>";
  html += "<input type=\"number\" id=\"fade_time\" name=\"fade_time\" min=\"0\" max=\"15\" value=\"" + String(ballastState.fade_time) + "\"></div>";
  html += "</div>";
  html += "</div>";

  html += "<div style=\"margin-top:20px;\">";
  html += "<label style=\"display:flex;align-items:center;cursor:pointer;\">";
  html += "<input type=\"checkbox\" id=\"addr_auto\" name=\"addr_auto\"" + String(ballastState.address_mode_auto ? " checked" : "") + " style=\"margin-right:8px;\">";
  html += "<span>Allow automatic commissioning (address can be changed by DALI master)</span></label>";
  html += "</div>";

  html += "<button type=\"submit\" style=\"margin-top:20px;\">Save Configuration</button>";
  html += "</form>";

  html += "</div>";

  html += "<div class=\"card\" style=\"margin-top:20px;\">";
  html += "<h2>Current State</h2>";
  html += "<div style=\"display:grid;grid-template-columns:1fr 1fr 1fr;gap:12px;margin-bottom:16px;\">";
  html += "<div style=\"padding:12px;background:var(--bg-secondary);border-radius:8px;text-align:center;\">";
  html += "<div style=\"font-size:24px;font-weight:bold;color:var(--accent-green);\">" + String(ballastState.actual_level) + "</div>";
  html += "<div style=\"font-size:12px;color:var(--text-secondary);\">Actual Level</div></div>";
  html += "<div style=\"padding:12px;background:var(--bg-secondary);border-radius:8px;text-align:center;\">";
  html += "<div style=\"font-size:24px;font-weight:bold;color:var(--accent-purple);\">" + String((ballastState.actual_level / 254.0) * 100.0, 1) + "%</div>";
  html += "<div style=\"font-size:12px;color:var(--text-secondary);\">Brightness</div></div>";
  html += "<div style=\"padding:12px;background:var(--bg-secondary);border-radius:8px;text-align:center;\">";
  html += "<div style=\"font-size:24px;font-weight:bold;color:" + String(ballastState.lamp_arc_power_on ? "var(--accent-green)" : "var(--text-secondary)") + ";\">" + String(ballastState.lamp_arc_power_on ? "ON" : "OFF") + "</div>";
  html += "<div style=\"font-size:12px;color:var(--text-secondary);\">Lamp Status</div></div>";
  html += "</div>";
  html += "</div>";

  html += "<div class=\"card\" style=\"margin-top:20px;\">";
  html += "<h2>Recent Messages</h2>";
  html += "<p class=\"subtitle\">Last " + String(RECENT_MESSAGES_SIZE) + " DALI commands received</p>";
  html += "<button onclick=\"loadRecentMessages()\" style=\"background:var(--accent-green);margin-bottom:16px;\">Refresh</button>";
  html += "<div id=\"recent-messages\" style=\"font-size:12px;font-family:monospace;max-height:300px;overflow-y:auto;\"></div>";
  html += "</div>";

  html += "<script>";
  html += "function saveBallast(e){e.preventDefault();";
  html += "const form=new FormData(document.getElementById('ballast-form'));";
  html += "fetch('/ballast/save',{method:'POST',body:new URLSearchParams(form)})";
  html += ".then(r=>r.json()).then(d=>showModal(d.title,d.message,d.success)).catch(e=>showModal('Error',e,false));}";

  html += "function loadRecentMessages(){";
  html += "fetch('/api/recent').then(r=>r.json()).then(d=>{";
  html += "let html='';d.forEach(msg=>{if(msg.timestamp>0){";
  html += "html+='<div style=\"border-bottom:1px solid var(--border-color);padding:8px;\">';";
  html += "html+='<strong>'+msg.command_type+'</strong> '+msg.description+'<br>';";
  html += "html+='<span style=\"color:var(--text-secondary);\">Raw: '+msg.raw+'</span></div>';}});";
  html += "document.getElementById('recent-messages').innerHTML=html||'<p>No messages yet</p>';";
  html += "}).catch(e=>{document.getElementById('recent-messages').innerHTML='<p>Error loading messages</p>';});}";

  html += "loadRecentMessages();";
  html += "</script>";

  html += buildHTMLFooter();
  server.send(200, "text/html", html);
}

void handleBallastSave() {
  if (!checkAuth()) return;

  ballastState.short_address = server.arg("address").toInt();
  ballastState.device_type = server.arg("device_type").toInt();
  ballastState.min_level = server.arg("min_level").toInt();
  ballastState.max_level = server.arg("max_level").toInt();
  ballastState.power_on_level = server.arg("power_on_level").toInt();
  ballastState.fade_time = server.arg("fade_time").toInt();
  ballastState.address_mode_auto = server.hasArg("addr_auto");

  saveBallastConfig();

  server.send(200, "application/json", "{\"success\":true,\"title\":\"✓ Saved\",\"message\":\"Ballast configuration saved\"}");
}

void handleBallastControl() {
  if (!checkAuth()) return;

  if (server.hasArg("level")) {
    uint8_t level = server.arg("level").toInt();
    setLevel(level);
  }

  server.send(200, "application/json", "{\"success\":true,\"title\":\"✓ OK\",\"message\":\"Level set\"}");
}

void handleAPIRecent() {
  if (!checkAuth()) return;

  String json = "[";
  for (int i = 0; i < RECENT_MESSAGES_SIZE; i++) {
    int idx = (recentMessagesIndex - 1 - i + RECENT_MESSAGES_SIZE) % RECENT_MESSAGES_SIZE;
    const BallastMessage& msg = recentMessages[idx];
    if (msg.timestamp == 0) continue;

    if (json.length() > 1) json += ",";
    json += "{\"timestamp\":" + String(msg.timestamp) + ",";
    json += "\"command_type\":\"" + msg.command_type + "\",";
    json += "\"address\":" + String(msg.address) + ",";
    json += "\"value\":" + String(msg.value) + ",";
    json += "\"raw\":\"";
    if (msg.raw_bytes[0] < 0x10) json += "0";
    json += String(msg.raw_bytes[0], HEX);
    if (msg.raw_bytes[1] < 0x10) json += "0";
    json += String(msg.raw_bytes[1], HEX);
    json += "\",\"description\":\"" + msg.description + "\"}";
  }
  json += "]";

  server.send(200, "application/json", json);
}
