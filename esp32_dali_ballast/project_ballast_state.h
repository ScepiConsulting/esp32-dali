#ifndef PROJECT_BALLAST_STATE_H
#define PROJECT_BALLAST_STATE_H

#include <Arduino.h>

// Device Type enumeration (IEC 62386 part numbers)
enum DeviceType {
    DT0_NORMAL = 0,           // Normal fluorescent - IEC 62386-102
    DT6_LED = 6,              // LED - IEC 62386-207
    DT8_COLOUR = 8            // Colour control (RGB, RGBW, Tc) - IEC 62386-209
};

// DT8 Colour Type (sub-modes within Device Type 8)
enum DT8ColourType {
    DT8_MODE_XY = 0,          // CIE xy chromaticity coordinates
    DT8_MODE_TC = 1,          // Colour temperature Tc
    DT8_MODE_PRIMARY_N = 2,   // Primary N dimming
    DT8_MODE_RGBWAF = 3       // RGBWAF channels (RGB, RGBW, etc.)
};

// How the address was assigned
enum AddressSource {
    ADDR_UNASSIGNED = 0,          // No address assigned yet (255)
    ADDR_MANUAL = 1,              // Manually set via web interface
    ADDR_COMMISSIONED = 2         // Assigned via DALI commissioning
};

struct BallastState {
    uint8_t short_address;        // 0-63 (configurable), 255 = unaddressed
    uint8_t device_type;          // Device type (DT0, DT6, DT8)
    bool address_mode_auto;       // true = automatic commissioning, false = manual
    uint8_t address_source;       // How address was assigned (AddressSource enum)

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

    // DT8 RGB/RGBW color control - Active values
    uint8_t color_r;              // Red channel (0-254)
    uint8_t color_g;              // Green channel (0-254)
    uint8_t color_b;              // Blue channel (0-254)
    uint8_t color_w;              // White channel (0-254) - for RGBW
    uint8_t color_a;              // Amber channel (0-254) - optional
    uint8_t color_f;              // Free color channel (0-254) - optional

    // DT8 RGB/RGBW color control - Temporary values (set before Activate)
    uint8_t temp_color_r;         // Temporary Red channel
    uint8_t temp_color_g;         // Temporary Green channel
    uint8_t temp_color_b;         // Temporary Blue channel
    uint8_t temp_color_w;         // Temporary White channel
    uint8_t temp_color_a;         // Temporary Amber channel
    uint8_t temp_color_f;         // Temporary Free color channel
    uint8_t rgbwaf_control;       // RGBWAF control flags (which channels are active)

    // DT8 Color Temperature control
    uint16_t color_temp_mirek;    // Color temperature in mirek (153-370 typical, = 1000000/Kelvin)
    uint16_t temp_color_temp;     // Temporary color temperature (before Activate)
    uint16_t color_temp_tc_coolest;    // Coolest color temp limit (mirek)
    uint16_t color_temp_tc_warmest;    // Warmest color temp limit (mirek)
    uint16_t color_temp_physical_coolest; // Physical coolest limit
    uint16_t color_temp_physical_warmest; // Physical warmest limit

    // DT8 XY Chromaticity coordinates (CIE 1931)
    uint16_t color_x;             // X coordinate (0-65535 maps to 0.0-1.0)
    uint16_t color_y;             // Y coordinate (0-65535 maps to 0.0-1.0)
    uint16_t temp_color_x;        // Temporary X coordinate
    uint16_t temp_color_y;        // Temporary Y coordinate

    // DT8 Color type and status
    uint8_t active_color_type;    // 0=xy, 1=Tc, 2=primaryN, 3=RGBWAF
    uint8_t color_status;         // Color status flags
    uint8_t color_type_features;  // Supported color types bitmap

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
    bool quiescent_mode;          // DALI-2 quiescent mode (stop sending events)
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
