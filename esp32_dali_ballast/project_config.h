#ifndef PROJECT_CONFIG_H
#define PROJECT_CONFIG_H

// =============================================================================
// ESP32 DALI Ballast - Project Configuration
// =============================================================================

// Project identification
#define PROJECT_NAME "ESP32 DALI Ballast"
#define PROJECT_SHORT_NAME "Ballast"
#define PROJECT_HOME_SUBTITLE "Virtual DALI Ballast Emulator"

// Access Point configuration
#define AP_SSID_PREFIX "SCEPI-DALI-BALLAST"
#define AP_PASSWORD "daliconfig"

// MQTT defaults
#define MQTT_DEFAULT_PREFIX "home/dali/ballast/"
#define MQTT_DEFAULT_CLIENT_PREFIX "dali-ballast"

// Navigation - Function page
#define APP_NAV_NAME "DALI"
#define APP_NAV_URL "/dali"

// Hardware - Built-in LED (WS2812 on GPIO21)
#define BUILTIN_LED_PIN 21

// No feature flags: ENABLE_MQTT_FILTER_UI stays at the base default (0), so the
// appMqttFilter* hooks fall back to the base's weak no-op defaults.

// =============================================================================
// DALI Hardware Configuration
// =============================================================================

#define DALI_TX_PIN 17
#define DALI_RX_PIN 14
#define DALI_TIMER_FREQ 9600000

// Onboard LED pin (WS2812 RGB LED on Waveshare ESP32-S3-PICO)
#define LED_PIN 21
#define LED_COUNT 1
#define LED_COLOR_ORDER 1  // 0=RGB, 1=GRB, 2=BGR, 3=RBG, 4=GBR, 5=BRG

// DALI timing settings
#define RECENT_MESSAGES_SIZE 20
#define QUERY_RESPONSE_TIMEOUT_MS 22
#define RESPONSE_QUEUE_SIZE 10
#define BUS_IDLE_TIMEOUT_MS 100
#define MAX_RESPONSE_RETRIES 3

#endif
