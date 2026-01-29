# ESP32 DALI Projects

A collection of ESP32-S3 based projects for DALI (Digital Addressable Lighting Interface) control and WiFi connectivity testing.

## ⚠️ Disclaimer

> **This repository was created with extensive use of AI assistance (Claude/Cascade).**
> 
> As a result, the code may contain:
> - Bugs or unexpected behavior
> - Suboptimal or inefficient implementations
> - Redundant or overly verbose code
> 
> **This project was created for internal use only.** Feel free to use and modify it for your own non-commercial purposes, but please be aware of the above limitations. No warranty is provided.
> 
> Contributions, bug reports, and improvements are welcome!

## � Acknowledgments

- **[python-dali](https://github.com/sde1000/python-dali/)** by Stephen Early - Excellent Python DALI library that served as a reference for protocol implementation
- **[DALI Lighting Protocol](https://jared.geek.nz/2025/06/dali-lighting-protocol/)** by Jared Sanson - Fantastic article explaining DALI protocol details
- DALI protocol implementation based on DALI_Lib
- Web interfaces use modern CSS with CSS variables
- MQTT integration via PubSubClient library
- Projects follow IEC 62386 DALI standard

## �📁 Projects

### 🎛️ [ESP32 DALI Bridge](esp32_dali_bridge/)

A professional bridge between DALI and MQTT, acting as a DALI master/controller.

**Key Features:**
- DALI bus communication and device control
- Bus monitoring (detects external DALI masters)
- Passive device discovery (learns devices from bus traffic)
- MQTT integration with full topic support
- Device scanning, commissioning, and command queue
- Web interface with modern UI
- OTA updates and diagnostics

**Use Cases:** Control DALI lighting via MQTT, monitor DALI bus activity, integrate with Home Assistant

📖 **[Full Documentation →](esp32_dali_bridge/README.md)**

---

### 💡 [ESP32 DALI Ballast](esp32_dali_ballast/)

A virtual DALI ballast emulator that appears on the bus as a controllable DALI slave device.

**Key Features:**
- Emulates DALI ballast behavior
- DALI-2 compliant immediate responses (2.91-22ms)
- DT8 color support (RGB, RGBW, Color Temperature)
- Configurable address (0-63)
- MQTT state publishing
- Fade and scene support
- Web interface for configuration

**Use Cases:** Test DALI controllers without physical ballasts, monitor DALI commands, simulate multiple devices

📖 **[Full Documentation →](esp32_dali_ballast/README.md)**

---

### 📡 [ESP32 WiFi Test](esp32_wifi_test/)

A simple WiFi configuration and web server test project.

**Key Features:**
- WiFi management (STA/AP modes)
- Basic web interface
- OTA updates
- Theme system
- Persistent storage

**Use Cases:** WiFi testing, template for ESP32 web projects, learning example

📖 **[Full Documentation →](esp32_wifi_test/README.md)**

---

## 🔧 Hardware

All projects use the same hardware configuration:

**Recommended:**
- [Waveshare ESP32-S3-Pico](https://www.waveshare.com/esp32-s3-pico.htm) - ESP32-S3 board
- [Waveshare Pico-DALI2](https://www.waveshare.com/pico-dali2.htm) - DALI transceiver (for DALI projects)

**Connections:**
- DALI TX: GPIO17
- DALI RX: GPIO14
- Power: 5V USB

## 🚀 Quick Start

### Prerequisites

```bash
# Install Arduino CLI
brew install arduino-cli  # macOS
# or: curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh

# Install ESP32 core
arduino-cli config init
arduino-cli core update-index
arduino-cli core install esp32:esp32@3.3.5

# Install required libraries
arduino-cli lib install "PubSubClient@2.8"
arduino-cli lib install "ArduinoJson@7.4.2"
```

### Build & Flash

```bash
# Navigate to project directory
cd <project_name>

# Build
arduino-cli compile --fqbn esp32:esp32:esp32s3 --output-dir build .

# Flash to ESP32
esptool.py --chip esp32s3 --port /dev/ttyACM0 --baud 460800 \
  write_flash 0x0 build/<project_name>.ino.merged.bin
```

## 📊 Project Comparison

| Feature | WiFi Test | DALI Bridge | DALI Ballast |
|---------|-----------|-------------|--------------|
| **Role** | Template | DALI Master | DALI Slave |
| **DALI Control** | ❌ | ✅ Send commands | ❌ |
| **DALI Response** | ❌ | ❌ | ✅ Respond to queries |
| **Bus Monitoring** | ❌ | ✅ | ✅ |
| **MQTT** | ❌ | ✅ Pub/Sub | ✅ Publish only |
| **Device Scanning** | ❌ | ✅ | ❌ |
| **Configurable Address** | ❌ | ❌ | ✅ (0-63) |
| **Web Interface** | Basic | Full control | Configuration |
| **OTA Updates** | ✅ | ✅ | ✅ |
| **Use Case** | Testing | Control DALI | Emulate DALI device |

## 🎯 Typical Setup

```
┌─────────────────┐
│  MQTT Broker    │
│  (Mosquitto)    │
└────────┬────────┘
         │
    ┌────┴────┐
    │         │
┌───▼──┐  ┌──▼───┐
│Bridge│  │Home  │
│      │  │Assist│
└───┬──┘  └──────┘
    │
DALI Bus
    │
┌───┴────┬────────┬────────┐
│        │        │        │
▼        ▼        ▼        ▼
Ballast  Real     Real     Real
(ESP32)  Ballast  Ballast  Ballast
```

**Bridge** sends commands to control all ballasts
**Ballast** (ESP32) emulates a real DALI device for testing
**Real Ballasts** are actual LED drivers/ballasts

## 📚 Documentation

### Project READMEs
- [ESP32 DALI Bridge](esp32_dali_bridge/README.md) - Complete bridge documentation
- [ESP32 DALI Ballast](esp32_dali_ballast/README.md) - Complete ballast documentation
- [ESP32 WiFi Test](esp32_wifi_test/README.md) - WiFi test documentation

### Planning Documentation
- [Bridge Planning Docs](ai_helpers/esp32_dali_bridge/) - Implementation plans, testing guides
- [Ballast Planning Docs](ai_helpers/esp32_dali_ballast/) - Implementation plan, hardware setup, protocol reference

## 🔒 Security Notes

- Web interfaces use HTTP Basic Auth (not encrypted without HTTPS)
- Credentials stored in NVS (plain text, not in source code)
- No sensitive data in source code - credentials entered via web UI
- Default AP passwords should be changed before deployment
- Consider using VPN or isolated network for production

## 📝 Version Information

- **DALI Bridge:** v1.0.0
- **DALI Ballast:** v1.0.0
- **WiFi Test:** No formal versioning

## 🤝 Contributing

These are personal projects, but suggestions and bug reports are welcome.

## 📄 License

This project is licensed under **CC BY-NC-SA 4.0** (Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International).

**You are free to:**
- **Share** — copy and redistribute the material
- **Adapt** — remix, transform, and build upon the material

**Under the following terms:**
- **Attribution** — You must give appropriate credit
- **NonCommercial** — You may not use the material for commercial purposes
- **ShareAlike** — If you remix or transform, you must distribute under the same license

See [LICENSE](LICENSE) file for full details.

---

**Built with ❤️ for home automation and lighting control**

**© 2026 Scepi Consulting Kft.**
