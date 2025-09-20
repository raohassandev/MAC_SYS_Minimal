#ifndef TEMPERATURE_H
#define TEMPERATURE_H

#include <Arduino.h>

// Temperature sensor types
#define TEMP_SENSOR_NONE 0
#define TEMP_SENSOR_AM2302 1
#define TEMP_SENSOR_DS18B20 2
#define TEMP_SENSOR_LM35 3

// Default sensor pin configuration (can be changed via web interface)
#define DEFAULT_AM2302_PIN 33      // Temperature + Humidity sensor
#define DEFAULT_DS18B20_PIN 32     // Waterproof temperature sensor  
#define DEFAULT_LM35_PIN 35        // Analog temperature sensor (ADC)

// EEPROM storage addresses
#define SENSOR_CONFIG_EEPROM_ADDR 544
#define SENSOR_CONFIG_MAGIC 0xA5C3  // Magic number to validate config

// Sensor configuration structure
struct SensorConfig {
    uint16_t magic;           // Magic number for validation
    
    // DS18B20 Configuration
    bool ds18b20_enabled;
    uint8_t ds18b20_pin;
    uint8_t ds18b20_priority; // 0=highest, 1=medium, 2=lowest
    
    // AM2302 Configuration  
    bool am2302_enabled;
    uint8_t am2302_pin;
    uint8_t am2302_priority;
    
    // LM35 Configuration
    bool lm35_enabled;
    uint8_t lm35_pin;
    uint8_t lm35_priority;
    
    // Advanced settings
    bool auto_fallback;       // Automatically fallback to next sensor on failure
    uint16_t read_interval;   // Temperature read interval in ms
    float temp_offset;        // Global temperature offset calibration
    
    uint8_t checksum;         // Simple checksum for data integrity
};

// Temperature control constants
#define TEMP_READ_INTERVAL 5000           // Read temperature every 5 seconds
#define DEFAULT_SETPOINT 25.0             // Default temperature setpoint
#define DEFAULT_DELTA_TEMP 1.0            // Default hysteresis
#define DEFAULT_DELIVERY_COMPENSATION 0.0 // Default sensor compensation

// Operation modes (from MAC_SYS specification)
#define OPERATION_MODE_DIRECT 0    // Direct Mode: Always use manual setpoint
#define OPERATION_MODE_SCHEDULE 1  // Schedule Mode: Follow schedules only

// Control modes
#define CONTROL_MODE_CENTRAL true  // Central control enabled
#define CONTROL_MODE_LOCAL false   // Local manual control

// Function prototypes
void initializeTemperatureSensors();
void testSensorAvailability();
void selectBestSensor();
float readTemperature();
bool isTemperatureSensorAvailable();
void setTemperatureSensorType(uint8_t type);

// Individual sensor functions
float readAM2302Temperature();
float readAM2302Humidity();
float readDS18B20Temperature(); 
float readLM35Temperature();

// Sensor availability
bool isAM2302Available();
bool isDS18B20Available();
bool isLM35Available();

// Temperature control
void runTemperatureControl();
bool shouldStartCooling(float currentTemp, float setpoint, float delta);
bool shouldStopCooling(float currentTemp, float setpoint, float delta);
float applyDeliveryCompensation(float rawTemp, float compensation);

// Individual sensor reading functions
float readDS18B20Temperature();
float readAM2302Temperature();
float readAM2302Humidity();
float readLM35Temperature();

// Sensor availability functions
bool isDS18B20Available();
bool isAM2302Available(); 
bool isLM35Available();

// Utility functions
const char* getTemperatureSensorName(uint8_t type);
void printTemperatureDebug();

// Sensor configuration functions
void loadSensorConfig();
void saveSensorConfig();
void resetSensorConfigToDefaults();
SensorConfig* getSensorConfig();
bool validateSensorConfig(const SensorConfig* config);
uint8_t calculateConfigChecksum(const SensorConfig* config);

// Dynamic sensor management
void applySensorConfiguration();
bool testSensorOnPin(uint8_t sensorType, uint8_t pin);
String getSensorTestResult(uint8_t sensorType, uint8_t pin);
void updateSensorPriorities();

// Debug functions for web interface
int getDS18B20DeviceCount();
bool getDS18B20ParasiticPower();
int getDS18B20Resolution();
const char* getAM2302StatusString();
int getAM2302StatusCode();
int getLM35ADCReading();

// Global variables
extern uint8_t currentTempSensorType;
extern float lastValidTemperature;
extern unsigned long lastTempReadTime;
extern bool temperatureSensorsInitialized;

// Sensor configuration
extern SensorConfig g_sensorConfig;

// Dynamic sensor pin assignments (updated from config)
extern uint8_t DS18B20_PIN;
extern uint8_t AM2302_PIN;
extern uint8_t LM35_PIN;

// Sensor availability flags
extern bool am2302Available;
extern bool ds18b20Available;
extern bool lm35Available;

#endif // TEMPERATURE_H
