#include "hardware.h"
#include "config.h"

// Hardware state variables
static uint8_t relay_states = 0x00;        // Current relay states
static uint8_t input_states = 0x00;        // Current input states
static uint8_t pcf8574_error_count = 0;
static unsigned long last_pcf_read = 0;

// RTC instance
RTC_DS1307 rtc;
bool rtc_available = false;

// I2C communication functions
bool i2c_write_byte(uint8_t device_addr, uint8_t data);
bool i2c_read_byte(uint8_t device_addr, uint8_t* data);
void handleI2CError(const char* operation, uint8_t device_addr);

// Core Hardware Initialization
bool initializeI2C() {
    DEBUG_PRINTLN("Initializing I2C bus...");
    
    // Initialize I2C with explicit pins and frequency
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.setClock(I2C_FREQUENCY); // Set I2C frequency for stability
    delay(200); // Allow I2C bus to stabilize
    
    i2c_initialized = true;
    DEBUG_PRINTLN("I2C bus initialized successfully");
    
    // Scan for I2C devices
    DEBUG_PRINTLN("Scanning I2C bus...");
    bool devices_found = false;
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        uint8_t error = Wire.endTransmission();
        if (error == 0) {
            DEBUG_PRINTF("I2C device found at address 0x%02X\n", addr);
            devices_found = true;
        }
    }
    
    if (!devices_found) {
        DEBUG_PRINTLN("WARNING: No I2C devices found! Check connections and pull-up resistors.");
    }
    
    return true;
}

bool initializeHardware() {
    DEBUG_PRINTLN("Initializing hardware components...");
    
    // Initialize I2C bus first
    if (!initializeI2C()) {
        DEBUG_PRINTLN("ERROR: I2C initialization failed");
        return false;
    }
    
    // Initialize RTC
    if (!initializeRTC()) {
        DEBUG_PRINTLN("WARNING: RTC initialization failed");
    }
    
    // Initialize PCF8574 devices
    if (!initializePCF8574()) {
        DEBUG_PRINTLN("WARNING: PCF8574 initialization failed - continuing without I/O expansion");
    }
    
    // Perform hardware self-test
    if (!performHardwareSelfTest()) {
        DEBUG_PRINTLN("WARNING: Hardware self-test failed");
    }
    
    DEBUG_PRINTLN("Hardware initialization complete");
    return true;
}

// PCF8574 Functions
bool initializePCF8574() {
    DEBUG_PRINTLN("Initializing PCF8574 I/O expanders...");
    
    // Test output expander
    if (!i2c_write_byte(PCF8574_OUTPUT_ADDR, 0x00)) {
        DEBUG_PRINTF("ERROR: PCF8574 output expander not found at address 0x%02X\n", PCF8574_OUTPUT_ADDR);
        return false;
    }
    
    // Test input expander
    uint8_t test_data;
    if (!i2c_read_byte(PCF8574_INPUT_ADDR, &test_data)) {
        DEBUG_PRINTF("ERROR: PCF8574 input expander not found at address 0x%02X\n", PCF8574_INPUT_ADDR);
        return false;
    }
    
    // Initialize all relays to OFF
    setAllRelayStates(0x00);
    
    DEBUG_PRINTLN("PCF8574 I/O expanders initialized successfully");
    return true;
}

bool setRelayState(uint8_t relay_num, bool state) {
    if (relay_num >= 6) {
        DEBUG_PRINTF("ERROR: Invalid relay number: %d (0-5)\n", relay_num);
        return false;
    }
    
    if (state) {
        relay_states |= (1 << relay_num);
    } else {
        relay_states &= ~(1 << relay_num);
    }
    
    if (i2c_write_byte(PCF8574_OUTPUT_ADDR, relay_states)) {
        DEBUG_PRINTF("Relay %d %s\n", relay_num, state ? "ON" : "OFF");
        return true;
    } else {
        handleI2CError("setRelayState", PCF8574_OUTPUT_ADDR);
        return false;
    }
}

bool getRelayState(uint8_t relay_num) {
    if (relay_num >= 6) {
        return false;
    }
    return (relay_states & (1 << relay_num)) != 0;
}

uint8_t getAllRelayStates() {
    return relay_states;
}

bool setAllRelayStates(uint8_t states) {
    relay_states = states;
    
    if (i2c_write_byte(PCF8574_OUTPUT_ADDR, relay_states)) {
        DEBUG_PRINTF("All relays set to: 0x%02X\n", relay_states);
        return true;
    } else {
        handleI2CError("setAllRelayStates", PCF8574_OUTPUT_ADDR);
        return false;
    }
}

bool readInputStates() {
    if (i2c_read_byte(PCF8574_INPUT_ADDR, &input_states)) {
        last_pcf_read = millis();
        pcf8574_error_count = 0;
        return true;
    } else {
        handleI2CError("readInputStates", PCF8574_INPUT_ADDR);
        return false;
    }
}

bool getInputState(uint8_t input_num) {
    if (input_num >= 6) {
        return false;
    }
    
    // Read fresh input states if it's been a while
    if (millis() - last_pcf_read > 100) {
        readInputStates();
    }
    
    return (input_states & (1 << input_num)) != 0;
}

uint8_t getAllInputStates() {
    readInputStates();
    return input_states;
}

// HVAC Control Functions
bool setCompressorState(bool state) {
    // Compressor is typically connected to relay 0
    static bool last_compressor_state = false;
    static unsigned long last_compressor_change = 0;
    
    // Implement minimum cycle times for compressor protection
    if (state != last_compressor_state) {
        unsigned long time_since_change = millis() - last_compressor_change;
        
        if (state && time_since_change < MIN_COMPRESSOR_CYCLE_TIME) {
            DEBUG_PRINTLN("Compressor start blocked - minimum off time not met");
            return false;
        }
        
        if (!state && time_since_change < MIN_COMPRESSOR_CYCLE_TIME) {
            DEBUG_PRINTLN("Compressor stop blocked - minimum on time not met");
            return false;
        }
        
        last_compressor_change = millis();
        last_compressor_state = state;
    }
    
    g_system_status.compressor_running = state;
    
    if (setRelayState(0, state)) {
        DEBUG_PRINTF("Compressor %s\n", state ? "STARTED" : "STOPPED");
        return true;
    }
    
    return false;
}

bool setFanState(bool state) {
    // Fan is typically connected to relay 1
    if (setRelayState(1, state)) {
        DEBUG_PRINTF("Fan %s\n", state ? "STARTED" : "STOPPED");
        return true;
    }
    
    return false;
}

bool setHeatingState(bool state) {
    // Heating is typically connected to relay 2
    if (setRelayState(2, state)) {
        DEBUG_PRINTF("Heating %s\n", state ? "ON" : "OFF");
        return true;
    }
    
    return false;
}

bool emergencyStop() {
    DEBUG_PRINTLN("EMERGENCY STOP - Shutting down all HVAC systems");
    
    bool success = true;
    
    // Turn off all HVAC relays
    success &= setCompressorState(false);
    success &= setFanState(false);
    success &= setHeatingState(false);
    
    // Set system state to error
    g_system_status.state = STATE_ERROR;
    g_system_status.last_error = ERROR_NONE; // Will be set by calling function
    
    return success;
}

// RTC Functions
bool initializeRTC() {
    DEBUG_PRINTLN("Initializing RTC...");
    
    if (!rtc.begin()) {
        DEBUG_PRINTLN("ERROR: RTC not found");
        rtc_available = false;
        return false;
    }
    
    if (!rtc.isrunning()) {
        DEBUG_PRINTLN("RTC is NOT running, setting time...");
        // Set to compile time if not running
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
    
    rtc_available = true;
    
    DateTime now = rtc.now();
    DEBUG_PRINTF("RTC initialized - Current time: %04d-%02d-%02d %02d:%02d:%02d\n",
                 now.year(), now.month(), now.day(),
                 now.hour(), now.minute(), now.second());
    
    return true;
}

bool setRTCTime(uint16_t year, uint8_t month, uint8_t day, 
                uint8_t hour, uint8_t minute, uint8_t second) {
    if (!rtc_available) {
        DEBUG_PRINTLN("ERROR: RTC not available");
        return false;
    }
    
    DateTime dt(year, month, day, hour, minute, second);
    rtc.adjust(dt);
    
    DEBUG_PRINTF("RTC time set to: %04d-%02d-%02d %02d:%02d:%02d\n",
                 year, month, day, hour, minute, second);
    
    return true;
}

DateTime getCurrentTime() {
    if (rtc_available) {
        return rtc.now();
    } else {
        // Return epoch time if RTC not available
        return DateTime(1970, 1, 1, 0, 0, 0);
    }
}

bool isRTCAvailable() {
    return rtc_available;
}

uint16_t getCurrentMinutes() {
    if (rtc_available) {
        DateTime now = rtc.now();
        return now.hour() * 60 + now.minute();
    }
    return INVALID_TIME;
}

uint8_t getCurrentDayOfWeek() {
    if (rtc_available) {
        DateTime now = rtc.now();
        return now.dayOfTheWeek();
    }
    return 0;
}

// System Health Functions
bool performHardwareSelfTest() {
    DEBUG_PRINTLN("Performing hardware self-test...");
    
    bool test_passed = true;
    
    // Test I2C bus
    if (!i2c_initialized) {
        DEBUG_PRINTLN("SELF-TEST FAIL: I2C not initialized");
        test_passed = false;
    }
    
    // Test PCF8574 communication
    uint8_t test_data;
    if (!i2c_read_byte(PCF8574_INPUT_ADDR, &test_data)) {
        DEBUG_PRINTLN("SELF-TEST FAIL: PCF8574 input expander communication");
        test_passed = false;
    }
    
    if (!i2c_write_byte(PCF8574_OUTPUT_ADDR, 0x00)) {
        DEBUG_PRINTLN("SELF-TEST FAIL: PCF8574 output expander communication");
        test_passed = false;
    }
    
    // Test relay cycling (brief test)
    for (int i = 0; i < 3; i++) {
        if (!setRelayState(i, true)) {
            DEBUG_PRINTF("SELF-TEST FAIL: Relay %d on\n", i);
            test_passed = false;
        }
        delay(10);
        if (!setRelayState(i, false)) {
            DEBUG_PRINTF("SELF-TEST FAIL: Relay %d off\n", i);
            test_passed = false;
        }
    }
    
    // Test input reading
    readInputStates();
    
    DEBUG_PRINTF("Hardware self-test: %s\n", test_passed ? "PASSED" : "FAILED");
    return test_passed;
}

void resetHardwareErrors() {
    pcf8574_error_count = 0;
    DEBUG_PRINTLN("Hardware error counters reset");
}

void updateHardwareStatus() {
    // Read input states periodically
    if (millis() - last_pcf_read > 1000) {
        readInputStates();
    }
    
    // Check for hardware errors
    if (pcf8574_error_count > 5) {
        DEBUG_PRINTLN("WARNING: Multiple PCF8574 communication errors detected");
        g_system_status.last_error = ERROR_I2C;
        g_system_status.error_count++;
    }
}

// Private helper functions
bool i2c_write_byte(uint8_t device_addr, uint8_t data) {
    if (!i2c_initialized) {
        return false;
    }
    
    Wire.beginTransmission(device_addr);
    Wire.write(data);
    uint8_t error = Wire.endTransmission();
    
    if (error == 0) {
        return true;
    } else {
        pcf8574_error_count++;
        DEBUG_PRINTF("I2C write error %d to device 0x%02X\n", error, device_addr);
        return false;
    }
}

bool i2c_read_byte(uint8_t device_addr, uint8_t* data) {
    if (!i2c_initialized || !data) {
        return false;
    }
    
    Wire.requestFrom(device_addr, (uint8_t)1);
    
    if (Wire.available()) {
        *data = Wire.read();
        return true;
    } else {
        pcf8574_error_count++;
        DEBUG_PRINTF("I2C read error from device 0x%02X\n", device_addr);
        return false;
    }
}

void handleI2CError(const char* operation, uint8_t device_addr) {
    DEBUG_PRINTF("I2C Error in %s with device 0x%02X\n", operation, device_addr);
    pcf8574_error_count++;
    
    if (pcf8574_error_count > 10) {
        DEBUG_PRINTLN("Critical I2C errors - system may be unstable");
        g_system_status.last_error = ERROR_I2C;
        g_system_status.error_count++;
    }
}