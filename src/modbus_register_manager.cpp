#include "modbus_register_manager.h"
#include "config.h"
#include "simple_temp_control.h"
#include "relay_control.h"
#include "temperature.h"
#include "wifi_professional.h"
#include <WiFi.h>
#include <esp_system.h>

// External references to system components
extern SimpleTempController simple_temp;
extern RelayController relay_controller;
// WiFiProfessional removed - using WiFi directly
extern bool am2302Available, ds18b20Available, lm35Available;
extern float lastValidTemperature;
extern uint8_t currentTempSensorType;
extern unsigned long system_start_time;

static uint32_t total_reads = 0;
static uint32_t total_writes = 0;

ModbusRegisterManager::ModbusRegisterManager() :
    last_system_update(0),
    last_diagnostics_update(0),
    auto_save_config(true)
{
    // Initialize register arrays
    memset(coils, 0, sizeof(coils));
    memset(discrete_inputs, 0, sizeof(discrete_inputs));
    memset(input_registers, 0, sizeof(input_registers));
    memset(holding_registers, 0, sizeof(holding_registers));
}

void ModbusRegisterManager::begin() {
    DEBUG_PRINTLN("Modbus Register Manager initialized");
    
    // Initial system state update
    updateFromSystem();
}

void ModbusRegisterManager::updateFromSystem() {
    unsigned long current_time = millis();
    
    // Update temperature sensors and system status frequently (100ms)
    if (current_time - last_system_update >= 100) {
        updateTemperatureSensors();
        updateSystemStatus();
        updateConfiguration();
        last_system_update = current_time;
    }
    
    // Update diagnostics less frequently (1000ms)
    if (current_time - last_diagnostics_update >= 1000) {
        updateDiagnostics();
        last_diagnostics_update = current_time;
    }
}

void ModbusRegisterManager::applyToSystem() {
    applyControlChanges();
    applyConfigurationChanges();
}

bool ModbusRegisterManager::getBit(const uint8_t* array, uint16_t bit_index) const {
    uint16_t byte_index = bit_index / 8;
    uint8_t bit_offset = bit_index % 8;
    return (array[byte_index] & (1 << bit_offset)) != 0;
}

void ModbusRegisterManager::setBit(uint8_t* array, uint16_t bit_index, bool value) {
    uint16_t byte_index = bit_index / 8;
    uint8_t bit_offset = bit_index % 8;
    
    if (value) {
        array[byte_index] |= (1 << bit_offset);
    } else {
        array[byte_index] &= ~(1 << bit_offset);
    }
}

void ModbusRegisterManager::updateTemperatureSensors() {
    // DS18B20 Temperature Sensor (Input Register 30001)
    input_registers[ModbusRegisters::DS18B20_TEMP_1] = ds18b20Available ? 
        floatToModbus(readDS18B20Temperature()) : MODBUS_INVALID_VALUE;
    input_registers[ModbusRegisters::DS18B20_TEMP_2] = MODBUS_INVALID_VALUE; // Not implemented
    input_registers[ModbusRegisters::DS18B20_TEMP_3] = MODBUS_INVALID_VALUE; // Not implemented
    
    // AM2302 Temperature and Humidity (Input Registers 30011-30012)
    input_registers[ModbusRegisters::AM2302_TEMP] = am2302Available ? 
        floatToModbus(readAM2302Temperature()) : MODBUS_INVALID_VALUE;
    input_registers[ModbusRegisters::AM2302_HUMIDITY] = am2302Available ? 
        floatToModbus(readAM2302Humidity()) : MODBUS_INVALID_VALUE;
    
    // LM35 Temperature (Input Register 30021)
    input_registers[ModbusRegisters::LM35_TEMP] = lm35Available ? 
        floatToModbus(readLM35Temperature()) : MODBUS_INVALID_VALUE;
    
    // Current active temperature reading (Input Register 30031)
    float current_temp = simple_temp.getCurrentTemp();
    input_registers[30] = (current_temp != -999.0f) ? floatToModbus(current_temp) : MODBUS_INVALID_VALUE;
    
    // Sensor availability flags (Discrete Inputs 10031-10035)
    setBit(discrete_inputs, 30, ds18b20Available);    // 10031
    setBit(discrete_inputs, 31, false);               // 10032 - DS18B20 #2 not implemented
    setBit(discrete_inputs, 32, false);               // 10033 - DS18B20 #3 not implemented
    setBit(discrete_inputs, 33, am2302Available);     // 10034
    setBit(discrete_inputs, 34, lm35Available);       // 10035
}

void ModbusRegisterManager::updateSystemStatus() {
    // System status (Discrete Inputs 10001-10020)
    setBit(discrete_inputs, ModbusRegisters::SYSTEM_RUNNING, simple_temp.getConfig().enabled);
    setBit(discrete_inputs, ModbusRegisters::COMPRESSOR_ON, simple_temp.isCompressorOn());
    setBit(discrete_inputs, ModbusRegisters::FAN_ON, simple_temp.isFanOn());
    setBit(discrete_inputs, ModbusRegisters::HEATER_ON, simple_temp.isHeaterOn());
    setBit(discrete_inputs, ModbusRegisters::TEMPERATURE_OK, simple_temp.isSensorValid());
    setBit(discrete_inputs, ModbusRegisters::NETWORK_CONNECTED, WiFi.isConnected());
    setBit(discrete_inputs, ModbusRegisters::EMERGENCY_STOP, simple_temp.isEmergencyStopped());
    
    // Manual mode status (Discrete Input 10013)
    setBit(discrete_inputs, 12, simple_temp.getConfig().mode == TEMP_MODE_MANUAL);
    
    // Relay states (Discrete Inputs 10041-10048)
    for (int i = 0; i < NUM_RELAYS && i < 8; i++) {
        setBit(discrete_inputs, 40 + i, relay_controller.getRelayState(i));
    }
    
    // Digital input states (Discrete Inputs 10051-10058)
    for (int i = 0; i < NUM_DIGITAL_INPUTS && i < 8; i++) {
        setBit(discrete_inputs, 50 + i, relay_controller.getInputState(i));
    }
}

void ModbusRegisterManager::updateConfiguration() {
    // Temperature control configuration (Holding Registers 40001-40030)
    holding_registers[ModbusRegisters::TEMP_SETPOINT] = floatToModbus(simple_temp.getConfig().setpoint);
    holding_registers[ModbusRegisters::TEMP_DELTA] = floatToModbus(simple_temp.getConfig().delta_temp);
    holding_registers[ModbusRegisters::CONTROL_MODE] = (uint16_t)simple_temp.getConfig().mode;
    holding_registers[ModbusRegisters::ACTIVE_SENSOR] = (uint16_t)currentTempSensorType;
    holding_registers[ModbusRegisters::EMERGENCY_HIGH] = floatToModbus(simple_temp.getConfig().emergency_high);
    holding_registers[ModbusRegisters::EMERGENCY_LOW] = floatToModbus(simple_temp.getConfig().emergency_low);
    
    // Relay assignments (Holding Registers 40031-40034)
    holding_registers[30] = (uint16_t)simple_temp.getConfig().compressor_relay;
    holding_registers[31] = (uint16_t)simple_temp.getConfig().heater_relay;
    holding_registers[32] = (uint16_t)simple_temp.getConfig().fan_relay;
    holding_registers[33] = (uint16_t)simple_temp.getConfig().aux_relay;
    
    // Timing protection (Holding Registers 40041-40042)
    holding_registers[40] = simple_temp.getConfig().min_on_time / 1000;  // Convert to seconds
    holding_registers[41] = simple_temp.getConfig().min_off_time / 1000;
    
    // Statistics (Holding Registers 40051-40055)
    holding_registers[50] = (uint16_t)(simple_temp.getConfig().total_runtime / 3600000); // Hours
    holding_registers[51] = (uint16_t)(simple_temp.getConfig().compressor_cycles & 0xFFFF);
    holding_registers[52] = floatToModbus(simple_temp.getConfig().min_temp_recorded);
    holding_registers[53] = floatToModbus(simple_temp.getConfig().max_temp_recorded);
}

void ModbusRegisterManager::updateDiagnostics() {
    // System information (Input Registers 30201-30230)
    unsigned long uptime = (system_start_time > 0) ? (millis() - system_start_time) : millis();
    input_registers[ModbusRegisters::SYSTEM_UPTIME_LOW] = (uint16_t)(uptime & 0xFFFF);
    input_registers[ModbusRegisters::SYSTEM_UPTIME_HIGH] = (uint16_t)((uptime >> 16) & 0xFFFF);
    input_registers[ModbusRegisters::FREE_HEAP] = (uint16_t)(ESP.getFreeHeap() / 1024); // KB
    
    // CPU temperature if available
    float cpu_temp = temperatureRead();
    input_registers[ModbusRegisters::CPU_TEMP] = floatToModbus(cpu_temp);
    
    // Network information (Input Registers 30241-30250)
    if (WiFi.isConnected()) {
        input_registers[240] = (uint16_t)WiFi.RSSI(); // WiFi signal strength
        IPAddress ip = WiFi.localIP();
        input_registers[241] = (ip[0] << 8) | ip[1]; // IP address high
        input_registers[242] = (ip[2] << 8) | ip[3]; // IP address low
    } else {
        input_registers[240] = 0;
        input_registers[241] = 0;
        input_registers[242] = 0;
    }
    
    // Modbus statistics (Input Registers 30361-30370)
    input_registers[ModbusRegisters::MODBUS_REQUESTS_LOW] = (uint16_t)(total_reads & 0xFFFF);
    input_registers[ModbusRegisters::MODBUS_REQUESTS_HIGH] = (uint16_t)((total_reads >> 16) & 0xFFFF);
    input_registers[362] = (uint16_t)(total_writes & 0xFFFF);
    input_registers[363] = (uint16_t)((total_writes >> 16) & 0xFFFF);
}

void ModbusRegisterManager::applyControlChanges() {
    // Apply coil changes to system controls
    bool compressor_ctrl = getBit(coils, ModbusRegisters::COMPRESSOR_CTRL);
    bool fan_ctrl = getBit(coils, ModbusRegisters::FAN_CTRL);
    bool heater_ctrl = getBit(coils, ModbusRegisters::HEATER_CTRL);
    bool system_enable = getBit(coils, ModbusRegisters::SYSTEM_ENABLE);
    bool manual_mode = getBit(coils, ModbusRegisters::MANUAL_MODE);
    
    // Apply system enable/disable
    if (system_enable != simple_temp.getConfig().enabled) {
        simple_temp.setEnabled(system_enable);
    }
    
    // Apply manual controls when in manual mode
    if (manual_mode && simple_temp.getConfig().mode == TEMP_MODE_MANUAL) {
        // Direct relay control in manual mode
        relay_controller.setRelay(simple_temp.getConfig().compressor_relay, compressor_ctrl);
        relay_controller.setRelay(simple_temp.getConfig().fan_relay, fan_ctrl);
        relay_controller.setRelay(simple_temp.getConfig().heater_relay, heater_ctrl);
    }
    
    // Emergency stop handling
    if (getBit(coils, 25)) { // Emergency stop coil (00026)
        simple_temp.emergencyStop();
        setBit(coils, 25, false); // Reset the coil after triggering
    }
    
    // Emergency clear handling
    if (getBit(coils, 26)) { // Emergency clear coil (00027)
        simple_temp.clearEmergency();
        setBit(coils, 26, false); // Reset the coil after triggering
    }
}

void ModbusRegisterManager::applyConfigurationChanges() {
    bool config_changed = false;
    
    // Temperature setpoint
    float new_setpoint = modbusToFloat(holding_registers[ModbusRegisters::TEMP_SETPOINT]);
    if (abs(new_setpoint - simple_temp.getConfig().setpoint) > 0.1f) {
        simple_temp.setSetpoint(new_setpoint);
        config_changed = true;
    }
    
    // Delta temperature
    float new_delta = modbusToFloat(holding_registers[ModbusRegisters::TEMP_DELTA]);
    if (abs(new_delta - simple_temp.getConfig().delta_temp) > 0.1f) {
        simple_temp.setDelta(new_delta);
        config_changed = true;
    }
    
    // Control mode
    TempControlMode new_mode = (TempControlMode)holding_registers[ModbusRegisters::CONTROL_MODE];
    if (new_mode != simple_temp.getConfig().mode && new_mode <= TEMP_MODE_MANUAL) {
        simple_temp.setMode(new_mode);
        config_changed = true;
    }
    
    // Active sensor selection
    int new_sensor = (int)holding_registers[ModbusRegisters::ACTIVE_SENSOR];
    if (new_sensor != currentTempSensorType && new_sensor >= 0 && new_sensor <= 4) {
        // Validate sensor is available before switching
        bool sensor_available = false;
        switch (new_sensor) {
            case TEMP_SENSOR_NONE: sensor_available = true; break; // Always allow "none"
            case TEMP_SENSOR_AM2302: sensor_available = am2302Available; break;
            case TEMP_SENSOR_DS18B20: sensor_available = ds18b20Available; break;
            case TEMP_SENSOR_LM35: sensor_available = lm35Available; break;
        }
        
        if (sensor_available) {
            setTemperatureSensorType(new_sensor);
            config_changed = true;
        }
    }
    
    // Emergency limits
    float new_high = modbusToFloat(holding_registers[ModbusRegisters::EMERGENCY_HIGH]);
    float new_low = modbusToFloat(holding_registers[ModbusRegisters::EMERGENCY_LOW]);
    if (abs(new_high - simple_temp.getConfig().emergency_high) > 0.1f || 
        abs(new_low - simple_temp.getConfig().emergency_low) > 0.1f) {
        simple_temp.setEmergencyLimits(new_low, new_high);
        config_changed = true;
    }
    
    // Auto-save configuration if enabled and changes were made
    if (config_changed && auto_save_config) {
        simple_temp.saveConfig();
    }
}

uint16_t ModbusRegisterManager::floatToModbus(float value, float scale) const {
    if (isnan(value) || isinf(value)) {
        return MODBUS_INVALID_VALUE;
    }
    
    int32_t scaled_value = (int32_t)(value * scale);
    
    // Clamp to 16-bit signed range
    if (scaled_value > 32767) scaled_value = 32767;
    if (scaled_value < -32768) scaled_value = -32768;
    
    return (uint16_t)scaled_value;
}

float ModbusRegisterManager::modbusToFloat(uint16_t value, float scale) const {
    if (value == MODBUS_INVALID_VALUE) {
        return NAN;
    }
    
    // Convert from unsigned to signed 16-bit
    int16_t signed_value = (int16_t)value;
    return (float)signed_value / scale;
}

uint16_t ModbusRegisterManager::timeToModbus(unsigned long time_ms) const {
    // Convert milliseconds to minutes, clamped to 16-bit range
    unsigned long minutes = time_ms / 60000;
    return (uint16_t)(minutes > 65535 ? 65535 : minutes);
}

// Read functions
bool ModbusRegisterManager::readCoils(uint16_t address, uint16_t quantity, uint8_t* data) {
    if (!data || address + quantity > MAX_COILS) {
        return false;
    }
    
    uint16_t byte_count = (quantity + 7) / 8;
    memset(data, 0, byte_count);
    
    for (uint16_t i = 0; i < quantity; i++) {
        if (getBit(coils, address + i)) {
            data[i / 8] |= (1 << (i % 8));
        }
    }
    
    total_reads++;
    return true;
}

bool ModbusRegisterManager::readDiscreteInputs(uint16_t address, uint16_t quantity, uint8_t* data) {
    if (!data || address + quantity > MAX_DISCRETE_INPUTS) {
        return false;
    }
    
    uint16_t byte_count = (quantity + 7) / 8;
    memset(data, 0, byte_count);
    
    for (uint16_t i = 0; i < quantity; i++) {
        if (getBit(discrete_inputs, address + i)) {
            data[i / 8] |= (1 << (i % 8));
        }
    }
    
    total_reads++;
    return true;
}

bool ModbusRegisterManager::readHoldingRegisters(uint16_t address, uint16_t quantity, uint16_t* data) {
    if (!data || address + quantity > MAX_HOLDING_REGISTERS) {
        return false;
    }
    
    // Copy registers directly (protocol handler manages byte order)
    for (uint16_t i = 0; i < quantity; i++) {
        data[i] = holding_registers[address + i];
    }
    
    total_reads++;
    return true;
}

bool ModbusRegisterManager::readInputRegisters(uint16_t address, uint16_t quantity, uint16_t* data) {
    if (!data || address + quantity > MAX_INPUT_REGISTERS) {
        return false;
    }
    
    // Copy registers directly (protocol handler manages byte order)
    for (uint16_t i = 0; i < quantity; i++) {
        data[i] = input_registers[address + i];
    }
    
    total_reads++;
    return true;
}

// Write functions
bool ModbusRegisterManager::writeCoil(uint16_t address, bool value) {
    if (address >= MAX_COILS) {
        return false;
    }
    
    setBit(coils, address, value);
    applyToSystem(); // Apply changes immediately
    
    total_writes++;
    return true;
}

bool ModbusRegisterManager::writeHoldingRegister(uint16_t address, uint16_t value) {
    if (address >= MAX_HOLDING_REGISTERS) {
        return false;
    }
    
    // Store value directly (protocol handler manages byte order)
    holding_registers[address] = value;
    applyToSystem(); // Apply changes immediately
    
    total_writes++;
    return true;
}

bool ModbusRegisterManager::writeMultipleCoils(uint16_t address, uint16_t quantity, const uint8_t* data) {
    if (!data || address + quantity > MAX_COILS) {
        return false;
    }
    
    for (uint16_t i = 0; i < quantity; i++) {
        bool value = (data[i / 8] & (1 << (i % 8))) != 0;
        setBit(coils, address + i, value);
    }
    
    applyToSystem(); // Apply changes immediately
    
    total_writes++;
    return true;
}

bool ModbusRegisterManager::writeMultipleRegisters(uint16_t address, uint16_t quantity, const uint16_t* data) {
    if (!data || address + quantity > MAX_HOLDING_REGISTERS) {
        return false;
    }
    
    for (uint16_t i = 0; i < quantity; i++) {
        // Store value directly (protocol handler manages byte order)
        holding_registers[address + i] = data[i];
    }
    
    applyToSystem(); // Apply changes immediately
    
    total_writes++;
    return true;
}

String ModbusRegisterManager::getRegisterDump(uint8_t register_type, uint16_t start_addr, uint16_t count) const {
    String dump = "Register Dump:\n";
    
    switch (register_type) {
        case 1: // Coils
            dump += "Coils " + String(start_addr + 1) + "-" + String(start_addr + count) + ":\n";
            for (uint16_t i = 0; i < count && (start_addr + i) < MAX_COILS; i++) {
                dump += String(start_addr + i + 1) + ": " + String(getBit(coils, start_addr + i) ? "ON" : "OFF") + "\n";
            }
            break;
            
        case 2: // Discrete Inputs
            dump += "Discrete Inputs " + String(start_addr + 10001) + "-" + String(start_addr + count + 10000) + ":\n";
            for (uint16_t i = 0; i < count && (start_addr + i) < MAX_DISCRETE_INPUTS; i++) {
                dump += String(start_addr + i + 10001) + ": " + String(getBit(discrete_inputs, start_addr + i) ? "ON" : "OFF") + "\n";
            }
            break;
            
        case 3: // Holding Registers
            dump += "Holding Registers " + String(start_addr + 40001) + "-" + String(start_addr + count + 40000) + ":\n";
            for (uint16_t i = 0; i < count && (start_addr + i) < MAX_HOLDING_REGISTERS; i++) {
                dump += String(start_addr + i + 40001) + ": " + String(holding_registers[start_addr + i]) + "\n";
            }
            break;
            
        case 4: // Input Registers
            dump += "Input Registers " + String(start_addr + 30001) + "-" + String(start_addr + count + 30000) + ":\n";
            for (uint16_t i = 0; i < count && (start_addr + i) < MAX_INPUT_REGISTERS; i++) {
                dump += String(start_addr + i + 30001) + ": " + String(input_registers[start_addr + i]) + "\n";
            }
            break;
    }
    
    return dump;
}

void ModbusRegisterManager::clearRegisters(uint8_t register_type) {
    switch (register_type) {
        case 1: // Coils
            memset(coils, 0, sizeof(coils));
            break;
        case 2: // Discrete Inputs
            memset(discrete_inputs, 0, sizeof(discrete_inputs));
            break;
        case 3: // Holding Registers
            memset(holding_registers, 0, sizeof(holding_registers));
            break;
        case 4: // Input Registers
            memset(input_registers, 0, sizeof(input_registers));
            break;
    }
}

uint32_t ModbusRegisterManager::getTotalReads() const {
    return total_reads;
}

uint32_t ModbusRegisterManager::getTotalWrites() const {
    return total_writes;
}