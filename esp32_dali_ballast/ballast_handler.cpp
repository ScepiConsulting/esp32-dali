#include "ballast_handler.h"
#include "config.h"
#include "diagnostics.h"
#include "mqtt_handler.h"
#include <Preferences.h>

Dali dali;
BallastState ballastState;
BallastMessage recentMessages[RECENT_MESSAGES_SIZE];
uint8_t recentMessagesIndex = 0;
// Queue removed - DALI-2 backward frames have no retry mechanism
unsigned long lastBusActivityTime = 0;
bool busIsIdle = true;

hw_timer_t *timer = NULL;

// Fade time lookup table (milliseconds)
const uint32_t fade_times[] = {
    0, 707, 1000, 1410, 2000, 2830, 4000, 5660,
    8000, 11310, 16000, 22630, 32000, 45250, 64000, 90510
};

uint8_t bus_is_high() {
  return digitalRead(DALI_RX_PIN);
}

void bus_set_low() {
  digitalWrite(DALI_TX_PIN, LOW);
}

void bus_set_high() {
  digitalWrite(DALI_TX_PIN, HIGH);
}

void ARDUINO_ISR_ATTR onTimer() {
  dali.timer();
}

void ballastInit() {
  // CRITICAL: Load config FIRST before starting hardware
  // to prevent race condition where DALI interrupt uses uninitialized address
  loadBallastConfig();

  pinMode(DALI_RX_PIN, INPUT);
  pinMode(DALI_TX_PIN, OUTPUT);

  // Initialize DALI library (but don't start timer yet)
  dali.begin(bus_is_high, bus_set_high, bus_set_low);
  
  // Start timer LAST - after config is loaded and DALI is initialized
  // This prevents receiving frames before address is set
  timer = timerBegin(DALI_TIMER_FREQ);
  timerAttachInterrupt(timer, &onTimer);
  timerAlarm(timer, 1000, true, 0);

  updateLED();

#ifdef DEBUG_SERIAL
  Serial.printf("DALI Ballast initialized - Address: %d (0x%02X), Device Type: %d\n",
                ballastState.short_address, ballastState.short_address, ballastState.device_type);
  Serial.printf("Address mode: %s\n", ballastState.address_mode_auto ? "Auto" : "Manual");
  if (!ballastState.address_mode_auto) {
    Serial.printf("Expected address byte for commands: 0x%02X\n", (ballastState.short_address << 1) | 1);
    Serial.printf("Expected address byte for DAPC: 0x%02X\n", (ballastState.short_address << 1));
  }
#endif
}

void loadBallastConfig() {
  Preferences preferences;
  preferences.begin("ballast", true);

  ballastState.address_mode_auto = preferences.getBool("addr_auto", true);
  ballastState.short_address = preferences.getUChar("address", 255);
  ballastState.address_source = preferences.getUChar("addr_src", ADDR_UNASSIGNED);
  ballastState.device_type = preferences.getUChar("dev_type", DT0_NORMAL);
  ballastState.active_color_type = preferences.getUChar("color_type", DT8_MODE_RGBWAF);
  
  // Migration: if address is valid (0-63) but source is unassigned, assume it was manual
  if (ballastState.short_address != 255 && ballastState.address_source == ADDR_UNASSIGNED) {
    ballastState.address_source = ADDR_MANUAL;
  }
  ballastState.min_level = preferences.getUChar("min_level", 1);
  ballastState.max_level = preferences.getUChar("max_level", 254);
  ballastState.power_on_level = preferences.getUChar("power_on", 254);
  ballastState.system_failure_level = preferences.getUChar("sys_fail", 254);
  ballastState.fade_time = preferences.getUChar("fade_time", 0);
  ballastState.fade_rate = preferences.getUChar("fade_rate", 7);
  ballastState.dtr0 = 0;
  ballastState.dtr1 = 0;
  ballastState.dtr2 = 0;

  // DT6 LED channels
  ballastState.channel_warm = preferences.getUChar("ch_warm", 254);
  ballastState.channel_cool = preferences.getUChar("ch_cool", 254);

  // DT8 RGB/RGBW colors
  ballastState.color_r = preferences.getUChar("color_r", 255);
  ballastState.color_g = preferences.getUChar("color_g", 255);
  ballastState.color_b = preferences.getUChar("color_b", 255);
  ballastState.color_w = preferences.getUChar("color_w", 255);
  ballastState.color_a = preferences.getUChar("color_a", 0);
  ballastState.color_f = preferences.getUChar("color_f", 0);

  // DT8 Color Temperature (stored as mirek internally, but saved as Kelvin for user convenience)
  uint16_t saved_kelvin = preferences.getUShort("cct_kelvin", 4000);
  ballastState.color_temp_mirek = (saved_kelvin > 0) ? (1000000 / saved_kelvin) : 250;  // Convert Kelvin to mirek
  ballastState.color_temp_tc_coolest = 153;   // ~6500K
  ballastState.color_temp_tc_warmest = 370;   // ~2700K
  ballastState.color_temp_physical_coolest = 153;
  ballastState.color_temp_physical_warmest = 370;

  for (int i = 0; i < 16; i++) {
    String key = "scene" + String(i);
    ballastState.scene_levels[i] = preferences.getUChar(key.c_str(), 0xFF);
  }

  preferences.end();

  // Initialize state
  ballastState.actual_level = ballastState.power_on_level;
  ballastState.target_level = ballastState.power_on_level;
  ballastState.ballast_failure = false;
  ballastState.lamp_arc_power_on = (ballastState.actual_level > 0);
  ballastState.lamp_failure = false;
  ballastState.limit_error = false;
  ballastState.fade_running = false;
  ballastState.reset_state = false;
  ballastState.last_command_time = millis();

  // Initialize commissioning state
  ballastState.random_address = 0;
  ballastState.search_address = 0;
  ballastState.initialise_state = false;
  ballastState.withdraw_state = false;
}

void saveBallastConfig() {
  Preferences preferences;
  preferences.begin("ballast", false);

  preferences.putBool("addr_auto", ballastState.address_mode_auto);
  preferences.putUChar("address", ballastState.short_address);
  preferences.putUChar("addr_src", ballastState.address_source);
  preferences.putUChar("dev_type", ballastState.device_type);
  preferences.putUChar("color_type", ballastState.active_color_type);
  preferences.putUChar("min_level", ballastState.min_level);
  preferences.putUChar("max_level", ballastState.max_level);
  preferences.putUChar("power_on", ballastState.power_on_level);
  preferences.putUChar("sys_fail", ballastState.system_failure_level);
  preferences.putUChar("fade_time", ballastState.fade_time);
  preferences.putUChar("fade_rate", ballastState.fade_rate);

  // DT6 LED channels
  preferences.putUChar("ch_warm", ballastState.channel_warm);
  preferences.putUChar("ch_cool", ballastState.channel_cool);

  // DT8 RGB/RGBW colors
  preferences.putUChar("color_r", ballastState.color_r);
  preferences.putUChar("color_g", ballastState.color_g);
  preferences.putUChar("color_b", ballastState.color_b);
  preferences.putUChar("color_w", ballastState.color_w);
  preferences.putUChar("color_a", ballastState.color_a);
  preferences.putUChar("color_f", ballastState.color_f);

  // DT8 Color Temperature (convert mirek back to Kelvin for storage)
  uint16_t kelvin_to_save = (ballastState.color_temp_mirek > 0) ? (1000000 / ballastState.color_temp_mirek) : 4000;
  preferences.putUShort("cct_kelvin", kelvin_to_save);

  for (int i = 0; i < 16; i++) {
    String key = "scene" + String(i);
    preferences.putUChar(key.c_str(), ballastState.scene_levels[i]);
  }

  preferences.end();
#ifdef DEBUG_SERIAL
  Serial.println("[Ballast] Config saved to flash");
#endif
}

void monitorDaliBus() {
  uint8_t rx_data[4];
  uint8_t result = dali.rx(rx_data);

  // Handle received frames
  // DALI-1 control gear uses only 8-bit (backward) and 16-bit (forward) frames
  // 24-bit frames are either:
  //   1. DALI-2 device commands (specific format with device address in byte 0)
  //   2. Two merged 16-bit frames (common issue with fast consecutive commands)
  
  if (result == 24) {
    // 24-bit frame - DALI-2 device command (IEC 62386-103)
    // Format: [address byte] [instance byte] [opcode]
    uint8_t addr_byte = rx_data[0];
    uint8_t inst_byte = rx_data[1];
    uint8_t opcode = rx_data[2];

#ifdef DEBUG_SERIAL
    Serial.printf("[DALI] 24-bit device command: addr=0x%02X inst=0x%02X opcode=0x%02X\n", 
                  addr_byte, inst_byte, opcode);
#endif

    updateBusActivity();
    
    // Check if this command is for us
    bool for_us = false;
    if (addr_byte == 0xFF) {
      // Broadcast to all devices
      for_us = true;
    } else if (addr_byte == 0xFD) {
      // Broadcast to unaddressed devices
      for_us = (ballastState.short_address == 0xFF);
    } else if ((addr_byte & 0x01) == 0x01) {
      // Short address: 0AAAAAA1
      uint8_t addr = (addr_byte >> 1) & 0x3F;
      for_us = (addr == ballastState.short_address);
    }
    
    if (for_us) {
      processDeviceCommand(inst_byte, opcode);
    }
    return;
  }

  if (result == 16) {
    uint8_t addr_byte = rx_data[0];
    uint8_t data_byte = rx_data[1];
    
    updateBusActivity();

#ifdef DEBUG_SERIAL
    Serial.printf("[DALI] Received 16-bit: 0x%02X 0x%02X\n", addr_byte, data_byte);
#endif

    // Check for special commands first (broadcast to all devices)
    if (isSpecialCommand(addr_byte)) {
      processSpecialCommand(addr_byte, data_byte);
    } else if (isForThisBallast(addr_byte)) {
      processCommand(addr_byte, data_byte);
    }
  }
}

void updateBusActivity() {
  lastBusActivityTime = millis();
  busIsIdle = false;
}

bool isBusIdle() {
  unsigned long now = millis();
  unsigned long timeSinceActivity = now - lastBusActivityTime;
  
  if (timeSinceActivity >= BUS_IDLE_TIMEOUT_MS) {
    busIsIdle = true;
  }
  
  return busIsIdle;
}

// Note: DALI-2 backward frames have no retry mechanism
// Response must be sent within 2.91-22ms after forward frame
// If it fails, it fails - master will assume no response

// Check if address byte is a DALI special command (broadcast to all devices)
// Special commands have patterns: 101CCCCS (0xA1-0xBF odd) or 110CCCCS (0xC1-0xDF odd)
bool isSpecialCommand(uint8_t addr_byte) {
  // Must have LSB = 1 (command mode)
  if (!(addr_byte & 0x01)) return false;
  
  // Pattern 101xxxxx (0xA0-0xBF) - but only odd values (bit 0 = 1)
  if ((addr_byte >= 0xA1) && (addr_byte <= 0xBF) && (addr_byte & 0x01)) return true;
  
  // Pattern 110xxxxx (0xC0-0xDF) - but only odd values (bit 0 = 1)  
  if ((addr_byte >= 0xC1) && (addr_byte <= 0xDF) && (addr_byte & 0x01)) return true;
  
  return false;
}

// Process DALI special commands (broadcast commands with parameter in data byte)
void processSpecialCommand(uint8_t cmd_byte, uint8_t param) {
#ifdef DEBUG_SERIAL
  Serial.printf("[DALI] Special command: 0x%02X param: 0x%02X\n", cmd_byte, param);
#endif

  switch (cmd_byte) {
    case 0xA1: // TERMINATE - exit initialisation mode
      ballastState.initialise_state = false;
      ballastState.withdraw_state = false;
#ifdef DEBUG_SERIAL
      Serial.println("[Special] TERMINATE - exiting initialisation mode");
#endif
      break;

    case 0xA3: // DTR0 - set data transfer register 0
      ballastState.dtr0 = param;
#ifdef DEBUG_SERIAL
      Serial.printf("[Special] DTR0 = %d (0x%02X)\n", param, param);
#endif
      break;

    case 0xA5: // INITIALISE - enter initialisation mode
      // param: 0x00 = all devices, 0xFF = unaddressed only, (addr<<1)|1 = specific address
      // Only respond if address_mode_auto is true (allows commissioning)
      if (ballastState.address_mode_auto) {
        bool should_init = false;
        if (param == 0x00) {
          should_init = true;  // All devices
        } else if (param == 0xFF) {
          should_init = (ballastState.short_address == 255);  // Unaddressed only
        } else if ((param & 0x01) && ballastState.short_address == ((param >> 1) & 0x3F)) {
          should_init = true;  // Specific address matches
        }
        if (should_init) {
          ballastState.initialise_state = true;
          ballastState.withdraw_state = false;
#ifdef DEBUG_SERIAL
          Serial.printf("[Special] INITIALISE - entering initialisation mode (param=0x%02X)\n", param);
#endif
        }
      }
      break;

    case 0xA7: // RANDOMISE - generate random address
      if (ballastState.initialise_state) {
        ballastState.random_address = (esp_random() & 0xFFFFFF);
#ifdef DEBUG_SERIAL
        Serial.printf("[Special] RANDOMISE - new random address: 0x%06X\n", ballastState.random_address);
#endif
      }
      break;

    case 0xA9: // COMPARE - compare random address with search address
      if (ballastState.initialise_state && !ballastState.withdraw_state) {
        if (ballastState.random_address <= ballastState.search_address) {
          sendBackwardFrame(0xFF);  // Send immediately, not queued
#ifdef DEBUG_SERIAL
          Serial.printf("[Special] COMPARE - YES (0x%06X <= 0x%06X)\n",
                       ballastState.random_address, ballastState.search_address);
#endif
        }
      }
      break;

    case 0xAB: // WITHDRAW - withdraw from commissioning
      if (ballastState.initialise_state && 
          ballastState.random_address == ballastState.search_address) {
        ballastState.withdraw_state = true;
#ifdef DEBUG_SERIAL
        Serial.println("[Special] WITHDRAW - withdrawn from commissioning");
#endif
      }
      break;

    case 0xB1: // SEARCHADDRH - set search address high byte
      ballastState.search_address = (ballastState.search_address & 0x00FFFF) | ((uint32_t)param << 16);
#ifdef DEBUG_SERIAL
      Serial.printf("[Special] SEARCHADDRH = 0x%02X, search_addr = 0x%06X\n", param, ballastState.search_address);
#endif
      break;

    case 0xB3: // SEARCHADDRM - set search address middle byte
      ballastState.search_address = (ballastState.search_address & 0xFF00FF) | ((uint32_t)param << 8);
#ifdef DEBUG_SERIAL
      Serial.printf("[Special] SEARCHADDRM = 0x%02X, search_addr = 0x%06X\n", param, ballastState.search_address);
#endif
      break;

    case 0xB5: // SEARCHADDRL - set search address low byte
      ballastState.search_address = (ballastState.search_address & 0xFFFF00) | param;
#ifdef DEBUG_SERIAL
      Serial.printf("[Special] SEARCHADDRL = 0x%02X, search_addr = 0x%06X\n", param, ballastState.search_address);
#endif
      break;

    case 0xB7: // PROGRAM SHORT ADDRESS
      if (ballastState.initialise_state && !ballastState.withdraw_state &&
          ballastState.random_address == ballastState.search_address) {
        uint8_t new_addr = (param >> 1) & 0x3F;
        ballastState.short_address = new_addr;
        ballastState.address_source = ADDR_COMMISSIONED;  // Mark as commissioned via DALI
        // Note: Not saving to flash - save manually via web UI
        publishBallastConfig();
#ifdef DEBUG_SERIAL
        Serial.printf("[Special] PROGRAM_SHORT_ADDRESS = %d (commissioned)\n", new_addr);
#endif
      }
      break;

    case 0xB9: // VERIFY SHORT ADDRESS
      if (ballastState.initialise_state) {
        uint8_t check_addr = (param >> 1) & 0x3F;
        if (check_addr == ballastState.short_address) {
          sendBackwardFrame(0xFF);
#ifdef DEBUG_SERIAL
          Serial.printf("[Special] VERIFY_SHORT_ADDRESS - YES (addr=%d)\n", check_addr);
#endif
        }
      }
      break;

    case 0xBB: // QUERY SHORT ADDRESS
      if (ballastState.initialise_state && !ballastState.withdraw_state &&
          ballastState.random_address == ballastState.search_address) {
        sendBackwardFrame((ballastState.short_address << 1) | 0x01);
#ifdef DEBUG_SERIAL
        Serial.printf("[Special] QUERY_SHORT_ADDRESS - responding with %d\n", ballastState.short_address);
#endif
      }
      break;

    case 0xC1: // ENABLE DEVICE TYPE
      ballastState.enabled_device_type = param;
#ifdef DEBUG_SERIAL
      Serial.printf("[Special] ENABLE_DEVICE_TYPE = %d\n", param);
#endif
      break;

    case 0xC3: // DTR1 - set data transfer register 1
      ballastState.dtr1 = param;
#ifdef DEBUG_SERIAL
      Serial.printf("[Special] DTR1 = %d (0x%02X)\n", param, param);
#endif
      break;

    case 0xC5: // DTR2 - set data transfer register 2
      ballastState.dtr2 = param;
#ifdef DEBUG_SERIAL
      Serial.printf("[Special] DTR2 = %d (0x%02X)\n", param, param);
#endif
      break;

    default:
#ifdef DEBUG_SERIAL
      Serial.printf("[Special] Unknown special command: 0x%02X param: 0x%02X\n", cmd_byte, param);
#endif
      break;
  }
}

// Process DALI-2 device commands (24-bit frames, IEC 62386-103)
void processDeviceCommand(uint8_t inst_byte, uint8_t opcode) {
#ifdef DEBUG_SERIAL
  Serial.printf("[DALI-2] Device command: inst=0x%02X opcode=0x%02X\n", inst_byte, opcode);
#endif

  // inst_byte 0xFE = command to device itself (not instance-specific)
  // inst_byte 0xFF = command to all instances
  
  switch (opcode) {
    case 0x00: // IdentifyDevice
#ifdef DEBUG_SERIAL
      Serial.println("[DALI-2] IdentifyDevice - should blink/identify");
#endif
      break;

    case 0x01: // ResetPowerCycleSeen
      ballastState.reset_state = false;
#ifdef DEBUG_SERIAL
      Serial.println("[DALI-2] ResetPowerCycleSeen");
#endif
      break;

    case 0x10: // Reset
      // Reset device to default state
      ballastState.actual_level = ballastState.power_on_level;
      ballastState.fade_running = false;
      ballastState.reset_state = true;
#ifdef DEBUG_SERIAL
      Serial.println("[DALI-2] Reset device");
#endif
      break;

    case 0x1D: // StartQuiescentMode
      // Enter quiescent mode - stop sending events
      ballastState.quiescent_mode = true;
#ifdef DEBUG_SERIAL
      Serial.println("[DALI-2] StartQuiescentMode - entering quiet mode");
#endif
      break;

    case 0x1E: // StopQuiescentMode
      // Exit quiescent mode - resume normal operation
      ballastState.quiescent_mode = false;
#ifdef DEBUG_SERIAL
      Serial.println("[DALI-2] StopQuiescentMode - resuming normal mode");
#endif
      break;

    case 0x30: // QueryDeviceStatus
      {
        uint8_t status = 0;
        // Bit 0: inputDeviceError
        // Bit 1: quiescentMode
        if (ballastState.quiescent_mode) status |= 0x02;
        // Bit 2: shortAddress is MASK
        if (ballastState.short_address == 0xFF) status |= 0x04;
        // Bit 3: applicationActive
        // Bit 4: applicationControllerError
        // Bit 5: powerCycleSeen
        if (ballastState.reset_state) status |= 0x20;
        // Bit 6: resetState
        if (ballastState.reset_state) status |= 0x40;
        sendBackwardFrame(status);
#ifdef DEBUG_SERIAL
        Serial.printf("[DALI-2] QueryDeviceStatus - responding 0x%02X\n", status);
#endif
      }
      break;

    case 0x33: // QueryMissingShortAddress
      if (ballastState.short_address == 0xFF) {
        sendBackwardFrame(0xFF);  // YES - missing short address
#ifdef DEBUG_SERIAL
        Serial.println("[DALI-2] QueryMissingShortAddress - YES (no address)");
#endif
      }
      // No response if we have an address
      break;

    case 0x34: // QueryVersionNumber
      sendBackwardFrame(0x02);  // Version 2.0
#ifdef DEBUG_SERIAL
      Serial.println("[DALI-2] QueryVersionNumber - responding 0x02");
#endif
      break;

    case 0x35: // QueryNumberOfInstances
      sendBackwardFrame(0x01);  // 1 instance (the light)
#ifdef DEBUG_SERIAL
      Serial.println("[DALI-2] QueryNumberOfInstances - responding 0x01");
#endif
      break;

    case 0x36: // QueryContentDTR0
      sendBackwardFrame(ballastState.dtr0);
#ifdef DEBUG_SERIAL
      Serial.printf("[DALI-2] QueryContentDTR0 - responding 0x%02X\n", ballastState.dtr0);
#endif
      break;

    case 0x37: // QueryContentDTR1
      sendBackwardFrame(ballastState.dtr1);
#ifdef DEBUG_SERIAL
      Serial.printf("[DALI-2] QueryContentDTR1 - responding 0x%02X\n", ballastState.dtr1);
#endif
      break;

    case 0x38: // QueryContentDTR2
      sendBackwardFrame(ballastState.dtr2);
#ifdef DEBUG_SERIAL
      Serial.printf("[DALI-2] QueryContentDTR2 - responding 0x%02X\n", ballastState.dtr2);
#endif
      break;

    default:
#ifdef DEBUG_SERIAL
      Serial.printf("[DALI-2] Unknown device opcode: 0x%02X\n", opcode);
#endif
      break;
  }
}

bool isForThisBallast(uint8_t addr_byte) {
  // Direct arc power (LSB = 0)
  if (!(addr_byte & 0x01)) {
    // Broadcast DAPC is 0xFE (address 63 with bit 0=0)
    if (addr_byte == 0xFE) return true;
    
    uint8_t addr = (addr_byte >> 1) & 0x3F;
#ifdef DEBUG_SERIAL
    if (addr == ballastState.short_address) {
      Serial.printf("[Ballast] DAPC for me: addr_byte=0x%02X, decoded=%d, my_addr=%d\n", 
                    addr_byte, addr, ballastState.short_address);
    }
#endif
    return (addr == ballastState.short_address);
  }

  // Command (LSB = 1)
  // Broadcast command is 0xFF (all bits set)
  if (addr_byte == 0xFF) return true;
  
  uint8_t addr = (addr_byte >> 1) & 0x3F;
#ifdef DEBUG_SERIAL
  if (addr == ballastState.short_address) {
    Serial.printf("[Ballast] Command for me: addr_byte=0x%02X, decoded=%d, my_addr=%d\n", 
                  addr_byte, addr, ballastState.short_address);
  }
#endif
  return (addr == ballastState.short_address);
}

void processCommand(uint8_t addr_byte, uint8_t data_byte) {
  incrementRxCount();

  BallastMessage msg;
  msg.timestamp = millis();
  msg.is_query_response = false;
  msg.raw_bytes[0] = addr_byte;
  msg.raw_bytes[1] = data_byte;
  msg.address = ballastState.short_address;
  msg.source = "bus";

  // Direct arc power (bit 0 = 0)
  if (!(addr_byte & 0x01)) {
    if (data_byte <= 254) {
      setLevel(data_byte);
      msg.command_type = "set_brightness";
      msg.value = data_byte;
      msg.value_percent = (data_byte / 254.0) * 100.0;
      msg.description = "Set to " + String(data_byte) + " (" + String(msg.value_percent, 1) + "%)";

      addRecentMessage(msg);
      publishBallastCommand(msg);
      publishBallastState();
    }
    return;
  }

  // Standard commands (data_byte contains the command code)
  switch (data_byte) {
    case DALI_OFF:
      setLevel(0);
      msg.command_type = "off";
      msg.value = 0;
      msg.value_percent = 0;
      msg.description = "Off";
      addRecentMessage(msg);
      publishBallastCommand(msg);
      publishBallastState();
      break;

    case DALI_UP:
      msg.command_type = "fade_up";
      msg.description = "Fade up";
      addRecentMessage(msg);
      publishBallastCommand(msg);
      break;

    case DALI_DOWN:
      msg.command_type = "fade_down";
      msg.description = "Fade down";
      addRecentMessage(msg);
      publishBallastCommand(msg);
      break;

    case DALI_RECALL_MAX_LEVEL:
      setLevel(ballastState.max_level);
      msg.command_type = "recall_max";
      msg.value = ballastState.max_level;
      msg.value_percent = (ballastState.max_level / 254.0) * 100.0;
      msg.description = "Recall max level";
      addRecentMessage(msg);
      publishBallastCommand(msg);
      publishBallastState();
      break;

    case DALI_RECALL_MIN_LEVEL:
      setLevel(ballastState.min_level);
      msg.command_type = "recall_min";
      msg.value = ballastState.min_level;
      msg.value_percent = (ballastState.min_level / 254.0) * 100.0;
      msg.description = "Recall min level";
      addRecentMessage(msg);
      publishBallastCommand(msg);
      publishBallastState();
      break;

    case DALI_CONTINUOUS_UP:
      setLevel(ballastState.max_level);
      msg.command_type = "continuous_up";
      msg.value = ballastState.max_level;
      msg.value_percent = (ballastState.max_level / 254.0) * 100.0;
      msg.description = "Continuous up (fade to max)";
      addRecentMessage(msg);
      publishBallastCommand(msg);
      publishBallastState();
      break;

    case DALI_CONTINUOUS_DOWN:
      setLevel(ballastState.min_level);
      msg.command_type = "continuous_down";
      msg.value = ballastState.min_level;
      msg.value_percent = (ballastState.min_level / 254.0) * 100.0;
      msg.description = "Continuous down (fade to min)";
      addRecentMessage(msg);
      publishBallastCommand(msg);
      publishBallastState();
      break;

    case DALI_STORE_DTR_AS_FADE_TIME:
      ballastState.fade_time = ballastState.dtr0 & 0x0F;
      msg.command_type = "store_dtr_as_fade_time";
      msg.value = ballastState.fade_time;
      msg.description = "Store DTR as fade time: " + String(ballastState.fade_time);
      addRecentMessage(msg);
      publishBallastCommand(msg);
      publishBallastConfig();
      break;

    case DALI_STORE_DTR_AS_FADE_RATE:
      ballastState.fade_rate = ballastState.dtr0 & 0x0F;
      msg.command_type = "store_dtr_as_fade_rate";
      msg.value = ballastState.fade_rate;
      msg.description = "Store DTR as fade rate: " + String(ballastState.fade_rate);
      addRecentMessage(msg);
      publishBallastCommand(msg);
      publishBallastConfig();
      break;

    case DALI_STORE_DTR_AS_MAX_LEVEL:
      ballastState.max_level = ballastState.dtr0;
      msg.command_type = "store_dtr_as_max_level";
      msg.value = ballastState.max_level;
      msg.description = "Store DTR as max level: " + String(ballastState.max_level);
      addRecentMessage(msg);
      publishBallastCommand(msg);
      publishBallastConfig();
      break;

    case DALI_STORE_DTR_AS_MIN_LEVEL:
      ballastState.min_level = ballastState.dtr0;
      msg.command_type = "store_dtr_as_min_level";
      msg.value = ballastState.min_level;
      msg.description = "Store DTR as min level: " + String(ballastState.min_level);
      addRecentMessage(msg);
      publishBallastCommand(msg);
      publishBallastConfig();
      break;

    case DALI_STORE_DTR_AS_POWER_ON_LEVEL:
      ballastState.power_on_level = ballastState.dtr0;
      msg.command_type = "store_dtr_as_power_on_level";
      msg.value = ballastState.power_on_level;
      msg.description = "Store DTR as power-on level: " + String(ballastState.power_on_level);
      addRecentMessage(msg);
      publishBallastCommand(msg);
      publishBallastConfig();
      break;

    case DALI_STORE_DTR_AS_SYSTEM_FAILURE_LEVEL:
      ballastState.system_failure_level = ballastState.dtr0;
      msg.command_type = "store_dtr_as_system_failure_level";
      msg.value = ballastState.system_failure_level;
      msg.description = "Store DTR as system failure level: " + String(ballastState.system_failure_level);
      addRecentMessage(msg);
      publishBallastCommand(msg);
      publishBallastConfig();
      break;

    case DALI_QUERY_STATUS:
      sendBackwardFrame(getStatusByte());
      msg.is_query_response = true;
      msg.command_type = "query_status";
      msg.response_byte = getStatusByte();
      msg.description = "Query status";
      addRecentMessage(msg);
      publishBallastCommand(msg);
      incrementTxCount();
      break;

    case DALI_QUERY_CONTROL_GEAR:
      sendBackwardFrame(0xFF);
      msg.is_query_response = true;
      msg.command_type = "query_control_gear";
      msg.response_byte = 0xFF;
      msg.description = "Query control gear: Present";
      addRecentMessage(msg);
      publishBallastCommand(msg);
      incrementTxCount();
      break;

    case DALI_QUERY_LAMP_FAILURE:
      sendBackwardFrame(ballastState.lamp_failure ? 0xFF : 0x00);
      msg.is_query_response = true;
      msg.command_type = "query_lamp_failure";
      msg.response_byte = ballastState.lamp_failure ? 0xFF : 0x00;
      msg.description = "Query lamp failure: " + String(ballastState.lamp_failure ? "Yes" : "No");
      addRecentMessage(msg);
      publishBallastCommand(msg);
      incrementTxCount();
      break;

    case DALI_QUERY_LAMP_POWER_ON:
      sendBackwardFrame(ballastState.lamp_arc_power_on ? 0xFF : 0x00);
      msg.is_query_response = true;
      msg.command_type = "query_lamp_power_on";
      msg.response_byte = ballastState.lamp_arc_power_on ? 0xFF : 0x00;
      msg.description = "Query lamp power: " + String(ballastState.lamp_arc_power_on ? "On" : "Off");
      addRecentMessage(msg);
      publishBallastCommand(msg);
      incrementTxCount();
      break;

    case DALI_QUERY_ACTUAL_LEVEL:
      sendBackwardFrame(ballastState.actual_level);
      msg.is_query_response = true;
      msg.command_type = "query_actual_level";
      msg.response_byte = ballastState.actual_level;
      msg.value = ballastState.actual_level;
      msg.value_percent = (ballastState.actual_level / 254.0) * 100.0;
      msg.description = "Query actual level: " + String(ballastState.actual_level) + " (" + String(msg.value_percent, 1) + "%)";
      addRecentMessage(msg);
      publishBallastCommand(msg);
      incrementTxCount();
      break;

    case DALI_QUERY_MAX_LEVEL:
      sendBackwardFrame(ballastState.max_level);
      msg.is_query_response = true;
      msg.command_type = "query_max_level";
      msg.response_byte = ballastState.max_level;
      msg.description = "Query max level: " + String(ballastState.max_level);
      addRecentMessage(msg);
      publishBallastCommand(msg);
      incrementTxCount();
      break;

    case DALI_QUERY_MIN_LEVEL:
      sendBackwardFrame(ballastState.min_level);
      msg.is_query_response = true;
      msg.command_type = "query_min_level";
      msg.response_byte = ballastState.min_level;
      msg.description = "Query min level: " + String(ballastState.min_level);
      addRecentMessage(msg);
      publishBallastCommand(msg);
      incrementTxCount();
      break;

    case DALI_QUERY_POWER_ON_LEVEL:
      sendBackwardFrame(ballastState.power_on_level);
      msg.is_query_response = true;
      msg.command_type = "query_power_on_level";
      msg.response_byte = ballastState.power_on_level;
      msg.description = "Query power-on level: " + String(ballastState.power_on_level);
      addRecentMessage(msg);
      publishBallastCommand(msg);
      incrementTxCount();
      break;

    case DALI_QUERY_MISSING_SHORT_ADDRESS:
      // YES (0xFF) = missing address, NO (0x00) = has address
      sendBackwardFrame((ballastState.short_address == 255) ? 0xFF : 0x00);
      msg.is_query_response = true;
      msg.command_type = "query_missing_short_address";
      msg.response_byte = (ballastState.short_address == 255) ? 0xFF : 0x00;
      msg.description = "Query missing short address: " + String((ballastState.short_address == 255) ? "Yes (unaddressed)" : "No (has address)");
      addRecentMessage(msg);
      publishBallastCommand(msg);
      incrementTxCount();
      break;

    case DALI_QUERY_LIMIT_ERROR:
      sendBackwardFrame(0x00); // No limit error
      msg.is_query_response = true;
      msg.command_type = "query_limit_error";
      msg.response_byte = 0x00;
      msg.description = "Query limit error: No";
      addRecentMessage(msg);
      publishBallastCommand(msg);
      incrementTxCount();
      break;

    case DALI_QUERY_RESET_STATE:
      sendBackwardFrame(0x00); // Not in reset state
      msg.is_query_response = true;
      msg.command_type = "query_reset_state";
      msg.response_byte = 0x00;
      msg.description = "Query reset state: No";
      addRecentMessage(msg);
      publishBallastCommand(msg);
      incrementTxCount();
      break;

    case DALI_QUERY_VERSION_NUMBER:
      sendBackwardFrame(0x02); // DALI-2 Version 2.0
      msg.is_query_response = true;
      msg.command_type = "query_version_number";
      msg.response_byte = 0x02;
      msg.description = "Query version: 2.0 (DALI-2)";
      addRecentMessage(msg);
      publishBallastCommand(msg);
      incrementTxCount();
      break;

    case DALI_QUERY_CONTENT_DTR0:
      sendBackwardFrame(ballastState.dtr0);
      msg.is_query_response = true;
      msg.command_type = "query_content_dtr0";
      msg.response_byte = ballastState.dtr0;
      msg.value = ballastState.dtr0;
      msg.description = "Query DTR0: " + String(ballastState.dtr0);
      addRecentMessage(msg);
      publishBallastCommand(msg);
      incrementTxCount();
      break;

    case DALI_QUERY_DEVICE_TYPE:
      sendBackwardFrame(ballastState.device_type);
      msg.is_query_response = true;
      msg.command_type = "query_device_type";
      msg.response_byte = ballastState.device_type;
      msg.description = "Query device type: " + String(ballastState.device_type);
      addRecentMessage(msg);
      publishBallastCommand(msg);
      incrementTxCount();
      break;

    case DALI_QUERY_PHYSICAL_MINIMUM_LEVEL:
      sendBackwardFrame(ballastState.min_level);
      msg.is_query_response = true;
      msg.command_type = "query_physical_minimum";
      msg.response_byte = ballastState.min_level;
      msg.description = "Query physical minimum: " + String(ballastState.min_level);
      addRecentMessage(msg);
      publishBallastCommand(msg);
      incrementTxCount();
      break;

    case DALI_QUERY_POWER_FAILURE:
      sendBackwardFrame(0x00); // No power failure
      msg.is_query_response = true;
      msg.command_type = "query_power_failure";
      msg.response_byte = 0x00;
      msg.description = "Query power failure: No";
      addRecentMessage(msg);
      publishBallastCommand(msg);
      incrementTxCount();
      break;

    case DALI_QUERY_SYSTEM_FAILURE_LEVEL:
      sendBackwardFrame(ballastState.system_failure_level);
      msg.is_query_response = true;
      msg.command_type = "query_system_failure_level";
      msg.response_byte = ballastState.system_failure_level;
      msg.description = "Query system failure level: " + String(ballastState.system_failure_level);
      addRecentMessage(msg);
      publishBallastCommand(msg);
      incrementTxCount();
      break;

    case DALI_QUERY_FADE_TIME_RATE:
      // Upper 4 bits = fade time, lower 4 bits = fade rate
      sendBackwardFrame((ballastState.fade_time << 4) | (ballastState.fade_rate & 0x0F));
      msg.is_query_response = true;
      msg.command_type = "query_fade_time_rate";
      msg.response_byte = (ballastState.fade_time << 4) | (ballastState.fade_rate & 0x0F);
      msg.description = "Query fade time/rate: " + String(ballastState.fade_time) + "/" + String(ballastState.fade_rate);
      addRecentMessage(msg);
      publishBallastCommand(msg);
      incrementTxCount();
      break;

    // NOTE: Commissioning commands (INITIALISE, RANDOMISE, COMPARE, WITHDRAW, etc.)
    // are handled in processSpecialCommand() via isSpecialCommand() check.
    // They use special address bytes (0xA1-0xDF) and won't reach this switch.

    default:
      // Check for scene commands (0x10-0x1F)
      if (data_byte >= 0x10 && data_byte <= 0x1F) {
        uint8_t scene = data_byte - 0x10;
        recallScene(scene);
        msg.command_type = "go_to_scene";
        msg.value = scene;
        msg.description = "Go to scene " + String(scene);
        addRecentMessage(msg);
        publishBallastCommand(msg);
        publishBallastState();
      }
      // Check for query scene level (0xB0-0xBF)
      else if (data_byte >= 0xB0 && data_byte <= 0xBF) {
        uint8_t scene = data_byte - 0xB0;
        uint8_t level = queryScene(scene);
        sendBackwardFrame(level);
        msg.is_query_response = true;
        msg.command_type = "query_scene_level";
        msg.response_byte = level;
        msg.value = scene;
        msg.description = "Query scene " + String(scene) + " level: " + String(level);
        addRecentMessage(msg);
        publishBallastCommand(msg);
        incrementTxCount();
      }
      // Check for DT8 color commands (0xE0-0xFC) - only if device type 8 is enabled
      else if (data_byte >= 0xE0 && data_byte <= 0xFC && ballastState.enabled_device_type == 8) {
        processDT8Command(data_byte, msg);
      }
      break;
  }
}

// Process DT8 Color Commands (IEC 62386-209)
void processDT8Command(uint8_t cmd, BallastMessage& msg) {
#ifdef DEBUG_SERIAL
  Serial.printf("[DT8] Processing color command: 0x%02X (DTR0=%d, DTR1=%d, DTR2=%d)\n", 
                cmd, ballastState.dtr0, ballastState.dtr1, ballastState.dtr2);
#endif

  switch (cmd) {
    case DALI_DT8_SET_TEMP_X_COORD:
      // Set temporary X coordinate (DTR0=LSB, DTR1=MSB)
      ballastState.temp_color_x = (ballastState.dtr1 << 8) | ballastState.dtr0;
      msg.command_type = "dt8_set_temp_x";
      msg.description = "DT8: Set temp X=" + String(ballastState.temp_color_x);
      addRecentMessage(msg);
      publishBallastCommand(msg);
#ifdef DEBUG_SERIAL
      Serial.printf("[DT8] Temp X coordinate: %d\n", ballastState.temp_color_x);
#endif
      break;

    case DALI_DT8_SET_TEMP_Y_COORD:
      // Set temporary Y coordinate (DTR0=LSB, DTR1=MSB)
      ballastState.temp_color_y = (ballastState.dtr1 << 8) | ballastState.dtr0;
      msg.command_type = "dt8_set_temp_y";
      msg.description = "DT8: Set temp Y=" + String(ballastState.temp_color_y);
      addRecentMessage(msg);
      publishBallastCommand(msg);
#ifdef DEBUG_SERIAL
      Serial.printf("[DT8] Temp Y coordinate: %d\n", ballastState.temp_color_y);
#endif
      break;

    case DALI_DT8_ACTIVATE:
      // Apply all temporary color values
      ballastState.color_x = ballastState.temp_color_x;
      ballastState.color_y = ballastState.temp_color_y;
      ballastState.color_temp_mirek = ballastState.temp_color_temp;
      ballastState.color_r = ballastState.temp_color_r;
      ballastState.color_g = ballastState.temp_color_g;
      ballastState.color_b = ballastState.temp_color_b;
      ballastState.color_w = ballastState.temp_color_w;
      ballastState.color_a = ballastState.temp_color_a;
      ballastState.color_f = ballastState.temp_color_f;
      msg.command_type = "dt8_activate";
      msg.description = "DT8: Activate colors (R=" + String(ballastState.color_r) + 
                        " G=" + String(ballastState.color_g) + 
                        " B=" + String(ballastState.color_b) + 
                        " W=" + String(ballastState.color_w) + ")";
      addRecentMessage(msg);
      publishBallastCommand(msg);
      publishBallastState();
#ifdef DEBUG_SERIAL
      Serial.printf("[DT8] ACTIVATE - RGB(%d,%d,%d) W=%d Tc=%d mirek\n", 
                    ballastState.color_r, ballastState.color_g, ballastState.color_b,
                    ballastState.color_w, ballastState.color_temp_mirek);
#endif
      break;

    case DALI_DT8_SET_TEMP_COLOUR_TEMP:
      // Set temporary color temperature (DTR0=LSB, DTR1=MSB in mirek)
      ballastState.temp_color_temp = (ballastState.dtr1 << 8) | ballastState.dtr0;
      {
        uint16_t kelvin = (ballastState.temp_color_temp > 0) ? (1000000 / ballastState.temp_color_temp) : 0;
        msg.command_type = "dt8_set_temp_colour_temp";
        msg.description = "DT8: Set temp color temp=" + String(ballastState.temp_color_temp) + 
                          " mirek (" + String(kelvin) + "K)";
      }
      addRecentMessage(msg);
      publishBallastCommand(msg);
#ifdef DEBUG_SERIAL
      Serial.printf("[DT8] Temp color temp: %d mirek\n", ballastState.temp_color_temp);
#endif
      break;

    case DALI_DT8_COLOUR_TEMP_STEP_COOLER:
      // Step color temperature cooler (lower mirek = higher Kelvin = cooler)
      if (ballastState.temp_color_temp > ballastState.color_temp_tc_coolest) {
        ballastState.temp_color_temp--;
      }
      msg.command_type = "dt8_colour_temp_step_cooler";
      msg.description = "DT8: Step cooler, temp=" + String(ballastState.temp_color_temp) + " mirek";
      addRecentMessage(msg);
      publishBallastCommand(msg);
      break;

    case DALI_DT8_COLOUR_TEMP_STEP_WARMER:
      // Step color temperature warmer (higher mirek = lower Kelvin = warmer)
      if (ballastState.temp_color_temp < ballastState.color_temp_tc_warmest) {
        ballastState.temp_color_temp++;
      }
      msg.command_type = "dt8_colour_temp_step_warmer";
      msg.description = "DT8: Step warmer, temp=" + String(ballastState.temp_color_temp) + " mirek";
      addRecentMessage(msg);
      publishBallastCommand(msg);
      break;

    case DALI_DT8_SET_TEMP_RGB:
      // Set temporary RGB (DTR0=R, DTR1=G, DTR2=B)
      ballastState.temp_color_r = ballastState.dtr0;
      ballastState.temp_color_g = ballastState.dtr1;
      ballastState.temp_color_b = ballastState.dtr2;
      ballastState.active_color_type = 3;  // RGBWAF mode
      msg.command_type = "dt8_set_temp_rgb";
      msg.description = "DT8: Set temp RGB(" + String(ballastState.temp_color_r) + "," + 
                        String(ballastState.temp_color_g) + "," + 
                        String(ballastState.temp_color_b) + ")";
      addRecentMessage(msg);
      publishBallastCommand(msg);
#ifdef DEBUG_SERIAL
      Serial.printf("[DT8] Temp RGB: R=%d G=%d B=%d\n", 
                    ballastState.temp_color_r, ballastState.temp_color_g, ballastState.temp_color_b);
#endif
      break;

    case DALI_DT8_SET_TEMP_WAF:
      // Set temporary WAF (DTR0=W, DTR1=A, DTR2=F)
      ballastState.temp_color_w = ballastState.dtr0;
      ballastState.temp_color_a = ballastState.dtr1;
      ballastState.temp_color_f = ballastState.dtr2;
      msg.command_type = "dt8_set_temp_waf";
      msg.description = "DT8: Set temp WAF(W=" + String(ballastState.temp_color_w) + ",A=" + 
                        String(ballastState.temp_color_a) + ",F=" + 
                        String(ballastState.temp_color_f) + ")";
      addRecentMessage(msg);
      publishBallastCommand(msg);
#ifdef DEBUG_SERIAL
      Serial.printf("[DT8] Temp WAF: W=%d A=%d F=%d\n", 
                    ballastState.temp_color_w, ballastState.temp_color_a, ballastState.temp_color_f);
#endif
      break;

    case DALI_DT8_SET_TEMP_RGBWAF_CTRL:
      // Set RGBWAF control flags (DTR0=control)
      ballastState.rgbwaf_control = ballastState.dtr0;
      msg.command_type = "dt8_set_temp_rgbwaf_ctrl";
      msg.description = "DT8: Set RGBWAF control=0x" + String(ballastState.rgbwaf_control, HEX);
      addRecentMessage(msg);
      publishBallastCommand(msg);
#ifdef DEBUG_SERIAL
      Serial.printf("[DT8] RGBWAF control: 0x%02X\n", ballastState.rgbwaf_control);
#endif
      break;

    case DALI_DT8_QUERY_GEAR_FEATURES:
      // Query gear features/status - respond with capabilities
      {
        uint8_t features = 0x00;  // No auto-calibration support
        sendBackwardFrame(features);
        msg.is_query_response = true;
        msg.command_type = "dt8_query_gear_features";
        msg.response_byte = features;
        msg.description = "DT8: Query gear features=0x" + String(features, HEX);
        addRecentMessage(msg);
        publishBallastCommand(msg);
        incrementTxCount();
      }
      break;

    case DALI_DT8_QUERY_COLOUR_STATUS:
      // Query colour status
      {
        uint8_t status = ballastState.color_status;
        // Set active color type bits
        if (ballastState.active_color_type == 0) status |= 0x10;      // xy active
        else if (ballastState.active_color_type == 1) status |= 0x20; // Tc active
        else if (ballastState.active_color_type == 2) status |= 0x40; // primaryN active
        else if (ballastState.active_color_type == 3) status |= 0x80; // RGBWAF active
        sendBackwardFrame(status);
        msg.is_query_response = true;
        msg.command_type = "dt8_query_colour_status";
        msg.response_byte = status;
        msg.description = "DT8: Query colour status=0x" + String(status, HEX);
        addRecentMessage(msg);
        publishBallastCommand(msg);
        incrementTxCount();
      }
      break;

    case DALI_DT8_QUERY_COLOUR_TYPE_FEATURES:
      // Query colour type features - what color modes are supported
      {
        // Bits: 0=xy, 1=Tc, 2-4=primaryN count, 5-7=RGBWAF channels
        // We support: xy, Tc, and RGBWAF (4 channels for RGBW)
        uint8_t features = DALI_DT8_FEATURE_XY_CAPABLE | DALI_DT8_FEATURE_TC_CAPABLE;
        features |= (4 << 5);  // 4 RGBWAF channels (R, G, B, W)
        sendBackwardFrame(features);
        msg.is_query_response = true;
        msg.command_type = "dt8_query_colour_type_features";
        msg.response_byte = features;
        msg.description = "DT8: Query colour type features=0x" + String(features, HEX);
        addRecentMessage(msg);
        publishBallastCommand(msg);
        incrementTxCount();
#ifdef DEBUG_SERIAL
        Serial.printf("[DT8] Colour type features: 0x%02X (xy=%d, Tc=%d, RGBWAF=%d ch)\n", 
                      features, (features & 0x01), (features & 0x02) >> 1, (features >> 5) & 0x07);
#endif
      }
      break;

    case DALI_DT8_QUERY_COLOUR_VALUE:
      // Query colour value - DTR0 selects which value to return
      {
        uint8_t response = 0;
        String value_name = "unknown";
        
        switch (ballastState.dtr0) {
          case DALI_DT8_QCV_X_COORD:
            response = ballastState.color_x >> 8;  // Return MSB
            value_name = "X coord MSB";
            break;
          case DALI_DT8_QCV_Y_COORD:
            response = ballastState.color_y >> 8;  // Return MSB
            value_name = "Y coord MSB";
            break;
          case DALI_DT8_QCV_COLOUR_TEMP:
            response = ballastState.color_temp_mirek >> 8;  // Return MSB
            value_name = "Colour temp MSB";
            break;
          case DALI_DT8_QCV_RED_DIM_LEVEL:
            response = ballastState.color_r;
            value_name = "Red";
            break;
          case DALI_DT8_QCV_GREEN_DIM_LEVEL:
            response = ballastState.color_g;
            value_name = "Green";
            break;
          case DALI_DT8_QCV_BLUE_DIM_LEVEL:
            response = ballastState.color_b;
            value_name = "Blue";
            break;
          case DALI_DT8_QCV_WHITE_DIM_LEVEL:
            response = ballastState.color_w;
            value_name = "White";
            break;
          case DALI_DT8_QCV_AMBER_DIM_LEVEL:
            response = ballastState.color_a;
            value_name = "Amber";
            break;
          case DALI_DT8_QCV_FREECOLOUR_DIM_LEVEL:
            response = ballastState.color_f;
            value_name = "Freecolour";
            break;
          case DALI_DT8_QCV_RGBWAF_CONTROL:
            response = ballastState.rgbwaf_control;
            value_name = "RGBWAF control";
            break;
          case DALI_DT8_QCV_TC_COOLEST:
            response = ballastState.color_temp_tc_coolest >> 8;
            value_name = "Tc coolest MSB";
            break;
          case DALI_DT8_QCV_TC_WARMEST:
            response = ballastState.color_temp_tc_warmest >> 8;
            value_name = "Tc warmest MSB";
            break;
          case DALI_DT8_QCV_TEMP_RED:
            response = ballastState.temp_color_r;
            value_name = "Temp Red";
            break;
          case DALI_DT8_QCV_TEMP_GREEN:
            response = ballastState.temp_color_g;
            value_name = "Temp Green";
            break;
          case DALI_DT8_QCV_TEMP_BLUE:
            response = ballastState.temp_color_b;
            value_name = "Temp Blue";
            break;
          case DALI_DT8_QCV_TEMP_WHITE:
            response = ballastState.temp_color_w;
            value_name = "Temp White";
            break;
          case DALI_DT8_QCV_TEMP_COLOUR_TYPE:
            response = ballastState.active_color_type;
            value_name = "Colour type";
            break;
          default:
            response = 0xFF;  // MASK for unknown
            value_name = "Unknown(" + String(ballastState.dtr0) + ")";
            break;
        }
        
        sendBackwardFrame(response);
        msg.is_query_response = true;
        msg.command_type = "dt8_query_colour_value";
        msg.response_byte = response;
        msg.description = "DT8: Query " + value_name + "=" + String(response);
        addRecentMessage(msg);
        publishBallastCommand(msg);
        incrementTxCount();
#ifdef DEBUG_SERIAL
        Serial.printf("[DT8] Query colour value (DTR0=%d): %s = %d\n", 
                      ballastState.dtr0, value_name.c_str(), response);
#endif
      }
      break;

    case DALI_DT8_QUERY_RGBWAF_CONTROL:
      // Query RGBWAF control
      sendBackwardFrame(ballastState.rgbwaf_control);
      msg.is_query_response = true;
      msg.command_type = "dt8_query_rgbwaf_control";
      msg.response_byte = ballastState.rgbwaf_control;
      msg.description = "DT8: Query RGBWAF control=0x" + String(ballastState.rgbwaf_control, HEX);
      addRecentMessage(msg);
      publishBallastCommand(msg);
      incrementTxCount();
      break;

    default:
#ifdef DEBUG_SERIAL
      Serial.printf("[DT8] Unknown command: 0x%02X\n", cmd);
#endif
      msg.command_type = "dt8_unknown";
      msg.description = "DT8: Unknown cmd 0x" + String(cmd, HEX);
      addRecentMessage(msg);
      publishBallastCommand(msg);
      break;
  }
}

void setLevel(uint8_t level) {
  if (level > 254) level = 254;

  // Apply min/max limits
  if (level > 0 && level < ballastState.min_level) {
    level = ballastState.min_level;
  }
  if (level > ballastState.max_level) {
    level = ballastState.max_level;
  }

  ballastState.target_level = level;

  // Instant change if fade time is 0
  if (ballastState.fade_time == 0) {
    ballastState.actual_level = level;
    ballastState.lamp_arc_power_on = (level > 0);
    ballastState.fade_running = false;
    updateLED();
  } else {
    // Start fade
    ballastState.fade_running = true;
    ballastState.last_command_time = millis();
  }

#ifdef DEBUG_SERIAL
  Serial.printf("[Ballast] Set level to %d (%s)\n", level, ballastState.fade_running ? "fading" : "instant");
#endif
}

void updateFade() {
  if (!ballastState.fade_running) return;

  unsigned long elapsed = millis() - ballastState.last_command_time;
  uint32_t fade_duration = fade_times[ballastState.fade_time];

  if (elapsed >= fade_duration) {
    // Fade complete
    ballastState.actual_level = ballastState.target_level;
    ballastState.lamp_arc_power_on = (ballastState.actual_level > 0);
    ballastState.fade_running = false;

    updateLED();
    publishBallastState();

#ifdef DEBUG_SERIAL
    Serial.printf("[Ballast] Fade complete - Level: %d\n", ballastState.actual_level);
#endif
  } else {
    // Calculate intermediate level
    float progress = (float)elapsed / (float)fade_duration;
    int16_t start_level = ballastState.actual_level;
    int16_t target_level = ballastState.target_level;
    int16_t diff = target_level - start_level;

    ballastState.actual_level = start_level + (diff * progress);
    ballastState.lamp_arc_power_on = (ballastState.actual_level > 0);
    updateLED();
  }
}

void updateLED() {
  // Map DALI level (0-254) to brightness (0-255)
  uint8_t brightness = map(ballastState.actual_level, 0, 254, 0, 255);

  uint8_t r, g, b;

  switch (ballastState.device_type) {
    case DT0_NORMAL:
    case DT6_LED:
      // Normal/LED mode: White light with brightness control
      r = g = b = brightness;
      break;

    case DT8_COLOUR:
      // DT8 Colour mode - behavior depends on active_color_type
      switch (ballastState.active_color_type) {
        case DT8_MODE_TC:
          // Color Temperature mode: Convert mirek to RGB
          {
            uint16_t kelvin = (ballastState.color_temp_mirek > 0) ? (1000000 / ballastState.color_temp_mirek) : 4000;

            // Simplified color temperature to RGB conversion
            if (kelvin <= 3000) {
              // Warm white (orange-ish)
              r = 255;
              g = map(kelvin, 2700, 3000, 180, 220);
              b = map(kelvin, 2700, 3000, 100, 150);
            } else if (kelvin <= 4500) {
              // Neutral white
              r = map(kelvin, 3000, 4500, 255, 245);
              g = map(kelvin, 3000, 4500, 220, 240);
              b = map(kelvin, 3000, 4500, 150, 220);
            } else {
              // Cool white (blue-ish)
              r = map(kelvin, 4500, 6500, 245, 220);
              g = map(kelvin, 4500, 6500, 240, 235);
              b = 255;
            }

            // Scale by brightness
            r = (r * brightness) / 255;
            g = (g * brightness) / 255;
            b = (b * brightness) / 255;
          }
          break;

        case DT8_MODE_RGBWAF:
        default:
          // RGBWAF mode: Use RGB values scaled by brightness
          // If W channel is set, blend it in (WS2812 doesn't have separate W)
          if (ballastState.color_w > 0) {
            uint8_t w = (ballastState.color_w * brightness) / 255;
            r = min(255, (ballastState.color_r * brightness) / 255 + w);
            g = min(255, (ballastState.color_g * brightness) / 255 + w);
            b = min(255, (ballastState.color_b * brightness) / 255 + w);
          } else {
            r = (ballastState.color_r * brightness) / 255;
            g = (ballastState.color_g * brightness) / 255;
            b = (ballastState.color_b * brightness) / 255;
          }
          break;
      }
      break;

    default:
      // Fallback: White
      r = g = b = brightness;
      break;
  }

  // Apply color order mapping
  // 0=RGB, 1=GRB, 2=BGR, 3=RBG, 4=GBR, 5=BRG
  uint8_t c1, c2, c3;
  switch (LED_COLOR_ORDER) {
    case 0: c1 = r; c2 = g; c3 = b; break; // RGB
    case 1: c1 = g; c2 = r; c3 = b; break; // GRB
    case 2: c1 = b; c2 = g; c3 = r; break; // BGR
    case 3: c1 = r; c2 = b; c3 = g; break; // RBG
    case 4: c1 = g; c2 = b; c3 = r; break; // GBR
    case 5: c1 = b; c2 = r; c3 = g; break; // BRG
    default: c1 = r; c2 = g; c3 = b; break; // Default to RGB
  }

  neopixelWrite(LED_PIN, c1, c2, c3);
}

void sendBackwardFrame(uint8_t data) {
  // DALI requires backward frame within 7-22ms of forward frame
  // Send immediately, don't queue
  updateBusActivity();
  
  uint8_t response[1] = {data};
  int result = dali.tx_wait(response, 8, QUERY_RESPONSE_TIMEOUT_MS);
  
  if (result >= 0) {
    incrementTxCount();
#ifdef DEBUG_SERIAL
    Serial.printf("[Ballast] Sent response: 0x%02X\n", data);
#endif
  } else {
    incrementErrorCount();
#ifdef DEBUG_SERIAL
    Serial.printf("[Ballast] Response transmission failed: 0x%02X\n", data);
#endif
  }
}

uint8_t getStatusByte() {
  uint8_t status = 0x00;

  if (ballastState.ballast_failure) status |= 0x01;
  if (ballastState.lamp_failure) status |= 0x02;
  if (ballastState.lamp_arc_power_on) status |= 0x04;
  if (ballastState.limit_error) status |= 0x08;
  if (ballastState.fade_running) status |= 0x10;
  if (ballastState.reset_state) status |= 0x20;

  return status;
}

void addRecentMessage(const BallastMessage& msg) {
  recentMessages[recentMessagesIndex] = msg;
  recentMessagesIndex = (recentMessagesIndex + 1) % RECENT_MESSAGES_SIZE;
}

void recallScene(uint8_t scene) {
  if (scene > 15) return;
  if (ballastState.scene_levels[scene] != 0xFF) {
    setLevel(ballastState.scene_levels[scene]);
  }
}

void storeScene(uint8_t scene, uint8_t level) {
  if (scene > 15) return;
  ballastState.scene_levels[scene] = level;
  // Note: Not saving to flash - save manually via web UI
}

uint8_t queryScene(uint8_t scene) {
  if (scene > 15) return 0xFF;
  return ballastState.scene_levels[scene];
}
