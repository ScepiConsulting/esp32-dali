#include "project_function.h"
#include "project_config.h"
#include "base_web.h"
#include "base_mqtt.h"
#include "project_ballast_handler.h"
#include "project_mqtt.h"

unsigned long lastFadeUpdate = 0;
const unsigned long FADE_UPDATE_INTERVAL = 50;

void functionInit() {
  ballastInit();

  server.on(NAV_FUNCTION_URL, HTTP_GET, handleFunctionPage);
  server.on("/dali", HTTP_POST, handleBallastSave);
  server.on("/dali/control", HTTP_POST, handleBallastControl);
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

  String html = buildHTMLHeader("Ballast Configuration");

  html += "<div class=\"card\">";
  html += "<h1>Ballast Configuration</h1>";
  html += "<p class=\"subtitle\">Configure virtual DALI ballast behavior</p>";

  html += "<form method=\"POST\" action=\"/dali\" id=\"ballast-form\" onsubmit=\"saveBallastConfig(event)\">";

  html += "<label for=\"device_type\">Device Type</label>";
  uint8_t ui_dt = ballastState.device_type;
  if (ballastState.device_type == DT8_COLOUR) {
    if (ballastState.active_color_type == DT8_MODE_TC) ui_dt = 10;
    else if (ballastState.color_w > 0) ui_dt = 9;
    else ui_dt = 8;
  }
  html += "<select id=\"device_type\" name=\"device_type\" onchange=\"updateDeviceTypeFields()\" style=\"width:100%;padding:12px;border:2px solid var(--border-color);border-radius:8px;font-size:16px;margin-bottom:16px;background:var(--bg-primary);color:var(--text-primary);\">";
  html += "<option value=\"0\"" + String(ui_dt == 0 ? " selected" : "") + ">Normal (IEC 62386-102)</option>";
  html += "<option value=\"6\"" + String(ui_dt == 6 ? " selected" : "") + ">LED (DT6 - IEC 62386-207)</option>";
  html += "<option value=\"8\"" + String(ui_dt == 8 ? " selected" : "") + ">RGB (DT8 - IEC 62386-209)</option>";
  html += "<option value=\"9\"" + String(ui_dt == 9 ? " selected" : "") + ">RGBW (DT8 - IEC 62386-209)</option>";
  html += "<option value=\"10\"" + String(ui_dt == 10 ? " selected" : "") + ">Colour Temperature (DT8 - IEC 62386-209)</option>";
  html += "</select>";

  html += "<div style=\"margin-bottom:16px;\">";
  html += "<label style=\"display:flex;align-items:center;cursor:pointer;\">";
  html += "<input type=\"checkbox\" id=\"address_auto\" name=\"address_auto\" value=\"1\"" + String(ballastState.address_mode_auto ? " checked" : "") + " onchange=\"updateAddressMode()\" style=\"width:auto;margin-right:8px;\">";
  html += "<span>Automatic Address (commissioning via DALI master)</span>";
  html += "</label>";
  html += "</div>";

  html += "<div id=\"manual-address\" style=\"" + String(ballastState.address_mode_auto ? "display:none;" : "") + "\">";
  html += "<label for=\"address\">Short Address (0-63)</label>";
  html += "<input type=\"number\" id=\"address\" name=\"address\" value=\"" + String(ballastState.short_address == 255 ? 0 : ballastState.short_address) + "\" min=\"0\" max=\"63\">";
  html += "</div>";

  html += "<div id=\"auto-address-info\" style=\"" + String(ballastState.address_mode_auto ? "" : "display:none;") + "padding:12px;background:var(--bg-secondary);border-radius:8px;margin-bottom:16px;\">";
  html += "<p style=\"margin:0;font-size:14px;color:var(--text-secondary);\">";
  html += "Current address: <strong>" + String(ballastState.short_address == 255 ? "Unaddressed" : String(ballastState.short_address)) + "</strong><br>";
  html += "The DALI master will assign an address automatically during commissioning.";
  html += "</p>";
  html += "</div>";

  html += "<h3 style=\"margin:20px 0 12px 0;font-size:16px;color:var(--text-primary);\">Base Settings (IEC 62386-102)</h3>";
  html += "<label for=\"min_level\">Minimum Level (0-254)</label>";
  html += "<input type=\"number\" id=\"min_level\" name=\"min_level\" value=\"" + String(ballastState.min_level) + "\" min=\"0\" max=\"254\">";
  html += "<label for=\"max_level\">Maximum Level (0-254)</label>";
  html += "<input type=\"number\" id=\"max_level\" name=\"max_level\" value=\"" + String(ballastState.max_level) + "\" min=\"0\" max=\"254\">";
  html += "<label for=\"power_on_level\">Power-On Level (0-254)</label>";
  html += "<input type=\"number\" id=\"power_on_level\" name=\"power_on_level\" value=\"" + String(ballastState.power_on_level) + "\" min=\"0\" max=\"254\">";
  html += "<label for=\"fade_time\">Fade Time (0-15)</label>";
  html += "<input type=\"number\" id=\"fade_time\" name=\"fade_time\" value=\"" + String(ballastState.fade_time) + "\" min=\"0\" max=\"15\">";
  html += "<label for=\"fade_rate\">Fade Rate (0-15)</label>";
  html += "<input type=\"number\" id=\"fade_rate\" name=\"fade_rate\" value=\"" + String(ballastState.fade_rate) + "\" min=\"0\" max=\"15\">";

  html += "<div id=\"dt6-fields\" style=\"display:none;\">";
  html += "<h3 style=\"margin:20px 0 12px 0;font-size:16px;color:var(--text-primary);\">LED Control (DT6 - IEC 62386-207)</h3>";
  html += "<label for=\"channel_warm\">Warm White Channel (0-254)</label>";
  html += "<input type=\"number\" id=\"channel_warm\" name=\"channel_warm\" value=\"" + String(ballastState.channel_warm) + "\" min=\"0\" max=\"254\">";
  html += "<label for=\"channel_cool\">Cool White Channel (0-254)</label>";
  html += "<input type=\"number\" id=\"channel_cool\" name=\"channel_cool\" value=\"" + String(ballastState.channel_cool) + "\" min=\"0\" max=\"254\">";
  html += "</div>";

  html += "<div id=\"rgb-fields\" style=\"display:none;\">";
  html += "<h3 style=\"margin:20px 0 12px 0;font-size:16px;color:var(--text-primary);\">RGB Color Control (DT8 - IEC 62386-209)</h3>";
  html += "<label for=\"color_r\">Red (0-255)</label>";
  html += "<input type=\"number\" id=\"color_r\" name=\"color_r\" value=\"" + String(ballastState.color_r) + "\" min=\"0\" max=\"255\">";
  html += "<label for=\"color_g\">Green (0-255)</label>";
  html += "<input type=\"number\" id=\"color_g\" name=\"color_g\" value=\"" + String(ballastState.color_g) + "\" min=\"0\" max=\"255\">";
  html += "<label for=\"color_b\">Blue (0-255)</label>";
  html += "<input type=\"number\" id=\"color_b\" name=\"color_b\" value=\"" + String(ballastState.color_b) + "\" min=\"0\" max=\"255\">";
  html += "</div>";

  html += "<div id=\"rgbw-fields\" style=\"display:none;\">";
  html += "<h3 style=\"margin:20px 0 12px 0;font-size:16px;color:var(--text-primary);\">RGBW Color Control (DT8 - IEC 62386-209)</h3>";
  html += "<label for=\"color_r_rgbw\">Red (0-255)</label>";
  html += "<input type=\"number\" id=\"color_r_rgbw\" name=\"color_r\" value=\"" + String(ballastState.color_r) + "\" min=\"0\" max=\"255\">";
  html += "<label for=\"color_g_rgbw\">Green (0-255)</label>";
  html += "<input type=\"number\" id=\"color_g_rgbw\" name=\"color_g\" value=\"" + String(ballastState.color_g) + "\" min=\"0\" max=\"255\">";
  html += "<label for=\"color_b_rgbw\">Blue (0-255)</label>";
  html += "<input type=\"number\" id=\"color_b_rgbw\" name=\"color_b\" value=\"" + String(ballastState.color_b) + "\" min=\"0\" max=\"255\">";
  html += "<label for=\"color_w\">White (0-255)</label>";
  html += "<input type=\"number\" id=\"color_w\" name=\"color_w\" value=\"" + String(ballastState.color_w) + "\" min=\"0\" max=\"255\">";
  html += "</div>";

  html += "<div id=\"cct-fields\" style=\"display:none;\">";
  html += "<h3 style=\"margin:20px 0 12px 0;font-size:16px;color:var(--text-primary);\">Colour Temperature Control (DT8 - IEC 62386-209)</h3>";
  html += "<label for=\"color_temp\">Color Temperature (Kelvin)</label>";
  uint16_t cfg_kelvin = (ballastState.color_temp_mirek > 0) ? (1000000 / ballastState.color_temp_mirek) : 4000;
  html += "<input type=\"number\" id=\"color_temp\" name=\"color_temp\" value=\"" + String(cfg_kelvin) + "\" min=\"2700\" max=\"6500\" step=\"100\">";
  html += "<p style=\"font-size:12px;color:var(--text-secondary);margin-top:-8px;\">Typical range: 2700K (warm) to 6500K (cool)</p>";
  html += "</div>";

  html += "<button type=\"submit\">Save Configuration</button>";
  html += "</form>";
  html += "</div>";

  html += "<div class=\"card\" style=\"margin-top:20px;\">";
  html += "<h1>Ballast Control</h1>";
  html += "<p class=\"subtitle\">Set current ballast values</p>";
  html += "<form id=\"control-form\" onsubmit=\"sendControl(event)\">";
  html += "<label for=\"ctrl_level\">Brightness Level (0-254)</label>";
  html += "<input type=\"number\" id=\"ctrl_level\" name=\"level\" value=\"" + String(ballastState.actual_level) + "\" min=\"0\" max=\"254\">";

  if (ballastState.device_type == DT8_COLOUR) {
    if (ballastState.active_color_type == DT8_MODE_TC) {
      html += "<h3 style=\"margin:20px 0 12px 0;font-size:16px;color:var(--text-primary);\">Color Temperature</h3>";
      html += "<label for=\"ctrl_temp\">Temperature (Kelvin)</label>";
      uint16_t ctrl_kelvin = (ballastState.color_temp_mirek > 0) ? (1000000 / ballastState.color_temp_mirek) : 4000;
      html += "<input type=\"number\" id=\"ctrl_temp\" name=\"color_temp\" value=\"" + String(ctrl_kelvin) + "\" min=\"2700\" max=\"6500\" step=\"100\">";
    } else {
      html += "<h3 style=\"margin:20px 0 12px 0;font-size:16px;color:var(--text-primary);\">RGB" + String(ballastState.color_w > 0 ? "W" : "") + " Color</h3>";
      html += "<label for=\"ctrl_r\">Red (0-255)</label>";
      html += "<input type=\"number\" id=\"ctrl_r\" name=\"color_r\" value=\"" + String(ballastState.color_r) + "\" min=\"0\" max=\"255\">";
      html += "<label for=\"ctrl_g\">Green (0-255)</label>";
      html += "<input type=\"number\" id=\"ctrl_g\" name=\"color_g\" value=\"" + String(ballastState.color_g) + "\" min=\"0\" max=\"255\">";
      html += "<label for=\"ctrl_b\">Blue (0-255)</label>";
      html += "<input type=\"number\" id=\"ctrl_b\" name=\"color_b\" value=\"" + String(ballastState.color_b) + "\" min=\"0\" max=\"255\">";
      html += "<label for=\"ctrl_w\">White (0-255)</label>";
      html += "<input type=\"number\" id=\"ctrl_w\" name=\"color_w\" value=\"" + String(ballastState.color_w) + "\" min=\"0\" max=\"255\">";
    }
  }

  html += "<button type=\"submit\">Apply</button>";
  html += "</form>";
  html += "</div>";

  html += "<script>";
  html += "function updateAddressMode(){";
  html += "const auto=document.getElementById('address_auto').checked;";
  html += "document.getElementById('manual-address').style.display=auto?'none':'block';";
  html += "document.getElementById('auto-address-info').style.display=auto?'block':'none';";
  html += "}";
  html += "function updateDeviceTypeFields(){";
  html += "const dt=document.getElementById('device_type').value;";
  html += "const sections=[{id:'dt6-fields',show:dt=='6'},{id:'rgb-fields',show:dt=='8'},{id:'rgbw-fields',show:dt=='9'},{id:'cct-fields',show:dt=='10'}];";
  html += "sections.forEach(s=>{const el=document.getElementById(s.id);el.style.display=s.show?'block':'none';";
  html += "el.querySelectorAll('input').forEach(i=>i.disabled=!s.show);});";
  html += "}";
  html += "updateDeviceTypeFields();";
  html += "function saveBallastConfig(e){";
  html += "e.preventDefault();";
  html += "const form=new FormData(e.target);";
  html += "fetch('/dali',{method:'POST',body:form})";
  html += ".then(r=>r.json())";
  html += ".then(d=>{showModal(d.title,d.message,d.success);if(d.success)setTimeout(()=>location.reload(),1500);})";
  html += ".catch(e=>showModal('✗ Error','Failed to save configuration: '+e,false));}";
  html += "function sendControl(e){";
  html += "e.preventDefault();";
  html += "const form=new FormData(e.target);";
  html += "fetch('/dali/control',{method:'POST',body:form})";
  html += ".then(r=>r.json())";
  html += ".then(d=>{showModal(d.title,d.message,d.success);if(d.success)setTimeout(()=>location.reload(),800);})";
  html += ".catch(e=>showModal('✗ Error','Failed to apply control: '+e,false));}";
  html += "</script>";

  html += buildHTMLFooter();
  server.send(200, "text/html", html);
}

void handleBallastSave() {
  if (!checkAuth()) return;

  ballastState.address_mode_auto = server.hasArg("address_auto");

  if (ballastState.address_mode_auto) {
    if (ballastState.address_source != ADDR_COMMISSIONED) {
      ballastState.short_address = 255;
      ballastState.address_source = ADDR_UNASSIGNED;
    }
  } else {
    ballastState.short_address = server.arg("address").toInt();
    ballastState.address_source = ADDR_MANUAL;
  }

  uint8_t ui_device_type = server.arg("device_type").toInt();
  if (ui_device_type == 8) {
    ballastState.device_type = DT8_COLOUR;
    ballastState.active_color_type = DT8_MODE_RGBWAF;
    ballastState.color_w = 0;
  } else if (ui_device_type == 9) {
    ballastState.device_type = DT8_COLOUR;
    ballastState.active_color_type = DT8_MODE_RGBWAF;
  } else if (ui_device_type == 10) {
    ballastState.device_type = DT8_COLOUR;
    ballastState.active_color_type = DT8_MODE_TC;
  } else {
    ballastState.device_type = ui_device_type;
  }

  if (server.hasArg("min_level")) ballastState.min_level = server.arg("min_level").toInt();
  if (server.hasArg("max_level")) ballastState.max_level = server.arg("max_level").toInt();
  if (server.hasArg("power_on_level")) ballastState.power_on_level = server.arg("power_on_level").toInt();
  if (server.hasArg("fade_time")) ballastState.fade_time = server.arg("fade_time").toInt();
  if (server.hasArg("fade_rate")) ballastState.fade_rate = server.arg("fade_rate").toInt();

  if (server.hasArg("channel_warm")) ballastState.channel_warm = server.arg("channel_warm").toInt();
  if (server.hasArg("channel_cool")) ballastState.channel_cool = server.arg("channel_cool").toInt();

  if (server.hasArg("color_r")) ballastState.color_r = server.arg("color_r").toInt();
  if (server.hasArg("color_g")) ballastState.color_g = server.arg("color_g").toInt();
  if (server.hasArg("color_b")) ballastState.color_b = server.arg("color_b").toInt();
  if (server.hasArg("color_w")) ballastState.color_w = server.arg("color_w").toInt();

  if (server.hasArg("color_temp")) {
    uint16_t input_kelvin = server.arg("color_temp").toInt();
    ballastState.color_temp_mirek = (input_kelvin > 0) ? (1000000 / input_kelvin) : 250;
  }

  saveBallastConfig();
  publishBallastConfig();

  server.send(200, "application/json", "{\"success\":true,\"title\":\"✓ Configuration Saved\",\"message\":\"Ballast configuration has been saved.\"}");
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
