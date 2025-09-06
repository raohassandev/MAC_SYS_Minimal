# ESP32 MAC-SYS Arduino Firmware Makefile
# Based on working MAC-SYS project structure

# Board configuration - ESP32-WROOM-DA Module (exact match for user's hardware)
BOARD = esp32:esp32:esp32da
ESP32_VERSION = 2.0.17
PORT ?= /dev/cu.usbserial-110
BUILD_DIR = build

# Arduino CLI
ARDUINO_CLI = arduino-cli

# Compile flags for ESP32 v3.2.0 compatibility
COMPILE_FLAGS = --warnings all \
                --build-property "build.partitions=default" \
                --build-property "build.flash_size=4MB" \
                --build-property "compiler.cpp.extra_flags=-DCORE_DEBUG_LEVEL=0"

.PHONY: all clean install-deps compile upload monitor check-cli

all: check-cli compile

# Check if arduino-cli is installed
check-cli:
	@which $(ARDUINO_CLI) > /dev/null || (echo "❌ arduino-cli not found. Please install it first." && exit 1)
	@echo "✅ arduino-cli found"

# Install dependencies
install-deps: check-cli
	@echo "📦 Installing ESP32 core and libraries..."
	$(ARDUINO_CLI) config add board_manager.url https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
	$(ARDUINO_CLI) core update-index
	$(ARDUINO_CLI) core install esp32:esp32
	@echo "📚 Installing required libraries..."
	$(ARDUINO_CLI) lib install "Adafruit SSD1306" "Adafruit GFX Library" "PCF8574" "ArduinoJson" "NTPClient" "RTClib"
	@echo "✅ Dependencies installed"

# Install specific ESP32 version for compatibility
install-esp32-v2:
	@echo "📦 Installing ESP32 v2.0.17 for compatibility..."
	$(ARDUINO_CLI) core install esp32:esp32@2.0.17
	@echo "✅ ESP32 v2.0.17 installed"

# Compile the firmware
compile: check-cli
	@echo "🔨 Compiling ESP32 firmware..."
	@echo "Board: $(BOARD)"
	@echo "Build flags: $(COMPILE_FLAGS)"
	$(ARDUINO_CLI) compile --fqbn $(BOARD) $(COMPILE_FLAGS) .
	@if [ $$? -eq 0 ]; then \
		echo "✅ Compilation successful!"; \
		echo "📦 Firmware binary: ./build/esp32.esp32.esp32/MAC_SYS_Arduino.ino.bin"; \
	else \
		echo "❌ Compilation failed!"; \
		exit 1; \
	fi

# Upload to board
upload: check-cli
	@echo "📡 Uploading to ESP32 on port $(PORT)..."
	$(ARDUINO_CLI) upload -p $(PORT) --fqbn $(BOARD) .
	@echo "✅ Upload complete"

# Upload with specific port
upload-port:
	@if [ -z "$(PORT)" ]; then \
		echo "❌ Please specify PORT: make upload-port PORT=/dev/cu.xxx"; \
		exit 1; \
	fi
	@echo "📡 Uploading to ESP32 on port $(PORT)..."
	$(ARDUINO_CLI) upload -p $(PORT) --fqbn $(BOARD) .

# Open serial monitor
monitor: check-cli
	@echo "📺 Opening serial monitor on port $(PORT)..."
	$(ARDUINO_CLI) monitor -p $(PORT) -c baudrate=115200

# Clean build files
clean:
	@echo "🧹 Cleaning build files..."
	@rm -rf $(BUILD_DIR)
	@rm -rf build.tmp
	@echo "✅ Clean complete"

# List available boards
list-boards: check-cli
	@echo "🔍 Available boards:"
	$(ARDUINO_CLI) board list

# Verify project structure
verify-structure:
	@echo "🔍 Verifying project structure..."
	@for file in MAC_SYS_Arduino.ino config.h network.h network.cpp hardware.h hardware.cpp auth.h auth.cpp display.h display.cpp wifi_manager.h wifi_manager.cpp; do \
		if [ -f "$$file" ]; then \
			echo "✅ $$file"; \
		else \
			echo "❌ $$file (missing)"; \
		fi \
	done

# Test compilation without upload
test-compile: check-cli
	@echo "🧪 Test compilation (dry run)..."
	$(ARDUINO_CLI) compile --fqbn $(BOARD) $(COMPILE_FLAGS) --verify .

# Show ESP32 core versions
show-cores: check-cli
	@echo "🔍 Installed ESP32 cores:"
	$(ARDUINO_CLI) core list | grep esp32

# Show installed libraries
show-libs: check-cli
	@echo "📚 Installed libraries:"
	$(ARDUINO_CLI) lib list

# Full setup (for first time users)
setup: install-deps verify-structure
	@echo "🎯 Setup complete! Ready to compile."
	@echo ""
	@echo "Next steps:"
	@echo "  1. make compile     # Compile the firmware"
	@echo "  2. make list-boards # Find your ESP32 port"
	@echo "  3. make upload PORT=/dev/cu.xxx # Upload firmware"

# Show help
help:
	@echo "ESP32 MAC-SYS Firmware Build System"
	@echo "===================================="
	@echo ""
	@echo "🛠️  Setup Commands:"
	@echo "  make setup          - Complete setup (first time)"
	@echo "  make install-deps   - Install required dependencies"
	@echo "  make install-esp32-v2 - Install ESP32 v2.0.17 for compatibility"
	@echo ""
	@echo "🔨 Build Commands:"
	@echo "  make compile        - Compile the firmware"
	@echo "  make test-compile   - Test compilation without upload"
	@echo "  make clean          - Clean build files"
	@echo ""
	@echo "📡 Upload Commands:"
	@echo "  make upload         - Upload firmware to ESP32"
	@echo "  make upload-port PORT=/dev/cu.xxx - Upload to specific port"
	@echo "  make monitor        - Open serial monitor"
	@echo ""
	@echo "🔍 Info Commands:"
	@echo "  make list-boards    - List connected boards"
	@echo "  make verify-structure - Check project files"
	@echo "  make show-cores     - Show ESP32 core versions"
	@echo "  make show-libs      - Show installed libraries"
	@echo ""
	@echo "📋 Example Workflow:"
	@echo "  make setup                           # First time setup"
	@echo "  make compile                         # Compile firmware"
	@echo "  make list-boards                     # Find ESP32 port"
	@echo "  make upload PORT=/dev/cu.usbserial-001 # Upload firmware"
	@echo "  make monitor                         # View serial output"
	@echo ""
	@echo "🆘 Troubleshooting:"
	@echo "  - If compilation fails with WiFi errors, try: make install-esp32-v2"
	@echo "  - Check project structure with: make verify-structure"
	@echo "  - Clean and rebuild with: make clean && make compile"

# Default target shows help
.DEFAULT_GOAL := help