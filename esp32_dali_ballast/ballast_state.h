#ifndef BALLAST_STATE_H
#define BALLAST_STATE_H

#include <Arduino.h>

// Device Type enumeration
enum DeviceType {
    DT0_NORMAL = 0,           // Normal - IEC 62386-102
    DT6_LED = 6,              // LED - IEC 62386-207
    DT8_RGB = 8,              // RGB - IEC 62386-209
    DT8_RGBW = 9,             // RGBW - IEC 62386-209
    DT8_COLOR_TEMP = 10       // Colour Temperature - IEC 62386-209
};

struct BallastState {
    uint8_t short_address;        // 0-63 (configurable), 255 = unaddressed
    uint8_t device_type;          // Device type (DT0, DT6, DT8)
    bool address_mode_auto;       // true = automatic commissioning, false = manual

    // Standard brightness control
    uint8_t actual_level;         // Current brightness 0-254
    uint8_t target_level;         // Target brightness for fading
    uint8_t min_level;            // Minimum level (default: 1)
    uint8_t max_level;            // Maximum level (default: 254)
    uint8_t power_on_level;       // Level on power-up (default: 254)
    uint8_t system_failure_level; // Level on system failure (default: 254)
    uint8_t fade_time;            // Fade time (0-15)
    uint8_t fade_rate;            // Fade rate (0-15)
    uint8_t scene_levels[16];     // Scene 0-15 levels
    uint8_t dtr0;                 // Data Transfer Register 0
    uint8_t dtr1;                 // Data Transfer Register 1
    uint8_t dtr2;                 // Data Transfer Register 2

    // DT6 LED - Two channel control (warm/cool)
    uint8_t channel_warm;         // Warm white channel (0-254)
    uint8_t channel_cool;         // Cool white channel (0-254)

    // DT8 RGB/RGBW color control
    uint8_t color_r;              // Red channel (0-255)
    uint8_t color_g;              // Green channel (0-255)
    uint8_t color_b;              // Blue channel (0-255)
    uint8_t color_w;              // White channel (0-255) - for RGBW
    uint8_t color_a;              // Amber channel (0-255) - optional
    uint8_t color_f;              // Free color channel (0-255) - optional

    // DT8 Color Temperature control
    uint16_t color_temp_kelvin;   // Color temperature in Kelvin (2700-6500 typical)

    // Commissioning state
    uint32_t random_address;      // Random address for commissioning
    uint32_t search_address;      // Search address during commissioning
    bool initialise_state;        // In initialise mode
    bool withdraw_state;          // Withdrawn from search
    uint8_t enabled_device_type;  // Currently enabled device type for extended commands

    // Status flags
    bool ballast_failure;         // Ballast failure status
    bool lamp_failure;            // Lamp failure status
    bool lamp_arc_power_on;       // Arc power status
    bool limit_error;             // Limit error status
    bool fade_running;            // Fade in progress
    bool reset_state;             // Reset state flag
    unsigned long last_command_time; // For fade timing
};

struct BallastMessage {
    unsigned long timestamp;
    bool is_query_response;   // true = query response sent, false = command received
    uint8_t raw_bytes[2];     // Received command
    uint8_t response_byte;    // Response sent (if query)
    String command_type;      // "set_level", "query_status", etc.
    uint8_t address;          // Target address
    uint8_t value;            // Level or command parameter
    float value_percent;      // Percentage for levels
    String description;       // Human-readable description
    String source;            // "bus"
};

struct QueuedResponse {
    uint8_t data;
    unsigned long queued_at;
    uint8_t retry_count;
};

#endif
