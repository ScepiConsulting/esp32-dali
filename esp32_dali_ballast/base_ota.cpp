#include "base_ota.h"
#include "base_config.h"
#include "project_version.h"
#include "base_web.h"
#include <Update.h>
#include "esp_task_wdt.h"

void otaInit() {
  server.on("/update", HTTP_GET, handleOTAPage);
  server.on("/update", HTTP_POST, []() {
    server.send(200, "text/plain", Update.hasError() ? "FAIL" : "OK");
    delay(2000);
    ESP.restart();
  }, handleOTAUpload);
}

void handleOTAPage() {
  if (!checkAuth()) return;

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
  html += "<p style=\"margin:4px 0;font-size:14px;\">1. Uploading firmware binary</p>";
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
