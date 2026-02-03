#ifndef PROJECT_CONFIG_H
#define PROJECT_CONFIG_H

// =============================================================================
// ESP32 DALI Bridge - Project Configuration
// =============================================================================

// Project identification
#define PROJECT_NAME "ESP32 DALI Bridge"
#define PROJECT_SHORT_NAME "Bridge"

// Access Point configuration
#define AP_SSID_PREFIX "SCEPI-DALI-BRIDGE"
#define AP_PASSWORD "scepi1234"

// MQTT defaults
#define MQTT_DEFAULT_PREFIX "scepi/v1/dali-bridge/"
#define MQTT_DEFAULT_CLIENT_PREFIX "dali-bridge"

// Navigation - Function page
#define NAV_FUNCTION_NAME "DALI Control"
#define NAV_FUNCTION_URL "/dali"

// Hardware - Built-in LED (not used, DALI uses GPIO17/14)
#define BUILTIN_LED_PIN 2

// Feature flags
#define ENABLE_MQTT_FILTER_UI true

// =============================================================================
// DALI Hardware Configuration
// =============================================================================

#define DALI_TX_PIN 17
#define DALI_RX_PIN 14
#define DALI_TIMER_FREQ 9600000

// DALI timing and queue settings
#define DALI_MIN_INTERVAL_MS 50
#define COMMAND_QUEUE_SIZE 50
#define RECENT_MESSAGES_SIZE 20

// Bus monitoring settings
#define BUS_IDLE_TIMEOUT_MS 150
#define BUS_ACTIVITY_WINDOW_MS 500
#define MAX_TX_RETRIES 5
#define QUERY_RESPONSE_TIMEOUT_MS 50

// Passive device discovery
#define DALI_MAX_ADDRESSES 64

// MQTT Monitor Filter Configuration
struct MonitorFilter {
    bool enable_dapc;           // Direct Arc Power Control (brightness)
    bool enable_control_cmds;   // Control commands (off, up, down, etc.)
    bool enable_queries;        // Query commands
    bool enable_responses;      // Backward frames (responses)
    bool enable_commissioning;  // Commissioning commands
    bool enable_self_sent;      // Commands sent by this bridge
    bool enable_bus_traffic;    // Commands from other devices
};

#endif
