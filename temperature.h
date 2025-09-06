#ifndef TEMPERATURE_H
#define TEMPERATURE_H

#include <Arduino.h>

// Temperature sensor types
#define TEMP_SENSOR_NONE 0
#define TEMP_SENSOR_AM2302 1
#define TEMP_SENSOR_DS18B20 2
#define TEMP_SENSOR_LM35 3

// Sensor pin configuration (from MAC_SYS standards)
#define AM2302_PIN 33      // Temperature + Humidity sensor
#define DS18B20_PIN 32     // Waterproof temperature sensor
#define LM35_PIN 35        // Analog temperature sensor (ADC)

// Temperature control constants
#define MIN_COMPRESSOR_CYCLE_TIME 180000  // 3 minutes minimum cycle time
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

// Utility functions
const char* getTemperatureSensorName(uint8_t type);
void printTemperatureDebug();

// Global variables
extern uint8_t currentTempSensorType;
extern float lastValidTemperature;
extern unsigned long lastTempReadTime;
extern bool temperatureSensorsInitialized;

// Sensor availability flags
extern bool am2302Available;
extern bool ds18b20Available;
extern bool lm35Available;

#endif // TEMPERATURE_H