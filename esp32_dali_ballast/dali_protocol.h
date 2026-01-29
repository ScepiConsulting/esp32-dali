#ifndef DALI_PROTOCOL_H
#define DALI_PROTOCOL_H

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
#define DALI_QUERY_RESET_STATE 0x95
#define DALI_QUERY_MISSING_SHORT_ADDRESS 0x96
#define DALI_QUERY_VERSION_NUMBER 0x97
#define DALI_QUERY_CONTENT_DTR0 0x98
#define DALI_QUERY_DEVICE_TYPE 0x99
#define DALI_QUERY_PHYSICAL_MINIMUM 0x9A
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

#define DALI_DATA_TRANSFER_REGISTER 0xA3
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
#define DALI_ENABLE_DEVICE_TYPE 0xC1
#define DALI_DATA_TRANSFER_REGISTER1 0xC3
#define DALI_DATA_TRANSFER_REGISTER2 0xC5

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
