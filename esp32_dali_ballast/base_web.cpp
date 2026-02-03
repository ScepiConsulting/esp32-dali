#include "base_web.h"
#include "base_config.h"
#include "project_version.h"
#include "project_config.h"
#include "base_wifi.h"
#include "base_mqtt.h"
#include "base_logos.h"
#include <WiFi.h>
#include <Preferences.h>

WebServer server(80);
String webInterfaceUsername = "";
String webInterfacePassword = "";

extern String getFunctionHomeHTML();
extern String getMqttStatusHTML();

bool checkAuth() {
  if (webInterfacePassword.length() == 0) return true;
  String username = webInterfaceUsername.length() > 0 ? webInterfaceUsername : "admin";
  if (!server.authenticate(username.c_str(), webInterfacePassword.c_str())) {
    server.requestAuthentication();
    return false;
  }
  return true;
}

void webCoreInit() {
  Preferences prefs;
  prefs.begin("web", true);
  webInterfaceUsername = prefs.getString("username", "admin");
  webInterfacePassword = prefs.getString("password", "");
  prefs.end();

  server.on("/", handleRoot);
  server.on("/dashboard", handleDashboard);
  server.on("/network", HTTP_GET, handleNetworkConfig);
  server.on("/network", HTTP_POST, handleNetworkSave);
  server.on("/save_web_auth", HTTP_POST, handleWebAuthSave);
  server.on("/logo_light", handleLogoLight);
  server.on("/logo_dark", handleLogoDark);
  server.on("/api/status", handleAPIStatus);
  server.on("/api/reboot", HTTP_POST, handleAPIReboot);

  server.onNotFound(handleRoot);

  server.begin();
#ifdef DEBUG_SERIAL
  Serial.println("Web server started");
#endif
}

void webCoreLoop() {
  server.handleClient();
}

void handleRoot() {
  if (!checkAuth()) return;
  if (apMode) {
    server.sendHeader("Location", "/network");
    server.send(302, "text/plain", "");
  } else {
    handleDashboard();
  }
}

void handleDashboard() {
  if (!checkAuth()) return;

  String html = buildHTMLHeader("Home");
  html += "<div class=\"card\">";
  html += "<h1>" + String(PROJECT_NAME) + "</h1>";
  html += "<p class=\"subtitle\">System Home</p>";

  html += getWiFiStatusHTML();
  html += getMqttStatusHTML();

  html += getFunctionHomeHTML();

  html += "</div>";
  html += buildHTMLFooter();
  server.send(200, "text/html", html);
}

void handleNetworkConfig() {
  if (!checkAuth()) return;

  String html = buildHTMLHeader("Network Configuration");
  html += "<div class=\"card\">";
  html += "<h1>Network Setup</h1>";
  html += "<p class=\"subtitle\">Configure your WiFi connection</p>";
  html += "<form action=\"/network\" method=\"POST\">";
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

void handleNetworkSave() {
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
  html += "<p><a href=\"/network\" style=\"color:var(--accent-green);\">← Back to Network Settings</a></p>";
  html += "</div>";
  html += buildHTMLFooter();
  server.send(200, "text/html", html);
}

void handleLogoLight() {
  server.send_P(200, "image/png", (const char*)logo_light_png, logo_light_png_len);
}

void handleLogoDark() {
  server.send_P(200, "image/png", (const char*)logo_dark_png, logo_dark_png_len);
}

void handleAPIStatus() {
  if (!checkAuth()) return;
  String json = "{";
  json += "\"project\":\"" + String(PROJECT_NAME) + "\",";
  json += "\"version\":\"" + String(FIRMWARE_VERSION) + "\",";
  json += "\"wifi\":" + getWiFiStatusJSON() + ",";
  json += "\"mqtt\":" + getMqttStatusJSON();
  json += "}";
  server.send(200, "application/json", json);
}

void handleAPIReboot() {
  server.send(200, "text/plain", "Rebooting...");
  delay(100);
  ESP.restart();
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
  html += "h2{color:var(--text-primary);margin:20px 0 12px 0;font-size:18px;}";
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
  html += "button:disabled{opacity:0.5;cursor:not-allowed;}";
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
  html += ".status-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:16px;margin-top:20px;}";
  html += ".status-item{background:var(--bg-secondary);padding:16px;border-radius:8px;}";
  html += ".status-label{color:var(--text-secondary);font-size:12px;text-transform:uppercase;margin-bottom:8px;}";
  html += ".status-value{color:var(--text-primary);font-size:20px;font-weight:600;margin-top:4px;}";
  html += "</style></head><body><div class=\"container\">";
  html += "<div class=\"header\">";
  html += "<div class=\"logo\"><a href=\"/\" style=\"display:block;\"><img src=\"/logo_light\" alt=\"" + String(PROJECT_NAME) + "\" id=\"logo-img\" onerror=\"this.style.display='none'\"></a></div>";
  html += "<div class=\"theme-toggle\"><button class=\"theme-btn\" onclick=\"cycleTheme()\" id=\"theme-btn\">☀️/🌙</button></div>";
  html += "</div>";
  html += "<div class=\"nav-bar\">";
  html += "<a href=\"/\">Home</a>";
  html += "<a href=\"/network\">Network</a>";
  html += "<a href=\"/mqtt\">MQTT</a>";
  html += "<a href=\"" + String(NAV_FUNCTION_URL) + "\">" + String(NAV_FUNCTION_NAME) + "</a>";
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
