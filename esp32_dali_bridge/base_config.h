#ifndef CONFIG_H
#define CONFIG_H

// =============================================================================
// ESP32 Base - Hardware Configuration
// =============================================================================
// This file contains hardware-level settings that are common across projects.
// Project-specific settings should go in project/project_config.h
// =============================================================================

// Watchdog timeout in seconds
#define WDT_TIMEOUT 30

// DNS server port for captive portal
#define DNS_PORT 53

// WiFi connection timeout (attempts * 500ms)
#define WIFI_CONNECT_ATTEMPTS 20

// MQTT reconnect settings
#define MQTT_INITIAL_RECONNECT_DELAY 5000
#define MQTT_MAX_RECONNECT_DELAY 60000
#define MQTT_MAX_PACKET_SIZE 512

// Status publish interval (ms)
#define STATUS_PUBLISH_INTERVAL 60000

// Debug serial output (comment out to disable)
#define DEBUG_SERIAL

#endif
