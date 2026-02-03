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

  html += "<div class=\"card\"><h1>DALI Control</h1>";
  html += "<p class=\"subtitle\">Send commands to DALI devices</p>";

  html += "<form action=\"/dali/send\" method=\"POST\" id=\"dali-form\" onsubmit=\"sendDaliCommand(event)\">";
  html += "<label for=\"address\">Device Address (0-63, 255=broadcast)</label>";
  html += "<input type=\"number\" id=\"address\" name=\"address\" value=\"0\" min=\"0\" max=\"255\" required>";

  html += "<label for=\"command\">Command</label>";
  html += "<select id=\"command\" name=\"command\" onchange=\"updateCommandForm()\" style=\"width:100%;padding:12px;border:2px solid var(--border-color);border-radius:8px;font-size:16px;margin-bottom:16px;background:var(--bg-primary);color:var(--text-primary);\">";
  html += "<option value=\"set_brightness\">Set Brightness</option>";
  html += "<option value=\"off\">Off</option>";
  html += "<option value=\"max\">Max</option>";
  html += "<option value=\"up\">Fade Up</option>";
  html += "<option value=\"down\">Fade Down</option>";
  html += "<option value=\"step_up\">Step Up</option>";
  html += "<option value=\"step_down\">Step Down</option>";
  html += "<option value=\"go_to_scene\">Go to Scene</option>";
  html += "<option value=\"query_status\">Query Status</option>";
  html += "<option value=\"query_actual_level\">Query Actual Level</option>";
  html += "</select>";

  html += "<div id=\"level-control\">";
  html += "<label for=\"level\">Brightness Level (0-254)</label>";
  html += "<input type=\"range\" id=\"level\" name=\"level\" value=\"128\" min=\"0\" max=\"254\" oninput=\"document.getElementById('level-value').innerText=this.value\">";
  html += "<p style=\"text-align:center;color:var(--text-secondary);margin-top:-8px;\">Level: <span id=\"level-value\">128</span></p>";
  html += "</div>";

  html += "<div id=\"scene-control\" style=\"display:none;\">";
  html += "<label for=\"scene\">Scene Number (0-15)</label>";
  html += "<input type=\"number\" id=\"scene\" name=\"scene\" value=\"0\" min=\"0\" max=\"15\">";
  html += "</div>";

  html += "<button type=\"submit\">Send Command</button>";
  html += "</form>";

  html += "<div style=\"margin-top:20px;\">";
  html += "<h2 style=\"font-size:16px;margin-bottom:12px;\">Quick Presets</h2>";
  html += "<div style=\"display:grid;grid-template-columns:1fr 1fr;gap:8px;\">";
  html += "<button onclick=\"sendPreset(255, 'off')\" style=\"width:auto;padding:10px;font-size:14px;\">All Off</button>";
  html += "<button onclick=\"sendPreset(255, 'max')\" style=\"width:auto;padding:10px;font-size:14px;\">All Max</button>";
  html += "<button onclick=\"sendPreset(255, 'set_brightness', 64)\" style=\"width:auto;padding:10px;font-size:14px;\">All 25%</button>";
  html += "<button onclick=\"sendPreset(255, 'set_brightness', 128)\" style=\"width:auto;padding:10px;font-size:14px;\">All 50%</button>";
  html += "</div></div>";

  html += "</div>";

  html += "<div class=\"card\"><h2>Device Scan</h2>";
  html += "<p class=\"subtitle\">Discover DALI devices on the bus</p>";
  html += "<button onclick=\"scanDevices()\">Scan for Devices</button>";
  html += "<div id=\"scan-results\" style=\"margin-top:16px;\"></div>";
  html += "</div>";

  html += "<div class=\"card\"><h2>Observed Devices</h2>";
  html += "<p class=\"subtitle\">Devices seen on the bus (passive discovery, no persistence)</p>";
  html += "<button onclick=\"refreshPassiveDevices()\">Refresh</button>";
  html += "<div id=\"passive-devices\" style=\"margin-top:16px;\"><p style=\"color:var(--text-secondary);\">Click Refresh to load...</p></div>";
  html += "</div>";

  html += "<div class=\"card\"><h2>Device Commissioning</h2>";
  html += "<p class=\"subtitle\">Automatically assign addresses to unaddressed devices</p>";
  html += "<div style=\"margin-bottom:16px;\">";
  html += "<label for=\"start-address\">Starting Address (0-63)</label>";
  html += "<input type=\"number\" id=\"start-address\" name=\"start-address\" value=\"0\" min=\"0\" max=\"63\" style=\"width:100%;padding:12px;border:2px solid var(--border-color);border-radius:8px;font-size:16px;margin-bottom:8px;background:var(--bg-primary);color:var(--text-primary);\">";
  html += "<p style=\"color:var(--text-secondary);font-size:13px;margin:0 0 12px 0;\">Devices will be assigned sequential addresses starting from this number</p>";
  html += "</div>";
  html += "<button onclick=\"startCommissioning()\" id=\"commission-btn\" style=\"background:var(--accent-purple);\">Start Commissioning</button>";
  html += "<div id=\"commission-progress\" style=\"display:none;margin-top:20px;padding:16px;background:var(--bg-secondary);border-radius:8px;\">";
  html += "<div style=\"display:flex;align-items:center;gap:12px;margin-bottom:12px;\">";
  html += "<div class=\"spinner\" style=\"width:24px;height:24px;border:3px solid var(--border-color);border-top-color:var(--accent-purple);border-radius:50%;animation:spin 1s linear infinite;\"></div>";
  html += "<h3 id=\"commission-state\" style=\"margin:0;font-size:16px;\">Initializing...</h3>";
  html += "</div>";
  html += "<div style=\"background:var(--bg-primary);border-radius:6px;height:8px;overflow:hidden;margin-bottom:12px;\">";
  html += "<div id=\"commission-bar\" style=\"height:100%;background:var(--accent-purple);width:0%;transition:width 0.3s;\"></div>";
  html += "</div>";
  html += "<p id=\"commission-message\" style=\"margin:0;font-size:14px;color:var(--text-secondary);\">Starting commissioning process...</p>";
  html += "<div id=\"commission-stats\" style=\"margin-top:12px;display:grid;grid-template-columns:1fr 1fr;gap:8px;font-size:13px;\">";
  html += "<div style=\"background:var(--bg-primary);padding:8px;border-radius:4px;\"><strong>Found:</strong> <span id=\"devices-found\">0</span></div>";
  html += "<div style=\"background:var(--bg-primary);padding:8px;border-radius:4px;\"><strong>Programmed:</strong> <span id=\"devices-programmed\">0</span></div>";
  html += "</div>";
  html += "</div>";
  html += "<div id=\"commission-results\" style=\"display:none;margin-top:16px;\"></div>";
  html += "</div>";

  html += "<style>@keyframes spin{to{transform:rotate(360deg);}}</style>";
  html += "<script>";
  html += "function sendDaliCommand(e){";
  html += "e.preventDefault();";
  html += "const form=new FormData(e.target);";
  html += "fetch('/dali/send',{method:'POST',body:form})";
  html += ".then(r=>r.json())";
  html += ".then(d=>showModal(d.title,d.message,d.success))";
  html += ".catch(e=>showModal('✗ Error','Failed to send command: '+e,false));}";
  html += "function updateCommandForm(){";
  html += "const cmd=document.getElementById('command').value;";
  html += "const addr=document.getElementById('address');";
  html += "const isQuery=cmd.startsWith('query_');";
  html += "if(isQuery){addr.max=63;if(addr.value>63)addr.value=0;}else{addr.max=255;}";
  html += "document.getElementById('level-control').style.display=(cmd==='set_brightness')?'block':'none';";
  html += "document.getElementById('scene-control').style.display=(cmd==='go_to_scene')?'block':'none';";
  html += "}";
  html += "function sendPreset(addr,cmd,level){";
  html += "const form=new FormData();form.append('address',addr);form.append('command',cmd);";
  html += "if(level!==undefined)form.append('level',level);";
  html += "fetch('/dali/send',{method:'POST',body:form})";
  html += ".then(r=>r.json())";
  html += ".then(d=>showModal(d.title,d.message,d.success))";
  html += ".catch(e=>showModal('✗ Error','Failed to send command: '+e,false));}";

  html += "function scanDevices(){";
  html += "document.getElementById('scan-results').innerHTML='<p>Scanning...</p>';";
  html += "fetch('/dali/scan',{method:'POST'}).then(r=>r.json()).then(d=>{";
  html += "let html='<p>Found '+d.total_found+' devices:</p><ul>';";
  html += "d.devices.forEach(dev=>html+='<li>Address '+dev.address+' - '+dev.status+'</li>');";
  html += "html+='</ul>';document.getElementById('scan-results').innerHTML=html;";
  html += "}).catch(e=>document.getElementById('scan-results').innerHTML='<p>Error: '+e+'</p>');";
  html += "}";
  html += "function refreshPassiveDevices(){";
  html += "document.getElementById('passive-devices').innerHTML='<p>Loading...</p>';";
  html += "fetch('/api/passive_devices').then(r=>r.json()).then(d=>{";
  html += "if(d.count===0){document.getElementById('passive-devices').innerHTML='<p style=\"color:var(--text-secondary);\">No devices observed yet. Traffic on the bus will be learned automatically.</p>';return;}";
  html += "let html='<p>'+d.count+' device(s) observed:</p>';";
  html += "html+='<div style=\"display:grid;grid-template-columns:repeat(auto-fill,minmax(120px,1fr));gap:8px;\">';";
  html += "d.devices.forEach(dev=>{";
  html += "let age=dev.last_seen<60?dev.last_seen+'s':(dev.last_seen<3600?Math.floor(dev.last_seen/60)+'m':Math.floor(dev.last_seen/3600)+'h');";
  html += "let lvl=dev.level>=0?dev.level:'?';";
  html += "html+='<div style=\"background:var(--bg-secondary);padding:10px;border-radius:6px;text-align:center;\">';";
  html += "html+='<div style=\"font-weight:600;font-size:18px;\">'+dev.address+'</div>';";
  html += "html+='<div style=\"font-size:12px;color:var(--text-secondary);\">Level: '+lvl+'</div>';";
  html += "html+='<div style=\"font-size:11px;color:var(--text-secondary);\">'+age+' ago</div>';";
  html += "html+='</div>';});";
  html += "html+='</div>';document.getElementById('passive-devices').innerHTML=html;";
  html += "}).catch(e=>document.getElementById('passive-devices').innerHTML='<p>Error: '+e+'</p>');";
  html += "}";
  html += "let commissionInterval=null;";
  html += "function startCommissioning(){";
  html += "const startAddr=document.getElementById('start-address').value;";
  html += "document.getElementById('commission-btn').disabled=true;";
  html += "document.getElementById('commission-progress').style.display='block';";
  html += "document.getElementById('commission-results').style.display='none';";
  html += "fetch('/dali/commission',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'start_address='+startAddr})";
  html += ".then(r=>r.json()).then(d=>{";
  html += "if(d.success){commissionInterval=setInterval(pollCommissionProgress,500);}";
  html += "else{showModal('✗ Error',d.message,false);document.getElementById('commission-btn').disabled=false;document.getElementById('commission-progress').style.display='none';}";
  html += "}).catch(e=>{showModal('✗ Error','Failed to start commissioning: '+e,false);document.getElementById('commission-btn').disabled=false;document.getElementById('commission-progress').style.display='none';});}";
  html += "function pollCommissionProgress(){";
  html += "fetch('/api/commission/progress').then(r=>r.json()).then(d=>{";
  html += "document.getElementById('commission-state').textContent=d.state.charAt(0).toUpperCase()+d.state.slice(1);";
  html += "document.getElementById('commission-bar').style.width=d.progress_percent+'%';";
  html += "document.getElementById('commission-message').textContent=d.status_message;";
  html += "document.getElementById('devices-found').textContent=d.devices_found;";
  html += "document.getElementById('devices-programmed').textContent=d.devices_programmed;";
  html += "if(d.state==='complete'||d.state==='error'){";
  html += "clearInterval(commissionInterval);";
  html += "document.getElementById('commission-btn').disabled=false;";
  html += "setTimeout(()=>{document.getElementById('commission-progress').style.display='none';";
  html += "let resultHtml='<div style=\"padding:16px;background:var(--bg-secondary);border-radius:8px;\">';";
  html += "if(d.state==='complete'){";
  html += "resultHtml+='<h3 style=\"color:var(--accent-green);margin:0 0 8px 0;\">✓ Commissioning Complete</h3>';";
  html += "resultHtml+='<p style=\"margin:0;\">Successfully programmed '+d.devices_programmed+' device(s)</p>';";
  html += "if(d.devices_programmed>0){resultHtml+='<p style=\"margin:8px 0 0 0;font-size:13px;color:var(--text-secondary);\">Addresses '+d.current_address+' to '+(d.next_free_address-1)+'</p>';}";
  html += "}else{";
  html += "resultHtml+='<h3 style=\"color:#ef4444;margin:0 0 8px 0;\">✗ Commissioning Failed</h3>';";
  html += "resultHtml+='<p style=\"margin:0;\">'+d.status_message+'</p>';}";
  html += "resultHtml+='</div>';";
  html += "document.getElementById('commission-results').innerHTML=resultHtml;";
  html += "document.getElementById('commission-results').style.display='block';},500);}";
  html += "}).catch(e=>console.error('Progress poll error:',e));}";
  html += "</script>";

  html += buildHTMLFooter();
  server.send(200, "text/html", html);
}

void handleDALISend() {
  if (!checkAuth()) return;

  String command = server.arg("command");
  uint8_t address = server.arg("address").toInt();
  uint8_t level = server.arg("level").toInt();
  uint8_t scene = server.arg("scene").toInt();

  DaliCommand cmd;
  cmd.command_type = command;
  cmd.address = address;
  cmd.address_type = (address == 0xFF) ? "broadcast" : "short";
  cmd.level = level;
  cmd.scene = scene;
  cmd.group = 0;
  cmd.fade_time = 0;
  cmd.fade_rate = 0;
  cmd.force = false;
  cmd.queued_at = millis();

  bool valid = validateDaliCommand(cmd);
  String json = "{";
  if (valid && enqueueDaliCommand(cmd)) {
    json += "\"success\":true,";
    json += "\"title\":\"✓ Command Sent\",";
    json += "\"message\":\"Command queued successfully and will be sent to the DALI bus.\"";
    json += "}";
    server.send(200, "application/json", json);
  } else {
    json += "\"success\":false,";
    json += "\"title\":\"✗ Command Failed\",";
    json += "\"message\":\"Invalid command or queue full. Please check your parameters and try again.\"";
    json += "}";
    server.send(400, "application/json", json);
  }
}

void handleDALIScan() {
  DaliScanResult result = scanDaliDevices();

  String json = "{\"scan_timestamp\":" + String(result.scan_timestamp) + ",\"devices\":[";
  for (size_t i = 0; i < result.devices.size(); i++) {
    if (i > 0) json += ",";
    const DaliDevice& dev = result.devices[i];
    json += "{\"address\":" + String(dev.address) + ",\"status\":\"ok\"}";
  }
  json += "],\"total_found\":" + String(result.total_found) + "}";

  server.send(200, "application/json", json);
}

void handleDALICommission() {
  uint8_t start_address = 0;
  if (server.hasArg("start_address")) {
    start_address = server.arg("start_address").toInt();
    if (start_address > 63) start_address = 0;
  }

#ifdef DEBUG_SERIAL
  Serial.printf("[Web] Starting commissioning from address %d\n", start_address);
#endif

  String json = "{";
  json += "\"success\":true,";
  json += "\"message\":\"Commissioning started\"";
  json += "}";
  server.send(200, "application/json", json);

  commissionDevices(start_address);
}

void handleAPICommissionProgress() {
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
  json += "\"start_timestamp\":" + String(commissioningProgress.start_timestamp) + ",";
  json += "\"devices_found\":" + String(commissioningProgress.devices_found) + ",";
  json += "\"devices_programmed\":" + String(commissioningProgress.devices_programmed) + ",";
  json += "\"current_address\":" + String(commissioningProgress.current_address) + ",";
  json += "\"next_free_address\":" + String(commissioningProgress.next_free_address) + ",";
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
