#ifndef PROJECT_DALI_PROTOCOL_H
#define PROJECT_DALI_PROTOCOL_H

#include <Arduino.h>

#define DALI_OFF 0x00
#define DALI_UP 0x01
#define DALI_DOWN 0x02
#define DALI_STEP_UP 0x03
#define DALI_STEP_DOWN 0x04
#define DALI_RECALL_MAX_LEVEL 0x05
#define DALI_RECALL_MIN_LEVEL 0x06
#define DALI_STEP_DOWN_AND_OFF 0x07
#define DALI_ON_AND_STEP_UP 0x08
#define DALI_ENABLE_DAPC_SEQUENCE 0x09
#define DALI_GO_TO_LAST_ACTIVE_LEVEL 0x0A
#define DALI_CONTINUOUS_UP 0x0B
#define DALI_CONTINUOUS_DOWN 0x0C
#define DALI_GO_TO_SCENE 0x10
#define DALI_RESET 0x20
#define DALI_STORE_ACTUAL_LEVEL_IN_DTR 0x21
#define DALI_STORE_DTR_AS_MAX_LEVEL 0x2A
#define DALI_STORE_DTR_AS_MIN_LEVEL 0x2B
#define DALI_STORE_DTR_AS_SYSTEM_FAILURE_LEVEL 0x2C
#define DALI_STORE_DTR_AS_POWER_ON_LEVEL 0x2D
#define DALI_STORE_DTR_AS_FADE_TIME 0x2E
#define DALI_STORE_DTR_AS_FADE_RATE 0x2F

#define DALI_QUERY_STATUS 0x90
#define DALI_QUERY_CONTROL_GEAR 0x91
#define DALI_QUERY_LAMP_FAILURE 0x92
#define DALI_QUERY_LAMP_POWER_ON 0x94
#define DALI_QUERY_LIMIT_ERROR 0x95
#define DALI_QUERY_RESET_STATE 0x93
#define DALI_QUERY_MISSING_SHORT_ADDRESS 0x96
#define DALI_QUERY_VERSION_NUMBER 0x97
#define DALI_QUERY_CONTENT_DTR0 0x98
#define DALI_QUERY_DEVICE_TYPE 0x99
#define DALI_QUERY_PHYSICAL_MINIMUM 0x9A
#define DALI_QUERY_PHYSICAL_MINIMUM_LEVEL 0x9A
#define DALI_QUERY_POWER_FAILURE 0x9B
#define DALI_QUERY_CONTENT_DTR1 0x9C
#define DALI_QUERY_CONTENT_DTR2 0x9D
#define DALI_QUERY_ACTUAL_LEVEL 0xA0
#define DALI_QUERY_MAX_LEVEL 0xA1
#define DALI_QUERY_MIN_LEVEL 0xA2
#define DALI_QUERY_POWER_ON_LEVEL 0xA3
#define DALI_QUERY_SYSTEM_FAILURE_LEVEL 0xA4
#define DALI_QUERY_FADE_TIME_RATE 0xA5
#define DALI_QUERY_SCENE_LEVEL 0xB0

#define DALI_SET_DTR0 0xA3
#define DALI_INITIALISE 0xA5
#define DALI_RANDOMISE 0xA7
#define DALI_COMPARE 0xA9
#define DALI_WITHDRAW 0xAB
#define DALI_SEARCHADDRH 0xB1
#define DALI_SEARCHADDRM 0xB3
#define DALI_SEARCHADDRL 0xB5
#define DALI_PROGRAM_SHORT_ADDRESS 0xB7
#define DALI_VERIFY_SHORT_ADDRESS 0xB9
#define DALI_QUERY_SHORT_ADDRESS 0xBB
#define DALI_QUERY_RANDOM_ADDRESS_H 0xBC
#define DALI_QUERY_RANDOM_ADDRESS_M 0xBD
#define DALI_QUERY_RANDOM_ADDRESS_L 0xBE
#define DALI_ENABLE_DEVICE_TYPE 0xC1
#define DALI_DATA_TRANSFER_REGISTER1 0xC3
#define DALI_DATA_TRANSFER_REGISTER2 0xC5

// DT8 Color Commands (IEC 62386-209) - require ENABLE_DEVICE_TYPE(8) first
#define DALI_DT8_SET_TEMP_X_COORD 0xE0        // Set temporary X coordinate (DTR0=LSB, DTR1=MSB)
#define DALI_DT8_SET_TEMP_Y_COORD 0xE1        // Set temporary Y coordinate (DTR0=LSB, DTR1=MSB)
#define DALI_DT8_ACTIVATE 0xE2                // Apply temporary color values
#define DALI_DT8_X_COORD_STEP_UP 0xE3         // Step X coordinate up
#define DALI_DT8_X_COORD_STEP_DOWN 0xE4       // Step X coordinate down
#define DALI_DT8_Y_COORD_STEP_UP 0xE5         // Step Y coordinate up
#define DALI_DT8_Y_COORD_STEP_DOWN 0xE6       // Step Y coordinate down
#define DALI_DT8_SET_TEMP_COLOUR_TEMP 0xE7    // Set temporary color temp (DTR0=LSB, DTR1=MSB mirek)
#define DALI_DT8_COLOUR_TEMP_STEP_COOLER 0xE8 // Step color temp cooler
#define DALI_DT8_COLOUR_TEMP_STEP_WARMER 0xE9 // Step color temp warmer
#define DALI_DT8_SET_TEMP_PRIMARY_N 0xEA      // Set temporary primary N dim level (DTR0=level, DTR1=N, DTR2=?)
#define DALI_DT8_SET_TEMP_RGB 0xEB            // Set temporary RGB (DTR0=R, DTR1=G, DTR2=B)
#define DALI_DT8_SET_TEMP_WAF 0xEC            // Set temporary WAF (DTR0=W, DTR1=A, DTR2=F)
#define DALI_DT8_SET_TEMP_RGBWAF_CTRL 0xED    // Set RGBWAF control (DTR0=control flags)
#define DALI_DT8_COPY_REPORT_TO_TEMP 0xEE     // Copy report values to temporary
#define DALI_DT8_STORE_TY_PRIMARY_N 0xF0      // Store TY primary N (send twice)
#define DALI_DT8_STORE_XY_COORD_PRIMARY_N 0xF1 // Store XY coordinate primary N (send twice)
#define DALI_DT8_STORE_COLOUR_TEMP_LIMIT 0xF2 // Store color temp limit (send twice)
#define DALI_DT8_STORE_GEAR_FEATURES 0xF3     // Store gear features status (send twice)
#define DALI_DT8_ASSIGN_COLOUR_TO_LINKED 0xF5 // Assign colour to linked channel (send twice)
#define DALI_DT8_START_AUTO_CALIBRATION 0xF6  // Start auto calibration
#define DALI_DT8_QUERY_GEAR_FEATURES 0xF7     // Query gear features/status
#define DALI_DT8_QUERY_COLOUR_STATUS 0xF8     // Query colour status
#define DALI_DT8_QUERY_COLOUR_TYPE_FEATURES 0xF9 // Query colour type features
#define DALI_DT8_QUERY_COLOUR_VALUE 0xFA      // Query colour value (DTR0 selects which value)
#define DALI_DT8_QUERY_RGBWAF_CONTROL 0xFB    // Query RGBWAF control
#define DALI_DT8_QUERY_ASSIGNED_COLOUR 0xFC   // Query assigned colour

// DT8 QueryColourValue DTR0 values
#define DALI_DT8_QCV_X_COORD 0
#define DALI_DT8_QCV_Y_COORD 1
#define DALI_DT8_QCV_COLOUR_TEMP 2
#define DALI_DT8_QCV_RED_DIM_LEVEL 9
#define DALI_DT8_QCV_GREEN_DIM_LEVEL 10
#define DALI_DT8_QCV_BLUE_DIM_LEVEL 11
#define DALI_DT8_QCV_WHITE_DIM_LEVEL 12
#define DALI_DT8_QCV_AMBER_DIM_LEVEL 13
#define DALI_DT8_QCV_FREECOLOUR_DIM_LEVEL 14
#define DALI_DT8_QCV_RGBWAF_CONTROL 15
#define DALI_DT8_QCV_TC_COOLEST 128
#define DALI_DT8_QCV_TC_PHYSICAL_COOLEST 129
#define DALI_DT8_QCV_TC_WARMEST 130
#define DALI_DT8_QCV_TC_PHYSICAL_WARMEST 131
#define DALI_DT8_QCV_TEMP_X_COORD 192
#define DALI_DT8_QCV_TEMP_Y_COORD 193
#define DALI_DT8_QCV_TEMP_COLOUR_TEMP 194
#define DALI_DT8_QCV_TEMP_RED 201
#define DALI_DT8_QCV_TEMP_GREEN 202
#define DALI_DT8_QCV_TEMP_BLUE 203
#define DALI_DT8_QCV_TEMP_WHITE 204
#define DALI_DT8_QCV_TEMP_AMBER 205
#define DALI_DT8_QCV_TEMP_FREECOLOUR 206
#define DALI_DT8_QCV_TEMP_RGBWAF_CTRL 207
#define DALI_DT8_QCV_TEMP_COLOUR_TYPE 208

// DT8 Color Type Features bits
#define DALI_DT8_FEATURE_XY_CAPABLE 0x01
#define DALI_DT8_FEATURE_TC_CAPABLE 0x02
#define DALI_DT8_FEATURE_PRIMARY_N_MASK 0x1C  // bits 2-4: number of primaries
#define DALI_DT8_FEATURE_RGBWAF_MASK 0xE0    // bits 5-7: RGBWAF channels

#define DALI_MAX 0xFE
#define DALI_MASK 0xFF

#define DALI_BROADCAST_ADDR 0xFF
#define DALI_GROUP_ADDR_START 0x80
#define DALI_GROUP_ADDR_END 0x8F

struct DaliMessage {
    unsigned long timestamp;
    uint8_t raw_bytes[4];
    uint8_t length;
    bool is_tx;
    String source;
    String raw;
    String description;

    struct {
        String type;
        uint8_t address;
        String address_type;
        uint8_t level;
        float level_percent;
    } parsed;
};

struct DaliCommand {
    String command_type;
    uint8_t address;
    String address_type;
    uint8_t level;
    uint8_t scene;
    uint8_t group;
    uint8_t fade_time;
    uint8_t fade_rate;
    bool force;
    unsigned long queued_at;
};

struct DaliDevice {
    uint8_t address;
    String type;
    String status;
    bool lamp_failure;
    uint8_t min_level;
    uint8_t max_level;
};

struct DaliScanResult {
    unsigned long scan_timestamp;
    std::vector<DaliDevice> devices;
    uint8_t total_found;
};

#endif
