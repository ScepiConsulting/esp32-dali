#ifndef WEB_CORE_H
#define WEB_CORE_H

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;
extern String webInterfaceUsername;
extern String webInterfacePassword;

void webCoreInit();
void webCoreLoop();
bool checkAuth();

String buildHTMLHeader(const String& title);
String buildHTMLFooter();

void handleRoot();
void handleDashboard();
void handleNetworkConfig();
void handleNetworkSave();
void handleWebAuthSave();
void handleLogoLight();
void handleLogoDark();
void handleAPIStatus();
void handleAPIReboot();

#endif
