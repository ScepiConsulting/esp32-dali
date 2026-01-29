# ESP32 DALI Ballast

A virtual DALI ballast emulator that appears on the bus as a controllable DALI device, built on ESP32-S3 with a modern web interface.

## 🌟 Features

- **DALI Ballast Emulation** - Appears as a standard DALI slave device on the bus
- **Query Response** - Responds to queries within 22ms (status, level, etc.)
- **Response Queueing** - Intelligent response queue with bus arbitration
- **Bus Arbitration** - Waits for bus idle before transmitting responses
- **Command Processing** - Handles all standard DALI commands (set level, fade, scenes)
- **Automatic Commissioning** - Supports DALI commissioning protocol
- **Configurable Address** - Set unique short address (0-63) or auto-assign
- **MQTT State Publishing** - Publishes state changes and received commands
- **Web Interface** - Modern, responsive UI with dark/light theme
- **Authentication** - Configurable username/password for web interface
- **OTA Updates** - Over-the-air firmware updates via web interface
- **WiFi Management** - STA and AP modes with automatic fallback
- **Fade Support** - Configurable fade time/rate (16 steps)
- **Scene Storage** - Store and recall scenes 0-15
- **Diagnostics** - Real-time system monitoring and statistics

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
- **DALI Master** - Zennio, Helvar, Tridonic, or other DALI controller

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
   - SSID: `ESP32-DALI-Ballast`
   - Password: `daliconfig`
   - Navigate to: `http://192.168.4.1`

3. **Configure WiFi**
   - Enter your WiFi credentials in "WiFi Setup" section
   - Click "Save WiFi Settings"
   - (Optional) Set web interface username/password in "Web Interface Security" section
   - Click "Save Settings" (separate button, no reboot)

4. **Device reboots** after WiFi save and connects to your network

5. **Configure Ballast**
   - Navigate to **Ballast** page
   - Set unique short address (0-63)
   - Configure min/max levels, fade time/rate
   - Click "Save Configuration"

6. **Connect to DALI bus**
   - Wire ballast to DALI bus alongside real devices
   - Zennio/Helvar controller will detect it as a standard DALI ballast

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
esp32_dali_ballast/
├── esp32_dali_ballast.ino   # Main sketch
├── config.h                 # Configuration constants
├── version.h                # Version information
├── ballast_handler.cpp/h    # Ballast logic and state management
├── ballast_state.h          # Ballast state structure
├── mqtt_handler.cpp/h       # MQTT client logic
├── web_interface.cpp/h      # Web server and UI
├── diagnostics.cpp/h        # System diagnostics
├── utils.cpp/h              # Utility functions
├── logos.h                  # Embedded logo images
├── dali_protocol.h          # DALI data structures
└── DALI_Lib.cpp/h           # Low-level DALI library
```

### Source Files Reference

| File | Purpose | Used By |
|------|---------|---------|
| `esp32_dali_ballast.ino` | Main sketch entry point | Arduino build system |
| `config.h` | Configuration constants (pins, credentials, timing) | All modules |
| `version.h` | Firmware version information | utils.cpp, web_interface.cpp |
| `ballast_state.h` | Ballast state structure and message format | ballast_handler.cpp |
| `ballast_handler.cpp/h` | Ballast logic, command processing, query responses | Main sketch, MQTT, web |
| `DALI_Lib.cpp/h` | Low-level DALI bus driver (Manchester encoding) | ballast_handler.cpp |
| `mqtt_handler.cpp/h` | MQTT client logic and state publishing | Main sketch, ballast handler |
| `web_interface.cpp/h` | Web server, HTML UI, and HTTP endpoints | Main sketch |
| `diagnostics.cpp/h` | System diagnostics and statistics tracking | All modules |
| `utils.cpp/h` | Utility functions (JSON parsing, hex conversion) | MQTT, web interface |
| `logos.h` | Embedded logo images in PROGMEM (light/dark theme) | web_interface.cpp |

## 🔨 Building

### Build from Command Line

```bash
# Navigate to project directory
cd esp32_dali_ballast

# Compile
arduino-cli compile --fqbn esp32:esp32:esp32s3 --output-dir build .

# Build output will be in build/esp32_dali_ballast.ino.merged.bin
```

### Build Configuration

Edit `esp32_dali_ballast/config.h` to customize:

```cpp
// Enable/disable serial debug logging
#define DEBUG_SERIAL

// DALI pins
#define DALI_TX_PIN 17
#define DALI_RX_PIN 14

// AP mode credentials
#define AP_SSID "ESP32-DALI-Ballast"
#define AP_PASSWORD "daliconfig"

// Query response timeout (critical!)
#define QUERY_RESPONSE_TIMEOUT_MS 22

// Response queue and bus arbitration
#define RESPONSE_QUEUE_SIZE 10
#define BUS_IDLE_TIMEOUT_MS 100
#define MAX_RESPONSE_RETRIES 3
```

### Build Size

Current build statistics:
- **Flash:** ~1.10 MB
- **RAM:** ~48 KB

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
  write_flash 0x0 build/esp32_dali_ballast.ino.merged.bin

# Monitor serial output (optional)
arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200

# Or using picocom
picocom -b 115200 /dev/ttyACM0
# Exit picocom: Ctrl+A then Ctrl+X
```

### Using Arduino IDE

1. Open `esp32_dali_ballast/esp32_dali_ballast.ino`
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
1. Connect to `ESP32-DALI-Ballast` AP
2. Navigate to WiFi page
3. Enter SSID and password in "WiFi Setup" section
4. Click "Save WiFi Settings" (device will reboot)
5. (Optional) Set web interface username/password in "Web Interface Security" section
6. Click "Save Settings" (no reboot, takes effect immediately)

**Via Serial (if needed):**
- WiFi credentials are stored in NVS (Non-Volatile Storage)
- To reset: erase flash with `esptool.py erase_flash`

### Ballast Configuration

Navigate to **Ballast** page in web interface:

| Setting | Description | Default | Range |
|---------|-------------|---------|-------|
| Short Address | Ballast address on DALI bus | 0 | 0-63, 255=unaddressed |
| Address Mode | Auto (commissioning) or Manual | Auto | Auto/Manual |
| Min Level | Minimum brightness level | 1 | 0-254 |
| Max Level | Maximum brightness level | 254 | 0-254 |
| Power-On Level | Level on startup | 254 | 0-254 |
| Fade Time | Fade duration (0=instant) | 0 | 0-15 |
| Fade Rate | Fade rate | 7 | 0-15 |

**Important:** 
- Short address must be unique on the DALI bus!
- Set to 255 (unaddressed) to participate in commissioning
- Auto mode enables automatic address assignment via DALI commissioning

### MQTT Configuration

Navigate to **MQTT** page in web interface:

| Setting | Description | Default |
|---------|-------------|---------|
| Broker | MQTT broker hostname/IP | - |
| Port | MQTT broker port | 1883 |
| Username | MQTT username (optional) | - |
| Password | MQTT password (optional) | - |
| Topic Prefix | Base topic for all messages | `home/dali/ballast/` |
| Client ID | MQTT client identifier | `dali-ballast-XXXXXX` |
| QoS | Quality of Service (0-2) | 0 |

**Client ID** is auto-generated from MAC address: `dali-ballast-` + last 6 hex digits

**Note:** Changing MQTT settings triggers a device reboot to apply new configuration.

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

## 📖 Usage

### Web Interface

Access the web interface at `http://<device-ip>`

**Pages:**
- **Home** - System status and MQTT topic examples
- **WiFi** - WiFi and password configuration
- **MQTT** - MQTT broker settings
- **Ballast** - Ballast configuration and status
- **Diagnostics** - System information and statistics
- **Update** - OTA firmware updates

### Ballast Operation

The ballast automatically responds to DALI commands from external masters:

**Supported Commands:**
- Direct Arc Power (set brightness 0-254)
- OFF
- UP / DOWN (fade up/down)
- STEP_UP / STEP_DOWN
- RECALL_MAX_LEVEL / RECALL_MIN_LEVEL
- GO_TO_SCENE (0-15)
- RESET

**Supported Queries:**
- QUERY_STATUS
- QUERY_CONTROL_GEAR
- QUERY_LAMP_FAILURE
- QUERY_LAMP_POWER_ON
- QUERY_ACTUAL_LEVEL
- QUERY_MAX_LEVEL / QUERY_MIN_LEVEL
- QUERY_POWER_ON_LEVEL
- QUERY_FADE_TIME_RATE
- QUERY_SCENE_LEVEL (0-15)

**Response Timing:**
- Query responses queued and sent when bus is idle
- Bus arbitration prevents collisions with other devices
- Target response time < 22ms (DALI requirement)
- 8-bit backward frames (standard DALI response format)
- Automatic retry on transmission failure (up to 3 attempts)

**Commissioning Support:**
- INITIALISE - Enter commissioning mode
- RANDOMISE - Generate random address
- COMPARE - Binary search response
- WITHDRAW - Remove from search
- PROGRAM_SHORT_ADDRESS - Accept assigned address
- VERIFY_SHORT_ADDRESS - Confirm address
- QUERY_SHORT_ADDRESS - Report assigned address

### MQTT Monitoring

All ballast activity is published to MQTT:

**State Changes:**
- Published to `{prefix}/state` whenever level changes
- Includes current level, target level, fade status

**Received Commands:**
- Published to `{prefix}/command` for all received DALI commands
- Includes command type, raw bytes, description

**Configuration:**
- Published to `{prefix}/config` when ballast settings change

## 📡 MQTT Topics

### State Topic
**Topic:** `{prefix}/state`
**Direction:** Publish (on level change)

```json
{
  "timestamp": 1234567890,
  "address": 5,
  "actual_level": 128,
  "level_percent": 50.4,
  "target_level": 128,
  "fade_running": false,
  "lamp_arc_power_on": true,
  "lamp_failure": false
}
```

### Command Topic
**Topic:** `{prefix}/command`
**Direction:** Publish (received commands)

```json
{
  "timestamp": 1234567890,
  "source": "bus",
  "command_type": "set_brightness",
  "address": 5,
  "value": 128,
  "value_percent": 50.4,
  "raw": "0B80",
  "description": "Set to 128 (50.4%)"
}
```

**Query Response Example:**
```json
{
  "timestamp": 1234567891,
  "source": "bus",
  "command_type": "query_actual_level",
  "address": 5,
  "is_query_response": true,
  "response": "0x80",
  "value": 128,
  "value_percent": 50.4,
  "raw": "0BA0",
  "description": "Query actual level: 128 (50.4%)"
}
```

### Config Topic
**Topic:** `{prefix}/config`
**Direction:** Publish (on config change)

```json
{
  "short_address": 5,
  "min_level": 1,
  "max_level": 254,
  "power_on_level": 254,
  "fade_time": 0,
  "fade_rate": 7
}
```

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

### Diagnostics Topic (Optional)
**Topic:** `{prefix}/diagnostics`
**Direction:** Publish (periodic, if enabled)

Published every 60 seconds when diagnostics are enabled in settings.

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

### DALI Master Can't Find Ballast

1. Verify ballast address is configured (0-63)
2. Check DALI wiring (TX: GPIO17, RX: GPIO14)
3. Ensure DALI bus power is present (~16V)
4. Verify proper isolation circuit
5. Check serial output for `[DALI] Received:` messages
6. Try different address to rule out conflicts

### Ballast Not Responding to Commands

1. Verify address matches what master is sending to
2. Check serial output shows received commands
3. Ensure ballast is powered on
4. Check DALI RX pin connection (GPIO14)
5. Monitor MQTT command topic for received messages

### Query Responses Not Working

1. Check serial output for response timing
2. Verify DALI TX pin connection (GPIO17)
3. Check if responses are being queued (serial output)
4. Monitor for "Waiting for bus idle" messages
5. On very busy buses, responses may be delayed waiting for idle period
6. Try power cycle of ballast
7. Check for DALI bus collisions
8. Verify response queue isn't full (serial output shows "Response queue full")

### Responses Delayed on Busy Bus

If responses are slow on a busy DALI bus:

1. This is normal behavior - bus arbitration waits for idle periods
2. Check serial output for "Waiting for bus idle to send response" messages
3. Bus waits for 100ms of silence before transmitting
4. Responses are queued and sent in order
5. Failed responses are automatically retried (up to 3 times)
6. If bus is constantly busy, consider reducing other master's polling rate

### Web Interface Not Loading

1. Check device IP address (serial output or router)
2. Try accessing via AP mode (192.168.4.1)
3. Clear browser cache
4. Check if password protection is enabled

### OTA Update Fails

1. Ensure stable WiFi connection
2. Don't disconnect power during update
3. Use correct .bin file (merged binary)
4. Check available flash space

### Serial Output Garbled

1. Set baud rate to 115200
2. Enable DEBUG_SERIAL in config.h

**Serial Monitoring Tools:**
```bash
# Using picocom (recommended)
picocom -b 115200 /dev/ttyACM0
# Exit: Ctrl+A then Ctrl+X

# Using screen
screen /dev/ttyACM0 115200
# Exit: Ctrl+A then K, then Y

# Using Arduino CLI
arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200
```

### Reset to Factory Defaults

```bash
# Erase all flash (WiFi, MQTT, passwords, ballast config)
esptool.py --chip esp32s3 --port /dev/ttyACM0 erase_flash

# Re-flash firmware
esptool.py --chip esp32s3 --port /dev/ttyACM0 --baud 460800 \
  write_flash 0x0 build/esp32_dali_ballast.ino.merged.bin
```

## 📊 Performance

- **Query Response:** < 22ms target (queued with bus arbitration)
- **Response Queue:** 10 responses
- **Bus Idle Detection:** 100ms timeout
- **Response Retries:** Up to 3 attempts on failure
- **Backward Frame:** 8-bit response
- **MQTT Buffer:** 512 bytes
- **Recent Messages:** Last 20 stored
- **Fade Update:** 50ms intervals
- **Watchdog:** 30 second timeout

## 🔒 Security Notes

- Web interface authentication uses HTTP Basic Auth (not encrypted without HTTPS)
- Default username is `admin` (configurable via web interface)
- Ballast config and credentials stored in NVS (plain text)
- Consider using VPN or isolated network for production
- Change default AP password in config.h before deployment

## 📝 Version Information

Current version: **1.0.0**
Build date: Embedded in firmware

Check version:
- Web interface: Home page
- Serial output: On boot
- Diagnostics page: System Information

## 🎯 Use Cases

1. **Testing DALI Controllers** - Test Zennio/Helvar/Tridonic without physical ballasts
2. **Bus Monitoring** - Log all DALI commands to MQTT for analysis
3. **Home Automation** - Bridge DALI to Home Assistant via MQTT
4. **Development** - Test DALI master software safely
5. **Training** - Learn DALI protocol hands-on
6. **Simulation** - Simulate multiple ballasts with multiple ESP32s
7. **Commissioning Testing** - Test automatic address assignment
8. **Multi-Master Environments** - Works alongside other DALI masters with bus arbitration

## 🤝 Contributing

This is a personal project, but suggestions and bug reports are welcome.

## 📄 License

MIT License - See LICENSE file for details

## 🙏 Acknowledgments

- DALI protocol implementation based on DALI_Lib
- Web interface uses modern CSS with CSS variables for theming
- MQTT integration via PubSubClient library
- Ballast emulation follows IEC 62386 DALI standard
- Bus arbitration implements collision avoidance for multi-master environments
- Commissioning protocol follows IEC 62386-102 specification

## 📚 Additional Documentation

See `ai_helpers/esp32_dali_ballast/` for detailed planning documentation:
- **IMPLEMENTATION_PLAN.md** - Complete technical specification
- **HARDWARE_SETUP.md** - Detailed hardware setup guide
- **DALI_BALLAST_PROTOCOL.md** - DALI protocol reference

---

**Built with ❤️ for home automation and lighting control**
