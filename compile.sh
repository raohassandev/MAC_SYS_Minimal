#!/bin/bash

echo "🚀 ESP32 MAC-SYS Firmware Compilation Script"
echo "============================================="

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if arduino-cli is installed
check_arduino_cli() {
    if ! command -v arduino-cli &> /dev/null; then
        print_error "arduino-cli is not installed."
        
        if [[ "$OSTYPE" == "darwin"* ]]; then
            print_status "Installing arduino-cli via Homebrew..."
            if command -v brew &> /dev/null; then
                brew install arduino-cli
            else
                print_error "Homebrew not found. Please install arduino-cli manually:"
                print_status "https://arduino.github.io/arduino-cli/latest/installation/"
                exit 1
            fi
        else
            print_error "Please install arduino-cli manually:"
            print_status "https://arduino.github.io/arduino-cli/latest/installation/"
            exit 1
        fi
    else
        print_success "arduino-cli found"
    fi
}

# Initialize arduino-cli configuration
init_arduino_cli() {
    if [ ! -d "$HOME/.arduino15" ]; then
        print_status "Initializing arduino-cli configuration..."
        arduino-cli config init
    fi
}

# Install ESP32 board support
install_esp32_core() {
    print_status "Installing ESP32 board support..."
    
    # Add ESP32 board manager URL
    arduino-cli config add board_manager.url https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
    
    # Update board manager index
    arduino-cli core update-index
    
    # Check if ESP32 core is already installed
    if arduino-cli core list | grep -q "esp32:esp32"; then
        print_success "ESP32 core already installed"
    else
        print_status "Installing ESP32 core..."
        arduino-cli core install esp32:esp32
    fi
}

# Install required libraries
install_libraries() {
    print_status "Installing required libraries..."
    
    # List of required libraries
    LIBRARIES=(
        "Adafruit SSD1306"
        "Adafruit GFX Library"
        "PCF8574"
        "ArduinoJson"
        "NTPClient"
        "RTClib"
    )
    
    for lib in "${LIBRARIES[@]}"; do
        print_status "Installing: $lib"
        arduino-cli lib install "$lib"
    done
    
    print_success "All libraries installed"
}

# Verify project structure
verify_project_structure() {
    print_status "Verifying project structure..."
    
    REQUIRED_FILES=(
        "MAC_SYS_Arduino.ino"
        "config.h"
        "network.h"
        "network.cpp"
        "hardware.h" 
        "hardware.cpp"
        "auth.h"
        "auth.cpp"
    )
    
    local missing_files=0
    
    for file in "${REQUIRED_FILES[@]}"; do
        if [ -f "$file" ]; then
            print_success "✓ $file"
        else
            print_error "✗ $file (missing)"
            missing_files=$((missing_files + 1))
        fi
    done
    
    if [ $missing_files -gt 0 ]; then
        print_error "Missing $missing_files required files"
        exit 1
    fi
    
    print_success "Project structure verified"
}

# Compile the firmware
compile_firmware() {
    print_status "Compiling ESP32 firmware..."
    print_status "Board: esp32:esp32:esp32"
    print_status "Flash size: 4MB"
    print_status "Partition scheme: Default"
    
    # Clean previous build
    if [ -d "build" ]; then
        print_status "Cleaning previous build..."
        rm -rf build
    fi
    
    # Compile with appropriate flags
    arduino-cli compile \
        --fqbn esp32:esp32:esp32 \
        --build-property "build.partitions=default" \
        --build-property "build.flash_size=4MB" \
        --build-property "compiler.cpp.extra_flags=-DCORE_DEBUG_LEVEL=0" \
        --warnings all \
        --verbose \
        .
    
    local compile_result=$?
    
    if [ $compile_result -eq 0 ]; then
        print_success "Compilation successful!"
        
        # Show binary location
        if [ -f "./build/esp32.esp32.esp32/MAC_SYS_Arduino.ino.bin" ]; then
            local file_size=$(ls -lh "./build/esp32.esp32.esp32/MAC_SYS_Arduino.ino.bin" | awk '{print $5}')
            print_success "Firmware binary: ./build/esp32.esp32.esp32/MAC_SYS_Arduino.ino.bin ($file_size)"
        fi
        
        # Show memory usage
        if [ -f "./build/esp32.esp32.esp32/MAC_SYS_Arduino.ino.elf" ]; then
            print_status "Memory usage:"
            arduino-cli compile --fqbn esp32:esp32:esp32 . 2>&1 | grep -A5 "Memory usage"
        fi
        
    else
        print_error "Compilation failed!"
        
        # Show common troubleshooting tips
        echo ""
        print_warning "Troubleshooting tips:"
        print_status "1. Check if all required libraries are installed: arduino-cli lib list"
        print_status "2. Try ESP32 v2.0.17 if WiFi issues: arduino-cli core install esp32:esp32@2.0.17"
        print_status "3. Clean and retry: rm -rf build && $0"
        print_status "4. Check ESP32 board connection and drivers"
        
        exit 1
    fi
}

# Show upload instructions
show_upload_instructions() {
    print_status "Upload Instructions:"
    echo ""
    print_status "1. Connect your ESP32 board via USB"
    print_status "2. Find your board's serial port:"
    print_status "   arduino-cli board list"
    echo ""
    print_status "3. Upload the firmware:"
    print_status "   arduino-cli upload -p /dev/cu.usbserial-XXXX --fqbn esp32:esp32:esp32 ."
    print_status "   (Replace XXXX with your actual port)"
    echo ""
    print_status "4. Monitor serial output:"
    print_status "   arduino-cli monitor -p /dev/cu.usbserial-XXXX -c baudrate=115200"
    echo ""
    
    # Try to detect boards
    print_status "Scanning for connected boards..."
    arduino-cli board list
}

# Main execution
main() {
    print_status "Starting firmware compilation process..."
    
    check_arduino_cli
    init_arduino_cli
    install_esp32_core
    install_libraries
    verify_project_structure
    compile_firmware
    show_upload_instructions
    
    print_success "Build process completed successfully! 🎉"
}

# Handle script arguments
case "${1:-}" in
    "--help"|"-h")
        echo "ESP32 MAC-SYS Firmware Compilation Script"
        echo ""
        echo "Usage: $0 [options]"
        echo ""
        echo "Options:"
        echo "  --help, -h     Show this help message"
        echo "  --clean, -c    Clean build files before compiling"
        echo "  --no-deps      Skip dependency installation"
        echo ""
        echo "Examples:"
        echo "  $0             # Full compilation with dependency check"
        echo "  $0 --clean     # Clean build and compile"
        echo "  $0 --no-deps   # Compile only, skip dependencies"
        ;;
    "--clean"|"-c")
        print_status "Cleaning build files..."
        rm -rf build build.tmp
        print_success "Clean completed"
        main
        ;;
    "--no-deps")
        print_status "Compiling without dependency check..."
        verify_project_structure
        compile_firmware
        show_upload_instructions
        ;;
    "")
        main
        ;;
    *)
        print_error "Unknown option: $1"
        print_status "Use --help for usage information"
        exit 1
        ;;
esac