#ifndef RELAY_CONTROL_H
#define RELAY_CONTROL_H

#include <Arduino.h>
#include <Wire.h>

// MAC-SYS Hardware Configuration
#define RELAY_I2C_ADDRESS 0x24    // PCF8574 relay output expander
#define INPUT_I2C_ADDRESS 0x22    // PCF8574 digital input expander

#define NUM_RELAYS 6
#define NUM_DIGITAL_INPUTS 6

// Relay states (inverted logic - active LOW)
#define RELAY_ON  0
#define RELAY_OFF 1

// Digital input states  
#define INPUT_ACTIVE   0  // Optocoupled inputs active LOW
#define INPUT_INACTIVE 1

class RelayController {
private:
    uint8_t relay_states;      // Current relay states (bits 0-5)
    uint8_t input_states;      // Current input states (bits 0-5)
    uint8_t last_input_states; // Previous input states for change detection
    unsigned long last_input_read;
    static const unsigned long INPUT_READ_INTERVAL = 50; // 50ms debounce
    
    bool pcf8574_write(uint8_t address, uint8_t data);
    uint8_t pcf8574_read(uint8_t address);
    
public:
    RelayController();
    
    // Initialization
    bool begin();
    
    // Relay control functions
    bool setRelay(uint8_t relay_num, bool state);
    bool getRelayState(uint8_t relay_num);
    void setAllRelays(uint8_t states);
    uint8_t getAllRelayStates();
    
    // Digital input functions  
    void updateInputs();
    bool getInputState(uint8_t input_num);
    uint8_t getAllInputStates();
    bool hasInputChanged(uint8_t input_num);
    
    // Status functions
    bool isRelayControllerConnected();
    bool isInputControllerConnected();
    String getRelayStatusString();
    String getInputStatusString();
};

// Global relay controller instance
extern RelayController relay_controller;

#endif // RELAY_CONTROL_H