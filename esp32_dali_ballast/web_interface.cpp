#include "web_interface.h"
#include "version.h"
#include "config.h"
#include "ballast_handler.h"
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
  server.on("/dali", HTTP_GET, handleBallastConfig);
  server.on("/dali", HTTP_POST, handleBallastSave);
  server.on("/dali/control", HTTP_POST, handleBallastControl);
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
  html += "<h1>ESP32 DALI Ballast</h1>";
  html += "<p class=\"subtitle\">Virtual DALI Ballast Emulator</p>";

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

    html += "<p style=\"margin:12px 0 4px 0;color:var(--text-secondary);\"><strong>Ballast Status:</strong> <code style=\"background:var(--bg-primary);padding:2px 6px;border-radius:3px;font-family:monospace;\">" + prefix + "ballast_status</code></p>";
    html += "<p style=\"margin:0 0 8px 0;font-size:12px;color:var(--text-secondary);\">Published when ballast state changes (level, fade, etc.)</p>";
    html += "<details style=\"margin:8px 0;padding:8px;background:var(--bg-primary);border-radius:4px;\">";
    html += "<summary style=\"cursor:pointer;color:var(--accent-green);font-weight:500;\">▸ Example: Ballast Status</summary>";
    html += "<pre style=\"background:var(--bg-secondary);padding:12px;border-radius:6px;overflow-x:auto;margin:8px 0;font-size:12px;\">{<br>  \"timestamp\": 1234567890,<br>  \"address\": 0,<br>  \"actual_level\": 128,<br>  \"level_percent\": 50.4,<br>  \"target_level\": 128,<br>  \"fade_running\": false,<br>  \"lamp_arc_power_on\": true,<br>  \"lamp_failure\": false<br>}</pre></details>";

    html += "<p style=\"margin:12px 0 4px 0;color:var(--text-secondary);\"><strong>Commands Received:</strong> <code style=\"background:var(--bg-primary);padding:2px 6px;border-radius:3px;font-family:monospace;\">" + prefix + "command</code></p>";
    html += "<p style=\"margin:0 0 8px 0;font-size:12px;color:var(--text-secondary);\">Published when ballast receives DALI commands from the bus</p>";
    html += "<details style=\"margin:8px 0;padding:8px;background:var(--bg-primary);border-radius:4px;\">";
    html += "<summary style=\"cursor:pointer;color:var(--accent-green);font-weight:500;\">▸ Example: Set Level Command</summary>";
    html += "<pre style=\"background:var(--bg-secondary);padding:12px;border-radius:6px;overflow-x:auto;margin:8px 0;font-size:12px;\">{<br>  \"timestamp\": 1234567890,<br>  \"source\": \"bus\",<br>  \"command_type\": \"set_brightness\",<br>  \"address\": 0,<br>  \"value\": 128,<br>  \"value_percent\": 50.4,<br>  \"raw\": \"0180\",<br>  \"description\": \"Set to 128 (50.4%)\"<br>}</pre></details>";

    html += "<details style=\"margin:8px 0;padding:8px;background:var(--bg-primary);border-radius:4px;\">";
    html += "<summary style=\"cursor:pointer;color:var(--accent-green);font-weight:500;\">▸ Example: Query Response</summary>";
    html += "<pre style=\"background:var(--bg-secondary);padding:12px;border-radius:6px;overflow-x:auto;margin:8px 0;font-size:12px;\">{<br>  \"timestamp\": 1234567891,<br>  \"source\": \"bus\",<br>  \"command_type\": \"query_actual_level\",<br>  \"address\": 0,<br>  \"is_query_response\": true,<br>  \"response\": \"0x80\",<br>  \"value\": 128,<br>  \"value_percent\": 50.4,<br>  \"raw\": \"01A0\",<br>  \"description\": \"Query actual level: 128 (50.4%)\"<br>}</pre></details>";

    html += "<p style=\"margin:12px 0 4px 0;color:var(--text-secondary);\"><strong>Configuration:</strong> <code style=\"background:var(--bg-primary);padding:2px 6px;border-radius:3px;font-family:monospace;\">" + prefix + "config</code></p>";
    html += "<p style=\"margin:0 0 8px 0;font-size:12px;color:var(--text-secondary);\">Ballast configuration published on connect and when changed</p>";
    html += "<details style=\"margin:8px 0;padding:8px;background:var(--bg-primary);border-radius:4px;\">";
    html += "<summary style=\"cursor:pointer;color:var(--accent-green);font-weight:500;\">▸ Example: Configuration</summary>";
    html += "<pre style=\"background:var(--bg-secondary);padding:12px;border-radius:6px;overflow-x:auto;margin:8px 0;font-size:12px;\">{<br>  \"address\": 0,<br>  \"min_level\": 1,<br>  \"max_level\": 254,<br>  \"power_on_level\": 254,<br>  \"system_failure_level\": 254,<br>  \"fade_time\": 0,<br>  \"fade_rate\": 7<br>}</pre></details>";

    html += "<p style=\"margin:12px 0 4px 0;color:var(--text-secondary);\"><strong>Device Status:</strong> <code style=\"background:var(--bg-primary);padding:2px 6px;border-radius:3px;font-family:monospace;\">" + prefix + "status</code></p>";
    html += "<p style=\"margin:0 0 8px 0;font-size:12px;color:var(--text-secondary);\">Device online/offline status with uptime and IP address</p>";
    html += "<details style=\"margin:8px 0;padding:8px;background:var(--bg-primary);border-radius:4px;\">";
    html += "<summary style=\"cursor:pointer;color:var(--accent-green);font-weight:500;\">▸ Example: Device Status</summary>";
    html += "<pre style=\"background:var(--bg-secondary);padding:12px;border-radius:6px;overflow-x:auto;margin:8px 0;font-size:12px;\">{<br>  \"status\": \"online\",<br>  \"uptime\": 12345,<br>  \"ip\": \"192.168.1.100\",<br>  \"client_id\": \"dali-ballast-AABBCC\"<br>}</pre></details>";

    html += "<p style=\"margin:12px 0 4px 0;color:var(--text-secondary);\"><strong>Diagnostics:</strong> <code style=\"background:var(--bg-primary);padding:2px 6px;border-radius:3px;font-family:monospace;\">" + prefix + "diagnostics</code></p>";
    html += "<p style=\"margin:0 0 8px 0;font-size:12px;color:var(--text-secondary);\">Published periodically with message counters and error counts</p>";
    html += "<details style=\"margin:8px 0;padding:8px;background:var(--bg-primary);border-radius:4px;\">";
    html += "<summary style=\"cursor:pointer;color:var(--accent-green);font-weight:500;\">▸ Example: Diagnostics</summary>";
    html += "<pre style=\"background:var(--bg-secondary);padding:12px;border-radius:6px;overflow-x:auto;margin:8px 0;font-size:12px;\">{<br>  \"rx_count\": 42,<br>  \"tx_count\": 15,<br>  \"error_count\": 0,<br>  \"uptime\": 12345<br>}</pre></details>";

    html += "</div></div>";
  }

  html += "</div>";

  // Current Status section
  html += "<div class=\"card\" style=\"margin-top:20px;\">";
  html += "<h2>Current Status</h2>";

  // Device type - show combined device_type + active_color_type for DT8
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

  // Address with source indicator
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

  // Brightness
  html += "<div class=\"status\"><div class=\"dot " + String(ballastState.lamp_arc_power_on ? "green" : "red") + "\"></div><span>Level: " + String(ballastState.actual_level) + " (" + String((ballastState.actual_level / 254.0) * 100.0, 1) + "%)</span></div>";

  // Color values based on device type and color mode
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

  // Fade status
  html += "<div class=\"status\"><div class=\"dot " + String(ballastState.fade_running ? "yellow" : "green") + "\"></div><span>Fade: " + String(ballastState.fade_running ? "Running" : "Idle") + "</span></div>";
  html += "</div>";

  // Recent Commands section
  html += "<div class=\"card\" style=\"margin-top:20px;\">";
  html += "<h2>Recent Commands</h2>";
  html += "<button onclick=\"loadRecentMessages()\" style=\"background:var(--accent-green);margin-bottom:16px;\">Refresh</button>";
  html += "<div id=\"recent-messages\" style=\"font-size:12px;font-family:monospace;max-height:300px;overflow-y:auto;\"></div>";
  html += "</div>";

  html += "<script>";
  html += "function loadRecentMessages(){";
  html += "fetch('/api/recent').then(r=>r.json()).then(d=>{";
  html += "let html='';";
  html += "d.forEach(msg=>{if(msg.timestamp>0){";
  html += "html+='<div style=\"border-bottom:1px solid var(--border-color);padding:8px;\">';";
  html += "html+='<strong>'+(msg.is_query_response?'RESP':'CMD')+'</strong> '+msg.description+'<br>';";
  html += "html+='<span style=\"color:var(--text-secondary);\">Raw: ';";
  html += "html+=msg.raw_bytes[0].toString(16).padStart(2,'0').toUpperCase();";
  html += "html+=msg.raw_bytes[1].toString(16).padStart(2,'0').toUpperCase();";
  html += "html+='</span></div>';}});";
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
  String defaultClientId = "dali-ballast-" + String((uint32_t)(mac & 0xFFFFFF), HEX);
  String clientId = prefs.getString("client_id", defaultClientId);

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
  html += "<button type=\"submit\">Save MQTT Settings</button>";
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

  Preferences prefs;
  prefs.begin("mqtt", false);
  prefs.putString("broker", broker);
  prefs.putUShort("port", port);
  prefs.putString("user", user);
  prefs.putString("pass", pass);
  prefs.putString("prefix", prefix);
  prefs.putString("client_id", clientId);
  prefs.putUChar("qos", qos);
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

void handleBallastConfig() {
  if (!checkAuth()) return;

  String html = buildHTMLHeader("Ballast Configuration");

  html += "<div class=\"card\">";
  html += "<h1>Ballast Configuration</h1>";
  html += "<p class=\"subtitle\">Configure virtual DALI ballast behavior</p>";

  html += "<form method=\"POST\" action=\"/dali\" id=\"ballast-form\" onsubmit=\"saveBallastConfig(event)\">";

  html += "<label for=\"device_type\">Device Type</label>";
  // Determine UI device type value from device_type + active_color_type
  uint8_t ui_dt = ballastState.device_type;
  if (ballastState.device_type == DT8_COLOUR) {
    if (ballastState.active_color_type == DT8_MODE_TC) ui_dt = 10;  // Color Temp
    else if (ballastState.color_w > 0) ui_dt = 9;  // RGBW (has white channel)
    else ui_dt = 8;  // RGB
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
  html += "<label for=\"color_r\">Red (0-255)</label>";
  html += "<input type=\"number\" id=\"color_r_rgbw\" name=\"color_r\" value=\"" + String(ballastState.color_r) + "\" min=\"0\" max=\"255\">";
  html += "<label for=\"color_g\">Green (0-255)</label>";
  html += "<input type=\"number\" id=\"color_g_rgbw\" name=\"color_g\" value=\"" + String(ballastState.color_g) + "\" min=\"0\" max=\"255\">";
  html += "<label for=\"color_b\">Blue (0-255)</label>";
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

  // Show color controls based on device type and color mode
  if (ballastState.device_type == DT8_COLOUR) {
    if (ballastState.active_color_type == DT8_MODE_TC) {
      html += "<h3 style=\"margin:20px 0 12px 0;font-size:16px;color:var(--text-primary);\">Color Temperature</h3>";
      html += "<label for=\"ctrl_temp\">Temperature (Kelvin)</label>";
      uint16_t ctrl_kelvin = (ballastState.color_temp_mirek > 0) ? (1000000 / ballastState.color_temp_mirek) : 4000;
      html += "<input type=\"number\" id=\"ctrl_temp\" name=\"color_temp\" value=\"" + String(ctrl_kelvin) + "\" min=\"2700\" max=\"6500\" step=\"100\">";
    } else {
      // RGBWAF mode - show RGB and optionally W
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
  
  // Handle address assignment
  if (ballastState.address_mode_auto) {
    // Auto mode: keep current address if commissioned, otherwise unaddressed
    if (ballastState.address_source != ADDR_COMMISSIONED) {
      ballastState.short_address = 255;
      ballastState.address_source = ADDR_UNASSIGNED;
    }
    // If already commissioned, keep the commissioned address
  } else {
    // Manual mode: set address from form
    ballastState.short_address = server.arg("address").toInt();
    ballastState.address_source = ADDR_MANUAL;
  }
  
  // Handle device type - UI uses 8/9/10 for DT8 sub-modes, but internally all are DT8
  uint8_t ui_device_type = server.arg("device_type").toInt();
  if (ui_device_type == 8) {
    ballastState.device_type = DT8_COLOUR;
    ballastState.active_color_type = DT8_MODE_RGBWAF;  // RGB mode
  } else if (ui_device_type == 9) {
    ballastState.device_type = DT8_COLOUR;
    ballastState.active_color_type = DT8_MODE_RGBWAF;  // RGBW mode (same as RGB, just uses W channel)
  } else if (ui_device_type == 10) {
    ballastState.device_type = DT8_COLOUR;
    ballastState.active_color_type = DT8_MODE_TC;      // Color Temperature mode
  } else {
    ballastState.device_type = ui_device_type;  // DT0 or DT6
  }

  // Standard fields (for Normal mode)
  if (server.hasArg("min_level")) ballastState.min_level = server.arg("min_level").toInt();
  if (server.hasArg("max_level")) ballastState.max_level = server.arg("max_level").toInt();
  if (server.hasArg("power_on_level")) ballastState.power_on_level = server.arg("power_on_level").toInt();
  if (server.hasArg("fade_time")) ballastState.fade_time = server.arg("fade_time").toInt();
  if (server.hasArg("fade_rate")) ballastState.fade_rate = server.arg("fade_rate").toInt();

  // DT6 LED channels
  if (server.hasArg("channel_warm")) ballastState.channel_warm = server.arg("channel_warm").toInt();
  if (server.hasArg("channel_cool")) ballastState.channel_cool = server.arg("channel_cool").toInt();

  // DT8 RGB/RGBW colors
  if (server.hasArg("color_r")) ballastState.color_r = server.arg("color_r").toInt();
  if (server.hasArg("color_g")) ballastState.color_g = server.arg("color_g").toInt();
  if (server.hasArg("color_b")) ballastState.color_b = server.arg("color_b").toInt();
  if (server.hasArg("color_w")) ballastState.color_w = server.arg("color_w").toInt();

  // DT8 Color Temperature
  if (server.hasArg("color_temp")) {
    uint16_t input_kelvin = server.arg("color_temp").toInt();
    ballastState.color_temp_mirek = (input_kelvin > 0) ? (1000000 / input_kelvin) : 250;
  }

  saveBallastConfig();
  publishBallastConfig();

  String json = "{";
  json += "\"success\":true,";
  json += "\"title\":\"✓ Configuration Saved\",";
  json += "\"message\":\"Ballast configuration has been saved successfully.\"";
  json += "}";
  server.send(200, "application/json", json);
}

void handleBallastControl() {
  if (!checkAuth()) return;

  // Set current brightness level
  if (server.hasArg("level")) {
    uint8_t level = server.arg("level").toInt();
    setLevel(level);
  }

  // Set current color values based on device type
  if (server.hasArg("color_r")) {
    ballastState.color_r = server.arg("color_r").toInt();
    updateLED();
  }
  if (server.hasArg("color_g")) {
    ballastState.color_g = server.arg("color_g").toInt();
    updateLED();
  }
  if (server.hasArg("color_b")) {
    ballastState.color_b = server.arg("color_b").toInt();
    updateLED();
  }
  if (server.hasArg("color_w")) {
    ballastState.color_w = server.arg("color_w").toInt();
    updateLED();
  }
  if (server.hasArg("color_temp")) {
    uint16_t input_kelvin = server.arg("color_temp").toInt();
    ballastState.color_temp_mirek = (input_kelvin > 0) ? (1000000 / input_kelvin) : 250;
    updateLED();
  }

  publishBallastState();

  String json = "{";
  json += "\"success\":true,";
  json += "\"title\":\"✓ Control Applied\",";
  json += "\"message\":\"Ballast values have been updated.\"";
  json += "}";
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
      if (json.length() > 1) json += ",";
      json += "{";
      json += "\"timestamp\":" + String(recentMessages[idx].timestamp) + ",";
      json += "\"is_query_response\":" + String(recentMessages[idx].is_query_response ? "true" : "false") + ",";
      json += "\"command_type\":\"" + recentMessages[idx].command_type + "\",";
      json += "\"description\":\"" + recentMessages[idx].description + "\",";
      json += "\"raw_bytes\":[" + String(recentMessages[idx].raw_bytes[0]) + "," + String(recentMessages[idx].raw_bytes[1]) + "]";
      json += "}";
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
