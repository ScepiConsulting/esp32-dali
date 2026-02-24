# ESP32 DALI Projects

ESP32-S3 based projects for DALI (Digital Addressable Lighting Interface) control, built on the modular `esp32-base` architecture.

## ⚠️ Disclaimer

> **This repository was created with extensive use of AI assistance (Claude/Cascade).**
> 
> The code may contain bugs, suboptimal implementations, or redundant code.
> **Created for internal use only.** Use and modify for non-commercial purposes at your own risk.
> Contributions and bug reports are welcome!

## 📥 Repository Setup

### Cloning the Repository

This repository uses **git submodules**. Clone with submodules:

```bash
git clone --recurse-submodules git@github.com:ScepiConsulting/esp32-dali.git
cd esp32-dali
```

If you already cloned without submodules:

```bash
git submodule update --init --recursive
```

### Updating the Submodule

To update `esp32-base` to the latest version from upstream:

```bash
# Fetch and update to latest commit on main branch
git submodule update --remote esp32-base

# Commit the submodule update
git add esp32-base
git commit -m "Update esp32-base to latest version"
```

To update to a specific tag or commit:

```bash
cd esp32-base
git fetch --tags
git checkout v1.2.0   # or a specific commit hash
cd ..
git add esp32-base
git commit -m "Update esp32-base to v1.2.0"
```

### Submodule Structure

The `esp32-base` directory is a git submodule containing shared base functionality. The `base_*` files and `.ino` files in each project folder are **symlinks** pointing to `esp32-base/src/`.

```
esp32-dali/
├── esp32-base/                    # Git submodule (DO NOT MODIFY)
│   └── src/
│       ├── base_*.cpp/h           # Shared base files
│       └── src.ino                # Main sketch template
├── esp32_dali_bridge/
│   ├── base_*.cpp/h → ../esp32-base/src/   # Symlinks
│   ├── esp32_dali_bridge.ino → ../esp32-base/src/src.ino
│   └── project_*.cpp/h            # Project-specific files
└── esp32_dali_ballast/
    ├── base_*.cpp/h → ../esp32-base/src/   # Symlinks
    ├── esp32_dali_ballast.ino → ../esp32-base/src/src.ino
    └── project_*.cpp/h            # Project-specific files
```

---

## 🏗️ Architecture

Both projects are built on the **esp32-base** modular architecture:

- **Base files** (`base_*.cpp/h`): Core functionality (WiFi, Web, MQTT, OTA, Diagnostics) - shared, not modified
- **Project files** (`project_*.cpp/h`): Project-specific customizations including DALI handlers

This architecture allows easy maintenance and consistent features across projects.

```
esp32_dali_bridge/              esp32_dali_ballast/
├── esp32_dali_bridge.ino       ├── esp32_dali_ballast.ino
│                               │
│ ─── Configuration ───         │ ─── Configuration ───
├── base_config.h               ├── base_config.h
├── project_version.h           ├── project_version.h
├── project_config.h            ├── project_config.h
│                               │
│ ─── Base Files (shared) ───   │ ─── Base Files (shared) ───
├── base_wifi.cpp/h             ├── base_wifi.cpp/h
├── base_web.cpp/h              ├── base_web.cpp/h
├── base_mqtt.cpp/h             ├── base_mqtt.cpp/h
├── base_diagnostics.cpp/h      ├── base_diagnostics.cpp/h
├── base_ota.cpp/h              ├── base_ota.cpp/h
├── base_logos.h                ├── base_logos.h
│                               │
│ ─── Project Files ───         │ ─── Project Files ───
├── project_function.cpp/h      ├── project_function.cpp/h
├── project_home.cpp/h          ├── project_home.cpp/h
├── project_diagnostics.cpp/h   ├── project_diagnostics.cpp/h
├── project_mqtt.cpp/h          ├── project_mqtt.cpp/h
├── project_dali_handler.cpp/h  ├── project_ballast_handler.cpp/h
├── project_dali_protocol.h     ├── project_ballast_state.h
└── project_dali_lib.cpp/h      ├── project_dali_protocol.h
                                └── project_dali_lib.cpp/h
```

## 📁 Projects

### 🎛️ ESP32 DALI Bridge (`esp32_dali_bridge/`)

A DALI master/controller that bridges DALI and MQTT.

**Features:**
- DALI bus communication and device control
- Bus monitoring (detects external DALI masters)
- Passive device discovery from bus traffic
- MQTT integration with command/monitor/scan topics
- Device scanning and commissioning
- Command queue with priority and retry
- Modern web interface with dark/light themes
- OTA updates and diagnostics

**MQTT Topics:**
| Topic | Direction | Description |
|-------|-----------|-------------|
| `home/dali/command` | Subscribe | Send DALI commands (JSON) |
| `home/dali/monitor` | Publish | All bus activity with source |
| `home/dali/status` | Publish | Device online status |
| `home/dali/scan/trigger` | Subscribe | Trigger bus scan |
| `home/dali/scan/result` | Publish | Scan results |
| `home/dali/commission/trigger` | Subscribe | Start commissioning |
| `home/dali/commission/progress` | Publish | Commissioning progress |

**Command Example:**
```json
{"command": "set_brightness", "address": 0, "level": 128}
```

---

### 💡 ESP32 DALI Ballast (`esp32_dali_ballast/`)

A virtual DALI ballast emulator (DALI slave device).

**Features:**
- Emulates DALI ballast behavior
- DALI-2 compliant responses (2.91-22ms timing)
- DT0 (Normal), DT6 (LED), DT8 (Color) device types
- RGB, RGBW, Color Temperature support (DT8)
- Configurable address (0-63) or unaddressed
- Automatic commissioning support
- Fade and scene support
- MQTT state publishing
- Web interface for configuration

**MQTT Topics:**
| Topic | Direction | Description |
|-------|-----------|-------------|
| `home/dali/ballast/state` | Publish | Current ballast state |
| `home/dali/ballast/config` | Publish | Ballast configuration |
| `home/dali/ballast/command` | Publish | Received DALI commands |
| `home/dali/ballast/status` | Publish | Online status (LWT) |

---

## 🔧 Hardware

### Recommended Setup
- [Waveshare ESP32-S3-Pico](https://www.waveshare.com/esp32-s3-pico.htm)
- [Waveshare Pico-DALI2](https://www.waveshare.com/pico-dali2.htm) transceiver

### Pin Connections
| Pin | Function |
|-----|----------|
| GPIO17 | DALI TX |
| GPIO14 | DALI RX |
| GPIO21 | WS2812 LED (Ballast only) |

### Supported ESP32 Variants

The code is compatible with all ESP32 variants. Adjust the build command and pin configuration as needed.

| Variant | FQBN | Default LED Pin | Flash Size |
|---------|------|-----------------|------------|
| **ESP32** | `esp32:esp32:esp32` | GPIO 2 | 4MB |
| **ESP32-S2** | `esp32:esp32:esp32s2` | GPIO 18 | 4MB |
| **ESP32-S3** | `esp32:esp32:esp32s3` | GPIO 48 (RGB) | 8MB+ |
| **ESP32-C3** | `esp32:esp32:esp32c3` | GPIO 8 | 4MB |
| **ESP32-C6** | `esp32:esp32:esp32c6` | GPIO 8 | 4MB |

### Detecting Your Board

Connect your ESP32 via USB and run:
```bash
esptool chip_id
```

Example output:
```
Chip type:          ESP32-S3 (revision v0.2)
Features:           WiFi, BLE, Embedded Flash 8MB
```

---

## � Requirements

### Software
- [Arduino IDE](https://www.arduino.cc/en/software) 2.x or [Arduino CLI](https://arduino.github.io/arduino-cli/)
- ESP32 Board Support Package (v3.x)
- PubSubClient library
- ArduinoJson library

### Installing Dependencies

**Arduino IDE:**
1. File → Preferences → Additional Board Manager URLs:
   ```
   https://espressif.github.io/arduino-esp32/package_esp32_index.json
   ```
2. Tools → Board → Boards Manager → Search "esp32" → Install
3. Sketch → Include Library → Manage Libraries:
   - Search "PubSubClient" → Install
   - Search "ArduinoJson" → Install

**Arduino CLI:**
```bash
# Install Arduino CLI (macOS)
brew install arduino-cli

# Or download from https://arduino.github.io/arduino-cli/

# Initialize config
arduino-cli config init

# Install ESP32 core
arduino-cli core update-index
arduino-cli core install esp32:esp32

# Install libraries
arduino-cli lib install "PubSubClient@2.8"
arduino-cli lib install "ArduinoJson@7.4.2"
```

---

## 🔨 Building

All commands should be run from the repository root folder.

We use the `min_spiffs` partition scheme which provides 1.875 MB per app partition (vs 1.25 MB default) while keeping OTA functionality.

### Using Makefile (Recommended)

The repository includes a `Makefile` that handles submodule reset and compilation.

**⚠️ Important:** Before every build, the submodule is automatically reset to discard any accidental changes:
```bash
git submodule foreach git reset --hard HEAD
git submodule foreach git checkout .
```

#### Quick Start

```bash
# Build both projects for ESP32-S3 (default)
make

# Build specific project
make bridge
make ballast

# Build for different board
make bridge BOARD=esp32c3
make ballast BOARD=esp32

# Flash to device
make flash-bridge PORT=/dev/cu.usbserial-0001
make flash-ballast PORT=/dev/cu.usbserial-0001

# Flash app only (faster, for updates)
make flash-bridge-app PORT=/dev/cu.usbserial-0001
```

#### Available Make Targets

| Target | Description |
|--------|-------------|
| `make` or `make all` | Build both bridge and ballast |
| `make bridge` | Build DALI Bridge only |
| `make ballast` | Build DALI Ballast only |
| `make flash-bridge` | Flash bridge (full, with bootloader) |
| `make flash-ballast` | Flash ballast (full, with bootloader) |
| `make flash-bridge-app` | Flash bridge app only (faster) |
| `make flash-ballast-app` | Flash ballast app only (faster) |
| `make submodule-reset` | Reset submodules to clean state |
| `make clean` | Remove build artifacts |
| `make help` | Show all options |

#### Make Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `BOARD` | `esp32s3` | Target: `esp32`, `esp32s2`, `esp32s3`, `esp32c3`, `esp32c6` |
| `PORT` | `/dev/ttyUSB0` | Serial port for flashing |
| `BUILD_DIR` | `./build` | Output directory |

### Manual Build Commands

<details>
<summary>Click to expand manual arduino-cli commands</summary>

#### DALI Bridge

**ESP32-S3 (recommended):**
```bash
arduino-cli compile -v --fqbn esp32:esp32:esp32s3:PartitionScheme=min_spiffs \
  --build-property "build.project_name=esp32_dali_bridge" \
  --output-dir ./build esp32_dali_bridge/
```

**ESP32:**
```bash
arduino-cli compile -v --fqbn esp32:esp32:esp32:PartitionScheme=min_spiffs \
  --build-property "build.project_name=esp32_dali_bridge" \
  --output-dir ./build esp32_dali_bridge/
```

**ESP32-C3:**
```bash
arduino-cli compile -v --fqbn esp32:esp32:esp32c3:PartitionScheme=min_spiffs \
  --build-property "build.project_name=esp32_dali_bridge" \
  --output-dir ./build esp32_dali_bridge/
```

**ESP32-C6:**
```bash
arduino-cli compile -v --fqbn esp32:esp32:esp32c6:PartitionScheme=min_spiffs \
  --build-property "build.project_name=esp32_dali_bridge" \
  --output-dir ./build esp32_dali_bridge/
```

**ESP32-S2:**
```bash
arduino-cli compile -v --fqbn esp32:esp32:esp32s2:PartitionScheme=min_spiffs \
  --build-property "build.project_name=esp32_dali_bridge" \
  --output-dir ./build esp32_dali_bridge/
```

#### DALI Ballast

**ESP32-S3 (recommended):**
```bash
arduino-cli compile -v --fqbn esp32:esp32:esp32s3:PartitionScheme=min_spiffs \
  --build-property "build.project_name=esp32_dali_ballast" \
  --output-dir ./build esp32_dali_ballast/
```

**ESP32:**
```bash
arduino-cli compile -v --fqbn esp32:esp32:esp32:PartitionScheme=min_spiffs \
  --build-property "build.project_name=esp32_dali_ballast" \
  --output-dir ./build esp32_dali_ballast/
```

**ESP32-C3:**
```bash
arduino-cli compile -v --fqbn esp32:esp32:esp32c3:PartitionScheme=min_spiffs \
  --build-property "build.project_name=esp32_dali_ballast" \
  --output-dir ./build esp32_dali_ballast/
```

**ESP32-C6:**
```bash
arduino-cli compile -v --fqbn esp32:esp32:esp32c6:PartitionScheme=min_spiffs \
  --build-property "build.project_name=esp32_dali_ballast" \
  --output-dir ./build esp32_dali_ballast/
```

**ESP32-S2:**
```bash
arduino-cli compile -v --fqbn esp32:esp32:esp32s2:PartitionScheme=min_spiffs \
  --build-property "build.project_name=esp32_dali_ballast" \
  --output-dir ./build esp32_dali_ballast/
```

</details>

### Build Output

After building, you'll find these files in the output directory:
- `esp32_dali_bridge.bin` / `esp32_dali_ballast.bin` - Main firmware
- `esp32_dali_bridge.bootloader.bin` / `esp32_dali_ballast.bootloader.bin` - Bootloader
- `esp32_dali_bridge.partitions.bin` / `esp32_dali_ballast.partitions.bin` - Partition table
- `esp32_dali_bridge.merged.bin` / `esp32_dali_ballast.merged.bin` - Combined binary for first flash

### Arduino IDE

1. Open `esp32_dali_bridge/esp32_dali_bridge.ino` or `esp32_dali_ballast/esp32_dali_ballast.ino`
2. Select board: Tools → Board → ESP32 Arduino → (your board variant)
3. Select partition scheme: Tools → Partition Scheme → **Minimal SPIFFS (1.9MB APP with OTA/190KB SPIFFS)**
4. Sketch → Export Compiled Binary

---

## 📤 Flashing

Replace `/dev/ttyUSB0` with your actual port:
- **macOS**: `/dev/cu.usbserial-*` or `/dev/cu.SLAB_USBtoUART`
- **Windows**: `COM3`, `COM4`, etc.
- **Linux**: `/dev/ttyUSB0`, `/dev/ttyACM0`

### First-time Flash (full)

Uses the merged binary which contains bootloader, partition table, and firmware.

**DALI Bridge - ESP32-S3:**
```bash
esptool --chip esp32s3 --port /dev/ttyUSB0 --baud 460800 \
  --before default_reset --after hard_reset write_flash \
  -z --flash_mode dio --flash_freq 80m --flash_size 8MB \
  0x0 ./build/esp32_dali_bridge.merged.bin
```

**DALI Bridge - ESP32:**
```bash
esptool --chip esp32 --port /dev/ttyUSB0 --baud 460800 \
  --before default_reset --after hard_reset write_flash \
  -z --flash_mode dio --flash_freq 80m --flash_size 4MB \
  0x0 ./build/esp32_dali_bridge.merged.bin
```

**DALI Ballast - ESP32-S3:**
```bash
esptool --chip esp32s3 --port /dev/ttyUSB0 --baud 460800 \
  --before default_reset --after hard_reset write_flash \
  -z --flash_mode dio --flash_freq 80m --flash_size 8MB \
  0x0 ./build/esp32_dali_ballast.merged.bin
```

**DALI Ballast - ESP32:**
```bash
esptool --chip esp32 --port /dev/ttyUSB0 --baud 460800 \
  --before default_reset --after hard_reset write_flash \
  -z --flash_mode dio --flash_freq 80m --flash_size 4MB \
  0x0 ./build/esp32_dali_ballast.merged.bin
```

### Firmware Update (app only)

For subsequent updates, flash only the app (~1.5MB, faster):

**DALI Bridge:**
```bash
esptool --chip esp32s3 --port /dev/ttyUSB0 --baud 460800 \
  write_flash 0x10000 ./build/esp32_dali_bridge.bin
```

**DALI Ballast:**
```bash
esptool --chip esp32s3 --port /dev/ttyUSB0 --baud 460800 \
  write_flash 0x10000 ./build/esp32_dali_ballast.bin
```

### Troubleshooting Flash Issues

If flashing fails:

1. **Lower the baud rate** - Try `--baud 230400` or `--baud 115200`
2. **Replace the USB cable** - Use a quality data cable, not a charge-only cable
3. **Hold BOOT button** - On some boards, hold BOOT while flashing starts
4. **Check the port** - Ensure correct port is specified
5. **Install drivers** - Some boards need CH340 or CP2102 USB drivers

---

## 🚀 First-time Setup

1. Connect ESP32 via USB
2. Build and flash using commands above
3. On first boot, the device creates an AP:
   - **Bridge:** `SCEPI-DALI-BRIDGE-XXXXXX`
   - **Ballast:** `SCEPI-DALI-BALLAST-XXXXXX`
4. Connect to the AP (password: `daliconfig`)
5. Navigate to `http://192.168.4.1` to configure WiFi

### OTA Updates (after initial setup)

1. Navigate to `http://<device-ip>/update`
2. Select the compiled `.bin` file
3. Click "Upload & Update"
4. Wait for completion and automatic reboot

---

## 🌐 Web Interface

| Page | URL | Description |
|------|-----|-------------|
| Home | `/` | System status overview |
| Network | `/network` | WiFi and web auth settings |
| MQTT | `/mqtt` | MQTT broker configuration |
| DALI Control / Ballast Config | `/dali` or `/ballast` | Project-specific controls |
| Diagnostics | `/diagnostics` | System info and stats |
| Update | `/update` | Firmware OTA update |

---

## 📊 Project Comparison

| Feature | DALI Bridge | DALI Ballast |
|---------|-------------|--------------|
| **Role** | DALI Master | DALI Slave |
| **Sends Commands** | ✅ | ❌ |
| **Responds to Queries** | ❌ | ✅ |
| **Bus Monitoring** | ✅ | ✅ |
| **MQTT** | Pub/Sub | Publish only |
| **Device Scanning** | ✅ | ❌ |
| **Commissioning** | ✅ Initiates | ✅ Responds |
| **Configurable Address** | N/A | ✅ (0-63) |

---

## 🎯 Typical Setup

```
┌─────────────────┐
│  MQTT Broker    │
└────────┬────────┘
         │
    ┌────┴────┐
    │         │
┌───▼──┐  ┌──▼───┐
│Bridge│  │Home  │
│      │  │Assist│
└───┬──┘  └──────┘
    │
DALI Bus ════════════════════
    │         │         │
┌───▼───┐ ┌───▼───┐ ┌───▼───┐
│Ballast│ │ Real  │ │ Real  │
│(ESP32)│ │Ballast│ │Ballast│
└───────┘ └───────┘ └───────┘
```

---

## 💻 Development Tips

- Enable `DEBUG_SERIAL` in `base_config.h` for serial debug output
- Use `mqttSubscribe()` and `mqttPublish()` helpers from `base_mqtt.h`
- Implement `onMqttConnected()` to subscribe to your topics
- Implement `onMqttMessage()` to handle incoming messages
- Data is only saved on explicit user action (no periodic SPIFFS writes)

### AI Coding Agent Guidelines

When working with this codebase, AI coding agents **MUST** follow these rules:

#### ⛔ DO NOT MODIFY (Read-Only)

These files are symlinks to the `esp32-base` submodule and must **NEVER** be modified:

**In `esp32_dali_bridge/`:**
- `base_config.h`, `base_diagnostics.cpp`, `base_diagnostics.h`, `base_logos.h`
- `base_mqtt.cpp`, `base_mqtt.h`, `base_ota.cpp`, `base_ota.h`
- `base_web.cpp`, `base_web.h`, `base_wifi.cpp`, `base_wifi.h`
- `esp32_dali_bridge.ino`

**In `esp32_dali_ballast/`:**
- `base_config.h`, `base_diagnostics.cpp`, `base_diagnostics.h`, `base_logos.h`
- `base_mqtt.cpp`, `base_mqtt.h`, `base_ota.cpp`, `base_ota.h`
- `base_web.cpp`, `base_web.h`, `base_wifi.cpp`, `base_wifi.h`
- `esp32_dali_ballast.ino`

**In `esp32-base/` (entire submodule):**
- Do not modify any files in this directory

#### ✅ CAN MODIFY

Only `project_*` files can be modified or created:
- `project_config.h`, `project_version.h`
- `project_function.cpp/h`, `project_home.cpp/h`
- `project_diagnostics.cpp/h`, `project_mqtt.cpp/h`
- `project_dali_*.cpp/h`, `project_ballast_*.cpp/h`

#### Summary Table

| File Pattern | Can Modify? | Can Create? |
|--------------|-------------|-------------|
| `esp32-base/*` | ❌ No | ❌ No |
| `base_*` | ❌ No | ❌ No |
| `*.ino` | ❌ No | ❌ No |
| `project_*` | ✅ Yes | ✅ Yes |

---

## 🔒 Security

- Web interface uses HTTP Basic Auth
- Credentials stored in NVS (not in source code)
- Default AP password: `daliconfig` - **change before deployment**
- Consider VPN or isolated network for production

---

## 📝 Version

- **DALI Bridge:** v1.0.1
- **DALI Ballast:** v1.0.1

---

## 🙏 Acknowledgments

- **[python-dali](https://github.com/sde1000/python-dali/)** - Protocol reference
- **[DALI Lighting Protocol](https://jared.geek.nz/2025/06/dali-lighting-protocol/)** - Excellent documentation
- Projects follow IEC 62386 DALI standard

---

## 📄 License

**CC BY-NC-SA 4.0** (Creative Commons Attribution-NonCommercial-ShareAlike 4.0)

- ✅ Share and adapt for non-commercial use
- ✅ Attribution required
- ✅ Share alike under same license

---

**© 2026 Scepi Consulting Kft.**
