# ESP32 DALI Projects

ESP32-S3 based projects for DALI (Digital Addressable Lighting Interface) control, built on the modular `esp32-base` architecture.

## ⚠️ Disclaimer

> **This repository was created with extensive use of AI assistance (Claude/Cascade).**
> 
> The code may contain bugs, suboptimal implementations, or redundant code.
> **Created for internal use only.** Use and modify for non-commercial purposes at your own risk.
> Contributions and bug reports are welcome!

## 📥 Repository Setup

### Cloning the Repositories

The `esp32-base` framework is **no longer a git submodule**. It is consumed as a PlatformIO library from a **sibling checkout**, declared in `platformio.ini`:

```ini
lib_deps = symlink://../esp32-base
```

So both repositories must be cloned **side by side**:

```bash
git clone git@github.com:ScepiConsulting/esp32-base.git
git clone git@github.com:ScepiConsulting/esp32-dali.git
cd esp32-dali
```

To update the base, simply pull in the sibling checkout:

```bash
cd ../esp32-base && git pull
```

There is no submodule to initialize, update, or reset before building.

### Directory Layout

```
<workspace>/
├── esp32-base/                    # Sibling checkout (DO NOT MODIFY)
│   └── src/
│       ├── base_api.h             # app* hooks the projects implement
│       ├── base_app.cpp/h         # Owns setup() / loop()
│       └── base_*.cpp/h           # WiFi, Web, MQTT, OTA, Diagnostics, i18n
└── esp32-dali/
    ├── platformio.ini             # Two envs: bridge + ballast
    ├── Makefile                   # Thin wrapper around `pio`
    ├── esp32_dali_bridge/
    │   ├── main.cpp               # Calls baseSetup() / baseLoop()
    │   └── project_*.cpp/h        # Project-specific files
    └── esp32_dali_ballast/
        ├── main.cpp               # Calls baseSetup() / baseLoop()
        └── project_*.cpp/h        # Project-specific files
```

No symlinks, no `.ino` files: each product folder contains a plain `main.cpp` plus its own `project_*` sources.

---

## 🏗️ Architecture

Both projects are built on the **esp32-base** modular architecture:

- **Base library** (`esp32-base`, `base_*.cpp/h`): Core functionality (WiFi, Web, MQTT, OTA, Diagnostics, i18n) - shared, not modified. Pulled in by PlatformIO via `lib_deps`.
- **Project files** (`project_*.cpp/h`): Project-specific customizations including DALI handlers

This architecture allows easy maintenance and consistent features across projects.

### Entry Point

The base owns `setup()` and `loop()`. Each product's `main.cpp` is just:

```cpp
#include "base_app.h"

void setup() { baseSetup(); }
void loop()  { baseLoop(); }
```

### Hooks

Projects extend the base through the `app*` hooks declared in the base's `src/base_api.h`:

| Hook | Purpose |
|------|---------|
| `appInit()` | Project initialization |
| `appLoop()` | Project main loop work |
| `appHomeHTML()` | Project section on the home page |
| `appDiagnosticSections()` | Extra diagnostics UI sections |
| `appDiagnosticsJSON()` | Extra diagnostics JSON fields |
| `appMqttConnected()` | Subscribe to project topics |
| `appMqttMessage()` | Handle incoming MQTT messages |
| `appMqttTopicsHTML()` | Document project topics in the UI |
| `appMqttFilter*` (trio, optional) | Optional MQTT filtering |

Every hook has a **weak default** in the base, so a project only implements the ones it needs.

```
esp32_dali_bridge/              esp32_dali_ballast/
├── main.cpp                    ├── main.cpp
│                               │
│ ─── Configuration ───         │ ─── Configuration ───
├── project_version.h           ├── project_version.h
├── project_config.h            ├── project_config.h
│                               │
│ ─── Project Files ───         │ ─── Project Files ───
├── project_function.cpp/h      ├── project_function.cpp/h
├── project_home.cpp            ├── project_home.cpp
├── project_diagnostics.cpp     ├── project_diagnostics.cpp
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

### Target Board

Both PlatformIO environments build for the **ESP32-S3**:

| Setting | Value |
|---------|-------|
| `board` | `esp32-s3-devkitc-1` |
| Flash size | 4MB |
| Partitions | `min_spiffs.csv` |
| `monitor_speed` | 115200 |

The code itself is portable to other ESP32 variants, but that requires changing the `board` in `platformio.ini` and adjusting the pin configuration.

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
- [PlatformIO](https://platformio.org/) (`pio` CLI, or the PlatformIO IDE extension)
- The `esp32-base` repository, cloned as a **sibling directory** next to `esp32-dali`

Everything else - the Espressif32 platform, the Arduino framework, PubSubClient and ArduinoJson - is declared in `platformio.ini` and resolved automatically by PlatformIO on the first build. There is no manual library or core installation step.

### Installing PlatformIO

```bash
# macOS / Linux
brew install platformio

# Or via pip
pip install -U platformio
```

---

## 🔨 Building

All commands should be run from the repository root folder.

We use the `min_spiffs` partition scheme which provides 1.875 MB per app partition (vs 1.25 MB default) while keeping OTA functionality.

### Project Layout

`platformio.ini` defines **two environments in a single project**. `src_dir` is the repository root, and each environment compiles only its own product folder via `build_src_filter`:

| Environment | Product |
|-------------|---------|
| `bridge` | DALI Bridge (`esp32_dali_bridge/`) |
| `ballast` | DALI Ballast (`esp32_dali_ballast/`) |

### Using PlatformIO (Recommended)

```bash
# Build both products
pio run

# Build a single product
pio run -e bridge
pio run -e ballast

# Build + upload over serial
pio run -e bridge -t upload --upload-port /dev/cu.usbserial-0001

# Serial monitor (115200 baud)
pio device monitor
```

### Using the Makefile

The `Makefile` is now a thin wrapper that forwards to `pio`, so the familiar targets keep working:

| Target | Description |
|--------|-------------|
| `make` or `make all` | Build both bridge and ballast |
| `make bridge` | Build DALI Bridge only |
| `make ballast` | Build DALI Ballast only |
| `make flash-bridge PORT=...` | Build + upload the bridge |
| `make flash-ballast PORT=...` | Build + upload the ballast |
| `make monitor-bridge PORT=...` | Serial monitor |
| `make monitor-ballast PORT=...` | Serial monitor |
| `make clean` | Remove build artifacts |
| `make help` | Show all options |

| Variable | Default | Description |
|----------|---------|-------------|
| `PORT` | `/dev/ttyUSB0` | Serial port for flashing / monitoring |

### Build Output

PlatformIO writes each environment's artifacts to `.pio/build/<env>/`:
- `.pio/build/bridge/firmware.bin` - DALI Bridge firmware (also used for OTA)
- `.pio/build/ballast/firmware.bin` - DALI Ballast firmware (also used for OTA)
- `bootloader.bin` / `partitions.bin` - Bootloader and partition table

---

## 📤 Flashing

Replace `/dev/ttyUSB0` with your actual port:
- **macOS**: `/dev/cu.usbserial-*` or `/dev/cu.SLAB_USBtoUART`
- **Windows**: `COM3`, `COM4`, etc.
- **Linux**: `/dev/ttyUSB0`, `/dev/ttyACM0`

### Serial Flash

PlatformIO builds and uploads (bootloader, partition table and firmware) in one step:

**DALI Bridge:**
```bash
pio run -e bridge -t upload --upload-port /dev/ttyUSB0
# or: make flash-bridge PORT=/dev/ttyUSB0
```

**DALI Ballast:**
```bash
pio run -e ballast -t upload --upload-port /dev/ttyUSB0
# or: make flash-ballast PORT=/dev/ttyUSB0
```

If `--upload-port` is omitted, PlatformIO auto-detects the port.

### Firmware Update (app only)

For subsequent updates, use the OTA web interface (see below) with the built firmware:
- `.pio/build/bridge/firmware.bin`
- `.pio/build/ballast/firmware.bin`

### Troubleshooting Flash Issues

If flashing fails:

1. **Lower the baud rate** - Set `upload_speed = 115200` in `platformio.ini`
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

### 🌍 Language

The web interface is **bilingual**: **Hungarian is the default**, English can be switched from the header, and the choice is persisted in NVS. UI strings are wrapped with `tr("magyar", "english")`. Serial/debug logs remain English only.

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

- Enable `DEBUG_SERIAL` in the base's `base_config.h` for serial debug output
- Use `mqttSubscribe()` and `mqttPublish()` helpers from `base_mqtt.h`
- Implement `appMqttConnected()` to subscribe to your topics
- Implement `appMqttMessage()` to handle incoming messages
- All hooks (`appInit`, `appLoop`, `appHomeHTML`, ...) are declared in the base's `src/base_api.h` and have weak defaults - implement only what you need
- Wrap user-facing UI strings with `tr("magyar", "english")`; keep serial logs English
- Data is only saved on explicit user action (no periodic SPIFFS writes)

### AI Coding Agent Guidelines

When working with this codebase, AI coding agents **MUST** follow these rules:

#### ⛔ DO NOT MODIFY (Read-Only)

The `esp32-base` sibling checkout is a **shared library** consumed via `lib_deps`. Do not modify anything inside it (`base_*.cpp/h`, `base_api.h`, `base_app.cpp/h`, ...) from this repository. Changes to the base belong in the `esp32-base` repository.

The project folders no longer contain `base_*` symlinks or `.ino` files - only `main.cpp` and `project_*` sources.

#### ✅ CAN MODIFY

Only `main.cpp` (rarely) and `project_*` files can be modified or created:
- `project_config.h`, `project_version.h`
- `project_function.cpp/h`, `project_home.cpp`
- `project_diagnostics.cpp`, `project_mqtt.cpp/h`
- `project_dali_*.cpp/h`, `project_ballast_*.cpp/h`

#### Summary Table

| File Pattern | Can Modify? | Can Create? |
|--------------|-------------|-------------|
| `../esp32-base/*` | ❌ No | ❌ No |
| `base_*` (in the base library) | ❌ No | ❌ No |
| `main.cpp` | ⚠️ Rarely | ❌ No |
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
