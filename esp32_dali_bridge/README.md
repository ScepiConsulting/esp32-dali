# ESP32 DALI Bridge

A professional bridge between DALI (Digital Addressable Lighting Interface) and MQTT, built on ESP32-S3 with a modern web interface.

## 🌟 Features

- **DALI Bus Communication** - Full bidirectional DALI protocol support
- **Bus Arbitration** - Smart collision avoidance for busy DALI buses
- **Bus Monitoring** - Detects and reports messages from external DALI masters
- **Auto-Commissioning** - Automatic device discovery and address assignment
- **MQTT Integration** - Publish/subscribe with configurable QoS and topics
- **Web Interface** - Modern, responsive UI with dark/light theme
- **Authentication** - Configurable username/password for web interface
- **OTA Updates** - Over-the-air firmware updates via web interface
- **WiFi Management** - STA and AP modes with automatic fallback
- **Device Scanning** - Automatic DALI device discovery
- **Command Queue** - Reliable command processing with bus idle detection
- **Diagnostics** - Real-time system monitoring and statistics
- **MQTT Monitoring** - Track all DALI bus activity with source attribution
- **Progress Tracking** - Real-time commissioning progress via MQTT

## 📋 Table of Contents

- [Hardware Requirements](#hardware-requirements)
- [Quick Start](#quick-start)
- [Local Development](#local-development)
- [Building](#building)
- [Flashing](#flashing)
- [Configuration](#configuration)
- [Usage](#usage)
- [MQTT Topics](#mqtt-topics)
- [Troubleshooting](#troubleshooting)
- [License](#license)

## 🔧 Hardware Requirements

### Required Components
- **ESP32-S3** development board
- **DALI interface circuit** connected to:
  - TX: GPIO17
  - RX: GPIO14
- **Power supply** - 5V USB or external power

### Recommended Hardware

**Tested Configuration:**
- [Waveshare ESP32-S3-Pico](https://www.waveshare.com/esp32-s3-pico.htm) - Compact ESP32-S3 board
- [Waveshare Pico-DALI2](https://www.waveshare.com/pico-dali2.htm) - DALI transceiver module

**Alternative Options:**
- ESP32-S3-DevKitC-1 or similar ESP32-S3 board
- Commercial DALI transceiver module (e.g., based on TPS92662)
- DIY circuit with optocouplers and current limiting

**Note:** DALI bus operates at 16V DC with Manchester encoding. Proper isolation is required.

## 🚀 Quick Start

### First-Time Setup

1. **Flash the firmware** (see [Flashing](#flashing) section)

2. **Connect to AP mode**
   - SSID: `ESP32-DALI-Setup`
   - Password: `daliconfig`
   - Navigate to: `http://192.168.4.1`

3. **Configure WiFi**
   - Enter your WiFi credentials in "WiFi Setup" section
   - Click "Save WiFi Settings"
   - (Optional) Set web interface username/password in "Web Interface Security" section
   - Click "Save Settings" (separate button, no reboot)

4. **Device reboots** after WiFi save and connects to your network

5. **Access via your network**
   - Find the device IP in your router's DHCP table
   - Or check serial output for IP address

## 💻 Local Development

### Prerequisites

- **Arduino CLI** - [Installation guide](https://arduino.github.io/arduino-cli/latest/installation/)
- **ESP32 Arduino Core** v3.3.5 or later
- **Required Libraries:**
  - PubSubClient (2.8)
  - ArduinoJson (7.4.2)
  - ESP32 built-in libraries (WiFi, WebServer, Preferences, SPIFFS, Update)

### Install Arduino CLI

```bash
# macOS
brew install arduino-cli

# Linux
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh

# Windows
choco install arduino-cli
```

### Install ESP32 Core

```bash
arduino-cli config init
arduino-cli core update-index
arduino-cli core install esp32:esp32@3.3.5
```

### Install Required Libraries

```bash
arduino-cli lib install "PubSubClient@2.8"
arduino-cli lib install "ArduinoJson@7.4.2"
```

### Project Structure

```
esp32-dali-bridge/
├── esp32_dali_bridge/          # Main source directory
│   ├── esp32_dali_bridge.ino   # Main sketch
│   ├── config.h                # Configuration constants
│   ├── version.h               # Version information
│   ├── dali_handler.cpp/h      # DALI protocol implementation
│   ├── mqtt_handler.cpp/h      # MQTT client logic
│   ├── web_interface.cpp/h     # Web server and UI
│   ├── diagnostics.cpp/h       # System diagnostics
│   ├── utils.cpp/h             # Utility functions
│   ├── logos.h                 # Embedded logo images
│   ├── dali_protocol.h         # DALI data structures
│   └── DALI_Lib.cpp/h          # Low-level DALI library
├── README.md                   # This file
└── build/                      # Compiled binaries (generated)
```

### Source Files Reference

| File | Purpose | Used By |
|------|---------|---------|
| `esp32_dali_bridge.ino` | Main sketch entry point | Arduino build system |
| `config.h` | Configuration constants (pins, credentials, timing) | All modules |
| `version.h` | Firmware version information | utils.cpp, web_interface.cpp |
| `dali_protocol.h` | DALI data structures and constants | dali_handler.cpp |
| `dali_handler.cpp/h` | DALI protocol implementation and command queue | Main sketch, MQTT, web |
| `DALI_Lib.cpp/h` | Low-level DALI bus driver (Manchester encoding) | dali_handler.cpp |
| `mqtt_handler.cpp/h` | MQTT client logic and topic handling | Main sketch, DALI handler |
| `web_interface.cpp/h` | Web server, HTML UI, and HTTP endpoints | Main sketch |
| `diagnostics.cpp/h` | System diagnostics and statistics tracking | All modules |
| `utils.cpp/h` | Utility functions (JSON parsing, hex conversion) | MQTT, web interface |
| `logos.h` | Embedded logo images in PROGMEM (light/dark theme) | web_interface.cpp |

## 🔨 Building

### Build from Command Line

```bash
# Navigate to project directory
cd esp32_dali_bridge

# Compile
arduino-cli compile --fqbn esp32:esp32:esp32s3 --output-dir build .

# Build output will be in build/esp32_dali_bridge.ino.merged.bin
```

### Build Configuration

Edit `esp32_dali_bridge/config.h` to customize:

```cpp
// Enable/disable serial debug logging (saves ~2.9KB when disabled)
#define DEBUG_SERIAL

// DALI pins
#define DALI_TX_PIN 17
#define DALI_RX_PIN 14

// AP mode credentials
#define AP_SSID "ESP32-DALI-Setup"
#define AP_PASSWORD "daliconfig"

// DALI timing and bus arbitration
#define DALI_MIN_INTERVAL_MS 50
#define COMMAND_QUEUE_SIZE 50
#define BUS_IDLE_TIMEOUT_MS 100
#define BUS_ACTIVITY_WINDOW_MS 500
```

### Build Size

Current build statistics:
- **Flash:** ~1.12 MB (85% of 1.31 MB)
- **RAM:** ~49 KB (15% of 327 KB)

## 📲 Flashing

### Using esptool.py (Recommended)

```bash
# Install esptool
pip install esptool

# Find your ESP32 port
# macOS: /dev/cu.usbserial-* or /dev/tty.usbserial-*
# Linux: /dev/ttyUSB* or /dev/ttyACM*
# Windows: COM*

# Flash the firmware
esptool.py --chip esp32s3 \
  --port /dev/ttyACM0 \
  --baud 460800 \
  write_flash 0x0 build/esp32_dali_bridge.ino.merged.bin

# Monitor serial output (optional)
arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200

# Or using picocom
picocom -b 115200 /dev/ttyACM0
# Exit picocom: Ctrl+A then Ctrl+X
```

### Using Arduino IDE

1. Open `esp32_dali_bridge/esp32_dali_bridge.ino`
2. Select board: **ESP32S3 Dev Module**
3. Select port
4. Click **Upload**

### First Boot

On first boot, the device will:
1. Format SPIFFS (~30 seconds)
2. Start in AP mode
3. Display connection info in serial output

## ⚙️ Configuration

### WiFi Configuration

**Via Web Interface:**
1. Connect to `ESP32-DALI-Setup` AP
2. Navigate to WiFi page
3. Enter SSID and password in "WiFi Setup" section
4. Click "Save WiFi Settings" (device will reboot)
5. (Optional) Set web interface username/password in "Web Interface Security" section
6. Click "Save Settings" (no reboot, takes effect immediately)

**Via Serial (if needed):**
- WiFi credentials are stored in NVS (Non-Volatile Storage)
- To reset: erase flash with `esptool.py erase_flash`

### MQTT Configuration

Navigate to **MQTT** page in web interface:

| Setting | Description | Default |
|---------|-------------|---------|
| Broker | MQTT broker hostname/IP | - |
| Port | MQTT broker port | 1883 |
| Username | MQTT username (optional) | - |
| Password | MQTT password (optional) | - |
| Topic Prefix | Base topic for all messages | `home/dali/` |
| Client ID | MQTT client identifier | `dali-bridge-XXXXXX` |
| QoS | Quality of Service (0-2) | 0 |

**Client ID** is auto-generated from MAC address: `dali-bridge-` + last 6 hex digits

**Note:** Changing MQTT settings triggers a device reboot to apply new configuration. You'll see a countdown before the reboot occurs.

### Web Interface Authentication

Set username and password to protect web interface access:
1. Go to **WiFi** page
2. Scroll to "Web Interface Security"
3. Enter username (default: `admin`)
4. Enter password (or leave empty to disable authentication)
5. Click "Save Settings"

**Authentication:**
- Username: Configurable (default: `admin`)
- Password: Optional (your chosen password)
- Uses HTTP Basic Authentication
- Credentials stored separately from WiFi settings

### Diagnostics

Enable MQTT diagnostics publishing:
1. Go to **Diagnostics** page
2. Check "Enable diagnostics publishing to MQTT"
3. Click "Save Settings"

Publishes system stats to `{prefix}/diagnostics` every 60 seconds.

**System Actions:**
- Reboot button available at bottom of Diagnostics page
- Useful for applying configuration changes or recovering from issues

## 📖 Usage

### Web Interface

Access the web interface at `http://<device-ip>`

**Pages:**
- **Home** - System status and MQTT topic examples
- **WiFi** - WiFi and password configuration
- **MQTT** - MQTT broker settings
- **DALI Control** - Send commands to DALI devices
- **Diagnostics** - System information and statistics
- **Update** - OTA firmware updates

### DALI Control

**Via Web Interface:**
1. Go to **DALI Control** page
2. Select command type
3. Enter device address (0-63 or 255 for broadcast)
4. Set level/scene as needed
5. Click "Send Command"

**Supported Commands:**
- Set Brightness (0-254)
- Off
- Recall Max/Min Level
- Go to Scene (0-15)
- Query Status
- Query Actual Level
- Query Device Type
- And more...

**Device Scanning:**
- Click "Scan DALI Bus" to discover devices
- Results show address, status, min/max levels
- Scan takes ~3-4 seconds for 64 addresses
- Uses bus arbitration to avoid collisions on busy buses

**Observed Devices (Passive Discovery):**
- Automatically learns devices from bus traffic without active scanning
- Shows devices that have responded to queries (not just queried addresses)
- Displays: address, last known level, time since last seen
- No persistence - clears on reboot
- Minimal RAM usage (~512 bytes for 64 addresses)
- Click "Refresh" on DALI Control page to view

**Device Commissioning:**
- Automatically assign addresses to unaddressed devices
- **Via Web Interface:**
  1. Go to **DALI Control** page
  2. Scroll to "Device Commissioning" section
  3. Enter starting address (0-63, default 0)
  4. Click "Start Commissioning"
  5. Watch real-time progress with percentage and status
  6. Results show devices found and programmed
- **Via MQTT:** Trigger via `{prefix}/commission/trigger`, monitor via `{prefix}/commission/progress`
- Supports starting from specific address (0-63)
- Binary search algorithm finds all devices efficiently
- Real-time progress updates with percentage completion

### Bus Monitoring

The ESP32 DALI Bridge continuously monitors the DALI bus for activity from external devices:

**What is monitored:**
- Commands from other DALI masters (e.g., Zennio, Helvar, Tridonic controllers)
- Responses from DALI ballasts/drivers
- Broadcast messages
- Group commands

**How it works:**
- Non-blocking receive checks every loop cycle (~10ms)
- All detected messages published to MQTT monitor topic
- Messages tagged with `"source": "bus"` (vs `"source": "self"` for ESP32 commands)
- Stored in recent messages buffer (last 20)
- RX counter incremented for diagnostics

**Use cases:**
- Monitor wall switch activity
- Track commands from building management systems
- Debug DALI bus issues
- Log all lighting control events

### MQTT Control

Send commands via MQTT to `{prefix}/command`:

```json
{
  "command": "set_brightness",
  "address": 5,
  "level": 128
}
```

**Available Commands:**

| Command | Parameters | Description |
|---------|------------|-------------|
| `set_brightness` | `address`, `level` (0-254) | Set device brightness |
| `off` | `address` | Turn device off |
| `max` | `address` | Recall max level |
| `min` | `address` | Recall min level |
| `go_to_scene` | `address`, `scene` (0-15) | Go to scene |
| `query_status` | `address` | Query device status |
| `query_actual_level` | `address` | Query current level |
| `query_device_type` | `address` | Query device type |

**Address:**
- `0-63`: Individual device (short address)
- `255`: Broadcast to all devices

## 📡 MQTT Topics

### Command Topic
**Topic:** `{prefix}/command`
**Direction:** Subscribe (receive commands)

```json
{
  "command": "set_brightness",
  "address": 0,
  "level": 128
}
```

### Monitor Topic
**Topic:** `{prefix}/monitor`
**Direction:** Publish (send bus activity)

```json
{
  "timestamp": 1234567890,
  "direction": "tx",
  "source": "self",
  "raw": "01FE",
  "parsed": {
    "type": "direct_arc_power",
    "address": 0,
    "address_type": "short",
    "level": 254,
    "level_percent": 99.6,
    "description": "Device 0: Set to 254 (99.6%)"
  }
}
```

**Source field:**
- `"self"` - Command sent by ESP32 (from web/MQTT)
- `"bus"` - Message detected from external DALI master

### Status Topic
**Topic:** `{prefix}/status`
**Direction:** Publish (LWT + periodic)

```json
{
  "status": "online",
  "uptime": 3600,
  "free_heap_kb": 236.5,
  "messages_rx": 42,
  "messages_tx": 38,
  "errors": 0,
  "bus_health": "OK"
}
```

Published:
- On connect (online)
- Every 60 seconds
- On disconnect (offline - LWT)

### Scan Topics
**Trigger:** `{prefix}/scan/trigger`
**Result:** `{prefix}/scan/result`

Trigger scan:
```json
{
  "action": "scan"
}
```

Result:
```json
{
  "scan_timestamp": 1234567890,
  "total_found": 3,
  "devices": [
    {
      "address": 0,
      "type": "short",
      "status": "ok",
      "min_level": 1,
      "max_level": 254
    }
  ]
}
```

### Commissioning Topics
**Trigger:** `{prefix}/commission/trigger`
**Progress:** `{prefix}/commission/progress`

Trigger commissioning (start from address 0):
```bash
mosquitto_pub -t "{prefix}/commission/trigger" -m "0"
```

Trigger commissioning (start from address 10):
```bash
mosquitto_pub -t "{prefix}/commission/trigger" -m "10"
```

Progress updates:
```json
{
  "state": "programming",
  "start_timestamp": 12345678,
  "devices_found": 3,
  "devices_programmed": 1,
  "current_address": 5,
  "next_free_address": 6,
  "current_random_address": "0xABCDEF",
  "status_message": "Programming device 2/3",
  "progress_percent": 75
}
```

**States:**
- `idle` - Not running
- `initializing` - Sending INITIALISE/RANDOMISE commands
- `searching` - Binary search for devices using COMPARE
- `programming` - Assigning short addresses
- `verifying` - Checking programmed devices respond
- `complete` - Successfully finished
- `error` - Failed with error message

### Diagnostics Topic (Optional)
**Topic:** `{prefix}/diagnostics`
**Direction:** Publish (periodic, if enabled)

Published every 60 seconds when diagnostics are enabled in settings.

```json
{
  "uptime": 3600,
  "free_heap_kb": 236.5,
  "messages_rx": 42,
  "messages_tx": 38,
  "errors": 0,
  "bus_health": "OK"
}
```

Enable in web interface: **Diagnostics** page → Check "Enable diagnostics publishing to MQTT"

## 🐛 Troubleshooting

### Device Won't Connect to WiFi

1. Check SSID/password are correct
2. Ensure 2.4GHz WiFi (ESP32 doesn't support 5GHz)
3. Check serial output for connection errors
4. Device falls back to AP mode after 20 failed attempts

### MQTT Not Connecting

1. Verify broker address and port
2. Check username/password if required
3. Ensure broker is reachable from ESP32's network
4. Check serial output: `[MQTT] connection failed, rc=X`
   - rc=-2: Connection refused
   - rc=-4: Connection timeout

### DALI Commands Not Working

1. Verify DALI wiring (TX: GPIO17, RX: GPIO14)
2. Check DALI bus power (should be ~16V)
3. Ensure proper isolation circuit
4. Try scanning for devices first
5. Check serial output for DALI errors
6. If bus is very busy, commands may be queued waiting for idle periods
7. Check serial output for "Waiting for bus idle" messages

### Commands Queued But Not Sending

If you see commands queued but not executing:

1. Check if another DALI master is flooding the bus (e.g., Zennio)
2. Monitor serial output for "Waiting for bus idle" messages
3. Bus arbitration waits for 100ms of silence before transmitting
4. On very busy buses, increase `BUS_IDLE_TIMEOUT_MS` in config.h
5. Check MQTT monitor topic to see bus activity frequency

### Not Detecting External DALI Commands

If commands from other DALI masters (e.g., Zennio, wall switches) aren't being detected:

1. Verify RX pin connection (GPIO14)
2. Check DALI bus is properly connected (both TX and RX lines)
3. Ensure DALI transceiver circuit supports bidirectional communication
4. Monitor serial output for `[DALI] Bus activity detected` messages
5. Check MQTT monitor topic for messages with `"source": "bus"`
6. Verify Messages RX counter is incrementing in Diagnostics page

### Web Interface Not Loading

1. Check device IP address (serial output or router)
2. Try accessing via AP mode (192.168.4.1)
3. Clear browser cache
4. Check if password protection is enabled

### OTA Update Fails

1. Ensure stable WiFi connection
2. Don't disconnect power during update
3. Use correct .bin file (merged binary)
4. Check available flash space (need ~1.2MB free)

### Serial Output Garbled

1. Set baud rate to 115200
2. Enable DEBUG_SERIAL in config.h
3. Some DALI library output may use `\r` causing overwriting

**Serial Monitoring Tools:**
```bash
# Using picocom (recommended for clean output)
picocom -b 115200 /dev/ttyACM0
# Exit: Ctrl+A then Ctrl+X

# Using screen
screen /dev/ttyACM0 115200
# Exit: Ctrl+A then K, then Y

# Using Arduino CLI
arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200

# Install picocom:
# macOS: brew install picocom
# Linux: sudo apt install picocom
```

### Reset to Factory Defaults

```bash
# Erase all flash (WiFi, MQTT, passwords)
esptool.py --chip esp32s3 --port /dev/ttyACM0 erase_flash

# Re-flash firmware
esptool.py --chip esp32s3 --port /dev/ttyACM0 --baud 460800 \
  write_flash 0x0 build/esp32_dali_bridge.ino.merged.bin
```

## 📊 Performance

- **Command Queue:** 50 commands
- **DALI Interval:** 50ms between commands
- **Bus Idle Detection:** 100ms timeout
- **Bus Arbitration:** Automatic collision avoidance
- **MQTT Buffer:** 512 bytes
- **Recent Messages:** Last 20 stored
- **Scan Speed:** ~3-4 seconds for 64 addresses (longer on busy buses)
- **Commissioning:** Binary search algorithm, ~5-10 seconds per device
- **Watchdog:** 30 second timeout

## 🔒 Security Notes

- Web interface authentication uses HTTP Basic Auth (not encrypted without HTTPS)
- Default username is `admin` (configurable via web interface)
- Web and MQTT credentials stored in NVS (plain text, not in source code)
- Consider using VPN or isolated network for production
- Change default AP password in config.h before deployment
- WiFi and web authentication credentials are stored separately
- No sensitive data in source code - credentials entered via web UI

## 📝 Version Information

Current version: **1.0.0**
Build date: Embedded in firmware

Check version:
- Web interface: Home page
- Serial output: On boot
- Diagnostics page: System Information

## 🤝 Contributing

This is a personal project, but suggestions and bug reports are welcome.

## 📄 License

MIT License - See LICENSE file for details

## 🙏 Acknowledgments

- DALI protocol implementation based on DALI_Lib
- Web interface uses modern CSS with CSS variables for theming
- MQTT integration via PubSubClient library
- Bus arbitration implements collision avoidance for multi-master environments
- Commissioning follows IEC 62386-102 DALI standard

---

**Built with ❤️ for home automation and lighting control**
