/*
 * ESP32 WiFi Test with Web Interface
 *
 * Features:
 * - WiFi Manager for easy configuration
 * - Web interface showing system values
 * - Captive portal for initial setup
 */

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <DNSServer.h>

// Configuration
#define AP_SSID "ESP32-Setup"
#define AP_PASSWORD "esp32config"
#define DNS_PORT 53

// Global objects
WebServer server(80);
DNSServer dnsServer;
Preferences preferences;

// WiFi credentials storage
String storedSSID = "";
String storedPassword = "";
bool wifiConnected = false;
bool apMode = true;

// System values to display
unsigned long uptime = 0;
float freeHeap = 0;
int wifiRSSI = 0;
String ipAddress = "";

// HTML pages
const char* htmlHeader = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>ESP32 WiFi Test</title>
    <style>
        * { box-sizing: border-box; font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; }
        body { margin: 0; padding: 20px; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); min-height: 100vh; }
        .container { max-width: 400px; margin: 0 auto; }
        .card { background: white; border-radius: 12px; padding: 24px; margin-bottom: 16px; box-shadow: 0 4px 20px rgba(0,0,0,0.15); }
        h1 { color: #333; margin: 0 0 8px 0; font-size: 24px; }
        h2 { color: #333; margin: 0 0 16px 0; font-size: 18px; }
        .subtitle { color: #666; margin: 0 0 20px 0; font-size: 14px; }
        .status { display: flex; align-items: center; gap: 8px; margin-bottom: 16px; }
        .dot { width: 12px; height: 12px; border-radius: 50%; }
        .dot.green { background: #22c55e; }
        .dot.red { background: #ef4444; }
        .dot.yellow { background: #eab308; }
        label { display: block; margin-bottom: 6px; color: #555; font-weight: 500; font-size: 14px; }
        input[type="text"], input[type="password"] {
            width: 100%; padding: 12px; border: 2px solid #e5e7eb; border-radius: 8px;
            font-size: 16px; margin-bottom: 16px; transition: border-color 0.2s;
        }
        input:focus { outline: none; border-color: #667eea; }
        button {
            width: 100%; padding: 14px; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white; border: none; border-radius: 8px; font-size: 16px; font-weight: 600;
            cursor: pointer; transition: transform 0.1s, box-shadow 0.2s;
        }
        button:hover { transform: translateY(-1px); box-shadow: 0 4px 12px rgba(102, 126, 234, 0.4); }
        button:active { transform: translateY(0); }
        .value-grid { display: grid; gap: 12px; }
        .value-item { background: #f8fafc; padding: 16px; border-radius: 8px; }
        .value-label { color: #64748b; font-size: 12px; text-transform: uppercase; letter-spacing: 0.5px; }
        .value-data { color: #1e293b; font-size: 20px; font-weight: 600; margin-top: 4px; }
        .refresh-btn { margin-top: 16px; background: #64748b; }
        .reset-btn { margin-top: 8px; background: #ef4444; }
        .reset-btn:hover { box-shadow: 0 4px 12px rgba(239, 68, 68, 0.4); }
        a { color: #667eea; text-decoration: none; }
        a:hover { text-decoration: underline; }
        .nav { background: white; border-radius: 12px; padding: 16px; text-align: center; box-shadow: 0 4px 20px rgba(0,0,0,0.15); }
    </style>
</head>
<body>
    <div class="container">
)rawliteral";

const char* htmlFooter = R"rawliteral(
    </div>
</body>
</html>
)rawliteral";

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n\n=== ESP32 WiFi Test ===");

    // Load stored credentials
    preferences.begin("wifi", false);
    storedSSID = preferences.getString("ssid", "");
    storedPassword = preferences.getString("password", "");
    preferences.end();

    Serial.printf("Stored SSID: %s\n", storedSSID.c_str());

    // Try to connect to stored WiFi
    if (storedSSID.length() > 0) {
        Serial.println("Attempting to connect to stored WiFi...");
        WiFi.mode(WIFI_STA);
        WiFi.begin(storedSSID.c_str(), storedPassword.c_str());

        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 20) {
            delay(500);
            Serial.print(".");
            attempts++;
        }

        if (WiFi.status() == WL_CONNECTED) {
            wifiConnected = true;
            apMode = false;
            ipAddress = WiFi.localIP().toString();
            Serial.printf("\nConnected! IP: %s\n", ipAddress.c_str());
        } else {
            Serial.println("\nFailed to connect, starting AP mode...");
        }
    }

    // Start AP mode if not connected
    if (!wifiConnected) {
        startAPMode();
    }

    // Setup web server routes
    server.on("/", handleRoot);
    server.on("/config", handleConfig);
    server.on("/save", HTTP_POST, handleSave);
    server.on("/values", handleValues);
    server.on("/reset", handleReset);
    server.onNotFound(handleRoot);  // Captive portal redirect

    server.begin();
    Serial.println("Web server started");
}

void loop() {
    if (apMode) {
        dnsServer.processNextRequest();
    }
    server.handleClient();

    // Update system values
    uptime = millis() / 1000;
    freeHeap = ESP.getFreeHeap() / 1024.0;
    if (wifiConnected) {
        wifiRSSI = WiFi.RSSI();
    }

    delay(10);
}

void startAPMode() {
    apMode = true;
    WiFi.mode(WIFI_AP);

    // Generate AP SSID with MAC suffix
    uint64_t mac = ESP.getEfuseMac();
    String apSSID = String(AP_SSID) + "-" + String((uint32_t)(mac & 0xFFFFFF), HEX);
    apSSID.toUpperCase();

    WiFi.softAP(apSSID.c_str(), AP_PASSWORD);
    ipAddress = WiFi.softAPIP().toString();

    // Start DNS server for captive portal
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

    Serial.printf("AP Mode started. SSID: %s, Password: %s\n", apSSID.c_str(), AP_PASSWORD);
    Serial.printf("Connect to http://%s\n", ipAddress.c_str());
}

void handleRoot() {
    if (apMode) {
        // Redirect to config page in AP mode
        server.sendHeader("Location", "/config");
        server.send(302, "text/plain", "");
    } else {
        // Show values page when connected
        server.sendHeader("Location", "/values");
        server.send(302, "text/plain", "");
    }
}

void handleConfig() {
    String html = htmlHeader;
    html += R"rawliteral(
        <div class="card">
            <h1>ESP32 WiFi Setup</h1>
            <p class="subtitle">Configure your WiFi connection</p>
            <div class="status">
                <div class="dot )rawliteral";
    html += apMode ? "yellow" : "green";
    html += R"rawliteral("></div>
                <span>)rawliteral";
    html += apMode ? "Access Point Mode" : "Connected to WiFi";
    html += R"rawliteral(</span>
            </div>
            <form action="/save" method="POST">
                <label for="ssid">WiFi Network Name (SSID)</label>
                <input type="text" id="ssid" name="ssid" placeholder="Enter SSID" value=")rawliteral";
    html += storedSSID;
    html += R"rawliteral(" required>
                <label for="password">WiFi Password</label>
                <input type="password" id="password" name="password" placeholder="Enter password">
                <button type="submit">Save & Connect</button>
            </form>
        </div>
        <div class="nav">
            <a href="/values">View System Values</a>
        </div>
    )rawliteral";
    html += htmlFooter;

    server.send(200, "text/html", html);
}

void handleSave() {
    String newSSID = server.arg("ssid");
    String newPassword = server.arg("password");

    if (newSSID.length() > 0) {
        // Save credentials
        preferences.begin("wifi", false);
        preferences.putString("ssid", newSSID);
        preferences.putString("password", newPassword);
        preferences.end();

        storedSSID = newSSID;
        storedPassword = newPassword;

        String html = htmlHeader;
        html += R"rawliteral(
            <div class="card">
                <h1>Settings Saved!</h1>
                <p class="subtitle">The ESP32 will now restart and connect to your WiFi network.</p>
                <p>If connection fails, the device will return to AP mode.</p>
                <p style="margin-top: 20px;"><strong>AP SSID:</strong> )rawliteral";
        html += AP_SSID;
        html += R"rawliteral(<br><strong>AP Password:</strong> )rawliteral";
        html += AP_PASSWORD;
        html += R"rawliteral(</p>
            </div>
        )rawliteral";
        html += htmlFooter;

        server.send(200, "text/html", html);

        delay(2000);
        ESP.restart();
    } else {
        server.sendHeader("Location", "/config");
        server.send(302, "text/plain", "");
    }
}

void handleValues() {
    String html = htmlHeader;
    html += R"rawliteral(
        <div class="card">
            <h1>System Status</h1>
            <p class="subtitle">ESP32 is running</p>
            <div class="status">
                <div class="dot )rawliteral";
    html += wifiConnected ? "green" : "yellow";
    html += R"rawliteral("></div>
                <span>)rawliteral";
    html += wifiConnected ? "WiFi Connected" : "AP Mode";
    html += R"rawliteral(</span>
            </div>
        </div>
        <div class="card">
            <h2>System Values</h2>
            <div class="value-grid">
                <div class="value-item">
                    <div class="value-label">Uptime</div>
                    <div class="value-data">)rawliteral";

    // Format uptime
    unsigned long secs = uptime;
    unsigned long mins = secs / 60;
    unsigned long hours = mins / 60;
    secs %= 60;
    mins %= 60;
    html += String(hours) + "h " + String(mins) + "m " + String(secs) + "s";

    html += R"rawliteral(</div>
                </div>
                <div class="value-item">
                    <div class="value-label">Free Heap Memory</div>
                    <div class="value-data">)rawliteral";
    html += String(freeHeap, 1) + " KB";
    html += R"rawliteral(</div>
                </div>
                <div class="value-item">
                    <div class="value-label">IP Address</div>
                    <div class="value-data">)rawliteral";
    html += ipAddress;
    html += R"rawliteral(</div>
                </div>)rawliteral";

    if (wifiConnected) {
        html += R"rawliteral(
                <div class="value-item">
                    <div class="value-label">WiFi Signal (RSSI)</div>
                    <div class="value-data">)rawliteral";
        html += String(wifiRSSI) + " dBm";
        html += R"rawliteral(</div>
                </div>
                <div class="value-item">
                    <div class="value-label">Connected Network</div>
                    <div class="value-data">)rawliteral";
        html += storedSSID;
        html += R"rawliteral(</div>
                </div>)rawliteral";
    }

    html += R"rawliteral(
                <div class="value-item">
                    <div class="value-label">Chip Model</div>
                    <div class="value-data">)rawliteral";
    html += ESP.getChipModel();
    html += R"rawliteral(</div>
                </div>
                <div class="value-item">
                    <div class="value-label">CPU Frequency</div>
                    <div class="value-data">)rawliteral";
    html += String(ESP.getCpuFreqMHz()) + " MHz";
    html += R"rawliteral(</div>
                </div>
            </div>
            <button class="refresh-btn" onclick="location.reload()">Refresh Values</button>
            <button class="reset-btn" onclick="if(confirm('Reset WiFi credentials and restart in AP mode?')) window.location='/reset'">Reset WiFi Config</button>
        </div>
        <div class="nav">
            <a href="/config">WiFi Settings</a>
        </div>
    )rawliteral";
    html += htmlFooter;

    server.send(200, "text/html", html);
}

void handleReset() {
    preferences.begin("wifi", false);
    preferences.clear();
    preferences.end();

    String html = htmlHeader;
    html += R"rawliteral(
        <div class="card">
            <h1>WiFi Config Reset</h1>
            <p class="subtitle">All WiFi settings have been cleared.</p>
            <p>The device will restart in AP mode.</p>
        </div>
    )rawliteral";
    html += htmlFooter;

    server.send(200, "text/html", html);

    delay(2000);
    ESP.restart();
}
