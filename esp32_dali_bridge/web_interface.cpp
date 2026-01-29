#include "web_interface.h"
#include "version.h"
#include "config.h"
#include "dali_handler.h"
#include "diagnostics.h"
#include "utils.h"
#include "mqtt_handler.h"
#include "logos.h"
#include <WiFi.h>
#include <Preferences.h>
#include <SPIFFS.h>
#include <Update.h>
#include "esp_task_wdt.h"

WebServer server(80);
String webInterfaceUsername = "";
String webInterfacePassword = "";

extern String storedSSID;
extern String storedPassword;
extern bool wifiConnected;
extern bool apMode;

bool checkAuth() {
  if (webInterfacePassword.length() == 0) return true;
  String username = webInterfaceUsername.length() > 0 ? webInterfaceUsername : "admin";
  if (!server.authenticate(username.c_str(), webInterfacePassword.c_str())) {
    server.requestAuthentication();
    return false;
  }
  return true;
}

void webInit() {
  Preferences prefs;
  prefs.begin("web", true);
  webInterfaceUsername = prefs.getString("username", "admin");
  webInterfacePassword = prefs.getString("password", "");
  prefs.end();

  server.on("/", handleRoot);
  server.on("/dashboard", handleDashboard);
  server.on("/config", handleWiFiConfig);
  server.on("/save", HTTP_POST, handleWiFiSave);
  server.on("/save_web_auth", HTTP_POST, handleWebAuthSave);
  server.on("/mqtt", HTTP_GET, handleMQTTConfig);
  server.on("/mqtt", HTTP_POST, handleMQTTSave);
  server.on("/dali", handleDALIControl);
  server.on("/dali/send", HTTP_POST, handleDALISend);
  server.on("/dali/scan", HTTP_POST, handleDALIScan);
  server.on("/dali/commission", HTTP_POST, handleDALICommission);
  server.on("/api/commission/progress", handleAPICommissionProgress);
  server.on("/diagnostics", HTTP_GET, handleSettings);
  server.on("/diagnostics", HTTP_POST, handleSettingsSave);
  server.on("/update", HTTP_GET, handleOTAPage);
  server.on("/update", HTTP_POST, []() {
    server.send(200, "text/plain", Update.hasError() ? "FAIL" : "OK");
    delay(2000);
    ESP.restart();
  }, handleOTAUpload);
  server.on("/logo_light", handleLogoLight);
  server.on("/logo_dark", handleLogoDark);
  server.on("/api/status", handleAPIStatus);
  server.on("/api/diagnostics", handleAPIDiagnostics);
  server.on("/api/recent", handleAPIRecent);
  server.on("/api/passive_devices", handleAPIPassiveDevices);
  server.on("/api/reboot", HTTP_POST, []() {
    server.send(200, "text/plain", "Rebooting...");
    delay(100);
    ESP.restart();
  });
  server.onNotFound(handleRoot);

  server.begin();
#ifdef DEBUG_SERIAL
  Serial.println("Web server started");
#endif
}

void webLoop() {
  server.handleClient();
}

void handleRoot() {
  if (!checkAuth()) return;
  if (apMode) {
    server.sendHeader("Location", "/config");
    server.send(302, "text/plain", "");
  } else {
    handleDashboard();
  }
}

void handleDashboard() {
  Preferences prefs;
  prefs.begin("mqtt", true);
  String broker = prefs.getString("broker", "");
  String prefix = prefs.getString("prefix", "home/dali/");
  prefs.end();

  bool mqttConfigured = (broker.length() > 0);
  bool mqttConnected = mqttConfigured && mqttClient.connected();

  String html = buildHTMLHeader("Home");
  html += "<div class=\"card\">";
  html += "<h1>ESP32 DALI Bridge</h1>";
  html += "<p class=\"subtitle\">System Home</p>";

  html += "<div class=\"status\">";
  html += "<div class=\"dot " + String(wifiConnected ? "green" : "yellow") + "\"></div>";
  html += "<span>WiFi: " + String(wifiConnected ? "Connected (" + WiFi.localIP().toString() + ")" : "AP Mode") + "</span>";
  html += "</div>";

  html += "<div class=\"status\">";
  html += "<div class=\"dot green\"></div>";
  html += "<span>DALI: Ready</span>";
  html += "</div>";

  html += "<div class=\"status\">";
  html += "<div class=\"dot " + String(mqttConnected ? "green" : (mqttConfigured ? "yellow" : "red")) + "\"></div>";
  html += "<span>MQTT: " + String(mqttConnected ? "Connected" : (mqttConfigured ? "Configured (not connected)" : "Not configured")) + "</span>";
  html += "</div>";

  if (mqttConnected) {
    html += "<div style=\"margin-top:20px;padding:16px;background:var(--bg-secondary);border-radius:8px;\">";
    html += "<h3 style=\"margin:0 0 12px 0;font-size:16px;cursor:pointer;\" onclick=\"document.getElementById('mqtt-topics').style.display=document.getElementById('mqtt-topics').style.display==='none'?'block':'none'\">📡 MQTT Topics ▼</h3>";
    html += "<div id=\"mqtt-topics\" style=\"display:none;font-size:13px;\">";

    html += "<p style=\"margin:12px 0 4px 0;color:var(--text-secondary);\"><strong>Command:</strong> <code style=\"background:var(--bg-primary);padding:2px 6px;border-radius:3px;font-family:monospace;\">" + prefix + "command</code></p>";
    html += "<p style=\"margin:0 0 8px 0;font-size:12px;color:var(--text-secondary);\">Subscribe to this topic to send commands to DALI devices</p>";
    html += "<details style=\"margin:8px 0;padding:8px;background:var(--bg-primary);border-radius:4px;\">";
    html += "<summary style=\"cursor:pointer;color:var(--accent-green);font-weight:500;\">▸ Example: Command</summary>";
    html += "<pre style=\"background:var(--bg-secondary);padding:12px;border-radius:6px;overflow-x:auto;margin:8px 0;font-size:12px;\">{<br>  \"command\": \"set_brightness\",<br>  \"address\": 0,<br>  \"level\": 128<br>}</pre></details>";

    html += "<p style=\"margin:12px 0 4px 0;color:var(--text-secondary);\"><strong>Monitor:</strong> <code style=\"background:var(--bg-primary);padding:2px 6px;border-radius:3px;font-family:monospace;\">" + prefix + "monitor</code></p>";
    html += "<p style=\"margin:0 0 8px 0;font-size:12px;color:var(--text-secondary);\">Publishes all DALI bus activity with source field (self/bus)</p>";
    html += "<details style=\"margin:8px 0;padding:8px;background:var(--bg-primary);border-radius:4px;\">";
    html += "<summary style=\"cursor:pointer;color:var(--accent-green);font-weight:500;\">▸ Example: Monitor Message (self)</summary>";
    html += "<pre style=\"background:var(--bg-secondary);padding:12px;border-radius:6px;overflow-x:auto;margin:8px 0;font-size:12px;\">{<br>  \"timestamp\": 1234567890,<br>  \"direction\": \"tx\",<br>  \"source\": \"self\",<br>  \"raw\": \"01FE\",<br>  \"parsed\": {<br>    \"type\": \"direct_arc_power\",<br>    \"address\": 0,<br>    \"level\": 254,<br>    \"level_percent\": 99.6,<br>    \"description\": \"Device 0: Set to 254 (99.6%)\"<br>  }<br>}</pre></details>";

    html += "<details style=\"margin:8px 0;padding:8px;background:var(--bg-primary);border-radius:4px;\">";
    html += "<summary style=\"cursor:pointer;color:var(--accent-green);font-weight:500;\">▸ Example: Monitor Message (bus)</summary>";
    html += "<pre style=\"background:var(--bg-secondary);padding:12px;border-radius:6px;overflow-x:auto;margin:8px 0;font-size:12px;\">{<br>  \"timestamp\": 1234567891,<br>  \"direction\": \"rx\",<br>  \"source\": \"bus\",<br>  \"raw\": \"0380\",<br>  \"parsed\": {<br>    \"type\": \"direct_arc_power\",<br>    \"address\": 1,<br>    \"level\": 128,<br>    \"level_percent\": 50.4,<br>    \"description\": \"Device 1: Set to 128 (50.4%)\"<br>  }<br>}</pre></details>";

    html += "<p style=\"margin:12px 0 4px 0;color:var(--text-secondary);\"><strong>Status:</strong> <code style=\"background:var(--bg-primary);padding:2px 6px;border-radius:3px;font-family:monospace;\">" + prefix + "status</code></p>";
    html += "<p style=\"margin:0 0 8px 0;font-size:12px;color:var(--text-secondary);\">Device status published on connect and periodically</p>";
    html += "<details style=\"margin:8px 0;padding:8px;background:var(--bg-primary);border-radius:4px;\">";
    html += "<summary style=\"cursor:pointer;color:var(--accent-green);font-weight:500;\">▸ Example: Status</summary>";
    html += "<pre style=\"background:var(--bg-secondary);padding:12px;border-radius:6px;overflow-x:auto;margin:8px 0;font-size:12px;\">{<br>  \"status\": \"online\",<br>  \"uptime\": 12345,<br>  \"ip\": \"192.168.1.100\"<br>}</pre></details>";

    html += "<p style=\"margin:12px 0 4px 0;color:var(--text-secondary);\"><strong>Scan Trigger:</strong> <code style=\"background:var(--bg-primary);padding:2px 6px;border-radius:3px;font-family:monospace;\">" + prefix + "scan/trigger</code></p>";
    html += "<p style=\"margin:0 0 8px 0;font-size:12px;color:var(--text-secondary);\">Publish to this topic to trigger a DALI bus scan</p>";
    html += "<details style=\"margin:8px 0;padding:8px;background:var(--bg-primary);border-radius:4px;\">";
    html += "<summary style=\"cursor:pointer;color:var(--accent-green);font-weight:500;\">▸ Example JSON</summary>";
    html += "<pre style=\"background:var(--bg-secondary);padding:12px;border-radius:6px;overflow-x:auto;margin:8px 0;font-size:12px;\">{<br>  \"action\": \"scan\"<br>}</pre></details>";

    html += "<p style=\"margin:12px 0 4px 0;color:var(--text-secondary);\"><strong>Scan Result:</strong> <code style=\"background:var(--bg-primary);padding:2px 6px;border-radius:3px;font-family:monospace;\">" + prefix + "scan/result</code></p>";
    html += "<p style=\"margin:0 0 8px 0;font-size:12px;color:var(--text-secondary);\">Scan results are published to this topic after completion</p>";
    html += "<details style=\"margin:8px 0;padding:8px;background:var(--bg-primary);border-radius:4px;\">";
    html += "<summary style=\"cursor:pointer;color:var(--accent-green);font-weight:500;\">▸ Example JSON</summary>";
    html += "<pre style=\"background:var(--bg-secondary);padding:12px;border-radius:6px;overflow-x:auto;margin:8px 0;font-size:12px;\">{<br>  \"scan_timestamp\": 1234567890,<br>  \"total_found\": 2,<br>  \"devices\": [<br>    {\"address\": 0, \"status\": \"ok\"},<br>    {\"address\": 1, \"status\": \"ok\"}<br>  ]<br>}</pre></details>";

    html += "<hr style=\"margin:20px 0;border:none;border-top:1px solid var(--border-color);\">";
    html += "<h3 style=\"margin:16px 0 8px 0;font-size:15px;color:var(--accent-purple);\">🔧 Commissioning (Auto-Addressing)</h3>";
    
    html += "<p style=\"margin:12px 0 4px 0;color:var(--text-secondary);\"><strong>Commission Trigger:</strong> <code style=\"background:var(--bg-primary);padding:2px 6px;border-radius:3px;font-family:monospace;\">" + prefix + "commission/trigger</code></p>";
    html += "<p style=\"margin:0 0 8px 0;font-size:12px;color:var(--text-secondary);\">Publish to this topic to start automatic device commissioning (assigns addresses to unaddressed devices)</p>";
    html += "<details style=\"margin:8px 0;padding:8px;background:var(--bg-primary);border-radius:4px;\">";
    html += "<summary style=\"cursor:pointer;color:var(--accent-green);font-weight:500;\">▸ Example: Start from address 0</summary>";
    html += "<pre style=\"background:var(--bg-secondary);padding:12px;border-radius:6px;overflow-x:auto;margin:8px 0;font-size:12px;\">mosquitto_pub -t \"" + prefix + "commission/trigger\" -m \"0\"</pre>";
    html += "<p style=\"margin:4px 0;font-size:11px;color:var(--text-secondary);\">Payload: Starting address (0-63) or empty for 0</p></details>";
    
    html += "<details style=\"margin:8px 0;padding:8px;background:var(--bg-primary);border-radius:4px;\">";
    html += "<summary style=\"cursor:pointer;color:var(--accent-green);font-weight:500;\">▸ Example: Start from address 10</summary>";
    html += "<pre style=\"background:var(--bg-secondary);padding:12px;border-radius:6px;overflow-x:auto;margin:8px 0;font-size:12px;\">mosquitto_pub -t \"" + prefix + "commission/trigger\" -m \"10\"</pre>";
    html += "<p style=\"margin:4px 0;font-size:11px;color:var(--text-secondary);\">Useful if you already have devices at addresses 0-9</p></details>";

    html += "<p style=\"margin:12px 0 4px 0;color:var(--text-secondary);\"><strong>Commission Progress:</strong> <code style=\"background:var(--bg-primary);padding:2px 6px;border-radius:3px;font-family:monospace;\">" + prefix + "commission/progress</code></p>";
    html += "<p style=\"margin:0 0 8px 0;font-size:12px;color:var(--text-secondary);\">Real-time progress updates during commissioning process</p>";
    html += "<details style=\"margin:8px 0;padding:8px;background:var(--bg-primary);border-radius:4px;\">";
    html += "<summary style=\"cursor:pointer;color:var(--accent-green);font-weight:500;\">▸ Example: Subscribe to progress</summary>";
    html += "<pre style=\"background:var(--bg-secondary);padding:12px;border-radius:6px;overflow-x:auto;margin:8px 0;font-size:12px;\">mosquitto_sub -t \"" + prefix + "commission/progress\"</pre></details>";
    
    html += "<details style=\"margin:8px 0;padding:8px;background:var(--bg-primary);border-radius:4px;\">";
    html += "<summary style=\"cursor:pointer;color:var(--accent-green);font-weight:500;\">▸ Example: Progress Message</summary>";
    html += "<pre style=\"background:var(--bg-secondary);padding:12px;border-radius:6px;overflow-x:auto;margin:8px 0;font-size:12px;\">{<br>  \"state\": \"programming\",<br>  \"start_timestamp\": 12345678,<br>  \"devices_found\": 3,<br>  \"devices_programmed\": 1,<br>  \"current_address\": 5,<br>  \"next_free_address\": 6,<br>  \"current_random_address\": \"0xABCDEF\",<br>  \"status_message\": \"Programming device 2/3\",<br>  \"progress_percent\": 75<br>}</pre>";
    html += "<p style=\"margin:4px 0;font-size:11px;color:var(--text-secondary);\"><strong>States:</strong> idle, initializing, searching, programming, verifying, complete, error</p></details>";

    html += "</div></div>";
  }

  html += "</div>";

  // Recent Messages section
  html += "<div class=\"card\" style=\"margin-top:20px;\"><h2>Recent Messages</h2>";
  html += "<p class=\"subtitle\">Last 20 DALI bus messages</p>";
  html += "<button onclick=\"loadRecentMessages()\" style=\"background:var(--accent-green);margin-bottom:16px;\">Refresh Messages</button>";
  html += "<div id=\"recent-messages\" style=\"font-size:12px;font-family:monospace;max-height:300px;overflow-y:auto;\"></div>";
  html += "</div>";

  html += "<script>";
  html += "function loadRecentMessages(){";
  html += "fetch('/api/recent').then(r=>r.json()).then(d=>{";
  html += "let html='';";
  html += "d.forEach(msg=>{if(msg.timestamp>0){";
  html += "html+='<div style=\"border-bottom:1px solid var(--border-color);padding:8px;\">';";
  html += "html+='<strong>'+(msg.is_tx?'TX':'RX')+'</strong> '+msg.parsed.description+'<br>';";
  html += "html+='<span style=\"color:var(--text-secondary);\">Raw: '+msg.raw+'</span>';";
  html += "html+='</div>';}});";
  html += "document.getElementById('recent-messages').innerHTML=html||'<p>No messages yet</p>';";
  html += "}).catch(e=>document.getElementById('recent-messages').innerHTML='<p>Error loading messages</p>');";
  html += "}";
  html += "loadRecentMessages();";
  html += "</script>";

  html += buildHTMLFooter();
  server.send(200, "text/html", html);
}

void handleWiFiConfig() {
  String html = buildHTMLHeader("WiFi Configuration");
  html += "<div class=\"card\">";
  html += "<h1>WiFi Setup</h1>";
  html += "<p class=\"subtitle\">Configure your WiFi connection</p>";
  html += "<form action=\"/save\" method=\"POST\">";
  html += "<label for=\"ssid\">WiFi Network Name (SSID)</label>";
  html += "<input type=\"text\" id=\"ssid\" name=\"ssid\" value=\"" + storedSSID + "\" required>";
  html += "<label for=\"password\">Password</label>";
  html += "<div class=\"input-wrapper\">";
  html += "<input type=\"password\" id=\"password\" name=\"password\" value=\"" + storedPassword + "\" placeholder=\"WiFi password\">";
  html += "<span class=\"eye-icon\" id=\"password-eye\" onclick=\"togglePassword('password')\"></span>";
  html += "</div>";
  html += "<button type=\"submit\" style=\"margin-top:16px;\">Save WiFi Settings</button>";
  html += "</form>";
  html += "</div>";

  Preferences webPrefs;
  webPrefs.begin("web", true);
  String webUsername = webPrefs.getString("username", "admin");
  String webPassword = webPrefs.getString("password", "");
  webPrefs.end();

  html += "<div class=\"card\" style=\"margin-top:20px;\">";
  html += "<h2>Web Interface Security</h2>";
  html += "<form method=\"POST\" action=\"/save_web_auth\">";
  html += "<label for=\"web_username\">Web Interface Username</label>";
  html += "<input type=\"text\" id=\"web_username\" name=\"web_username\" value=\"" + webUsername + "\" placeholder=\"admin\">";
  html += "<label for=\"web_password\">Web Interface Password</label>";
  html += "<div class=\"input-wrapper\">";
  html += "<input type=\"password\" id=\"web_password\" name=\"web_password\" value=\"" + webPassword + "\" placeholder=\"Leave empty for no password\">";
  html += "<span class=\"eye-icon\" id=\"web_password-eye\" onclick=\"togglePassword('web_password')\"></span>";
  html += "</div>";
  html += "<p style=\"color:var(--text-secondary);font-size:14px;margin-top:8px;\">Set a username and password to protect access to the web interface. Leave password empty to disable password protection.</p>";
  html += "<button type=\"submit\">Save Settings</button>";
  html += "</form>";
  html += "</div>";
  html += buildHTMLFooter();
  server.send(200, "text/html", html);
}

void handleWiFiSave() {
  String ssid = server.arg("ssid");
  String password = server.arg("password");

  Preferences prefs;
  prefs.begin("wifi", false);
  prefs.putString("ssid", ssid);
  prefs.putString("password", password);
  prefs.end();

  String html = buildHTMLHeader("Settings Saved");
  html += "<div class=\"card\">";
  html += "<h1>Settings Saved!</h1>";
  html += "<p>The ESP32 will now restart and connect to your WiFi network.</p>";
  html += "</div>";
  html += buildHTMLFooter();
  server.send(200, "text/html", html);

  delay(2000);
  ESP.restart();
}

void handleWebAuthSave() {
  String webUsername = server.arg("web_username");
  String webPassword = server.arg("web_password");

  Preferences prefs;
  prefs.begin("web", false);
  prefs.putString("username", webUsername);
  prefs.putString("password", webPassword);
  prefs.end();

  webInterfaceUsername = webUsername;
  webInterfacePassword = webPassword;

  String html = buildHTMLHeader("Settings Saved");
  html += "<div class=\"card\">";
  html += "<h1>Settings Saved!</h1>";
  html += "<p>Web interface authentication has been updated.</p>";
  html += "<p><a href=\"/config\" style=\"color:var(--accent-green);\">← Back to WiFi Settings</a></p>";
  html += "</div>";
  html += buildHTMLFooter();
  server.send(200, "text/html", html);
}

void handleMQTTConfig() {
  Preferences prefs;
  prefs.begin("mqtt", true);
  String broker = prefs.getString("broker", "");
  uint16_t port = prefs.getUShort("port", 1883);
  String user = prefs.getString("user", "");
  String pass = prefs.getString("pass", "");
  String prefix = prefs.getString("prefix", "home/dali/");
  uint8_t qos = prefs.getUChar("qos", 0);

  uint64_t mac = ESP.getEfuseMac();
  String defaultClientId = "dali-bridge-" + String((uint32_t)(mac & 0xFFFFFF), HEX);
  String clientId = prefs.getString("client_id", defaultClientId);

  // Load monitor filter settings
  bool mon_dapc = prefs.getBool("mon_dapc", true);
  bool mon_ctrl = prefs.getBool("mon_ctrl", true);
  bool mon_query = prefs.getBool("mon_query", true);
  bool mon_resp = prefs.getBool("mon_resp", true);
  bool mon_comm = prefs.getBool("mon_comm", true);
  bool mon_self = prefs.getBool("mon_self", true);
  bool mon_bus = prefs.getBool("mon_bus", true);

  prefs.end();

  String html = buildHTMLHeader("MQTT Configuration");
  html += "<div class=\"card\"><h1>MQTT Settings</h1>";
  html += "<p class=\"subtitle\">Configure MQTT broker connection</p>";
  html += "<form action=\"/mqtt\" method=\"POST\">";
  html += "<label for=\"broker\">MQTT Broker Host</label>";
  html += "<input type=\"text\" id=\"broker\" name=\"broker\" value=\"" + broker + "\" placeholder=\"mqtt.example.com or 192.168.1.100\">";
  html += "<label for=\"port\">MQTT Port</label>";
  html += "<input type=\"text\" id=\"port\" name=\"port\" value=\"" + String(port) + "\" placeholder=\"1883\">";
  html += "<label for=\"user\">Username (optional)</label>";
  html += "<input type=\"text\" id=\"user\" name=\"user\" value=\"" + user + "\" placeholder=\"Leave empty if not required\">";
  html += "<label for=\"pass\">Password (optional)</label>";
  html += "<div class=\"input-wrapper\">";
  html += "<input type=\"password\" id=\"pass\" name=\"pass\" value=\"" + pass + "\" placeholder=\"Leave empty if not required\">";
  html += "<span class=\"eye-icon\" id=\"pass-eye\" onclick=\"togglePassword('pass')\"></span>";
  html += "</div>";
  html += "<label for=\"prefix\">Topic Prefix</label>";
  html += "<input type=\"text\" id=\"prefix\" name=\"prefix\" value=\"" + prefix + "\" placeholder=\"home/dali/\">";
  html += "<label for=\"client_id\">Client ID</label>";
  html += "<input type=\"text\" id=\"client_id\" name=\"client_id\" value=\"" + clientId + "\" placeholder=\"" + defaultClientId + "\">";
  html += "<label for=\"qos\">QoS Level</label>";
  html += "<select id=\"qos\" name=\"qos\" style=\"width:100%;padding:12px;border:2px solid var(--border-color);border-radius:8px;font-size:16px;margin-bottom:16px;background:var(--bg-primary);color:var(--text-primary);\">";
  html += "<option value=\"0\"" + String(qos == 0 ? " selected" : "") + ">0 - At most once</option>";
  html += "<option value=\"1\"" + String(qos == 1 ? " selected" : "") + ">1 - At least once</option>";
  html += "<option value=\"2\"" + String(qos == 2 ? " selected" : "") + ">2 - Exactly once</option>";
  html += "</select>";
  
  // Monitor filter section
  html += "<h2 style=\"margin-top:24px;margin-bottom:12px;font-size:18px;\">Monitor Topic Filters</h2>";
  html += "<p style=\"margin-bottom:16px;color:var(--text-secondary);font-size:14px;\">Select which message types to publish to the monitor topic</p>";
  html += "<div style=\"display:grid;gap:8px;\">";
  html += "<label style=\"display:flex;align-items:center;cursor:pointer;\">";
  html += "<input type=\"checkbox\" name=\"mon_dapc\" value=\"1\"" + String(mon_dapc ? " checked" : "") + " style=\"margin-right:8px;\">";
  html += "<span>DAPC (Direct Arc Power / Brightness)</span></label>";
  html += "<label style=\"display:flex;align-items:center;cursor:pointer;\">";
  html += "<input type=\"checkbox\" name=\"mon_ctrl\" value=\"1\"" + String(mon_ctrl ? " checked" : "") + " style=\"margin-right:8px;\">";
  html += "<span>Control Commands (Off, Up, Down, etc.)</span></label>";
  html += "<label style=\"display:flex;align-items:center;cursor:pointer;\">";
  html += "<input type=\"checkbox\" name=\"mon_query\" value=\"1\"" + String(mon_query ? " checked" : "") + " style=\"margin-right:8px;\">";
  html += "<span>Query Commands (Status, Level, etc.)</span></label>";
  html += "<label style=\"display:flex;align-items:center;cursor:pointer;\">";
  html += "<input type=\"checkbox\" name=\"mon_resp\" value=\"1\"" + String(mon_resp ? " checked" : "") + " style=\"margin-right:8px;\">";
  html += "<span>Responses (Backward frames from ballasts)</span></label>";
  html += "<label style=\"display:flex;align-items:center;cursor:pointer;\">";
  html += "<input type=\"checkbox\" name=\"mon_comm\" value=\"1\"" + String(mon_comm ? " checked" : "") + " style=\"margin-right:8px;\">";
  html += "<span>Commissioning Commands</span></label>";
  html += "<label style=\"display:flex;align-items:center;cursor:pointer;\">";
  html += "<input type=\"checkbox\" name=\"mon_self\" value=\"1\"" + String(mon_self ? " checked" : "") + " style=\"margin-right:8px;\">";
  html += "<span>Self-sent (Commands from this bridge)</span></label>";
  html += "<label style=\"display:flex;align-items:center;cursor:pointer;\">";
  html += "<input type=\"checkbox\" name=\"mon_bus\" value=\"1\"" + String(mon_bus ? " checked" : "") + " style=\"margin-right:8px;\">";
  html += "<span>Bus Traffic (Commands from other devices)</span></label>";
  html += "</div>";
  
  html += "<button type=\"submit\" style=\"margin-top:24px;\">Save MQTT Settings</button>";
  html += "</form></div>";
  html += buildHTMLFooter();
  server.send(200, "text/html", html);
}

void handleMQTTSave() {
  String broker = server.arg("broker");
  uint16_t port = server.arg("port").toInt();
  String user = server.arg("user");
  String pass = server.arg("pass");
  String prefix = server.arg("prefix");
  String clientId = server.arg("client_id");
  uint8_t qos = server.arg("qos").toInt();

  if (port == 0) port = 1883;
  if (prefix.length() == 0) prefix = "home/dali/";
  if (!prefix.endsWith("/")) prefix += "/";

  // Get monitor filter settings from checkboxes
  bool mon_dapc = server.hasArg("mon_dapc");
  bool mon_ctrl = server.hasArg("mon_ctrl");
  bool mon_query = server.hasArg("mon_query");
  bool mon_resp = server.hasArg("mon_resp");
  bool mon_comm = server.hasArg("mon_comm");
  bool mon_self = server.hasArg("mon_self");
  bool mon_bus = server.hasArg("mon_bus");

  Preferences prefs;
  prefs.begin("mqtt", false);
  prefs.putString("broker", broker);
  prefs.putUShort("port", port);
  prefs.putString("user", user);
  prefs.putString("pass", pass);
  prefs.putString("prefix", prefix);
  prefs.putString("client_id", clientId);
  prefs.putUChar("qos", qos);
  
  // Save monitor filter settings
  prefs.putBool("mon_dapc", mon_dapc);
  prefs.putBool("mon_ctrl", mon_ctrl);
  prefs.putBool("mon_query", mon_query);
  prefs.putBool("mon_resp", mon_resp);
  prefs.putBool("mon_comm", mon_comm);
  prefs.putBool("mon_self", mon_self);
  prefs.putBool("mon_bus", mon_bus);
  
  prefs.end();

  String html = buildHTMLHeader("MQTT Settings Saved");
  html += "<div class=\"card\">";
  html += "<h1 style=\"color:var(--accent-green);\">MQTT Settings Saved</h1>";
  html += "<p>MQTT configuration has been updated. The device will now reboot to apply the new settings.</p>";
  html += "<p style=\"margin-top:20px;color:var(--text-secondary);\">Rebooting in <span id=\"countdown\">3</span> seconds...</p>";
  html += "</div>";
  html += "<script>";
  html += "let count=3;";
  html += "const interval=setInterval(function(){";
  html += "count--;";
  html += "document.getElementById('countdown').textContent=count;";
  html += "if(count<=0){clearInterval(interval);fetch('/api/reboot',{method:'POST'});}";
  html += "},1000);";
  html += "</script>";
  html += buildHTMLFooter();
  server.send(200, "text/html", html);
}

void handleDALIControl() {
  String html = buildHTMLHeader("DALI Control");

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
  html += "<button onclick=\"sendPreset(0, 'off')\" style=\"width:auto;padding:10px;font-size:14px;\">All Off</button>";
  html += "<button onclick=\"sendPreset(0, 'max')\" style=\"width:auto;padding:10px;font-size:14px;\">All Max</button>";
  html += "<button onclick=\"sendPreset(0, 'set_brightness', 64)\" style=\"width:auto;padding:10px;font-size:14px;\">All 25%</button>";
  html += "<button onclick=\"sendPreset(0, 'set_brightness', 128)\" style=\"width:auto;padding:10px;font-size:14px;\">All 50%</button>";
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
  html += "const form=new FormData();form.append('address',addr||255);form.append('command',cmd);";
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
  String json = daliScanResultToJSON(result);
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
  String json = commissioningProgressToJSON(commissioningProgress);
  server.send(200, "application/json", json);
}

void handleSettings() {
  Preferences prefs;
  prefs.begin("settings", true);
  bool diag_enabled = prefs.getBool("diag_en", false);
  prefs.end();

  updateDiagnostics();

  String html = buildHTMLHeader("Diagnostics");

  html += "<div class=\"card\"><h1>System Information</h1>";
  html += "<div class=\"status-grid\" style=\"display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:16px;margin-top:20px\">";
  html += "<div class=\"status-item\" style=\"background:var(--bg-secondary);padding:16px;border-radius:8px\">";
  html += "<div class=\"status-label\" style=\"color:var(--text-secondary);font-size:12px;text-transform:uppercase;margin-bottom:8px\">Firmware Version</div>";
  html += "<div class=\"value-data\" style=\"color:var(--text-primary);font-size:20px;font-weight:600;margin-top:4px;\">" + String(FIRMWARE_VERSION) + "</div>";
  html += "</div>";
  html += "<div class=\"status-item\" style=\"background:var(--bg-secondary);padding:16px;border-radius:8px\">";
  html += "<div class=\"status-label\" style=\"color:var(--text-secondary);font-size:12px;text-transform:uppercase;margin-bottom:8px\">Uptime</div>";
  html += "<div class=\"value-data\" style=\"color:var(--text-primary);font-size:20px;font-weight:600;margin-top:4px;\">";
  unsigned long secs = stats.uptime_seconds;
  unsigned long mins = secs / 60;
  unsigned long hours = mins / 60;
  unsigned long days = hours / 24;
  secs %= 60; mins %= 60; hours %= 24;
  if (days > 0) html += String(days) + "d ";
  html += String(hours) + "h " + String(mins) + "m";
  html += "</div></div>";
  html += "<div class=\"status-item\" style=\"background:var(--bg-secondary);padding:16px;border-radius:8px\">";
  html += "<div class=\"status-label\" style=\"color:var(--text-secondary);font-size:12px;text-transform:uppercase;margin-bottom:8px\">Free Heap</div>";
  html += "<div class=\"value-data\" style=\"color:var(--text-primary);font-size:20px;font-weight:600;margin-top:4px;\">" + String(stats.free_heap_kb, 1) + " KB</div>";
  html += "</div>";
  html += "<div class=\"status-item\" style=\"background:var(--bg-secondary);padding:16px;border-radius:8px\">";
  html += "<div class=\"value-label\" style=\"color:var(--text-secondary);font-size:12px;text-transform:uppercase;margin-bottom:8px\">Chip Model</div>";
  html += "<div class=\"value-data\" style=\"color:var(--text-primary);font-size:20px;font-weight:600;margin-top:4px;\">" + String(ESP.getChipModel()) + "</div>";
  html += "</div></div></div>";

  html += "<div class=\"card\" style=\"margin-top:20px;\"><h1>Diagnostics</h1>";
  html += "<p class=\"subtitle\">Enable or disable diagnostics export via MQTT</p>";
  html += "<form action=\"/diagnostics\" method=\"POST\">";
  html += "<div style=\"display:flex;align-items:center;gap:12px;margin-bottom:16px;\">";
  html += "<input type=\"checkbox\" id=\"diag_en\" name=\"diag_en\" value=\"1\"" + String(diag_enabled ? " checked" : "") + " style=\"width:auto;margin:0;\">";
  html += "<label for=\"diag_en\" style=\"margin:0;\">Enable diagnostics publishing to MQTT</label>";
  html += "</div>";
  html += "<button type=\"submit\">Save Settings</button>";
  html += "</form>";

  html += "<div style=\"margin-top:20px;\">";
  html += "<h2 style=\"font-size:16px;margin-bottom:12px;\">Current Statistics</h2>";
  html += "<div class=\"value-grid\" style=\"display:grid;gap:12px;\">";
  html += "<div class=\"value-item\" style=\"background:var(--bg-secondary);padding:12px;border-radius:8px;\">";
  html += "<div class=\"value-label\" style=\"color:var(--text-secondary);font-size:11px;\">Messages RX</div>";
  html += "<div class=\"value-data\" style=\"color:var(--text-primary);font-size:18px;font-weight:600;\">" + String(stats.messages_rx) + "</div>";
  html += "</div>";
  html += "<div class=\"value-item\" style=\"background:var(--bg-secondary);padding:12px;border-radius:8px;\">";
  html += "<div class=\"value-label\" style=\"color:var(--text-secondary);font-size:11px;\">Messages TX</div>";
  html += "<div class=\"value-data\" style=\"color:var(--text-primary);font-size:18px;font-weight:600;\">" + String(stats.messages_tx) + "</div>";
  html += "</div>";
  html += "<div class=\"value-item\" style=\"background:var(--bg-secondary);padding:12px;border-radius:8px;\">";
  html += "<div class=\"value-label\" style=\"color:var(--text-secondary);font-size:11px;\">Errors</div>";
  html += "<div class=\"value-data\" style=\"color:var(--text-primary);font-size:18px;font-weight:600;\">" + String(stats.errors) + "</div>";
  html += "</div>";
  html += "<div class=\"value-item\" style=\"background:var(--bg-secondary);padding:12px;border-radius:8px;\">";
  html += "<div class=\"value-label\" style=\"color:var(--text-secondary);font-size:11px;\">Bus Health</div>";
  html += "<div class=\"value-data\" style=\"color:var(--text-primary);font-size:18px;font-weight:600;\">" + stats.bus_health + "</div>";
  html += "</div></div>";
  html += "<button onclick=\"exportDiagnostics()\" style=\"margin-top:16px;background:var(--accent-green);\">Export Diagnostics JSON</button>";
  html += "</div>";
  html += "<div style=\"margin-top:20px;padding-top:20px;border-top:1px solid var(--border-color);\">";
  html += "<h2 style=\"font-size:16px;margin-bottom:12px;\">System Actions</h2>";
  html += "<button onclick=\"if(confirm('Are you sure you want to reboot the device?')){fetch('/api/reboot',{method:'POST'}).then(()=>{alert('Device is rebooting...');setTimeout(()=>window.location.href='/',5000);});}\" style=\"background:#ef4444;color:white;padding:12px 24px;border:none;border-radius:6px;font-size:16px;cursor:pointer;width:100%;\">🔄 Reboot Device</button>";
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

void handleSettingsSave() {
  bool diag_enabled = server.hasArg("diag_en");

  Preferences prefs;
  prefs.begin("settings", false);
  prefs.putBool("diag_en", diag_enabled);
  prefs.end();

  diagnostics_enabled = diag_enabled;

  String html = "<script>";
  html += "sessionStorage.setItem('message',JSON.stringify({title:'✓ Diagnostics Saved',message:'Diagnostics publishing is now " + String(diag_enabled ? "enabled" : "disabled") + ".',success:true}));";
  html += "window.location.href='/diagnostics';";
  html += "</script>";
  server.send(200, "text/html", html);
}

void handleOTAPage() {
  String html = buildHTMLHeader("Firmware Update");
  html += "<div class=\"card\">";
  html += "<h1>Firmware Update</h1>";
  html += "<p>Current Version: v" + String(FIRMWARE_VERSION) + "</p>";
  html += "<form method=\"POST\" action=\"/update\" enctype=\"multipart/form-data\" id=\"upload-form\">";
  html += "<input type=\"file\" name=\"firmware\" accept=\".bin\" required id=\"file-input\">";
  html += "<button type=\"submit\" id=\"upload-btn\">Upload & Update</button>";
  html += "</form>";
  html += "<div id=\"update-info\" style=\"display:none;margin-top:20px;\">";
  html += "<div style=\"background:var(--bg-secondary);border-radius:8px;padding:24px;text-align:center;\">";
  html += "<h2 style=\"margin:0 0 16px 0;color:var(--accent-green);\">⚡ Updating Firmware</h2>";
  html += "<p style=\"margin:8px 0;color:var(--text-secondary);\">The firmware is being uploaded and flashed to the device.</p>";
  html += "<p style=\"margin:8px 0;color:var(--text-secondary);\">This process takes approximately 30-60 seconds.</p>";
  html += "<p style=\"margin:16px 0 0 0;font-weight:600;color:#ef4444;\">⚠️ DO NOT disconnect power or close this page!</p>";
  html += "<div style=\"margin-top:24px;padding:16px;background:var(--bg-primary);border-radius:8px;border-left:4px solid var(--accent-purple);text-align:left;\">";
  html += "<p style=\"margin:0 0 8px 0;font-weight:600;\">What's happening:</p>";
  html += "<p style=\"margin:4px 0;font-size:14px;\">1. Uploading firmware binary (~1.1MB)</p>";
  html += "<p style=\"margin:4px 0;font-size:14px;\">2. Writing to flash memory</p>";
  html += "<p style=\"margin:4px 0;font-size:14px;\">3. Verifying integrity</p>";
  html += "<p style=\"margin:4px 0;font-size:14px;\">4. Rebooting device</p>";
  html += "</div></div></div>";
  html += "<div id=\"complete-info\" style=\"display:none;margin-top:20px;\">";
  html += "<div style=\"background:var(--bg-secondary);border-radius:8px;padding:48px;text-align:center;\">";
  html += "<h2 style=\"margin:0 0 24px 0;font-size:28px;color:var(--accent-green);\">✓ Update Complete!</h2>";
  html += "<p style=\"margin:16px 0;font-size:16px;color:var(--text-secondary);\">Device has been updated and is rebooting...</p>";
  html += "<button onclick=\"window.location.href='/'\" id=\"refresh-btn\" style=\"margin-top:32px;background:linear-gradient(135deg,var(--accent-green),var(--accent-purple));padding:16px 48px;border:none;border-radius:12px;color:white;font-size:18px;font-weight:600;cursor:pointer;box-shadow:0 4px 12px rgba(0,0,0,0.2);\">Go to Home (<span id=\"countdown\">10</span>s)</button>";
  html += "</div></div>";
  html += "<p style=\"color: #ef4444; margin-top: 16px;\">WARNING: Do not disconnect power during update!</p>";
  html += "</div>";
  html += "<script>";
  html += "function showUpdating(){";
  html += "document.getElementById('upload-form').style.display='none';";
  html += "document.getElementById('update-info').style.display='block';";
  html += "document.querySelectorAll('.nav-bar a').forEach(a=>a.style.pointerEvents='none');";
  html += "document.querySelectorAll('.nav-bar a').forEach(a=>a.style.opacity='0.5');";
  html += "}";
  html += "document.getElementById('upload-form').onsubmit=function(e){";
  html += "e.preventDefault();showUpdating();";
  html += "const formData=new FormData(e.target);";
  html += "fetch('/update',{method:'POST',body:formData})";
  html += ".then(r=>r.text())";
  html += ".then(result=>{";
  html += "if(result==='OK'){showComplete();}";
  html += "else{document.getElementById('update-info').innerHTML='<p style=\"color:#ef4444;\">Update failed!</p>';}";
  html += "}).catch(e=>{document.getElementById('update-info').innerHTML='<p style=\"color:#ef4444;\">Upload error: '+e+'</p>';});";
  html += "};";
  html += "function showComplete(){";
  html += "document.getElementById('update-info').style.display='none';";
  html += "document.getElementById('complete-info').style.display='block';";
  html += "document.querySelectorAll('.nav-bar a').forEach(a=>a.style.pointerEvents='auto');";
  html += "document.querySelectorAll('.nav-bar a').forEach(a=>a.style.opacity='1');";
  html += "let count=10;";
  html += "const interval=setInterval(function(){";
  html += "count--;document.getElementById('countdown').textContent=count;";
  html += "if(count<=0){clearInterval(interval);window.location.href='/';}";
  html += "},1000);}";
  html += "</script>";
  html += buildHTMLFooter();
  server.send(200, "text/html", html);
}

void handleOTAUpload() {
  HTTPUpload& upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
#ifdef DEBUG_SERIAL
    Serial.printf("OTA Update: %s\n", upload.filename.c_str());
#endif
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    esp_task_wdt_reset();
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
#ifdef DEBUG_SERIAL
      Serial.printf("Update Success: %u bytes\n", upload.totalSize);
#endif
    } else {
      Update.printError(Serial);
    }
  }
}

void handleLogoLight() {
  server.send_P(200, "image/png", (const char*)logo_light_png, logo_light_png_len);
}

void handleLogoDark() {
  server.send_P(200, "image/png", (const char*)logo_dark_png, logo_dark_png_len);
}

void handleAPIStatus() {
  String json = getStatusJSON();
  server.send(200, "application/json", json);
}

void handleAPIDiagnostics() {
  String json = getDiagnosticsJSON();
  server.send(200, "application/json", json);
}

void handleAPIRecent() {
  String json = "[";
  for (int i = 0; i < RECENT_MESSAGES_SIZE; i++) {
    int idx = (recentMessagesIndex + i) % RECENT_MESSAGES_SIZE;
    if (recentMessages[idx].timestamp > 0) {
      if (i > 0) json += ",";
      json += daliMessageToJSON(recentMessages[idx]);
    }
  }
  json += "]";
  server.send(200, "application/json", json);
}

String buildHTMLHeader(const String& title) {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset=\"UTF-8\">";
  html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  html += "<title>" + title + "</title>";
  html += "<script>";
  html += "(function(){";
  html += "const theme=getCookie('theme')||'auto';";
  html += "if(theme==='dark'||(theme==='auto'&&window.matchMedia('(prefers-color-scheme: dark)').matches)){";
  html += "document.documentElement.classList.add('dark-mode');}";
  html += "})();";
  html += "function getCookie(name){";
  html += "const value='; '+document.cookie;";
  html += "const parts=value.split('; '+name+'=');";
  html += "if(parts.length===2)return parts.pop().split(';').shift();}";
  html += "</script>";
  html += "<style>";
  html += ":root{--bg-primary:#ffffff;--bg-secondary:#f8fafc;--text-primary:#1e293b;--text-secondary:#64748b;";
  html += "--border-color:#e5e7eb;--accent-green:#22c55e;--accent-purple:#764ba2;--card-shadow:rgba(0,0,0,0.1);}";
  html += ".dark-mode{--bg-primary:#0f172a;--bg-secondary:#1e293b;--text-primary:#f8fafc;--text-secondary:#94a3b8;";
  html += "--border-color:#334155;--card-shadow:rgba(0,0,0,0.3);}";
  html += "*{box-sizing:border-box;font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;}";
  html += "body{margin:0;padding:20px;background:var(--bg-secondary);color:var(--text-primary);min-height:100vh;}";
  html += ".container{max-width:600px;margin:0 auto;}";
  html += ".header{display:flex;justify-content:space-between;align-items:center;margin-bottom:20px;}";
  html += ".logo{flex:1;}";
  html += ".logo img{max-width:150px;height:auto;}";
  html += ".theme-toggle{flex-shrink:0;}";
  html += ".theme-btn{padding:8px 12px;background:var(--bg-primary);color:var(--text-primary);";
  html += "border:2px solid var(--border-color);border-radius:6px;cursor:pointer;font-size:14px;font-weight:600;}";
  html += ".theme-btn:hover{border-color:var(--accent-green);}";
  html += ".card{background:var(--bg-primary);border-radius:12px;padding:24px;margin-bottom:16px;";
  html += "box-shadow:0 4px 20px var(--card-shadow);border:1px solid var(--border-color);}";
  html += "h1{color:var(--text-primary);margin:0 0 8px 0;font-size:24px;}";
  html += ".subtitle{color:var(--text-secondary);margin:0 0 20px 0;font-size:14px;}";
  html += ".status{display:flex;align-items:center;gap:8px;margin-bottom:16px;}";
  html += ".dot{width:12px;height:12px;border-radius:50%;}";
  html += ".dot.green{background:#22c55e;}.dot.yellow{background:#eab308;}.dot.red{background:#ef4444;}";
  html += "label{display:block;margin-bottom:6px;color:var(--text-secondary);font-weight:500;font-size:14px;}";
  html += ".input-wrapper{position:relative;margin-bottom:16px;}";
  html += ".input-wrapper input{width:100%;padding:12px;padding-right:40px;";
  html += "border:2px solid var(--border-color);border-radius:8px;font-size:16px;";
  html += "background:var(--bg-primary);color:var(--text-primary);}";
  html += ".eye-icon{position:absolute;right:12px;top:12px;cursor:pointer;user-select:none;font-size:18px;}";
  html += "input[type=\"text\"],input[type=\"password\"],input[type=\"number\"],input[type=\"file\"],select{width:100%;padding:12px;";
  html += "border:2px solid var(--border-color);border-radius:8px;font-size:16px;margin-bottom:16px;";
  html += "background:var(--bg-primary);color:var(--text-primary);}";
  html += "input:focus,select:focus{outline:none;border-color:var(--accent-green);}";
  html += "button{width:100%;padding:14px;background:linear-gradient(135deg,var(--accent-green) 0%,var(--accent-purple) 100%);";
  html += "color:white;border:none;border-radius:8px;font-size:16px;font-weight:600;cursor:pointer;}";
  html += "button:hover{opacity:0.9;}";
  html += ".nav-bar{background:var(--bg-primary);border-radius:12px;padding:12px;margin-bottom:20px;";
  html += "box-shadow:0 4px 20px var(--card-shadow);border:1px solid var(--border-color);";
  html += "display:flex;flex-wrap:wrap;gap:8px;justify-content:center;}";
  html += ".nav-bar a{color:var(--text-primary);text-decoration:none;padding:8px 16px;border-radius:6px;";
  html += "background:var(--bg-secondary);font-size:14px;font-weight:500;transition:all 0.2s;}";
  html += ".nav-bar a:hover{background:var(--accent-green);color:white;}";
  html += ".nav-bar a.active{background:var(--accent-purple);color:white;}";
  html += ".footer{text-align:center;color:var(--text-secondary);font-size:12px;margin-top:32px;padding:16px;}";
  html += ".modal{display:none;position:fixed;z-index:1000;left:0;top:0;width:100%;height:100%;";
  html += "background:rgba(0,0,0,0.5);backdrop-filter:blur(4px);}";
  html += ".modal-content{background:var(--bg-primary);margin:15% auto;padding:32px;border-radius:16px;";
  html += "max-width:400px;box-shadow:0 8px 32px rgba(0,0,0,0.3);border:1px solid var(--border-color);text-align:center;}";
  html += ".modal-content h2{margin:0 0 16px 0;font-size:24px;color:var(--text-primary);}";
  html += ".modal-content p{margin:0 0 24px 0;color:var(--text-secondary);font-size:16px;}";
  html += ".modal-btn{padding:12px 32px;background:linear-gradient(135deg,var(--accent-green),var(--accent-purple));";
  html += "color:white;border:none;border-radius:8px;font-size:16px;font-weight:600;cursor:pointer;}";
  html += ".modal-btn:hover{opacity:0.9;}";
  html += ".modal-success h2{color:var(--accent-green);}";
  html += ".modal-error h2{color:#ef4444;}";
  html += "</style></head><body><div class=\"container\">";
  html += "<div class=\"header\">";
  html += "<div class=\"logo\"><a href=\"/\" style=\"display:block;\"><img src=\"/logo_light\" alt=\"SCEPI DALI Bridge\" id=\"logo-img\" onerror=\"this.style.display='none'\"></a></div>";
  html += "<div class=\"theme-toggle\"><button class=\"theme-btn\" onclick=\"cycleTheme()\" id=\"theme-btn\">☀️/🌙</button></div>";
  html += "</div>";
  html += "<div class=\"nav-bar\">";
  html += "<a href=\"/\">Home</a>";
  html += "<a href=\"/config\">WiFi</a>";
  html += "<a href=\"/mqtt\">MQTT</a>";
  html += "<a href=\"/dali\">DALI</a>";
  html += "<a href=\"/diagnostics\">Diagnostics</a>";
  html += "<a href=\"/update\">Update</a>";
  html += "</div>";
  html += "<div id=\"modal\" class=\"modal\"><div class=\"modal-content\" id=\"modal-content\"></div></div>";
  return html;
}

String buildHTMLFooter() {
  String html = "<div class=\"footer\">&copy; 2026 Scepi Consulting Kft. v";
  html += FIRMWARE_VERSION;
  html += "</div></div><script>";
  html += "function cycleTheme(){";
  html += "const themes=['auto','light','dark'];";
  html += "const current=getCookie('theme')||'auto';";
  html += "const idx=themes.indexOf(current);";
  html += "const next=themes[(idx+1)%3];";
  html += "setTheme(next);}";
  html += "function setTheme(theme){";
  html += "document.cookie='theme='+theme+'; path=/; max-age=31536000';";
  html += "if(theme==='dark'||(theme==='auto'&&window.matchMedia('(prefers-color-scheme: dark)').matches)){";
  html += "document.documentElement.classList.add('dark-mode');}else{";
  html += "document.documentElement.classList.remove('dark-mode');}";
  html += "updateThemeButton(theme);updateLogo();}";
  html += "function updateThemeButton(theme){";
  html += "const labels={auto:'☀️/🌙',light:'☀️',dark:'🌙'};";
  html += "document.getElementById('theme-btn').textContent=labels[theme];}";
  html += "function updateLogo(){";
  html += "const isDark=document.documentElement.classList.contains('dark-mode');";
  html += "const img=document.getElementById('logo-img');";
  html += "if(img)img.src=isDark?'/logo_dark':'/logo_light';}";
  html += "function togglePassword(id){";
  html += "const input=document.getElementById(id);";
  html += "const icon=document.getElementById(id+'-eye');";
  html += "if(input.type==='password'){input.type='text';icon.textContent='🐵';}";
  html += "else{input.type='password';icon.textContent='🙈';}}";
  html += "const currentTheme=getCookie('theme')||'auto';";
  html += "updateThemeButton(currentTheme);updateLogo();";
  html += "document.querySelectorAll('.eye-icon').forEach(function(el){if(el.textContent==='')el.textContent='🙈';});";
  html += "const msg=sessionStorage.getItem('message');";
  html += "if(msg){const data=JSON.parse(msg);showModal(data.title,data.message,data.success);sessionStorage.removeItem('message');}";
  html += "function showModal(title,message,isSuccess){";
  html += "const modal=document.getElementById('modal');";
  html += "const content=document.getElementById('modal-content');";
  html += "content.className='modal-content '+(isSuccess?'modal-success':'modal-error');";
  html += "content.innerHTML='<h2>'+title+'</h2><p>'+message+'</p><button class=\"modal-btn\" onclick=\"closeModal()\">OK</button>';";
  html += "modal.style.display='block';}";
  html += "function closeModal(){document.getElementById('modal').style.display='none';}";
  html += "window.onclick=function(e){const modal=document.getElementById('modal');if(e.target===modal)closeModal();};";
  html += "</script></body></html>";
  return html;
}

void handleAPIPassiveDevices() {
  unsigned long now = millis();
  String json = "{\"devices\":[";
  bool first = true;
  
  for (int i = 0; i < DALI_MAX_ADDRESSES; i++) {
    if (passiveDevices[i].last_seen > 0) {
      if (!first) json += ",";
      first = false;
      
      unsigned long age_sec = (now - passiveDevices[i].last_seen) / 1000;
      json += "{\"address\":" + String(i);
      json += ",\"last_seen\":" + String(age_sec);
      json += ",\"level\":" + String(passiveDevices[i].last_level == 255 ? -1 : (int)passiveDevices[i].last_level);
      json += ",\"device_type\":" + String(passiveDevices[i].device_type == 255 ? -1 : (int)passiveDevices[i].device_type);
      json += "}";
    }
  }
  
  json += "],\"count\":" + String(getPassiveDeviceCount()) + "}";
  server.send(200, "application/json", json);
}
