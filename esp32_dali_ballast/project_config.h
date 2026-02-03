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
#define NAV_FUNCTION_NAME "DALI"
#define NAV_FUNCTION_URL "/dali"

// Hardware - Built-in LED (WS2812 on GPIO21)
#define BUILTIN_LED_PIN 21

// Feature flags
#define ENABLE_MQTT_FILTER_UI false

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
