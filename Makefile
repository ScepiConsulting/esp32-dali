# ESP32 DALI Projects — thin wrapper around PlatformIO.
#
# The repo migrated from arduino-cli (sketch folders + base symlinks + an
# esp32-base git submodule) to PlatformIO. The base is now consumed as a library
# from the sibling esp32-base checkout (see lib_deps in platformio.ini), so there
# is no submodule to reset and no symlinks to keep in sync.
#
# These targets exist so the familiar `make bridge` / `make flash-bridge` keep
# working; everything just forwards to `pio`.

PORT ?= /dev/ttyUSB0

.PHONY: all bridge ballast flash-bridge flash-ballast monitor-bridge monitor-ballast clean help

# Build both products
all:
	pio run

bridge:
	pio run -e bridge

ballast:
	pio run -e ballast

# Build + upload over serial
flash-bridge:
	pio run -e bridge -t upload --upload-port $(PORT)

flash-ballast:
	pio run -e ballast -t upload --upload-port $(PORT)

# Serial monitor
monitor-bridge:
	pio device monitor -e bridge --port $(PORT)

monitor-ballast:
	pio device monitor -e ballast --port $(PORT)

clean:
	pio run -t clean

help:
	@echo "ESP32 DALI Projects (PlatformIO)"
	@echo ""
	@echo "Targets:"
	@echo "  all              Build both products (default)"
	@echo "  bridge           Build DALI Bridge"
	@echo "  ballast          Build DALI Ballast"
	@echo "  flash-bridge     Build + upload the bridge   (PORT=...)"
	@echo "  flash-ballast    Build + upload the ballast  (PORT=...)"
	@echo "  monitor-bridge   Serial monitor              (PORT=...)"
	@echo "  monitor-ballast  Serial monitor              (PORT=...)"
	@echo "  clean            Remove build artifacts"
	@echo ""
	@echo "Variables:"
	@echo "  PORT   Serial port (default: /dev/ttyUSB0)"
	@echo ""
	@echo "OTA updates go through the device's web UI (/update)."
