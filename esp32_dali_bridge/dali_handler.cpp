#include "dali_handler.h"
#include "config.h"
#include "diagnostics.h"
#include "mqtt_handler.h"
#include "esp_task_wdt.h"

Dali dali;
DaliMessage recentMessages[RECENT_MESSAGES_SIZE];
uint8_t recentMessagesIndex = 0;
DaliCommand commandQueue[COMMAND_QUEUE_SIZE];
uint8_t queueHead = 0;
uint8_t queueTail = 0;
unsigned long lastDaliCommandTime = 0;
unsigned long lastBusActivityTime = 0;
bool busIsIdle = true;
CommissioningProgress commissioningProgress;
PassiveDevice passiveDevices[DALI_MAX_ADDRESSES];
uint8_t lastQueriedAddress = 255;  // Track last address that was queried (for passive discovery)

hw_timer_t *timer = NULL;

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

void daliInit() {
#ifdef DEBUG_SERIAL
  Serial.println("Initializing DALI...");
#endif

  pinMode(DALI_RX_PIN, INPUT);
  pinMode(DALI_TX_PIN, OUTPUT);

  timer = timerBegin(DALI_TIMER_FREQ);
  timerAttachInterrupt(timer, &onTimer);
  timerAlarm(timer, 1000, true, 0);

  dali.begin(bus_is_high, bus_set_high, bus_set_low);
  
  clearPassiveDevices();

#ifdef DEBUG_SERIAL
  Serial.println("DALI initialized");
#endif
}

// Passive device discovery functions
void clearPassiveDevices() {
  memset(passiveDevices, 0, sizeof(passiveDevices));
  for (int i = 0; i < DALI_MAX_ADDRESSES; i++) {
    passiveDevices[i].last_level = 255;    // Unknown
    passiveDevices[i].device_type = 255;   // Unknown
  }
}

uint8_t getPassiveDeviceCount() {
  uint8_t count = 0;
  for (int i = 0; i < DALI_MAX_ADDRESSES; i++) {
    if (passiveDevices[i].last_seen > 0) count++;
  }
  return count;
}

void updatePassiveDevice(uint8_t address, const DaliMessage& msg) {
  if (address >= DALI_MAX_ADDRESSES) return;
  
  passiveDevices[address].last_seen = millis();
  
  // Extract level from DAPC commands
  if (msg.parsed.type == "DAPC" || msg.parsed.type == "dapc") {
    passiveDevices[address].last_level = msg.parsed.level;
  }
}

// Called when we see a query command to an address - remember it for response matching
void trackQueryAddress(uint8_t address) {
  if (address < DALI_MAX_ADDRESSES) {
    lastQueriedAddress = address;
  }
}

// Called when we see a backward frame (response) - mark the last queried address as having a device
void handleResponse(const DaliMessage& msg) {
  if (lastQueriedAddress < DALI_MAX_ADDRESSES) {
    passiveDevices[lastQueriedAddress].last_seen = millis();
    passiveDevices[lastQueriedAddress].last_level = msg.parsed.level;
    passiveDevices[lastQueriedAddress].flags |= 0x01;  // Bit 0 = responded to query
    lastQueriedAddress = 255;  // Reset
  }
}

String getAddressPrefix(String address_type, uint8_t address) {
  if (address_type == "broadcast") {
    return "Broadcast";
  } else if (address_type == "group") {
    return "Group " + String(address);
  } else {
    return "Device " + String(address);
  }
}

DaliMessage parseDaliMessage(uint8_t* bytes, uint8_t length, bool is_tx) {
  DaliMessage msg;
  msg.timestamp = millis();
  msg.is_tx = is_tx;
  msg.source = "bus";
  msg.length = length;
  memcpy(msg.raw_bytes, bytes, min(length, (uint8_t)4));

  msg.parsed.type = "unknown";
  msg.parsed.address = 0;
  msg.parsed.address_type = "unknown";
  msg.parsed.level = 0;
  msg.parsed.level_percent = 0;

  if (length == 1) {
    // Backward frame (8-bit response from ballast)
    msg.parsed.type = "response";
    msg.parsed.address_type = "response";
    msg.parsed.level = bytes[0];
    msg.description = "Response: 0x" + String(bytes[0], HEX) + " (" + String(bytes[0]) + ")";
    return msg;
  }

  if (length == 3) {
    // 24-bit frame - DALI-2 device command (IEC 62386-103)
    // Format: [address byte] [instance byte] [opcode]
    uint8_t addr_byte = bytes[0];
    uint8_t inst_byte = bytes[1];
    uint8_t opcode = bytes[2];
    
    msg.parsed.type = "device_command";
    msg.length = 3;
    
    // Decode address byte
    if (addr_byte == 0xFF) {
      msg.parsed.address_type = "broadcast_device";
      msg.parsed.address = 0xFF;
    } else if (addr_byte == 0xFD) {
      msg.parsed.address_type = "broadcast_unaddressed";
      msg.parsed.address = 0xFD;
    } else if ((addr_byte & 0x01) == 0x01) {
      // Short address: 0AAAAAA1
      msg.parsed.address_type = "device_short";
      msg.parsed.address = (addr_byte >> 1) & 0x3F;
    } else if ((addr_byte & 0x81) == 0x80) {
      // Group address: 100GGGG0
      msg.parsed.address_type = "device_group";
      msg.parsed.address = (addr_byte >> 1) & 0x0F;
    } else {
      msg.parsed.address_type = "unknown";
      msg.parsed.address = addr_byte;
    }
    
    // Decode instance byte (0xFE = device command, not instance-specific)
    String inst_str;
    if (inst_byte == 0xFE) {
      inst_str = "device";
    } else if (inst_byte == 0xFF) {
      inst_str = "all_instances";
    } else if ((inst_byte & 0xE0) == 0x00) {
      inst_str = "instance_" + String(inst_byte & 0x1F);
    } else if ((inst_byte & 0xE0) == 0x80) {
      inst_str = "instance_group_" + String(inst_byte & 0x1F);
    } else if ((inst_byte & 0xE0) == 0x60) {
      inst_str = "instance_type_" + String(inst_byte & 0x1F);
    } else {
      inst_str = "inst_0x" + String(inst_byte, HEX);
    }
    
    // Decode opcode (DALI-2 device commands from IEC 62386-103)
    String opcode_str;
    switch (opcode) {
      case 0x00: opcode_str = "IdentifyDevice"; break;
      case 0x01: opcode_str = "ResetPowerCycleSeen"; break;
      case 0x10: opcode_str = "Reset"; break;
      case 0x11: opcode_str = "ResetMemoryBank"; break;
      case 0x14: opcode_str = "SetShortAddress"; break;
      case 0x15: opcode_str = "EnableWriteMemory"; break;
      case 0x16: opcode_str = "EnableApplicationController"; break;
      case 0x17: opcode_str = "DisableApplicationController"; break;
      case 0x18: opcode_str = "SetOperatingMode"; break;
      case 0x19: opcode_str = "AddToDeviceGroups0-15"; break;
      case 0x1A: opcode_str = "AddToDeviceGroups16-31"; break;
      case 0x1B: opcode_str = "RemoveFromDeviceGroups0-15"; break;
      case 0x1C: opcode_str = "RemoveFromDeviceGroups16-31"; break;
      case 0x1D: opcode_str = "StartQuiescentMode"; break;
      case 0x1E: opcode_str = "StopQuiescentMode"; break;
      case 0x1F: opcode_str = "EnablePowerCycleNotification"; break;
      case 0x20: opcode_str = "DisablePowerCycleNotification"; break;
      case 0x21: opcode_str = "SavePersistentVariables"; break;
      case 0x30: opcode_str = "QueryDeviceStatus"; break;
      case 0x31: opcode_str = "QueryApplicationControllerError"; break;
      case 0x32: opcode_str = "QueryInputDeviceError"; break;
      case 0x33: opcode_str = "QueryMissingShortAddress"; break;
      case 0x34: opcode_str = "QueryVersionNumber"; break;
      case 0x35: opcode_str = "QueryNumberOfInstances"; break;
      case 0x36: opcode_str = "QueryContentDTR0"; break;
      case 0x37: opcode_str = "QueryContentDTR1"; break;
      case 0x38: opcode_str = "QueryContentDTR2"; break;
      case 0x39: opcode_str = "QueryRandomAddressH"; break;
      case 0x3A: opcode_str = "QueryRandomAddressM"; break;
      case 0x3B: opcode_str = "QueryRandomAddressL"; break;
      case 0x3C: opcode_str = "ReadMemoryLocation"; break;
      default: opcode_str = "Opcode_0x" + String(opcode, HEX); break;
    }
    
    msg.description = "DALI-2 " + opcode_str + " [" + msg.parsed.address_type + " " + inst_str + "]";
    return msg;
  }

  if (length != 2) {
    msg.description = "Invalid frame (" + String(length) + " bytes) - Expected 1, 2, or 3-byte frame";
    return msg;
  }

  if (length == 2) {
    uint8_t addr_byte = bytes[0];
    uint8_t data_byte = bytes[1];

    if (!(addr_byte & 0x01)) {
      msg.parsed.type = "direct_arc_power";
      msg.parsed.address = (addr_byte >> 1) & 0x3F;

      if (msg.parsed.address == 0x3F && addr_byte == 0xFE) {
        msg.parsed.address_type = "broadcast";
        msg.parsed.address = 0xFF;
      } else if (addr_byte >= DALI_GROUP_ADDR_START && addr_byte <= DALI_GROUP_ADDR_END) {
        msg.parsed.address_type = "group";
        msg.parsed.address = (addr_byte >> 1) & 0x0F;
      } else {
        msg.parsed.address_type = "short";
      }

      msg.parsed.level = data_byte;
      if (data_byte <= 254) {
        msg.parsed.level_percent = (data_byte / 254.0) * 100.0;
      }

      if (msg.parsed.address_type == "broadcast") {
        msg.description = "Broadcast: Set all to " + String(data_byte) + " (" + String(msg.parsed.level_percent, 1) + "%)";
      } else if (msg.parsed.address_type == "group") {
        msg.description = "Group " + String(msg.parsed.address) + ": Set to " + String(data_byte) + " (" + String(msg.parsed.level_percent, 1) + "%)";
      } else {
        msg.description = "Device " + String(msg.parsed.address) + ": Set to " + String(data_byte) + " (" + String(msg.parsed.level_percent, 1) + "%)";
      }
    } else {
      msg.parsed.type = "command";
      msg.parsed.address = (addr_byte >> 1) & 0x3F;
      
      // Check for broadcast command (0xFF)
      if (addr_byte == 0xFF) {
        msg.parsed.address_type = "broadcast";
        msg.parsed.address = 0xFF;
      } else if (addr_byte >= DALI_GROUP_ADDR_START && addr_byte <= DALI_GROUP_ADDR_END) {
        msg.parsed.address_type = "group";
        msg.parsed.address = (addr_byte >> 1) & 0x0F;
      } else {
        msg.parsed.address_type = "short";
      }

      // Special commands with special address bytes (broadcast commands)
      if (addr_byte == 0xA3) {
        msg.parsed.type = "set_dtr0";
        msg.parsed.address_type = "broadcast";
        msg.parsed.address = 0xFF;
        msg.parsed.level = data_byte;
        msg.description = "Broadcast: Set DTR0 = " + String(data_byte);
      } else if (addr_byte == 0xC1) {
        msg.parsed.type = "enable_device_type";
        msg.parsed.address_type = "broadcast";
        msg.parsed.address = 0xFF;
        msg.parsed.level = data_byte;
        String dt_name;
        switch (data_byte) {
          case 0: dt_name = "Normal (DT0)"; break;
          case 6: dt_name = "LED (DT6)"; break;
          case 8: dt_name = "Colour (DT8)"; break;
          default: dt_name = "DT" + String(data_byte); break;
        }
        msg.description = "Broadcast: Enable Device Type " + dt_name;
      } else if (addr_byte == 0xC3) {
        msg.parsed.type = "set_dtr1";
        msg.parsed.address_type = "broadcast";
        msg.parsed.address = 0xFF;
        msg.parsed.level = data_byte;
        msg.description = "Broadcast: Set DTR1 = " + String(data_byte);
      } else if (addr_byte == 0xC5) {
        msg.parsed.type = "set_dtr2";
        msg.parsed.address_type = "broadcast";
        msg.parsed.address = 0xFF;
        msg.parsed.level = data_byte;
        msg.description = "Broadcast: Set DTR2 = " + String(data_byte);
      } else {

      String addrPrefix = getAddressPrefix(msg.parsed.address_type, msg.parsed.address);

      switch (data_byte) {
        case DALI_OFF:
          msg.parsed.type = "off";
          msg.description = addrPrefix + ": Off";
          break;
        case DALI_UP:
          msg.parsed.type = "up";
          msg.description = addrPrefix + ": Fade up";
          break;
        case DALI_DOWN:
          msg.parsed.type = "down";
          msg.description = addrPrefix + ": Fade down";
          break;
        case DALI_STEP_UP:
          msg.parsed.type = "step_up";
          msg.description = addrPrefix + ": Step up";
          break;
        case DALI_STEP_DOWN:
          msg.parsed.type = "step_down";
          msg.description = addrPrefix + ": Step down";
          break;
        case DALI_RECALL_MAX_LEVEL:
          msg.parsed.type = "recall_max";
          msg.description = addrPrefix + ": Recall max level";
          break;
        case DALI_RECALL_MIN_LEVEL:
          msg.parsed.type = "recall_min";
          msg.description = addrPrefix + ": Recall min level";
          break;
        case 0x0B: // DALI_CONTINUOUS_UP
          msg.parsed.type = "continuous_up";
          msg.description = addrPrefix + ": Continuous up (fade to max)";
          break;
        case 0x0C: // DALI_CONTINUOUS_DOWN
          msg.parsed.type = "continuous_down";
          msg.description = addrPrefix + ": Continuous down (fade to min)";
          break;
        case DALI_RESET:
          msg.parsed.type = "reset";
          msg.description = addrPrefix + ": Reset";
          break;
        case DALI_STORE_DTR_AS_MAX_LEVEL:
          msg.parsed.type = "store_dtr_as_max_level";
          msg.description = addrPrefix + ": Store DTR as max level";
          break;
        case DALI_STORE_DTR_AS_MIN_LEVEL:
          msg.parsed.type = "store_dtr_as_min_level";
          msg.description = addrPrefix + ": Store DTR as min level";
          break;
        case DALI_STORE_DTR_AS_SYSTEM_FAILURE_LEVEL:
          msg.parsed.type = "store_dtr_as_system_failure_level";
          msg.description = addrPrefix + ": Store DTR as system failure level";
          break;
        case DALI_STORE_DTR_AS_POWER_ON_LEVEL:
          msg.parsed.type = "store_dtr_as_power_on_level";
          msg.description = addrPrefix + ": Store DTR as power-on level";
          break;
        case DALI_STORE_DTR_AS_FADE_TIME:
          msg.parsed.type = "store_dtr_as_fade_time";
          msg.description = addrPrefix + ": Store DTR as fade time";
          break;
        case DALI_STORE_DTR_AS_FADE_RATE:
          msg.parsed.type = "store_dtr_as_fade_rate";
          msg.description = addrPrefix + ": Store DTR as fade rate";
          break;
        case DALI_QUERY_STATUS:
          msg.parsed.type = "query_status";
          msg.description = addrPrefix + ": Query status";
          break;
        case DALI_QUERY_CONTROL_GEAR:
          msg.parsed.type = "query_control_gear";
          msg.description = addrPrefix + ": Query control gear present";
          break;
        case DALI_QUERY_LAMP_FAILURE:
          msg.parsed.type = "query_lamp_failure";
          msg.description = addrPrefix + ": Query lamp failure";
          break;
        case DALI_QUERY_LAMP_POWER_ON:
          msg.parsed.type = "query_lamp_power_on";
          msg.description = addrPrefix + ": Query lamp power on";
          break;
        case DALI_QUERY_LIMIT_ERROR:
          msg.parsed.type = "query_limit_error";
          msg.description = addrPrefix + ": Query limit error";
          break;
        case DALI_QUERY_RESET_STATE:
          msg.parsed.type = "query_reset_state";
          msg.description = addrPrefix + ": Query reset state";
          break;
        case DALI_QUERY_MISSING_SHORT_ADDRESS:
          msg.parsed.type = "query_missing_short_address";
          msg.description = addrPrefix + ": Query missing short address";
          break;
        case DALI_QUERY_DEVICE_TYPE:
          msg.parsed.type = "query_device_type";
          msg.description = addrPrefix + ": Query device type";
          break;
        case DALI_QUERY_ACTUAL_LEVEL:
          msg.parsed.type = "query_actual_level";
          msg.description = addrPrefix + ": Query actual level";
          break;
        case DALI_QUERY_MAX_LEVEL:
          msg.parsed.type = "query_max_level";
          msg.description = addrPrefix + ": Query max level";
          break;
        case DALI_QUERY_MIN_LEVEL:
          msg.parsed.type = "query_min_level";
          msg.description = addrPrefix + ": Query min level";
          break;
        case DALI_QUERY_POWER_ON_LEVEL:
          msg.parsed.type = "query_power_on_level";
          msg.description = addrPrefix + ": Query power on level";
          break;
        case DALI_QUERY_FADE_TIME_RATE:
          msg.parsed.type = "query_fade_time_rate";
          msg.description = addrPrefix + ": Query fade time/rate";
          break;

        // Commissioning commands
        case DALI_INITIALISE:
          msg.parsed.type = "initialise";
          msg.description = "Commissioning: INITIALISE (0x" + String(data_byte, HEX) + ")";
          break;
        case DALI_RANDOMISE:
          msg.parsed.type = "randomise";
          msg.description = "Commissioning: RANDOMISE";
          break;
        case DALI_COMPARE:
          msg.parsed.type = "compare";
          msg.description = "Commissioning: COMPARE";
          break;
        case DALI_WITHDRAW:
          msg.parsed.type = "withdraw";
          msg.description = "Commissioning: WITHDRAW";
          break;
        case DALI_SEARCHADDRH:
          msg.parsed.type = "searchaddrh";
          msg.description = "Commissioning: SEARCHADDRH (0x" + String(data_byte, HEX) + ")";
          break;
        case DALI_SEARCHADDRM:
          msg.parsed.type = "searchaddrm";
          msg.description = "Commissioning: SEARCHADDRM (0x" + String(data_byte, HEX) + ")";
          break;
        case DALI_SEARCHADDRL:
          msg.parsed.type = "searchaddrl";
          msg.description = "Commissioning: SEARCHADDRL (0x" + String(data_byte, HEX) + ")";
          break;
        case DALI_PROGRAM_SHORT_ADDRESS:
          msg.parsed.type = "program_short_address";
          msg.description = "Commissioning: PROGRAM_SHORT_ADDRESS (" + String((data_byte >> 1) & 0x3F) + ")";
          break;
        case DALI_VERIFY_SHORT_ADDRESS:
          msg.parsed.type = "verify_short_address";
          msg.description = "Commissioning: VERIFY_SHORT_ADDRESS (" + String((data_byte >> 1) & 0x3F) + ")";
          break;
        case DALI_QUERY_SHORT_ADDRESS:
          msg.parsed.type = "query_short_address";
          msg.description = "Commissioning: QUERY_SHORT_ADDRESS";
          break;

        default:
          if (data_byte >= DALI_GO_TO_SCENE && data_byte < DALI_GO_TO_SCENE + 16) {
            msg.parsed.type = "go_to_scene";
            uint8_t scene = data_byte - DALI_GO_TO_SCENE;
            msg.description = addrPrefix + ": Go to scene " + String(scene);
          } else if (data_byte >= DALI_QUERY_SCENE_LEVEL && data_byte < DALI_QUERY_SCENE_LEVEL + 16) {
            msg.parsed.type = "query_scene_level";
            uint8_t scene = data_byte - DALI_QUERY_SCENE_LEVEL;
            msg.description = addrPrefix + ": Query scene " + String(scene) + " level";
          } else if (data_byte >= 0xE0 && data_byte <= 0xFC) {
            // DT8 Color Commands (IEC 62386-209) - only valid after ENABLE_DEVICE_TYPE(8)
            msg.parsed.type = "dt8_command";
            String dt8_cmd;
            switch (data_byte) {
              case 0xE0: dt8_cmd = "SetTemporaryXCoordinate"; break;
              case 0xE1: dt8_cmd = "SetTemporaryYCoordinate"; break;
              case 0xE2: dt8_cmd = "Activate"; break;
              case 0xE3: dt8_cmd = "XCoordinateStepUp"; break;
              case 0xE4: dt8_cmd = "XCoordinateStepDown"; break;
              case 0xE5: dt8_cmd = "YCoordinateStepUp"; break;
              case 0xE6: dt8_cmd = "YCoordinateStepDown"; break;
              case 0xE7: dt8_cmd = "SetTemporaryColourTemperature"; break;
              case 0xE8: dt8_cmd = "ColourTemperatureStepCooler"; break;
              case 0xE9: dt8_cmd = "ColourTemperatureStepWarmer"; break;
              case 0xEA: dt8_cmd = "SetTemporaryPrimaryNDimLevel"; break;
              case 0xEB: dt8_cmd = "SetTemporaryRGBDimLevel"; break;
              case 0xEC: dt8_cmd = "SetTemporaryWAFDimLevel"; break;
              case 0xED: dt8_cmd = "SetTemporaryRGBWAFControl"; break;
              case 0xEE: dt8_cmd = "CopyReportToTemporary"; break;
              case 0xF0: dt8_cmd = "StoreTYPrimaryN"; break;
              case 0xF1: dt8_cmd = "StoreXYCoordinatePrimaryN"; break;
              case 0xF2: dt8_cmd = "StoreColourTemperatureTcLimit"; break;
              case 0xF3: dt8_cmd = "StoreGearFeaturesStatus"; break;
              case 0xF5: dt8_cmd = "AssignColourToLinkedChannel"; break;
              case 0xF6: dt8_cmd = "StartAutoCalibration"; break;
              case 0xF7: dt8_cmd = "QueryGearFeaturesStatus"; break;
              case 0xF8: dt8_cmd = "QueryColourStatus"; break;
              case 0xF9: dt8_cmd = "QueryColourTypeFeatures"; break;
              case 0xFA: dt8_cmd = "QueryColourValue"; break;
              case 0xFB: dt8_cmd = "QueryRGBWAFControl"; break;
              case 0xFC: dt8_cmd = "QueryAssignedColour"; break;
              default: dt8_cmd = "DT8_0x" + String(data_byte, HEX); break;
            }
            msg.description = addrPrefix + ": DT8 " + dt8_cmd;
          } else {
            msg.description = addrPrefix + ": Command 0x" + String(data_byte, HEX);
          }
          break;
      }
      } // Close else block for DTR0 check
    }
  } else if (length == 1) {
    msg.parsed.type = "response";
    msg.description = "Response: 0x" + String(bytes[0], HEX) + " (" + String(bytes[0]) + ")";
  }

  return msg;
}

bool validateDaliCommand(const DaliCommand& cmd) {
  if (cmd.command_type == "set_brightness") {
    if (cmd.address > 63 && cmd.address != 0xFF) return false;
    if (cmd.level > 254) return false;
    return true;
  } else if (cmd.command_type == "off" || cmd.command_type == "max" ||
             cmd.command_type == "up" || cmd.command_type == "down" ||
             cmd.command_type == "step_up" || cmd.command_type == "step_down" ||
             cmd.command_type == "recall_max" || cmd.command_type == "recall_min" ||
             cmd.command_type == "reset") {
    if (cmd.address > 63 && cmd.address != 0xFF) return false;
    return true;
  } else if (cmd.command_type == "go_to_scene") {
    if (cmd.address > 63 && cmd.address != 0xFF) return false;
    if (cmd.scene > 15) return false;
    return true;
  } else if (cmd.command_type == "query_status" ||
             cmd.command_type == "query_lamp_failure" ||
             cmd.command_type == "query_lamp_power_on" ||
             cmd.command_type == "query_actual_level" ||
             cmd.command_type == "query_max_level" ||
             cmd.command_type == "query_min_level" ||
             cmd.command_type == "query_device_type" ||
             cmd.command_type == "query_scene_level") {
    if (cmd.address > 63) return false;
    if (cmd.command_type == "query_scene_level" && cmd.scene > 15) return false;
    return true;
  } else if (cmd.command_type == "raw") {
    return true;
  }

  return false;
}

bool enqueueDaliCommand(const DaliCommand& cmd) {
  uint8_t nextTail = (queueTail + 1) % COMMAND_QUEUE_SIZE;
  if (nextTail == queueHead) {
#ifdef DEBUG_SERIAL
    Serial.println("Command queue full!");
#endif
    incrementErrorCount();
    return false;
  }

  commandQueue[queueTail] = cmd;
  queueTail = nextTail;
  
#ifdef DEBUG_SERIAL
  Serial.printf("[Queue] Enqueued %s cmd to addr %d (priority=%d, queue size=%d)\n",
                cmd.command_type.c_str(), cmd.address, cmd.priority,
                (queueTail >= queueHead) ? (queueTail - queueHead) : (COMMAND_QUEUE_SIZE - queueHead + queueTail));
#endif
  
  return true;
}

void monitorDaliBus() {
  uint8_t rx_data[4];  // Buffer for up to 32 bits (4 bytes)
  uint8_t result = dali.rx(rx_data);

  if (result > 2) {
    uint8_t num_bytes = (result + 7) / 8;
    
    updateBusActivity();

#ifdef DEBUG_SERIAL
    Serial.printf("[DALI] Bus activity detected: %d bits, bytes: 0x%02X", result, rx_data[0]);
    if (num_bytes > 1) Serial.printf(" 0x%02X", rx_data[1]);
    if (num_bytes > 2) Serial.printf(" 0x%02X", rx_data[2]);
    Serial.println();
#endif

    incrementRxCount();
    DaliMessage msg = parseDaliMessage(rx_data, num_bytes, false);
    msg.source = "bus";
    
    // Capture raw bit samples from DALI library for debugging
    // rxdata contains 8 samples per byte (8x oversampling), MSB is oldest
    // We'll convert the decoded bytes to a bit string for easier analysis
    String raw_bits = "";
    for (int i = 0; i < num_bytes; i++) {
      for (int b = 7; b >= 0; b--) {
        raw_bits += ((rx_data[i] >> b) & 1) ? "1" : "0";
      }
      if (i < num_bytes - 1) raw_bits += " ";
    }
    msg.raw_bits = raw_bits;

    recentMessages[recentMessagesIndex] = msg;
    recentMessagesIndex = (recentMessagesIndex + 1) % RECENT_MESSAGES_SIZE;

    // Passive device tracking:
    // - Track query commands to remember which address was queried
    // - When a response comes, mark that address as having a real device
    if (msg.parsed.type == "response") {
      // Backward frame - a device responded!
      handleResponse(msg);
    } else if (msg.parsed.address_type == "short" && msg.parsed.address < DALI_MAX_ADDRESSES) {
      // Forward frame to a specific address - track it for response matching
      trackQueryAddress(msg.parsed.address);
    }

    publishMonitor(msg);
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

void processCommandQueue() {
  if (queueHead == queueTail) return;
  
  if (!isBusIdle()) {
#ifdef DEBUG_SERIAL
    static unsigned long lastIdleWarnTime = 0;
    if (millis() - lastIdleWarnTime > 1000) {
      Serial.printf("[Queue] Waiting for bus idle (last activity: %lums ago)...\n", 
                    millis() - lastBusActivityTime);
      lastIdleWarnTime = millis();
    }
#endif
    return;
  }
  
  if (!canSendDaliCommand()) {
    return;
  }

  DaliCommand cmd = commandQueue[queueHead];
  queueHead = (queueHead + 1) % COMMAND_QUEUE_SIZE;

#ifdef DEBUG_SERIAL
  Serial.printf("[DALI] Processing command: %s to address %d (waited %lums in queue)\n", 
                cmd.command_type.c_str(), cmd.address, millis() - cmd.queued_at);
#endif

  uint8_t data_byte = cmd.level;
  uint8_t sent_bytes[2];
  
  updateBusActivity();
  lastDaliCommandTime = millis();

  if (cmd.command_type == "set_brightness") {
    // DAPC: address byte has bit 0 = 0
    sent_bytes[0] = (cmd.address << 1);
    sent_bytes[1] = data_byte;
    dali.set_level(data_byte, cmd.address);
    incrementTxCount();
    DaliMessage msg = parseDaliMessage(sent_bytes, 2, true);
    msg.source = "self";
    publishMonitor(msg);
  } else if (cmd.command_type == "off") {
    // Command: address byte has bit 0 = 1
    sent_bytes[0] = (cmd.address << 1) | 1;
    sent_bytes[1] = DALI_OFF;
    dali.cmd(DALI_OFF, cmd.address);
    incrementTxCount();
    DaliMessage msg = parseDaliMessage(sent_bytes, 2, true);
    msg.source = "self";
    publishMonitor(msg);
  } else if (cmd.command_type == "max") {
    sent_bytes[0] = (cmd.address << 1) | 1;
    sent_bytes[1] = DALI_RECALL_MAX_LEVEL;
    dali.cmd(DALI_RECALL_MAX_LEVEL, cmd.address);
    incrementTxCount();
    DaliMessage msg = parseDaliMessage(sent_bytes, 2, true);
    msg.source = "self";
    publishMonitor(msg);
  } else if (cmd.command_type == "up") {
    sent_bytes[0] = (cmd.address << 1) | 1;
    sent_bytes[1] = DALI_UP;
    dali.cmd(DALI_UP, cmd.address);
    incrementTxCount();
    DaliMessage msg = parseDaliMessage(sent_bytes, 2, true);
    msg.source = "self";
    publishMonitor(msg);
  } else if (cmd.command_type == "down") {
    sent_bytes[0] = (cmd.address << 1) | 1;
    sent_bytes[1] = DALI_DOWN;
    dali.cmd(DALI_DOWN, cmd.address);
    incrementTxCount();
    DaliMessage msg = parseDaliMessage(sent_bytes, 2, true);
    msg.source = "self";
    publishMonitor(msg);
  } else if (cmd.command_type == "step_up") {
    sent_bytes[0] = (cmd.address << 1) | 1;
    sent_bytes[1] = DALI_STEP_UP;
    dali.cmd(DALI_STEP_UP, cmd.address);
    incrementTxCount();
    DaliMessage msg = parseDaliMessage(sent_bytes, 2, true);
    msg.source = "self";
    publishMonitor(msg);
  } else if (cmd.command_type == "step_down") {
    sent_bytes[0] = (cmd.address << 1) | 1;
    sent_bytes[1] = DALI_STEP_DOWN;
    dali.cmd(DALI_STEP_DOWN, cmd.address);
    incrementTxCount();
    DaliMessage msg = parseDaliMessage(sent_bytes, 2, true);
    msg.source = "self";
    publishMonitor(msg);
  } else if (cmd.command_type == "recall_max") {
    sent_bytes[0] = (cmd.address << 1) | 1;
    sent_bytes[1] = DALI_RECALL_MAX_LEVEL;
    dali.cmd(DALI_RECALL_MAX_LEVEL, cmd.address);
    incrementTxCount();
    DaliMessage msg = parseDaliMessage(sent_bytes, 2, true);
    msg.source = "self";
    publishMonitor(msg);
  } else if (cmd.command_type == "recall_min") {
    sent_bytes[0] = (cmd.address << 1) | 1;
    sent_bytes[1] = DALI_RECALL_MIN_LEVEL;
    dali.cmd(DALI_RECALL_MIN_LEVEL, cmd.address);
    incrementTxCount();
    DaliMessage msg = parseDaliMessage(sent_bytes, 2, true);
    msg.source = "self";
    publishMonitor(msg);
  } else if (cmd.command_type == "go_to_scene") {
    sent_bytes[0] = (cmd.address << 1) | 1;
    sent_bytes[1] = DALI_GO_TO_SCENE + cmd.scene;
    dali.cmd(DALI_GO_TO_SCENE + cmd.scene, cmd.address);
    incrementTxCount();
    DaliMessage msg = parseDaliMessage(sent_bytes, 2, true);
    msg.source = "self";
    publishMonitor(msg);
  } else if (cmd.command_type == "reset") {
    sent_bytes[0] = (cmd.address << 1) | 1;
    sent_bytes[1] = DALI_RESET;
    dali.cmd(DALI_RESET, cmd.address);
    incrementTxCount();
    DaliMessage msg = parseDaliMessage(sent_bytes, 2, true);
    msg.source = "self";
    publishMonitor(msg);
  } else if (cmd.command_type == "query_status") {
    sent_bytes[0] = (cmd.address << 1) | 1;
    sent_bytes[1] = DALI_QUERY_STATUS;
    int16_t result = dali.cmd(DALI_QUERY_STATUS, cmd.address);
    incrementTxCount();
    DaliMessage msg = parseDaliMessage(sent_bytes, 2, true);
    msg.source = "self";
    publishMonitor(msg);
    if (result >= 0) {
      incrementRxCount();
    }
  } else if (cmd.command_type == "query_lamp_failure") {
    sent_bytes[0] = (cmd.address << 1) | 1;
    sent_bytes[1] = DALI_QUERY_LAMP_FAILURE;
    int16_t result = dali.cmd(DALI_QUERY_LAMP_FAILURE, cmd.address);
    incrementTxCount();
    DaliMessage msg = parseDaliMessage(sent_bytes, 2, true);
    msg.source = "self";
    publishMonitor(msg);
    if (result >= 0) {
      incrementRxCount();
    }
  } else if (cmd.command_type == "query_lamp_power_on") {
    sent_bytes[0] = (cmd.address << 1) | 1;
    sent_bytes[1] = DALI_QUERY_LAMP_POWER_ON;
    int16_t result = dali.cmd(DALI_QUERY_LAMP_POWER_ON, cmd.address);
    incrementTxCount();
    DaliMessage msg = parseDaliMessage(sent_bytes, 2, true);
    msg.source = "self";
    publishMonitor(msg);
    if (result >= 0) {
      incrementRxCount();
    }
  } else if (cmd.command_type == "query_actual_level") {
    sent_bytes[0] = (cmd.address << 1) | 1;
    sent_bytes[1] = DALI_QUERY_ACTUAL_LEVEL;
    int16_t result = dali.cmd(DALI_QUERY_ACTUAL_LEVEL, cmd.address);
    incrementTxCount();
    DaliMessage msg = parseDaliMessage(sent_bytes, 2, true);
    msg.source = "self";
    publishMonitor(msg);
    if (result >= 0) {
      incrementRxCount();
    }
  } else if (cmd.command_type == "query_max_level") {
    sent_bytes[0] = (cmd.address << 1) | 1;
    sent_bytes[1] = DALI_QUERY_MAX_LEVEL;
    int16_t result = dali.cmd(DALI_QUERY_MAX_LEVEL, cmd.address);
    incrementTxCount();
    DaliMessage msg = parseDaliMessage(sent_bytes, 2, true);
    msg.source = "self";
    publishMonitor(msg);
    if (result >= 0) {
      incrementRxCount();
    }
  } else if (cmd.command_type == "query_min_level") {
    sent_bytes[0] = (cmd.address << 1) | 1;
    sent_bytes[1] = DALI_QUERY_MIN_LEVEL;
    int16_t result = dali.cmd(DALI_QUERY_MIN_LEVEL, cmd.address);
    incrementTxCount();
    DaliMessage msg = parseDaliMessage(sent_bytes, 2, true);
    msg.source = "self";
    publishMonitor(msg);
    if (result >= 0) {
      incrementRxCount();
    }
  } else if (cmd.command_type == "query_device_type") {
    sent_bytes[0] = (cmd.address << 1) | 1;
    sent_bytes[1] = DALI_QUERY_DEVICE_TYPE;
    int16_t result = dali.cmd(DALI_QUERY_DEVICE_TYPE, cmd.address);
    incrementTxCount();
    DaliMessage msg = parseDaliMessage(sent_bytes, 2, true);
    msg.source = "self";
    publishMonitor(msg);
    if (result >= 0) {
      incrementRxCount();
    }
  } else if (cmd.command_type == "query_scene_level") {
    sent_bytes[0] = (cmd.address << 1) | 1;
    sent_bytes[1] = DALI_QUERY_SCENE_LEVEL + cmd.scene;
    int16_t result = dali.cmd(DALI_QUERY_SCENE_LEVEL + cmd.scene, cmd.address);
    incrementTxCount();
    DaliMessage msg = parseDaliMessage(sent_bytes, 2, true);
    msg.source = "self";
    publishMonitor(msg);
    if (result >= 0) {
      incrementRxCount();
    }
  } else if (cmd.command_type == "set_rgb") {
    // DT8: Set RGB color (requires DTR0=R, DTR1=G, DTR2=B)
    dali.cmd(DALI_SET_DTR0 | 0x100, cmd.color_r);  // Special command
    delay(5);
    dali.cmd(DALI_SET_DTR1 | 0x100, cmd.color_g);
    delay(5);
    dali.cmd(DALI_SET_DTR2 | 0x100, cmd.color_b);
    delay(5);
    sent_bytes[0] = (cmd.address << 1) | 1;
    sent_bytes[1] = DALI_DT8_SET_TEMPORARY_RGB_DIMLEVEL;
    dali.cmd(DALI_DT8_SET_TEMPORARY_RGB_DIMLEVEL, cmd.address);
    delay(5);
    dali.cmd(DALI_DT8_ACTIVATE, cmd.address);
    incrementTxCount();
    DaliMessage msg = parseDaliMessage(sent_bytes, 2, true);
    msg.source = "self";
    publishMonitor(msg);
  } else if (cmd.command_type == "set_rgbw") {
    // DT8: Set RGBW color (R, G, B in RGB command, then W separately)
    dali.cmd(DALI_SET_DTR0 | 0x100, cmd.color_r);
    delay(5);
    dali.cmd(DALI_SET_DTR1 | 0x100, cmd.color_g);
    delay(5);
    dali.cmd(DALI_SET_DTR2 | 0x100, cmd.color_b);
    delay(5);
    dali.cmd(DALI_DT8_SET_TEMPORARY_RGB_DIMLEVEL, cmd.address);
    delay(5);
    dali.cmd(DALI_SET_DTR0 | 0x100, cmd.color_w);
    delay(5);
    dali.cmd(DALI_DT8_SET_TEMPORARY_WAF_DIMLEVEL, cmd.address);
    delay(5);
    sent_bytes[0] = (cmd.address << 1) | 1;
    sent_bytes[1] = DALI_DT8_ACTIVATE;
    dali.cmd(DALI_DT8_ACTIVATE, cmd.address);
    incrementTxCount();
    DaliMessage msg = parseDaliMessage(sent_bytes, 2, true);
    msg.source = "self";
    publishMonitor(msg);
  } else if (cmd.command_type == "set_color_temp") {
    // DT8: Set color temperature (requires DTR0=LSB, DTR1=MSB of mirek value)
    uint16_t mirek = 1000000 / cmd.color_temp_kelvin;  // Convert Kelvin to mirek
    dali.cmd(DALI_SET_DTR0 | 0x100, mirek & 0xFF);
    delay(5);
    dali.cmd(DALI_SET_DTR1 | 0x100, (mirek >> 8) & 0xFF);
    delay(5);
    sent_bytes[0] = (cmd.address << 1) | 1;
    sent_bytes[1] = DALI_DT8_SET_TEMPORARY_COLOUR_TEMPERATURE;
    dali.cmd(DALI_DT8_SET_TEMPORARY_COLOUR_TEMPERATURE, cmd.address);
    delay(5);
    dali.cmd(DALI_DT8_ACTIVATE, cmd.address);
    incrementTxCount();
    DaliMessage msg = parseDaliMessage(sent_bytes, 2, true);
    msg.source = "self";
    publishMonitor(msg);
  }

  updateLastActivity();
}

bool canSendDaliCommand() {
  return (millis() - lastDaliCommandTime) >= DALI_MIN_INTERVAL_MS;
}

void sendDaliCommand(uint8_t address, uint8_t level) {
  DaliCommand cmd;
  cmd.command_type = "set_brightness";
  cmd.address = address;
  cmd.address_type = (address == 0xFF) ? "broadcast" : "short";
  cmd.level = level;
  cmd.force = false;
  cmd.queued_at = millis();
  cmd.priority = 1;
  cmd.retry_count = 0;

  enqueueDaliCommand(cmd);
}

void addRecentMessage(const DaliMessage& msg) {
  recentMessages[recentMessagesIndex] = msg;
  recentMessagesIndex = (recentMessagesIndex + 1) % RECENT_MESSAGES_SIZE;
}

DaliScanResult scanDaliDevices() {
  DaliScanResult result;
  result.scan_timestamp = millis() / 1000;
  result.total_found = 0;

#ifdef DEBUG_SERIAL
  Serial.println("Scanning DALI bus for devices...");
  Serial.println("Using bus arbitration - waiting for idle periods between queries");
#endif

  for (uint8_t addr = 0; addr < 64; addr++) {
    esp_task_wdt_reset();
    
    // Retry up to 3 times per address to handle bus collisions
    int16_t rv = -1;
    for (uint8_t retry = 0; retry < 3 && rv < 0; retry++) {
      // Wait for longer idle period on busy bus (200ms instead of 100ms)
      unsigned long waitStart = millis();
      while (!isBusIdle() && (millis() - waitStart < 5000)) {
        delay(10);
      }
      
      if (!isBusIdle()) {
#ifdef DEBUG_SERIAL
        if (retry == 2) {
          Serial.printf("Scan timeout waiting for bus idle at address %d after %d retries\n", addr, retry + 1);
        }
#endif
        continue;
      }
      
      // Wait extra time to ensure bus is really idle (avoid collision with Zennio)
      delay(50);
      
      // Check idle again after delay
      if (!isBusIdle()) {
        continue;
      }
      
      updateBusActivity();
      rv = dali.cmd(DALI_QUERY_STATUS, addr);
      
#ifdef DEBUG_SERIAL
      if (rv < 0 && retry < 2) {
        Serial.printf("Address %d query failed (attempt %d/3), retrying...\n", addr, retry + 1);
      }
#endif
    }

    if (rv >= 0) {
      DaliDevice device;
      device.address = addr;
      device.type = "short";
      device.status = "ok";
      device.lamp_failure = false;
      device.min_level = 0;
      device.max_level = 254;

      esp_task_wdt_reset();

      delay(DALI_MIN_INTERVAL_MS);
      unsigned long queryWaitStart = millis();
      while (!isBusIdle() && (millis() - queryWaitStart < 5000)) {
        delay(10);
      }
      updateBusActivity();
      int16_t lamp_failure_rv = dali.cmd(DALI_QUERY_LAMP_FAILURE, addr);
      if (lamp_failure_rv > 0) {
        device.lamp_failure = true;
      }

      delay(DALI_MIN_INTERVAL_MS);
      queryWaitStart = millis();
      while (!isBusIdle() && (millis() - queryWaitStart < 5000)) {
        delay(10);
      }
      updateBusActivity();
      int16_t min_level_rv = dali.cmd(DALI_QUERY_MIN_LEVEL, addr);
      if (min_level_rv >= 0) {
        device.min_level = (uint8_t)min_level_rv;
      }

      delay(DALI_MIN_INTERVAL_MS);
      queryWaitStart = millis();
      while (!isBusIdle() && (millis() - queryWaitStart < 5000)) {
        delay(10);
      }
      updateBusActivity();
      int16_t max_level_rv = dali.cmd(DALI_QUERY_MAX_LEVEL, addr);
      if (max_level_rv >= 0) {
        device.max_level = (uint8_t)max_level_rv;
      }

      result.devices.push_back(device);
      result.total_found++;

#ifdef DEBUG_SERIAL
      Serial.printf("Found device at address %d (status=0x%x, min=%d, max=%d)\n",
                    addr, rv, device.min_level, device.max_level);
#endif

      delay(DALI_MIN_INTERVAL_MS);
    }
  }

#ifdef DEBUG_SERIAL
  Serial.printf("Scan complete: %d devices found\n", result.total_found);
#endif
  return result;
}

bool sendCommissioningCommand(uint8_t command, uint8_t data) {
  const int MAX_RETRIES = 10;
  const unsigned long SHORT_IDLE_TIMEOUT = 50;
  
  for (int retry = 0; retry < MAX_RETRIES; retry++) {
    unsigned long waitStart = millis();
    while (!isBusIdle() && (millis() - waitStart < SHORT_IDLE_TIMEOUT)) {
      delay(5);
    }
    
    if (isBusIdle()) {
      updateBusActivity();
      uint8_t addr_byte = 0xA5;
      int16_t result = dali.cmd(command, data);
      delay(DALI_MIN_INTERVAL_MS);
      
      if (result >= 0) {
#ifdef DEBUG_SERIAL
        if (retry > 0) {
          Serial.printf("[Commissioning] Command sent after %d retries\n", retry);
        }
#endif
        return true;
      }
    }
    
    delay(20);
  }
  
#ifdef DEBUG_SERIAL
  Serial.println("[Commissioning] Failed to send command after retries");
#endif
  return false;
}

int16_t queryCommissioning(uint8_t command) {
  const int MAX_RETRIES = 3;
  const unsigned long SHORT_IDLE_TIMEOUT = 50;
  
  for (int retry = 0; retry < MAX_RETRIES; retry++) {
    unsigned long waitStart = millis();
    while (!isBusIdle() && (millis() - waitStart < SHORT_IDLE_TIMEOUT)) {
      delay(5);
    }
    
    if (isBusIdle()) {
      updateBusActivity();
      uint8_t addr_byte = 0xA1;
      int16_t result = dali.cmd(command, addr_byte);
      delay(DALI_MIN_INTERVAL_MS);
      
      if (command == DALI_COMPARE) {
        if (result == 0xFF) return 1;
        if (result == -1) return 0;
        if (result < 0) return 1;
      } else {
        if (result >= 0) return result;
      }
    }
    
    delay(20);
  }
  
  return (command == DALI_COMPARE) ? 0 : -1;
}

void commissionDevices(uint8_t start_address) {
  esp_task_wdt_reset();
  
  commissioningProgress.state = COMM_INITIALIZING;
  commissioningProgress.start_timestamp = millis();
  commissioningProgress.devices_found = 0;
  commissioningProgress.devices_programmed = 0;
  commissioningProgress.current_address = 0;
  commissioningProgress.next_free_address = start_address;
  commissioningProgress.status_message = "Starting commissioning...";
  commissioningProgress.progress_percent = 0;
  
  publishCommissioningProgress(commissioningProgress);
  
#ifdef DEBUG_SERIAL
  Serial.println("[Commissioning] Starting DALI commissioning process");
  Serial.printf("[Commissioning] Starting address: %d\n", start_address);
#endif

  esp_task_wdt_reset();
  
  commissioningProgress.status_message = "Sending INITIALISE command...";
  commissioningProgress.progress_percent = 5;
  publishCommissioningProgress(commissioningProgress);
  
  if (!sendCommissioningCommand(DALI_INITIALISE, 0x00)) {
    commissioningProgress.state = COMM_ERROR;
    commissioningProgress.status_message = "Failed to send INITIALISE";
    publishCommissioningProgress(commissioningProgress);
    return;
  }
  delay(100);
  
  commissioningProgress.status_message = "Sending RANDOMISE command...";
  commissioningProgress.progress_percent = 10;
  publishCommissioningProgress(commissioningProgress);
  
  if (!sendCommissioningCommand(DALI_RANDOMISE, 0x00)) {
    commissioningProgress.state = COMM_ERROR;
    commissioningProgress.status_message = "Failed to send RANDOMISE";
    publishCommissioningProgress(commissioningProgress);
    return;
  }
  delay(100);
  
#ifdef DEBUG_SERIAL
  Serial.println("[Commissioning] Waiting 100ms after RANDOMISE for devices to generate random addresses");
#endif
  
  commissioningProgress.state = COMM_SEARCHING;
  commissioningProgress.status_message = "Searching for devices...";
  commissioningProgress.progress_percent = 15;
  publishCommissioningProgress(commissioningProgress);
  
  esp_task_wdt_reset();
  
  std::vector<uint32_t> foundDevices;
  
  while (true) {
    esp_task_wdt_reset();
    
    uint32_t addr = 0;
    
    commissioningProgress.status_message = "Searching for device...";
    commissioningProgress.progress_percent = 15 + (foundDevices.size() * 10);
    publishCommissioningProgress(commissioningProgress);
    
#ifdef DEBUG_SERIAL
    Serial.println("[Commissioning] Starting binary search for device");
#endif
    
    for (uint32_t i = 0; i < 24; i++) {
      esp_task_wdt_reset();
      
      uint32_t bit = 1 << (23 - i);
      addr |= bit;
      
      commissioningProgress.current_random_address = addr;
      
      uint8_t searchH = (addr >> 16) & 0xFF;
      uint8_t searchM = (addr >> 8) & 0xFF;
      uint8_t searchL = addr & 0xFF;
      
      if (!sendCommissioningCommand(DALI_SEARCHADDRH, searchH)) goto commission_error;
      if (!sendCommissioningCommand(DALI_SEARCHADDRM, searchM)) goto commission_error;
      if (!sendCommissioningCommand(DALI_SEARCHADDRL, searchL)) goto commission_error;
      
      int16_t compareResult = queryCommissioning(DALI_COMPARE);
      
      if (compareResult == 1) {
        addr &= ~bit;
      } else {
        addr |= bit;
      }
    }
    
    uint8_t searchH = (addr >> 16) & 0xFF;
    uint8_t searchM = (addr >> 8) & 0xFF;
    uint8_t searchL = addr & 0xFF;
    
    if (!sendCommissioningCommand(DALI_SEARCHADDRH, searchH)) break;
    if (!sendCommissioningCommand(DALI_SEARCHADDRM, searchM)) break;
    if (!sendCommissioningCommand(DALI_SEARCHADDRL, searchL)) break;
    
    int16_t finalCompare = queryCommissioning(DALI_COMPARE);
    if (finalCompare != 1) {
      addr++;
    }
    
    if (!sendCommissioningCommand(DALI_SEARCHADDRH, (addr >> 16) & 0xFF)) break;
    if (!sendCommissioningCommand(DALI_SEARCHADDRM, (addr >> 8) & 0xFF)) break;
    if (!sendCommissioningCommand(DALI_SEARCHADDRL, addr & 0xFF)) break;
    
    int16_t deviceFound = queryCommissioning(DALI_COMPARE);
    if (deviceFound != 1) {
      break;
    }
    
    foundDevices.push_back(addr);
    commissioningProgress.devices_found++;
    
#ifdef DEBUG_SERIAL
    Serial.printf("[Commissioning] Found device with random address 0x%06X\n", addr);
#endif
    
    esp_task_wdt_reset();
    
    if (!sendCommissioningCommand(DALI_WITHDRAW, 0x00)) break;
  }
  
  commission_error:
  ;
  
#ifdef DEBUG_SERIAL
  Serial.printf("[Commissioning] Search complete. Found %d devices\n", foundDevices.size());
#endif
  
  if (foundDevices.size() == 0) {
    commissioningProgress.state = COMM_COMPLETE;
    commissioningProgress.status_message = "No unaddressed devices found";
    commissioningProgress.progress_percent = 100;
    publishCommissioningProgress(commissioningProgress);
    return;
  }
  
  commissioningProgress.state = COMM_PROGRAMMING;
  
  esp_task_wdt_reset();
  
  if (!sendCommissioningCommand(DALI_INITIALISE, 0x00)) {
    commissioningProgress.state = COMM_ERROR;
    commissioningProgress.status_message = "Failed to re-initialize";
    publishCommissioningProgress(commissioningProgress);
    return;
  }
  delay(100);
  
  for (size_t i = 0; i < foundDevices.size(); i++) {
    esp_task_wdt_reset();
    uint32_t randomAddr = foundDevices[i];
    uint8_t newAddress = commissioningProgress.next_free_address;
    
    if (newAddress > 63) {
      commissioningProgress.state = COMM_ERROR;
      commissioningProgress.status_message = "No free addresses available";
      publishCommissioningProgress(commissioningProgress);
      break;
    }
    
    commissioningProgress.current_address = newAddress;
    commissioningProgress.current_random_address = randomAddr;
    commissioningProgress.status_message = "Programming device " + String(i + 1) + "/" + String(foundDevices.size());
    commissioningProgress.progress_percent = 60 + ((i * 30) / foundDevices.size());
    publishCommissioningProgress(commissioningProgress);
    
#ifdef DEBUG_SERIAL
    Serial.printf("[Commissioning] Programming device %d/%d (random 0x%06X -> address %d)\n",
                  i + 1, foundDevices.size(), randomAddr, newAddress);
#endif
    
    uint8_t searchH = (randomAddr >> 16) & 0xFF;
    uint8_t searchM = (randomAddr >> 8) & 0xFF;
    uint8_t searchL = randomAddr & 0xFF;
    
    if (!sendCommissioningCommand(DALI_SEARCHADDRH, searchH)) break;
    if (!sendCommissioningCommand(DALI_SEARCHADDRM, searchM)) break;
    if (!sendCommissioningCommand(DALI_SEARCHADDRL, searchL)) break;
    
    uint8_t programData = (newAddress << 1) | 0x01;
    if (!sendCommissioningCommand(DALI_PROGRAM_SHORT_ADDRESS, programData)) break;
    
    delay(200);
    
    commissioningProgress.devices_programmed++;
    commissioningProgress.next_free_address++;
  }
  
  commissioningProgress.state = COMM_VERIFYING;
  commissioningProgress.status_message = "Verifying programmed addresses...";
  commissioningProgress.progress_percent = 90;
  publishCommissioningProgress(commissioningProgress);
  
  esp_task_wdt_reset();
  
#ifdef DEBUG_SERIAL
  Serial.println("[Commissioning] Verifying programmed addresses...");
#endif
  
  delay(100);
  
  for (uint8_t addr = start_address; addr < commissioningProgress.next_free_address; addr++) {
    esp_task_wdt_reset();
    
    bool verified = false;
    for (int retry = 0; retry < 5; retry++) {
      unsigned long waitStart = millis();
      while (!isBusIdle() && (millis() - waitStart < 50)) {
        delay(5);
      }
      
      if (isBusIdle()) {
        updateBusActivity();
        int16_t result = dali.cmd(DALI_QUERY_STATUS, addr);
        if (result >= 0) {
          verified = true;
#ifdef DEBUG_SERIAL
          Serial.printf("[Commissioning] Verified device at address %d\n", addr);
#endif
          break;
        }
      }
      delay(20);
    }
    
    if (!verified) {
#ifdef DEBUG_SERIAL
      Serial.printf("[Commissioning] WARNING: Device at address %d not responding\n", addr);
#endif
    }
    delay(DALI_MIN_INTERVAL_MS);
  }
  
  esp_task_wdt_reset();
  
  commissioningProgress.state = COMM_COMPLETE;
  commissioningProgress.status_message = "Commissioning complete! Programmed " + 
                                         String(commissioningProgress.devices_programmed) + " devices";
  commissioningProgress.progress_percent = 100;
  publishCommissioningProgress(commissioningProgress);
  
#ifdef DEBUG_SERIAL
  Serial.printf("[Commissioning] Complete! Programmed %d devices (addresses %d-%d)\n",
                commissioningProgress.devices_programmed, start_address, 
                commissioningProgress.next_free_address - 1);
#endif
}
