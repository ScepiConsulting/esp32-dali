#include "project_function.h"
#include "project_config.h"
#include "base_web.h"
#include "base_mqtt.h"
#include "project_dali_handler.h"
#include <ArduinoJson.h>

void functionInit() {
  daliInit();

  server.on(NAV_FUNCTION_URL, HTTP_GET, handleFunctionPage);
  server.on("/dali/send", HTTP_POST, handleDALISend);
  server.on("/dali/scan", HTTP_POST, handleDALIScan);
  server.on("/dali/commission", HTTP_POST, handleDALICommission);
  server.on("/api/commission/progress", handleAPICommissionProgress);
  server.on("/api/recent", handleAPIRecent);
  server.on("/api/passive_devices", handleAPIPassiveDevices);
}

void functionLoop() {
  monitorDaliBus();
  processCommandQueue();
}

void handleFunctionPage() {
  if (!checkAuth()) return;

  String html = buildHTMLHeader(String(NAV_FUNCTION_NAME));

  html += "<div class=\"card\">";
  html += "<h1>DALI Control</h1>";
  html += "<p class=\"subtitle\">Control DALI devices on the bus</p>";

  html += "<div style=\"margin-top:20px;\">";
  html += "<h2>Quick Control</h2>";
  html += "<form id=\"dali-form\" onsubmit=\"sendDaliCommand(event)\">";
  html += "<div style=\"display:grid;grid-template-columns:1fr 1fr;gap:12px;margin-bottom:16px;\">";
  html += "<div><label for=\"address\">Address (0-63 or 255=broadcast)</label>";
  html += "<input type=\"number\" id=\"address\" name=\"address\" min=\"0\" max=\"255\" value=\"0\"></div>";
  html += "<div><label for=\"level\">Level (0-254)</label>";
  html += "<input type=\"number\" id=\"level\" name=\"level\" min=\"0\" max=\"254\" value=\"254\"></div>";
  html += "</div>";
  html += "<div style=\"display:grid;grid-template-columns:repeat(4,1fr);gap:8px;\">";
  html += "<button type=\"button\" onclick=\"sendCmd('off')\" style=\"background:#ef4444;\">Off</button>";
  html += "<button type=\"button\" onclick=\"sendCmd('max')\" style=\"background:var(--accent-green);\">Max</button>";
  html += "<button type=\"button\" onclick=\"sendCmd('up')\" style=\"background:var(--accent-purple);\">Up</button>";
  html += "<button type=\"button\" onclick=\"sendCmd('down')\" style=\"background:var(--accent-purple);\">Down</button>";
  html += "</div>";
  html += "<button type=\"submit\" style=\"margin-top:12px;\">Set Level</button>";
  html += "</form>";
  html += "</div>";

  html += "<div style=\"margin-top:24px;padding-top:24px;border-top:1px solid var(--border-color);\">";
  html += "<h2>Bus Operations</h2>";
  html += "<div style=\"display:grid;grid-template-columns:1fr 1fr;gap:12px;\">";
  html += "<button onclick=\"scanBus()\" style=\"background:var(--accent-green);\">Scan Bus</button>";
  html += "<button onclick=\"startCommission()\" style=\"background:var(--accent-purple);\">Commission Devices</button>";
  html += "</div>";
  html += "<div id=\"scan-result\" style=\"margin-top:16px;\"></div>";
  html += "</div>";

  html += "<div style=\"margin-top:24px;padding-top:24px;border-top:1px solid var(--border-color);\">";
  html += "<h2>Passive Devices</h2>";
  html += "<p style=\"color:var(--text-secondary);font-size:14px;margin-bottom:12px;\">Devices discovered from bus traffic</p>";
  html += "<button onclick=\"loadPassiveDevices()\" style=\"background:var(--accent-green);margin-bottom:12px;\">Refresh</button>";
  html += "<div id=\"passive-devices\" style=\"font-size:13px;\"></div>";
  html += "</div>";

  html += "</div>";

  html += "<div class=\"card\" style=\"margin-top:20px;\">";
  html += "<h2>Recent Messages</h2>";
  html += "<p class=\"subtitle\">Last " + String(RECENT_MESSAGES_SIZE) + " DALI bus messages</p>";
  html += "<button onclick=\"loadRecentMessages()\" style=\"background:var(--accent-green);margin-bottom:16px;\">Refresh</button>";
  html += "<div id=\"recent-messages\" style=\"font-size:12px;font-family:monospace;max-height:300px;overflow-y:auto;\"></div>";
  html += "</div>";

  html += "<script>";
  html += "function sendCmd(cmd){";
  html += "const addr=document.getElementById('address').value;";
  html += "const data={command:cmd,address:parseInt(addr)};";
  html += "fetch('/dali/send',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(data)})";
  html += ".then(r=>r.json()).then(d=>showModal(d.title,d.message,d.success)).catch(e=>showModal('Error',e,false));}";

  html += "function sendDaliCommand(e){e.preventDefault();";
  html += "const addr=document.getElementById('address').value;";
  html += "const level=document.getElementById('level').value;";
  html += "const data={command:'set_brightness',address:parseInt(addr),level:parseInt(level)};";
  html += "fetch('/dali/send',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(data)})";
  html += ".then(r=>r.json()).then(d=>showModal(d.title,d.message,d.success)).catch(e=>showModal('Error',e,false));}";

  html += "function scanBus(){";
  html += "document.getElementById('scan-result').innerHTML='<p>Scanning...</p>';";
  html += "fetch('/dali/scan',{method:'POST'}).then(r=>r.json()).then(d=>{";
  html += "let html='<p>Found '+d.total_found+' devices:</p><ul>';";
  html += "d.devices.forEach(dev=>{html+='<li>Address '+dev.address+' (min:'+dev.min_level+', max:'+dev.max_level+')</li>';});";
  html += "html+='</ul>';document.getElementById('scan-result').innerHTML=html;";
  html += "}).catch(e=>{document.getElementById('scan-result').innerHTML='<p style=\"color:#ef4444;\">Scan failed</p>';});}";

  html += "function startCommission(){";
  html += "if(!confirm('Start commissioning? This will assign addresses to unaddressed devices.'))return;";
  html += "fetch('/dali/commission',{method:'POST'}).then(r=>r.json()).then(d=>showModal(d.title,d.message,d.success));}";

  html += "function loadPassiveDevices(){";
  html += "fetch('/api/passive_devices').then(r=>r.json()).then(d=>{";
  html += "let html='<table style=\"width:100%;border-collapse:collapse;\"><tr><th>Addr</th><th>Level</th><th>Last Seen</th></tr>';";
  html += "d.devices.forEach(dev=>{html+='<tr><td>'+dev.address+'</td><td>'+dev.level+'</td><td>'+dev.last_seen+'s ago</td></tr>';});";
  html += "html+='</table>';document.getElementById('passive-devices').innerHTML=html||'No devices discovered yet';";
  html += "}).catch(e=>{document.getElementById('passive-devices').innerHTML='Error loading devices';});}";

  html += "function loadRecentMessages(){";
  html += "fetch('/api/recent').then(r=>r.json()).then(d=>{";
  html += "let html='';d.forEach(msg=>{if(msg.timestamp>0){";
  html += "html+='<div style=\"border-bottom:1px solid var(--border-color);padding:8px;\">';";
  html += "html+='<strong>'+(msg.is_tx?'TX':'RX')+'</strong> '+msg.parsed.description+'<br>';";
  html += "html+='<span style=\"color:var(--text-secondary);\">Raw: '+msg.raw+'</span></div>';}});";
  html += "document.getElementById('recent-messages').innerHTML=html||'<p>No messages yet</p>';";
  html += "}).catch(e=>{document.getElementById('recent-messages').innerHTML='<p>Error loading messages</p>';});}";

  html += "loadRecentMessages();loadPassiveDevices();";
  html += "</script>";

  html += buildHTMLFooter();
  server.send(200, "text/html", html);
}

void handleDALISend() {
  if (!checkAuth()) return;

  String body = server.arg("plain");
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, body);

  if (error) {
    server.send(400, "application/json", "{\"success\":false,\"title\":\"Error\",\"message\":\"Invalid JSON\"}");
    return;
  }

  DaliCommand cmd;
  cmd.command_type = doc["command"].as<String>();
  cmd.address = doc["address"] | 0;
  cmd.level = doc["level"] | 254;
  cmd.scene = doc["scene"] | 0;
  cmd.force = doc["force"] | false;
  cmd.queued_at = millis();
  cmd.priority = 1;
  cmd.retry_count = 0;

  if (validateDaliCommand(cmd) || cmd.force) {
    if (enqueueDaliCommand(cmd)) {
      server.send(200, "application/json", "{\"success\":true,\"title\":\"✓ Command Queued\",\"message\":\"Command added to queue\"}");
    } else {
      server.send(500, "application/json", "{\"success\":false,\"title\":\"Error\",\"message\":\"Queue full\"}");
    }
  } else {
    server.send(400, "application/json", "{\"success\":false,\"title\":\"Error\",\"message\":\"Invalid command\"}");
  }
}

void handleDALIScan() {
  if (!checkAuth()) return;

  DaliScanResult result = scanDaliDevices();

  String json = "{\"scan_timestamp\":" + String(result.scan_timestamp) + ",\"devices\":[";
  for (size_t i = 0; i < result.devices.size(); i++) {
    if (i > 0) json += ",";
    const DaliDevice& dev = result.devices[i];
    json += "{\"address\":" + String(dev.address) + ",\"min_level\":" + String(dev.min_level) + ",\"max_level\":" + String(dev.max_level) + "}";
  }
  json += "],\"total_found\":" + String(result.total_found) + "}";

  server.send(200, "application/json", json);
}

void handleDALICommission() {
  if (!checkAuth()) return;

  uint8_t start_address = 0;
  if (server.hasArg("start_address")) {
    start_address = server.arg("start_address").toInt();
  }

  commissionDevices(start_address);

  server.send(200, "application/json", "{\"success\":true,\"title\":\"✓ Commissioning Started\",\"message\":\"Check progress via MQTT or API\"}");
}

void handleAPICommissionProgress() {
  if (!checkAuth()) return;

  String stateStr;
  switch (commissioningProgress.state) {
    case COMM_IDLE: stateStr = "idle"; break;
    case COMM_INITIALIZING: stateStr = "initializing"; break;
    case COMM_SEARCHING: stateStr = "searching"; break;
    case COMM_PROGRAMMING: stateStr = "programming"; break;
    case COMM_VERIFYING: stateStr = "verifying"; break;
    case COMM_COMPLETE: stateStr = "complete"; break;
    case COMM_ERROR: stateStr = "error"; break;
    default: stateStr = "unknown"; break;
  }

  String json = "{\"state\":\"" + stateStr + "\",";
  json += "\"devices_found\":" + String(commissioningProgress.devices_found) + ",";
  json += "\"devices_programmed\":" + String(commissioningProgress.devices_programmed) + ",";
  json += "\"progress_percent\":" + String(commissioningProgress.progress_percent) + ",";
  json += "\"status_message\":\"" + commissioningProgress.status_message + "\"}";

  server.send(200, "application/json", json);
}

void handleAPIRecent() {
  if (!checkAuth()) return;

  String json = "[";
  for (int i = 0; i < RECENT_MESSAGES_SIZE; i++) {
    int idx = (recentMessagesIndex - 1 - i + RECENT_MESSAGES_SIZE) % RECENT_MESSAGES_SIZE;
    const DaliMessage& msg = recentMessages[idx];
    if (msg.timestamp == 0) continue;

    if (json.length() > 1) json += ",";
    json += "{\"timestamp\":" + String(msg.timestamp) + ",";
    json += "\"is_tx\":" + String(msg.is_tx ? "true" : "false") + ",";
    json += "\"raw\":\"";
    for (int b = 0; b < msg.length; b++) {
      if (msg.raw_bytes[b] < 0x10) json += "0";
      json += String(msg.raw_bytes[b], HEX);
    }
    json += "\",\"parsed\":{\"type\":\"" + msg.parsed.type + "\",";
    json += "\"address\":" + String(msg.parsed.address) + ",";
    json += "\"description\":\"" + msg.description + "\"}}";
  }
  json += "]";

  server.send(200, "application/json", json);
}

void handleAPIPassiveDevices() {
  if (!checkAuth()) return;

  unsigned long now = millis();
  String json = "{\"devices\":[";
  bool first = true;

  for (int i = 0; i < DALI_MAX_ADDRESSES; i++) {
    if (passiveDevices[i].last_seen > 0) {
      if (!first) json += ",";
      first = false;
      unsigned long ago = (now - passiveDevices[i].last_seen) / 1000;
      json += "{\"address\":" + String(i) + ",";
      json += "\"level\":" + String(passiveDevices[i].last_level) + ",";
      json += "\"last_seen\":" + String(ago) + "}";
    }
  }
  json += "],\"count\":" + String(getPassiveDeviceCount()) + "}";

  server.send(200, "application/json", json);
}
