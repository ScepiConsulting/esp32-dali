#ifndef CONFIG_H
#define CONFIG_H

// Uncomment to enable serial debug logging (saves ~40KB flash when disabled)
#define DEBUG_SERIAL

#define DALI_TX_PIN 17
#define DALI_RX_PIN 14
#define DALI_TIMER_FREQ 9600000

#define AP_SSID "ESP32-DALI-Setup"
#define AP_PASSWORD "daliconfig"
#define DNS_PORT 53

#define DALI_MIN_INTERVAL_MS 50
#define COMMAND_QUEUE_SIZE 50
#define RECENT_MESSAGES_SIZE 20

#define BUS_IDLE_TIMEOUT_MS 150  // Increased for busy bus with Zennio polling
#define BUS_ACTIVITY_WINDOW_MS 500
#define MAX_TX_RETRIES 5
#define QUERY_RESPONSE_TIMEOUT_MS 50

#define WDT_TIMEOUT 30

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
