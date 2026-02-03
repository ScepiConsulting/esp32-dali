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
  html += "<div class=\"dot " + String(ballastState.fade_running ? "yellow" : "green") + "\"></div>";
  html += "<span>Fade: " + String(ballastState.fade_running ? "Running" : "Idle") + "</span>";
  html += "</div>";

  html += "<div style=\"margin-top:12px;display:grid;grid-template-columns:1fr 1fr 1fr;gap:12px;\">";
  html += "<div style=\"padding:12px;background:var(--bg-secondary);border-radius:8px;\">";
  html += "<div style=\"font-size:24px;font-weight:bold;color:var(--accent-green);\">" + String(ballastState.actual_level) + "</div>";
  html += "<div style=\"font-size:12px;color:var(--text-secondary);\">Level</div>";
  html += "</div>";
  html += "<div style=\"padding:12px;background:var(--bg-secondary);border-radius:8px;\">";
  html += "<div style=\"font-size:24px;font-weight:bold;color:var(--accent-purple);\">" + String((ballastState.actual_level / 254.0) * 100.0, 1) + "%</div>";
  html += "<div style=\"font-size:12px;color:var(--text-secondary);\">Brightness</div>";
  html += "</div>";
  html += "<div style=\"padding:12px;background:var(--bg-secondary);border-radius:8px;\">";
  html += "<div style=\"font-size:24px;font-weight:bold;color:var(--accent-green);\">" + String(ballastRxCount) + "</div>";
  html += "<div style=\"font-size:12px;color:var(--text-secondary);\">Commands RX</div>";
  html += "</div>";
  html += "</div>";

  html += "</div>";

  html += "<div class=\"card\" style=\"margin-top:20px;\">";
  html += "<h2>Current Status</h2>";

  String deviceTypeName;
  switch (ballastState.device_type) {
    case DT0_NORMAL: deviceTypeName = "Normal (DT0)"; break;
    case DT6_LED: deviceTypeName = "LED (DT6)"; break;
    case DT8_COLOUR:
      switch (ballastState.active_color_type) {
        case DT8_MODE_TC: deviceTypeName = "Colour Temperature (DT8)"; break;
        case DT8_MODE_XY: deviceTypeName = "XY Chromaticity (DT8)"; break;
        default: 
          if (ballastState.color_w > 0) deviceTypeName = "RGBW (DT8)";
          else deviceTypeName = "RGB (DT8)";
          break;
      }
      break;
    default: deviceTypeName = "Unknown"; break;
  }
  html += "<div class=\"status\"><div class=\"dot green\"></div><span>Device Type: " + deviceTypeName + "</span></div>";

  String addrSourceStr;
  switch (ballastState.address_source) {
    case ADDR_MANUAL: addrSourceStr = "Manual"; break;
    case ADDR_COMMISSIONED: addrSourceStr = "Commissioned"; break;
    default: addrSourceStr = "Unassigned"; break;
  }
  html += "<div class=\"status\"><div class=\"dot green\"></div><span>Address: ";
  if (ballastState.short_address == 255) {
    html += "Unaddressed";
  } else {
    html += String(ballastState.short_address) + " (" + addrSourceStr + ")";
  }
  html += "</span></div>";

  html += "<div class=\"status\"><div class=\"dot " + String(ballastState.address_mode_auto ? "green" : "yellow") + "\"></div><span>Mode: " + String(ballastState.address_mode_auto ? "Auto (accepts commissioning)" : "Manual (fixed address)") + "</span></div>";

  if (ballastState.device_type == DT8_COLOUR) {
    if (ballastState.active_color_type == DT8_MODE_TC) {
      uint16_t display_kelvin = (ballastState.color_temp_mirek > 0) ? (1000000 / ballastState.color_temp_mirek) : 4000;
      html += "<div class=\"status\"><div class=\"dot green\"></div><span>Color Temp: " + String(display_kelvin) + "K</span></div>";
    } else {
      html += "<div class=\"status\"><div class=\"dot green\"></div><span>RGB: (" + String(ballastState.color_r) + ", " + String(ballastState.color_g) + ", " + String(ballastState.color_b) + ")</span></div>";
      if (ballastState.color_w > 0) {
        html += "<div class=\"status\"><div class=\"dot green\"></div><span>White: " + String(ballastState.color_w) + "</span></div>";
      }
    }
  }

  html += "</div>";

  html += "<div style=\"margin-top:20px;padding-top:20px;border-top:1px solid var(--border-color);\">";
  html += "<h2>Recent Commands</h2>";
  html += "<p style=\"color:var(--text-secondary);font-size:14px;margin-bottom:12px;\">Last " + String(RECENT_MESSAGES_SIZE) + " DALI commands received</p>";
  html += "<button onclick=\"loadRecentMessages()\" style=\"background:var(--accent-green);margin-bottom:16px;\">Refresh Messages</button>";
  html += "<div id=\"recent-messages\" style=\"font-size:12px;font-family:monospace;max-height:300px;overflow-y:auto;background:var(--bg-secondary);border-radius:8px;padding:8px;\"></div>";
  html += "</div>";

  html += "<script>";
  html += "function loadRecentMessages(){";
  html += "fetch('/api/recent').then(r=>r.json()).then(d=>{";
  html += "let html='';d.forEach(msg=>{if(msg.timestamp>0){";
  html += "html+='<div style=\"border-bottom:1px solid var(--border-color);padding:8px;\">';";
  html += "html+='<strong>'+msg.command_type+'</strong> '+msg.description+'<br>';";
  html += "html+='<span style=\"color:var(--text-secondary);\">Raw: '+msg.raw+'</span></div>';}});";
  html += "document.getElementById('recent-messages').innerHTML=html||'<p style=\"padding:8px;color:var(--text-secondary);\">No messages yet</p>';";
  html += "}).catch(e=>{document.getElementById('recent-messages').innerHTML='<p style=\"padding:8px;color:#ef4444;\">Error loading messages</p>';});}";
  html += "loadRecentMessages();";
  html += "</script>";

  return html;
}
