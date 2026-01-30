# ESP32 WiFi Test

A simple WiFi configuration and web server test project for ESP32-S3, used as a foundation for the DALI Bridge and Ballast projects.

## 🌟 Features

- **WiFi Management** - STA and AP modes with automatic fallback
- **Web Interface** - Basic responsive UI with dark/light theme
- **Captive Portal** - DNS server for easy AP mode access
- **Persistent Storage** - WiFi credentials saved in NVS
- **OTA Updates** - Over-the-air firmware updates
- **Theme System** - Auto/Light/Dark mode with cookie storage
- **Logo Support** - Embedded logos with theme switching

## 📋 Purpose

This project serves as a minimal WiFi and web server template that was used to develop the more complex DALI Bridge and Ballast projects. It includes:

- WiFi connection management
- Web server with modern UI
- Preferences (NVS) storage
- OTA update functionality
- Theme switching system
- Logo embedding

## 🔧 Hardware Requirements

- **ESP32-S3** development board (e.g., Waveshare ESP32-S3-Pico)
- **Power supply** - 5V USB

## 🚀 Quick Start

1. **Flash the firmware**
   ```bash
   cd esp32_wifi_test
   arduino-cli compile --fqbn esp32:esp32:esp32s3 --output-dir build .
   esptool.py --chip esp32s3 --port /dev/ttyACM0 --baud 460800 \
     write_flash 0x0 build/esp32_wifi_test.ino.merged.bin
   ```

2. **Connect to AP mode**
   - SSID: `ESP32-WiFi-Test`
   - Password: `testconfig`
   - Navigate to: `http://192.168.4.1`

3. **Configure WiFi**
   - Enter your WiFi credentials
   - Click "Save WiFi Settings"
   - Device reboots and connects to your network

## 💻 Building

```bash
# Navigate to project directory
cd esp32_wifi_test

# Compile
arduino-cli compile --fqbn esp32:esp32:esp32s3 --output-dir build .

# Flash
esptool.py --chip esp32s3 --port /dev/ttyACM0 --baud 460800 \
  write_flash 0x0 build/esp32_wifi_test.ino.merged.bin
```

## 📖 Usage

### Web Interface

Access at `http://<device-ip>` or `http://192.168.4.1` (AP mode)

**Features:**
- WiFi configuration
- Theme switching (Auto/Light/Dark)
- System information
- OTA firmware updates

### Configuration

WiFi credentials are stored in NVS and persist across reboots.

**Reset to defaults:**
```bash
esptool.py --chip esp32s3 --port /dev/ttyACM0 erase_flash
```

## 🔨 Code Structure

Single file project (`esp32_wifi_test.ino`) containing:
- WiFi management (STA/AP modes)
- Web server with HTML UI
- Preferences (NVS) handling
- DNS server for captive portal
- OTA update handler
- Theme system with cookies
- Embedded logos

## 📊 Build Size

- **Flash:** ~800 KB
- **RAM:** ~40 KB

## 🎯 Use Cases

1. **WiFi Testing** - Test ESP32 WiFi connectivity
2. **Template Project** - Starting point for ESP32 web projects
3. **Learning** - Example of WiFi + web server + OTA
4. **Development** - Foundation for DALI Bridge/Ballast projects

## 🔄 Related Projects

This code was used as the foundation for:
- **ESP32 DALI Bridge** - DALI master/controller with MQTT
- **ESP32 DALI Ballast** - DALI slave/ballast emulator

## 📝 Version Information

Simple test project - no formal versioning

## 📄 License

This project is licensed under **CC BY-NC-SA 4.0** (Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International).

See [LICENSE](../LICENSE) file for full details.

---

**Built with ❤️ as a WiFi test and template project**
