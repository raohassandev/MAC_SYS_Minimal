#ifndef MODBUS_REGISTER_MANAGER_H
#define MODBUS_REGISTER_MANAGER_H

#include <Arduino.h>
#include "modbus_protocol.h"

// Register array sizes (bits packed for coils/discrete inputs)
#define COIL_BYTES ((MAX_COILS + 7) / 8)
#define DISCRETE_INPUT_BYTES ((MAX_DISCRETE_INPUTS + 7) / 8)

// Invalid sensor value
#define MODBUS_INVALID_VALUE 0x7FFF

class ModbusRegisterManager {
private:
    // Register storage
    uint8_t coils[COIL_BYTES];                              // Coils 00001-00320 (bit-packed)
    uint8_t discrete_inputs[DISCRETE_INPUT_BYTES];          // Discrete Inputs 10001-10340 (bit-packed)
    uint16_t input_registers[MAX_INPUT_REGISTERS];          // Input Registers 30001-30400
    uint16_t holding_registers[MAX_HOLDING_REGISTERS];      // Holding Registers 40001-41050
    
    unsigned long last_system_update;
    unsigned long last_diagnostics_update;
    bool auto_save_config;
    
    // Bit manipulation helpers
    bool getBit(const uint8_t* array, uint16_t bit_index) const;
    void setBit(uint8_t* array, uint16_t bit_index, bool value);
    
    // System integration helpers
    void updateTemperatureSensors();
    void updateSystemStatus();
    void updateDiagnostics();
    void updateConfiguration();
    void applyControlChanges();
    void applyConfigurationChanges();
    
    // Data conversion helpers
    uint16_t floatToModbus(float value, float scale = 10.0f) const;
    float modbusToFloat(uint16_t value, float scale = 10.0f) const;
    uint16_t timeToModbus(unsigned long time_ms) const;
    
public:
    ModbusRegisterManager();
    
    // Initialization
    void begin();
    void updateFromSystem();
    void applyToSystem();
    
    // Read functions
    bool readCoils(uint16_t address, uint16_t quantity, uint8_t* data);
    bool readDiscreteInputs(uint16_t address, uint16_t quantity, uint8_t* data);
    bool readHoldingRegisters(uint16_t address, uint16_t quantity, uint16_t* data);
    bool readInputRegisters(uint16_t address, uint16_t quantity, uint16_t* data);
    
    // Write functions
    bool writeCoil(uint16_t address, bool value);
    bool writeHoldingRegister(uint16_t address, uint16_t value);
    bool writeMultipleCoils(uint16_t address, uint16_t quantity, const uint8_t* data);
    bool writeMultipleRegisters(uint16_t address, uint16_t quantity, const uint16_t* data);
    
    // Configuration
    void setAutoSaveConfig(bool enable) { auto_save_config = enable; }
    bool getAutoSaveConfig() const { return auto_save_config; }
    
    // Diagnostics
    String getRegisterDump(uint8_t register_type, uint16_t start_addr, uint16_t count) const;
    void clearRegisters(uint8_t register_type);
    
    // System health
    uint32_t getTotalReads() const;
    uint32_t getTotalWrites() const;
    unsigned long getLastUpdateTime() const { return last_system_update; }
};

// Register mapping constants from MODBUS_REGISTER_MAP.md
namespace ModbusRegisters {
    // Temperature Sensors (Input Registers 30001-30030)
    const uint16_t DS18B20_TEMP_1 = 0;          // 30001
    const uint16_t DS18B20_TEMP_2 = 1;          // 30002  
    const uint16_t DS18B20_TEMP_3 = 2;          // 30003
    const uint16_t AM2302_TEMP = 10;            // 30011
    const uint16_t AM2302_HUMIDITY = 11;        // 30012
    const uint16_t LM35_TEMP = 20;              // 30021
    
    // System Status (Discrete Inputs 10001-10050)
    const uint16_t SYSTEM_RUNNING = 0;          // 10001
    const uint16_t COMPRESSOR_ON = 1;           // 10002
    const uint16_t FAN_ON = 2;                  // 10003
    const uint16_t HEATER_ON = 3;               // 10004
    const uint16_t TEMPERATURE_OK = 10;         // 10011
    const uint16_t NETWORK_CONNECTED = 11;      // 10012
    const uint16_t EMERGENCY_STOP = 20;         // 10021
    
    // Control Coils (00001-00050)
    const uint16_t COMPRESSOR_CTRL = 0;         // 00001
    const uint16_t FAN_CTRL = 1;                // 00002
    const uint16_t HEATER_CTRL = 2;             // 00003
    const uint16_t SYSTEM_ENABLE = 10;          // 00011
    const uint16_t MANUAL_MODE = 11;            // 00012
    const uint16_t SCHEDULE_ENABLE = 20;        // 00021
    
    // Configuration (Holding Registers 40001-40100)
    const uint16_t TEMP_SETPOINT = 0;           // 40001
    const uint16_t TEMP_DELTA = 1;              // 40002
    const uint16_t CONTROL_MODE = 2;            // 40003
    const uint16_t ACTIVE_SENSOR = 10;          // 40011
    const uint16_t EMERGENCY_HIGH = 20;         // 40021
    const uint16_t EMERGENCY_LOW = 21;          // 40022
    
    // System Info (Input Registers 30201-30250)
    const uint16_t SYSTEM_UPTIME_LOW = 200;     // 30201
    const uint16_t SYSTEM_UPTIME_HIGH = 201;    // 30202
    const uint16_t FREE_HEAP = 210;             // 30211
    const uint16_t CPU_TEMP = 220;              // 30221
    
    // Diagnostics (Input Registers 30351-30400)
    const uint16_t LAST_ERROR = 350;            // 30351
    const uint16_t ERROR_COUNT = 351;           // 30352
    const uint16_t MODBUS_REQUESTS_LOW = 360;   // 30361
    const uint16_t MODBUS_REQUESTS_HIGH = 361;  // 30362
}

#endif // MODBUS_REGISTER_MANAGER_H