#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <DNSServer.h>

extern DNSServer dnsServer;
extern String storedSSID;
extern String storedPassword;
extern bool wifiConnected;
extern bool apMode;

void wifiInit();
void wifiLoop();
void startAPMode();
String getWiFiStatusHTML();
String getWiFiStatusJSON();

#endif
