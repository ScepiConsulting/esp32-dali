#include "base_api.h"
#include "base_i18n.h"
#include "project_function.h"
#include "project_config.h"
#include "project_dali_handler.h"
#include "base_diagnostics.h"

String appHomeHTML() {
  String html = "";

  html += "<div style=\"margin-top:20px;padding-top:20px;border-top:1px solid var(--border-color);\">";
  html += "<h2>" + String(tr("DALI busz állapota", "DALI Bus Status")) + "</h2>";

  html += "<div class=\"status\">";
  html += "<div class=\"dot " + String(busIsIdle ? "green" : "yellow") + "\"></div>";
  html += "<span>" + String(tr("Busz: ", "Bus: ")) + String(busIsIdle ? tr("Üresjárat", "Idle") : tr("Aktív", "Active")) + "</span>";
  html += "</div>";

  uint8_t queueSize = (queueTail >= queueHead) ? (queueTail - queueHead) : (COMMAND_QUEUE_SIZE - queueHead + queueTail);
  html += "<div class=\"status\">";
  html += "<div class=\"dot " + String(queueSize > 0 ? "yellow" : "green") + "\"></div>";
  html += "<span>" + String(tr("Sor: ", "Queue: ")) + String(queueSize) + String(tr(" parancs", " commands")) + "</span>";
  html += "</div>";

  html += "<div style=\"margin-top:12px;display:grid;grid-template-columns:1fr 1fr 1fr;gap:12px;\">";
  html += "<div style=\"padding:12px;background:var(--bg-secondary);border-radius:8px;\">";
  html += "<div style=\"font-size:24px;font-weight:bold;color:var(--accent-green);\">" + String(daliRxCount) + "</div>";
  html += "<div style=\"font-size:12px;color:var(--text-secondary);\">" + String(tr("RX üzenetek", "RX Messages")) + "</div>";
  html += "</div>";
  html += "<div style=\"padding:12px;background:var(--bg-secondary);border-radius:8px;\">";
  html += "<div style=\"font-size:24px;font-weight:bold;color:var(--accent-purple);\">" + String(daliTxCount) + "</div>";
  html += "<div style=\"font-size:12px;color:var(--text-secondary);\">" + String(tr("TX üzenetek", "TX Messages")) + "</div>";
  html += "</div>";
  html += "<div style=\"padding:12px;background:var(--bg-secondary);border-radius:8px;\">";
  html += "<div style=\"font-size:24px;font-weight:bold;color:var(--accent-green);\">" + String(getPassiveDeviceCount()) + "</div>";
  html += "<div style=\"font-size:12px;color:var(--text-secondary);\">" + String(tr("Talált eszközök", "Devices Found")) + "</div>";
  html += "</div>";
  html += "</div>";

  html += "</div>";

  html += "<div style=\"margin-top:20px;padding-top:20px;border-top:1px solid var(--border-color);\">";
  html += "<h2>" + String(tr("Legutóbbi üzenetek", "Recent Messages")) + "</h2>";
  html += "<p style=\"color:var(--text-secondary);font-size:14px;margin-bottom:12px;\">" + String(tr("Az utolsó 20 DALI busz üzenet", "Last 20 DALI bus messages")) + "</p>";
  html += "<button onclick=\"loadRecentMessages()\" style=\"background:var(--accent-green);margin-bottom:16px;\">" + String(tr("Üzenetek frissítése", "Refresh Messages")) + "</button>";
  html += "<div id=\"recent-messages\" style=\"font-size:12px;font-family:monospace;max-height:300px;overflow-y:auto;background:var(--bg-secondary);border-radius:8px;padding:8px;\"></div>";
  html += "</div>";

  html += "<script>";
  html += "function loadRecentMessages(){";
  html += "fetch('/api/recent').then(r=>r.json()).then(d=>{";
  html += "let html='';";
  html += "d.forEach(msg=>{if(msg.timestamp>0){";
  html += "html+='<div style=\"border-bottom:1px solid var(--border-color);padding:8px;\">';";
  html += "html+='<strong>'+(msg.is_tx?'TX':'RX')+'</strong> '+msg.parsed.description+'<br>';";
  html += "html+='<span style=\"color:var(--text-secondary);\">" + String(tr("Nyers: ", "Raw: ")) + "'+msg.raw+'</span>';";
  html += "html+='</div>';}});";
  html += "document.getElementById('recent-messages').innerHTML=html||'<p style=\"padding:8px;color:var(--text-secondary);\">" + String(tr("Nincs még üzenet", "No messages yet")) + "</p>';";
  html += "}).catch(e=>document.getElementById('recent-messages').innerHTML='<p style=\"padding:8px;color:#ef4444;\">" + String(tr("Hiba az üzenetek betöltésekor", "Error loading messages")) + "</p>');";
  html += "}";
  html += "loadRecentMessages();";
  html += "</script>";

  return html;
}
