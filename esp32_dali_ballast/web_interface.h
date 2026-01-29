#ifndef WEB_INTERFACE_H
#define WEB_INTERFACE_H

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

void webInit();
void webLoop();

void handleRoot();
void handleDashboard();
void handleWiFiConfig();
void handleWiFiSave();
void handleWebAuthSave();
void handleMQTTConfig();
void handleMQTTSave();
void handleBallastConfig();
void handleBallastSave();
void handleBallastControl();
void handleSettings();
void handleSettingsSave();
void handleOTAPage();
void handleOTAUpload();
void handleLogoLight();
void handleLogoDark();
void handleAPIStatus();
void handleAPIDiagnostics();
void handleAPIRecent();

String buildHTMLHeader(const String& title);
String buildHTMLFooter();

#endif
