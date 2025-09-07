#include "relay_control.h"
#include "config.h"

// Global relay controller instance
RelayController relay_controller;

RelayController::RelayController() {
    relay_states = 0xFF;      // All relays OFF initially (inverted logic)
    input_states = 0xFF;      // All inputs inactive initially
    last_input_states = 0xFF;
    last_input_read = 0;
}

bool RelayController::begin() {
    DEBUG_PRINTLN("Initializing MAC-SYS relay and I/O controllers...");
    
    // Initialize I2C if not already done
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    
    // Test relay controller connection
    if (!isRelayControllerConnected()) {
        DEBUG_PRINTLN("❌ Relay controller not detected at 0x24");
        return false;
    }
    
    // Test input controller connection  
    if (!isInputControllerConnected()) {
        DEBUG_PRINTLN("❌ Input controller not detected at 0x22");
        return false;
    }
    
    // Initialize all relays to OFF state
    if (!pcf8574_write(RELAY_I2C_ADDRESS, relay_states)) {
        DEBUG_PRINTLN("❌ Failed to initialize relay states");
        return false;
    }
    
    DEBUG_PRINTLN("✅ MAC-SYS relay and I/O controllers initialized successfully");
    DEBUG_PRINTF("🔌 Relays: 0x%02X, Inputs: 0x%02X\n", relay_states, input_states);
    
    return true;
}

bool RelayController::pcf8574_write(uint8_t address, uint8_t data) {
    Wire.beginTransmission(address);
    Wire.write(data);
    int result = Wire.endTransmission();
    return (result == 0);
}

uint8_t RelayController::pcf8574_read(uint8_t address) {
    Wire.requestFrom(address, (uint8_t)1);
    if (Wire.available()) {
        return Wire.read();
    }
    return 0xFF; // Default to all high if read fails
}

bool RelayController::setRelay(uint8_t relay_num, bool state) {
    if (relay_num >= NUM_RELAYS) {
        DEBUG_PRINTF("❌ Invalid relay number: %d\n", relay_num);
        return false;
    }
    
    // Update relay state in memory (inverted logic)
    if (state) {
        relay_states &= ~(1 << relay_num);  // Clear bit = ON
    } else {
        relay_states |= (1 << relay_num);   // Set bit = OFF
    }
    
    // Write to hardware
    if (pcf8574_write(RELAY_I2C_ADDRESS, relay_states)) {
        DEBUG_PRINTF("🔌 Relay %d: %s\n", relay_num + 1, state ? "ON" : "OFF");
        return true;
    } else {
        DEBUG_PRINTF("❌ Failed to control relay %d\n", relay_num + 1);
        return false;
    }
}

bool RelayController::getRelayState(uint8_t relay_num) {
    if (relay_num >= NUM_RELAYS) {
        return false;
    }
    
    // Return inverted state (0 = ON, 1 = OFF)
    return !(relay_states & (1 << relay_num));
}

void RelayController::setAllRelays(uint8_t states) {
    // Invert the states for MAC-SYS hardware (active LOW)
    relay_states = ~states;
    
    if (pcf8574_write(RELAY_I2C_ADDRESS, relay_states)) {
        DEBUG_PRINTF("🔌 All relays set to: 0x%02X\n", states);
    } else {
        DEBUG_PRINTLN("❌ Failed to set all relay states");
    }
}

uint8_t RelayController::getAllRelayStates() {
    // Return inverted states (convert hardware states to logical states)
    return ~relay_states;
}

void RelayController::updateInputs() {
    unsigned long current_time = millis();
    
    // Read inputs at specified interval for debouncing
    if (current_time - last_input_read >= INPUT_READ_INTERVAL) {
        last_input_states = input_states;
        input_states = pcf8574_read(INPUT_I2C_ADDRESS);
        last_input_read = current_time;
    }
}

bool RelayController::getInputState(uint8_t input_num) {
    if (input_num >= NUM_DIGITAL_INPUTS) {
        return false;
    }
    
    // Return inverted state (0 = ACTIVE, 1 = INACTIVE)
    return !(input_states & (1 << input_num));
}

uint8_t RelayController::getAllInputStates() {
    // Return inverted states (convert hardware states to logical states)
    return ~input_states;
}

bool RelayController::hasInputChanged(uint8_t input_num) {
    if (input_num >= NUM_DIGITAL_INPUTS) {
        return false;
    }
    
    uint8_t current_bit = (input_states >> input_num) & 1;
    uint8_t last_bit = (last_input_states >> input_num) & 1;
    
    return (current_bit != last_bit);
}

bool RelayController::isRelayControllerConnected() {
    Wire.beginTransmission(RELAY_I2C_ADDRESS);
    return (Wire.endTransmission() == 0);
}

bool RelayController::isInputControllerConnected() {
    Wire.beginTransmission(INPUT_I2C_ADDRESS);
    return (Wire.endTransmission() == 0);
}

String RelayController::getRelayStatusString() {
    String status = "Relays: ";
    for (int i = 0; i < NUM_RELAYS; i++) {
        status += "R" + String(i + 1) + ":" + (getRelayState(i) ? "ON" : "OFF");
        if (i < NUM_RELAYS - 1) status += ", ";
    }
    return status;
}

String RelayController::getInputStatusString() {
    String status = "Inputs: ";
    for (int i = 0; i < NUM_DIGITAL_INPUTS; i++) {
        status += "I" + String(i + 1) + ":" + (getInputState(i) ? "ACTIVE" : "INACTIVE");
        if (i < NUM_DIGITAL_INPUTS - 1) status += ", ";
    }
    return status;
}