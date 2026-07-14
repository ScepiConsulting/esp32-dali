#include "base_api.h"
#include "base_i18n.h"
#include "project_function.h"
#include "project_config.h"
#include "project_ballast_handler.h"
#include "base_diagnostics.h"

String appHomeHTML() {
  String html = "";

  html += "<div style=\"margin-top:20px;padding-top:20px;border-top:1px solid var(--border-color);\">";
  html += "<h2>" + String(tr("Előtét állapota", "Ballast Status")) + "</h2>";

  html += "<div class=\"status\">";
  html += "<div class=\"dot " + String(ballastState.lamp_arc_power_on ? "green" : "red") + "\"></div>";
  html += "<span>" + String(tr("Lámpa: ", "Lamp: ")) + String(ballastState.lamp_arc_power_on ? tr("BE", "ON") : tr("KI", "OFF")) + "</span>";
  html += "</div>";

  html += "<div class=\"status\">";
  html += "<div class=\"dot " + String(ballastState.fade_running ? "yellow" : "green") + "\"></div>";
  html += "<span>" + String(tr("Fade: ", "Fade: ")) + String(ballastState.fade_running ? tr("Fut", "Running") : tr("Tétlen", "Idle")) + "</span>";
  html += "</div>";

  html += "<div style=\"margin-top:12px;display:grid;grid-template-columns:1fr 1fr 1fr;gap:12px;\">";
  html += "<div style=\"padding:12px;background:var(--bg-secondary);border-radius:8px;\">";
  html += "<div style=\"font-size:24px;font-weight:bold;color:var(--accent-green);\">" + String(ballastState.actual_level) + "</div>";
  html += "<div style=\"font-size:12px;color:var(--text-secondary);\">" + String(tr("Szint", "Level")) + "</div>";
  html += "</div>";
  html += "<div style=\"padding:12px;background:var(--bg-secondary);border-radius:8px;\">";
  html += "<div style=\"font-size:24px;font-weight:bold;color:var(--accent-purple);\">" + String((ballastState.actual_level / 254.0) * 100.0, 1) + "%</div>";
  html += "<div style=\"font-size:12px;color:var(--text-secondary);\">" + String(tr("Fényerő", "Brightness")) + "</div>";
  html += "</div>";
  html += "<div style=\"padding:12px;background:var(--bg-secondary);border-radius:8px;\">";
  html += "<div style=\"font-size:24px;font-weight:bold;color:var(--accent-green);\">" + String(ballastRxCount) + "</div>";
  html += "<div style=\"font-size:12px;color:var(--text-secondary);\">" + String(tr("Fogadott parancsok", "Commands RX")) + "</div>";
  html += "</div>";
  html += "</div>";

  html += "</div>";

  html += "<div class=\"card\" style=\"margin-top:20px;\">";
  html += "<h2>" + String(tr("Aktuális állapot", "Current Status")) + "</h2>";

  String deviceTypeName;
  switch (ballastState.device_type) {
    case DT0_NORMAL: deviceTypeName = tr("Normál (DT0)", "Normal (DT0)"); break;
    case DT6_LED: deviceTypeName = "LED (DT6)"; break;
    case DT8_COLOUR:
      switch (ballastState.active_color_type) {
        case DT8_MODE_TC: deviceTypeName = tr("Színhőmérséklet (DT8)", "Colour Temperature (DT8)"); break;
        case DT8_MODE_XY: deviceTypeName = tr("XY színkoordináta (DT8)", "XY Chromaticity (DT8)"); break;
        default:
          if (ballastState.color_w > 0) deviceTypeName = "RGBW (DT8)";
          else deviceTypeName = "RGB (DT8)";
          break;
      }
      break;
    default: deviceTypeName = tr("Ismeretlen", "Unknown"); break;
  }
  html += "<div class=\"status\"><div class=\"dot green\"></div><span>" + String(tr("Eszköztípus: ", "Device Type: ")) + deviceTypeName + "</span></div>";

  String addrSourceStr;
  switch (ballastState.address_source) {
    case ADDR_MANUAL: addrSourceStr = tr("Kézi", "Manual"); break;
    case ADDR_COMMISSIONED: addrSourceStr = tr("Címkiosztással", "Commissioned"); break;
    default: addrSourceStr = tr("Nincs hozzárendelve", "Unassigned"); break;
  }
  html += "<div class=\"status\"><div class=\"dot green\"></div><span>" + String(tr("Cím: ", "Address: "));
  if (ballastState.short_address == 255) {
    html += tr("Cím nélküli", "Unaddressed");
  } else {
    html += String(ballastState.short_address) + " (" + addrSourceStr + ")";
  }
  html += "</span></div>";

  html += "<div class=\"status\"><div class=\"dot " + String(ballastState.address_mode_auto ? "green" : "yellow") + "\"></div><span>" + String(tr("Mód: ", "Mode: ")) + String(ballastState.address_mode_auto ? tr("Automatikus (elfogadja a címkiosztást)", "Auto (accepts commissioning)") : tr("Kézi (rögzített cím)", "Manual (fixed address)")) + "</span></div>";

  if (ballastState.device_type == DT8_COLOUR) {
    if (ballastState.active_color_type == DT8_MODE_TC) {
      uint16_t display_kelvin = (ballastState.color_temp_mirek > 0) ? (1000000 / ballastState.color_temp_mirek) : 4000;
      html += "<div class=\"status\"><div class=\"dot green\"></div><span>" + String(tr("Színhőmérséklet: ", "Color Temp: ")) + String(display_kelvin) + "K</span></div>";
    } else {
      html += "<div class=\"status\"><div class=\"dot green\"></div><span>RGB: (" + String(ballastState.color_r) + ", " + String(ballastState.color_g) + ", " + String(ballastState.color_b) + ")</span></div>";
      if (ballastState.color_w > 0) {
        html += "<div class=\"status\"><div class=\"dot green\"></div><span>" + String(tr("Fehér: ", "White: ")) + String(ballastState.color_w) + "</span></div>";
      }
    }
  }

  html += "</div>";

  html += "<div style=\"margin-top:20px;padding-top:20px;border-top:1px solid var(--border-color);\">";
  html += "<h2>" + String(tr("Legutóbbi parancsok", "Recent Commands")) + "</h2>";
  html += "<p style=\"color:var(--text-secondary);font-size:14px;margin-bottom:12px;\">" + String(tr("Az utolsó ", "Last ")) + String(RECENT_MESSAGES_SIZE) + String(tr(" fogadott DALI parancs", " DALI commands received")) + "</p>";
  html += "<button onclick=\"loadRecentMessages()\" style=\"background:var(--accent-green);margin-bottom:16px;\">" + String(tr("Üzenetek frissítése", "Refresh Messages")) + "</button>";
  html += "<div id=\"recent-messages\" style=\"font-size:12px;font-family:monospace;max-height:300px;overflow-y:auto;background:var(--bg-secondary);border-radius:8px;padding:8px;\"></div>";
  html += "</div>";

  html += "<script>";
  html += "function loadRecentMessages(){";
  html += "fetch('/api/recent').then(r=>r.json()).then(d=>{";
  html += "let html='';d.forEach(msg=>{if(msg.timestamp>0){";
  html += "html+='<div style=\"border-bottom:1px solid var(--border-color);padding:8px;\">';";
  html += "html+='<strong>'+msg.command_type+'</strong> '+msg.description+'<br>';";
  html += "html+='<span style=\"color:var(--text-secondary);\">" + String(tr("Nyers: ", "Raw: ")) + "'+msg.raw+'</span></div>';}});";
  html += "document.getElementById('recent-messages').innerHTML=html||'<p style=\"padding:8px;color:var(--text-secondary);\">" + String(tr("Még nincsenek üzenetek", "No messages yet")) + "</p>';";
  html += "}).catch(e=>{document.getElementById('recent-messages').innerHTML='<p style=\"padding:8px;color:#ef4444;\">" + String(tr("Hiba az üzenetek betöltésekor", "Error loading messages")) + "</p>';});}";
  html += "loadRecentMessages();";
  html += "</script>";

  return html;
}
