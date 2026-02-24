# ESP32 DALI Projects Makefile
# Builds both esp32_dali_bridge and esp32_dali_ballast

# Default board (can be overridden: make BOARD=esp32c3)
BOARD ?= esp32s3
PARTITION ?= min_spiffs
BUILD_DIR ?= ./build

# Board to FQBN mapping
ifeq ($(BOARD),esp32)
    FQBN = esp32:esp32:esp32
    CHIP = esp32
    FLASH_SIZE = 4MB
else ifeq ($(BOARD),esp32s2)
    FQBN = esp32:esp32:esp32s2
    CHIP = esp32s2
    FLASH_SIZE = 4MB
else ifeq ($(BOARD),esp32s3)
    FQBN = esp32:esp32:esp32s3
    CHIP = esp32s3
    FLASH_SIZE = 8MB
else ifeq ($(BOARD),esp32c3)
    FQBN = esp32:esp32:esp32c3
    CHIP = esp32c3
    FLASH_SIZE = 4MB
else ifeq ($(BOARD),esp32c6)
    FQBN = esp32:esp32:esp32c6
    CHIP = esp32c6
    FLASH_SIZE = 4MB
else
    $(error Unknown BOARD: $(BOARD). Use: esp32, esp32s2, esp32s3, esp32c3, esp32c6)
endif

# Serial port (override with: make PORT=/dev/cu.usbserial-xxx)
PORT ?= /dev/ttyUSB0

.PHONY: all clean bridge ballast flash-bridge flash-ballast submodule-reset help

# Default target
all: bridge ballast

# Reset submodules before build (required to undo any accidental changes)
submodule-reset:
	@echo "=== Resetting submodules ==="
	git submodule foreach git reset --hard HEAD
	git submodule foreach git checkout .

# Build DALI Bridge
bridge: submodule-reset
	@echo "=== Building DALI Bridge for $(BOARD) ==="
	arduino-cli compile -v \
		--fqbn $(FQBN):PartitionScheme=$(PARTITION) \
		--build-property "build.project_name=esp32_dali_bridge" \
		--output-dir $(BUILD_DIR) \
		esp32_dali_bridge/

# Build DALI Ballast
ballast: submodule-reset
	@echo "=== Building DALI Ballast for $(BOARD) ==="
	arduino-cli compile -v \
		--fqbn $(FQBN):PartitionScheme=$(PARTITION) \
		--build-property "build.project_name=esp32_dali_ballast" \
		--output-dir $(BUILD_DIR) \
		esp32_dali_ballast/

# Flash DALI Bridge (full flash with merged binary)
flash-bridge:
	@echo "=== Flashing DALI Bridge to $(PORT) ==="
	esptool --chip $(CHIP) --port $(PORT) --baud 460800 \
		--before default_reset --after hard_reset write_flash \
		-z --flash_mode dio --flash_freq 80m --flash_size $(FLASH_SIZE) \
		0x0 $(BUILD_DIR)/esp32_dali_bridge.merged.bin

# Flash DALI Ballast (full flash with merged binary)
flash-ballast:
	@echo "=== Flashing DALI Ballast to $(PORT) ==="
	esptool --chip $(CHIP) --port $(PORT) --baud 460800 \
		--before default_reset --after hard_reset write_flash \
		-z --flash_mode dio --flash_freq 80m --flash_size $(FLASH_SIZE) \
		0x0 $(BUILD_DIR)/esp32_dali_ballast.merged.bin

# Flash app only (faster, for updates)
flash-bridge-app:
	@echo "=== Flashing DALI Bridge app only to $(PORT) ==="
	esptool --chip $(CHIP) --port $(PORT) --baud 460800 \
		write_flash 0x10000 $(BUILD_DIR)/esp32_dali_bridge.bin

flash-ballast-app:
	@echo "=== Flashing DALI Ballast app only to $(PORT) ==="
	esptool --chip $(CHIP) --port $(PORT) --baud 460800 \
		write_flash 0x10000 $(BUILD_DIR)/esp32_dali_ballast.bin

# Clean build artifacts
clean:
	@echo "=== Cleaning build directory ==="
	rm -rf $(BUILD_DIR)/*

# Help
help:
	@echo "ESP32 DALI Projects Makefile"
	@echo ""
	@echo "Usage: make [target] [BOARD=xxx] [PORT=xxx]"
	@echo ""
	@echo "Targets:"
	@echo "  all              Build both bridge and ballast (default)"
	@echo "  bridge           Build DALI Bridge"
	@echo "  ballast          Build DALI Ballast"
	@echo "  flash-bridge     Flash DALI Bridge (full)"
	@echo "  flash-ballast    Flash DALI Ballast (full)"
	@echo "  flash-bridge-app Flash DALI Bridge (app only, faster)"
	@echo "  flash-ballast-app Flash DALI Ballast (app only, faster)"
	@echo "  submodule-reset  Reset submodules to clean state"
	@echo "  clean            Remove build artifacts"
	@echo "  help             Show this help"
	@echo ""
	@echo "Variables:"
	@echo "  BOARD    Target board: esp32, esp32s2, esp32s3 (default), esp32c3, esp32c6"
	@echo "  PORT     Serial port (default: /dev/ttyUSB0)"
	@echo "  BUILD_DIR Build output directory (default: ./build)"
	@echo ""
	@echo "Examples:"
	@echo "  make                           # Build both for ESP32-S3"
	@echo "  make bridge BOARD=esp32c3      # Build bridge for ESP32-C3"
	@echo "  make flash-bridge PORT=/dev/cu.usbserial-0001"
