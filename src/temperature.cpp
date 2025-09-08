#include "temperature.h"
#include "config.h"
#include "hardware.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <DHTesp.h>
#include <EEPROM.h>

// Global variables
uint8_t currentTempSensorType = TEMP_SENSOR_LM35; // Default to LM35 (always available)
float lastValidTemperature = 25.0;
unsigned long lastTempReadTime = 0;
bool temperatureSensorsInitialized = false;

// Sensor configuration
SensorConfig g_sensorConfig;

// Dynamic sensor pin assignments (updated from config)
uint8_t DS18B20_PIN = DEFAULT_DS18B20_PIN;
uint8_t AM2302_PIN = DEFAULT_AM2302_PIN;
uint8_t LM35_PIN = DEFAULT_LM35_PIN;

// Sensor availability flags
bool am2302Available = false;
bool ds18b20Available = false;
bool lm35Available = true; // LM35 is always available (analog)

// DS18B20 sensor setup - using pointers for dynamic initialization
OneWire* ds18b20Wire = nullptr;
DallasTemperature* ds18b20Sensor = nullptr;

// AM2302 (DHT22) sensor setup
DHTesp am2302Sensor;

void initializeTemperatureSensors() {
    DEBUG_PRINTLN("[TEMP] Initializing temperature sensors with configuration system...");
    
    // Load sensor configuration from EEPROM
    loadSensorConfig();
    
    // Apply the loaded configuration
    applySensorConfiguration();
    
    temperatureSensorsInitialized = true;
    DEBUG_PRINTF("[OK] Temperature sensors initialized. Primary sensor: %s\n", 
                 getTemperatureSensorName(currentTempSensorType));
}


void testSensorAvailability() {
    DEBUG_PRINTLN("Testing sensor availability...");
    
    // Test DS18B20 using working scanner method
    OneWire testWire(DS18B20_PIN);
    DallasTemperature testSensor(&testWire);
    testSensor.begin();
    
    delay(100); // Short delay for initialization
    int deviceCount = testSensor.getDeviceCount();
    DEBUG_PRINTF("🔍 DS18B20: Found %d device(s)\n", deviceCount);
    
    if (deviceCount > 0) {
        testSensor.requestTemperatures();
        delay(1000); // Full conversion delay
        float ds18b20Temp = testSensor.getTempCByIndex(0);
        
        if (ds18b20Temp != DEVICE_DISCONNECTED_C && ds18b20Temp > -40 && ds18b20Temp < 125) {
            ds18b20Available = true;
            DEBUG_PRINTF("[OK] DS18B20 (GPIO32): %.2f°C - WORKING\n", ds18b20Temp);
        } else {
            ds18b20Available = false;
            DEBUG_PRINTF("❌ DS18B20 (GPIO32): Invalid reading %.2f°C\n", ds18b20Temp);
        }
    } else {
        ds18b20Available = false;
        DEBUG_PRINTF("❌ DS18B20 (GPIO32): No devices found\n");
    }
    
    // Test AM2302 sensor (GPIO33) - quick initial test during boot
    delay(500); // Reduced delay for initial test
    float am2302Temp = am2302Sensor.getTemperature();
    float am2302Humidity = am2302Sensor.getHumidity();
    
    if (!isnan(am2302Temp) && !isnan(am2302Humidity) && am2302Temp > -40 && am2302Temp < 80 && am2302Sensor.getStatus() == DHTesp::ERROR_NONE) {
        am2302Available = true;
        DEBUG_PRINTF("[OK] AM2302 (GPIO33): %.1f°C, %.1f%% RH - WORKING\n", am2302Temp, am2302Humidity);
    } else {
        am2302Available = false;
        DEBUG_PRINTF("❌ AM2302 (GPIO33): No response or invalid data - %s\n", am2302Sensor.getStatusString());
    }
    
    // Test LM35 (ADC reading on GPIO35)
    int lm35Reading = analogRead(LM35_PIN);
    lm35Available = (lm35Reading > 100 && lm35Reading < 3000); // Reasonable ADC range
    DEBUG_PRINTF("📊 LM35 (GPIO35): ADC=%d - %s\n", 
                 lm35Reading, lm35Available ? "Available" : "Not connected");
    
    DEBUG_PRINTF("🔍 Final availability - DS18B20: %s, AM2302: %s, LM35: %s\n",
                 ds18b20Available ? "YES" : "NO",
                 am2302Available ? "YES" : "NO", 
                 lm35Available ? "YES" : "NO");
}

void selectBestSensor() {
    // Priority: DS18B20 > AM2302 > LM35 (prefer hardware sensors over analog)
    if (ds18b20Available) {
        currentTempSensorType = TEMP_SENSOR_DS18B20;
        DEBUG_PRINTLN("Selected DS18B20 as primary temperature sensor");
    } else if (am2302Available) {
        currentTempSensorType = TEMP_SENSOR_AM2302;
        DEBUG_PRINTLN("Selected AM2302 as primary temperature sensor");
    } else if (lm35Available) {
        currentTempSensorType = TEMP_SENSOR_LM35;
        DEBUG_PRINTLN("Selected LM35 as primary temperature sensor (fallback)");
    } else {
        currentTempSensorType = TEMP_SENSOR_NONE;
        DEBUG_PRINTLN("WARNING: No temperature sensors available!");
    }
}

float readTemperature() {
    // Rate limiting: only read every TEMP_READ_INTERVAL
    if (millis() - lastTempReadTime < TEMP_READ_INTERVAL) {
        return lastValidTemperature;
    }
    
    lastTempReadTime = millis();
    float temperature = NAN;
    
    // Read from current sensor
    switch (currentTempSensorType) {
        case TEMP_SENSOR_AM2302:
            temperature = readAM2302Temperature();
            break;
        case TEMP_SENSOR_DS18B20:
            temperature = readDS18B20Temperature();
            break;
        case TEMP_SENSOR_LM35:
            temperature = readLM35Temperature();
            break;
        default:
            DEBUG_PRINTLN("No temperature sensor configured");
            return lastValidTemperature;
    }
    
    // Validate reading
    if (!isnan(temperature) && temperature > -40.0 && temperature < 100.0) {
        lastValidTemperature = temperature;
        DEBUG_PRINTF("Temperature reading: %.2f°C from %s\n", 
                     temperature, getTemperatureSensorName(currentTempSensorType));
    } else {
        DEBUG_PRINTF("Invalid temperature reading from %s, using last valid: %.2f°C\n",
                     getTemperatureSensorName(currentTempSensorType), lastValidTemperature);
    }
    
    return lastValidTemperature;
}

float readAM2302Temperature() {
    if (!am2302Available) {
        return NAN;
    }
    
    // DHT22/AM2302 has minimum 2-second reading interval
    static unsigned long lastReading = 0;
    static float lastValidReading = NAN;
    
    unsigned long now = millis();
    if (now - lastReading < 2000) {
        // Return cached reading to avoid overwhelming the sensor
        return lastValidReading;
    }
    
    lastReading = now;
    
    // Read temperature from AM2302 sensor
    float temperature = am2302Sensor.getTemperature();
    
    if (isnan(temperature) || am2302Sensor.getStatus() != DHTesp::ERROR_NONE) {
        DEBUG_PRINTF("❌ AM2302: Failed to read temperature - %s\n", am2302Sensor.getStatusString());
        // Don't immediately mark as unavailable - could be temporary
        return lastValidReading;
    }
    
    // Validate reading range
    if (temperature < -40.0 || temperature > 80.0) {
        DEBUG_PRINTF("[WARNING] AM2302: Temperature out of range: %.1f°C\n", temperature);
        return lastValidReading;
    }
    
    // Valid reading
    lastValidReading = temperature;
    DEBUG_PRINTF("[TEMP] AM2302 REAL: %.1f°C\n", temperature);
    return temperature;
}

float readAM2302Humidity() {
    if (!am2302Available) {
        return NAN;
    }
    
    // DHT22/AM2302 has minimum 2-second reading interval
    static unsigned long lastReading = 0;
    static float lastValidReading = NAN;
    
    unsigned long now = millis();
    if (now - lastReading < 2000) {
        // Return cached reading
        return lastValidReading;
    }
    
    lastReading = now;
    
    // Read humidity from AM2302 sensor
    float humidity = am2302Sensor.getHumidity();
    
    if (isnan(humidity) || am2302Sensor.getStatus() != DHTesp::ERROR_NONE) {
        DEBUG_PRINTF("❌ AM2302: Failed to read humidity - %s\n", am2302Sensor.getStatusString());
        return lastValidReading;
    }
    
    // Validate reading range (0-100% RH)
    if (humidity < 0.0 || humidity > 100.0) {
        DEBUG_PRINTF("[WARNING] AM2302: Humidity out of range: %.1f%%\n", humidity);
        return lastValidReading;
    }
    
    // Valid reading
    lastValidReading = humidity;
    DEBUG_PRINTF("💧 AM2302 HUMIDITY: %.1f%%\n", humidity);
    return humidity;
}

float readDS18B20Temperature() {
    static bool firstRead = true;
    
    // Use the working scanner method for all readings
    OneWire tempWire(DS18B20_PIN);
    DallasTemperature tempSensor(&tempWire);
    tempSensor.begin();
    
    delay(100); // Short initialization delay
    int deviceCount = tempSensor.getDeviceCount();
    
    if (firstRead) {
        DEBUG_PRINTF("DS18B20: First read - found %d device(s)\n", deviceCount);
        firstRead = false;
    }
    
    if (deviceCount > 0) {
        DEBUG_PRINT("DS18B20: Requesting temperatures...");
        tempSensor.requestTemperatures();
        delay(1000); // Full conversion delay
        DEBUG_PRINTLN("DONE");

        float temperatureC = tempSensor.getTempCByIndex(0);

        if (temperatureC == DEVICE_DISCONNECTED_C) {
            DEBUG_PRINTLN("DS18B20: Error reading temperature data.");
            ds18b20Available = false;
            // Fallback to LM35 if DS18B20 fails
            if (currentTempSensorType == TEMP_SENSOR_DS18B20) {
                DEBUG_PRINTLN("DS18B20 failed - switching to LM35 fallback");
                currentTempSensorType = TEMP_SENSOR_LM35;
            }
            return NAN;
        } else {
            DEBUG_PRINTF("DS18B20: Temperature: %.1f *C\n", temperatureC);
            ds18b20Available = true;  // Confirm it's working
        }
        return temperatureC;
    } else {
        DEBUG_PRINTLN("DS18B20: No devices found during read");
        ds18b20Available = false;
        // Fallback to LM35 if DS18B20 not found
        if (currentTempSensorType == TEMP_SENSOR_DS18B20) {
            DEBUG_PRINTLN("DS18B20 not found - switching to LM35 fallback");
            currentTempSensorType = TEMP_SENSOR_LM35;
        }
        return NAN;
    }
}

float readLM35Temperature() {
    // LM35 not physically connected - always return NAN
    return NAN;
    
    int adcReading = analogRead(LM35_PIN);
    DEBUG_PRINTF("📊 LM35 ADC reading: %d\n", adcReading);
    
    // Check if we have a meaningful ADC reading
    if (adcReading > 0 && adcReading < 4095) {
        // Real LM35 sensor reading
        // ESP32 ADC: 12-bit (0-4095), 3.3V reference
        // LM35: 10mV per degree Celsius
        
        float voltage = (adcReading / 4095.0) * 3.3;
        float temperature = voltage * 100.0; // Convert to Celsius
        
        // Validate reading (LM35 range: -55°C to +150°C, but typically 0-50°C)
        if (temperature >= 0 && temperature <= 80.0) {
            DEBUG_PRINTF("[TEMP] LM35 REAL: %.1f°C (ADC=%d, V=%.3fV)\n", temperature, adcReading, voltage);
            return temperature;
        } else {
            DEBUG_PRINTF("[WARNING] LM35 out of range: %.1f°C - using fallback\n", temperature);
        }
    }
    
    // Fallback: Use room temperature simulation if no valid reading
    static float roomTemp = 24.0;
    static unsigned long lastRoomUpdate = 0;
    
    if (millis() - lastRoomUpdate > 15000) { // Slow room temperature drift
        roomTemp += random(-3, 4) / 10.0; // ±0.3°C slow variation
        roomTemp = constrain(roomTemp, 20.0, 28.0);
        lastRoomUpdate = millis();
    }
    
    DEBUG_PRINTF("🏠 Room temp fallback: %.1f°C (ADC=%d invalid)\n", roomTemp, adcReading);
    return roomTemp;
}

void runTemperatureControl() {
    // Get current temperature with delivery compensation
    float rawTemp = readTemperature();
    float compensatedTemp = applyDeliveryCompensation(rawTemp, g_system_config.delivery_compensation);
    
    // Get target setpoint based on operation mode
    float targetSetpoint = g_system_config.ac_setpoint;
    bool shouldProceedWithControl = true;
    
    DEBUG_PRINTF("[TEMP] CONTROL: Raw=%.1f°C, Compensated=%.1f°C, Target=%.1f°C\n",
                 rawTemp, compensatedTemp, targetSetpoint);
    
    // Operation mode logic (from MAC_SYS specification)
    if (g_system_config.operation_mode == OPERATION_MODE_DIRECT) {
        // Direct Mode: Always use manual setpoint
        targetSetpoint = g_system_config.ac_setpoint;
        shouldProceedWithControl = true;
        DEBUG_PRINTLN("🎯 DIRECT MODE: Using manual setpoint");
    } else {
        // Schedule Mode: Only operate when schedule is active
        // TODO: Implement schedule checking
        // For now, assume no schedules are active
        shouldProceedWithControl = false;
        DEBUG_PRINTLN("🎯 SCHEDULE MODE: No schedules implemented yet - AC OFF");
    }
    
    // Central vs Local control mode
    if (!g_system_config.central_control_mode) {
        // Local mode: Force compressor ON for manual control
        DEBUG_PRINTLN("📍 LOCAL MODE: Force compressor ON");
        if (!g_system_status.compressor_running) {
            setCompressorState(true);
        }
        return;
    }
    
    if (!g_system_config.ac_control_enabled) {
        // AC disabled: Force compressor OFF
        DEBUG_PRINTLN("📍 AC DISABLED: Force compressor OFF");
        if (g_system_status.compressor_running) {
            setCompressorState(false);
        }
        return;
    }
    
    // Temperature-based hysteresis control
    if (!shouldProceedWithControl) {
        // No control needed - turn off compressor
        if (g_system_status.compressor_running) {
            setCompressorState(false);
            DEBUG_PRINTLN("🚫 No control required - turning OFF compressor");
        }
        return;
    }
    
    // Hysteresis control logic
    float deltaTemp = g_system_config.delta_temperature;
    bool shouldCool = false;
    
    if (!g_system_status.compressor_running) {
        // Compressor OFF - check if we should turn it ON
        if (shouldStartCooling(compensatedTemp, targetSetpoint, deltaTemp)) {
            shouldCool = true;
            DEBUG_PRINTF("🔥 START COOLING: %.1f°C > %.1f°C\n", compensatedTemp, targetSetpoint);
        }
    } else {
        // Compressor ON - check if we should turn it OFF
        if (!shouldStopCooling(compensatedTemp, targetSetpoint, deltaTemp)) {
            shouldCool = true; // Continue cooling
            DEBUG_PRINTF("🔥 CONTINUE COOLING: %.1f°C > %.1f°C\n", 
                         compensatedTemp, targetSetpoint - deltaTemp);
        } else {
            DEBUG_PRINTF("❄️ STOP COOLING: %.1f°C <= %.1f°C\n", 
                         compensatedTemp, targetSetpoint - deltaTemp);
        }
    }
    
    // Apply control decision with compressor protection
    if (shouldCool && !g_system_status.compressor_running) {
        setCompressorState(true);
    } else if (!shouldCool && g_system_status.compressor_running) {
        setCompressorState(false);
    }
}

bool shouldStartCooling(float currentTemp, float setpoint, float delta) {
    // Start cooling when temperature exceeds setpoint
    return currentTemp > setpoint;
}

bool shouldStopCooling(float currentTemp, float setpoint, float delta) {
    // Stop cooling when temperature drops to (setpoint - delta)
    return currentTemp <= (setpoint - delta);
}

float applyDeliveryCompensation(float rawTemp, float compensation) {
    // Apply delivery compensation: compensated = raw + compensation
    return rawTemp + compensation;
}

const char* getTemperatureSensorName(uint8_t type) {
    switch (type) {
        case TEMP_SENSOR_AM2302: return "AM2302";
        case TEMP_SENSOR_DS18B20: return "DS18B20";
        case TEMP_SENSOR_LM35: return "LM35";
        default: return "None";
    }
}

bool isTemperatureSensorAvailable() {
    return temperatureSensorsInitialized && (currentTempSensorType != TEMP_SENSOR_NONE);
}

void setTemperatureSensorType(uint8_t type) {
    if (type <= TEMP_SENSOR_LM35) {
        currentTempSensorType = type;
        DEBUG_PRINTF("Temperature sensor changed to: %s\n", getTemperatureSensorName(type));
    }
}

bool isAM2302Available() { return am2302Available; }
bool isDS18B20Available() { return ds18b20Available; }
bool isLM35Available() { return lm35Available; }

void printTemperatureDebug() {
    DEBUG_PRINTLN("=== TEMPERATURE SENSOR STATUS ===");
    DEBUG_PRINTF("Primary Sensor: %s\n", getTemperatureSensorName(currentTempSensorType));
    DEBUG_PRINTF("Last Reading: %.2f°C\n", lastValidTemperature);
    DEBUG_PRINTF("Sensors Available: AM2302=%s, DS18B20=%s, LM35=%s\n",
                 am2302Available ? "YES" : "NO",
                 ds18b20Available ? "YES" : "NO",
                 lm35Available ? "YES" : "NO");
}

// Debug functions for web interface
int getDS18B20DeviceCount() {
    return ds18b20Sensor ? ds18b20Sensor->getDeviceCount() : 0;
}

bool getDS18B20ParasiticPower() {
    return ds18b20Sensor ? ds18b20Sensor->isParasitePowerMode() : false;
}

int getDS18B20Resolution() {
    return ds18b20Sensor ? ds18b20Sensor->getResolution() : 0;
}

const char* getAM2302StatusString() {
    return am2302Sensor.getStatusString();
}

int getAM2302StatusCode() {
    return am2302Sensor.getStatus();
}

int getLM35ADCReading() {
    return analogRead(LM35_PIN);
}

// ========================================
// SENSOR CONFIGURATION MANAGEMENT
// ========================================

uint8_t calculateConfigChecksum(const SensorConfig* config) {
    uint8_t checksum = 0;
    const uint8_t* data = (const uint8_t*)config;
    for (size_t i = 0; i < sizeof(SensorConfig) - 1; i++) { // -1 to exclude checksum field
        checksum ^= data[i];
    }
    return checksum;
}

void resetSensorConfigToDefaults() {
    DEBUG_PRINTLN("🔧 Resetting sensor configuration to defaults...");
    
    g_sensorConfig.magic = SENSOR_CONFIG_MAGIC;
    
    // DS18B20 defaults (highest priority)
    g_sensorConfig.ds18b20_enabled = true;
    g_sensorConfig.ds18b20_pin = DEFAULT_DS18B20_PIN;
    g_sensorConfig.ds18b20_priority = 0; // Highest priority
    
    // AM2302 defaults (medium priority)
    g_sensorConfig.am2302_enabled = false; // Disabled by default (placeholder)
    g_sensorConfig.am2302_pin = DEFAULT_AM2302_PIN;
    g_sensorConfig.am2302_priority = 1;
    
    // LM35 defaults (lowest priority - fallback)
    g_sensorConfig.lm35_enabled = true;
    g_sensorConfig.lm35_pin = DEFAULT_LM35_PIN;
    g_sensorConfig.lm35_priority = 2; // Lowest priority
    
    // Advanced settings
    g_sensorConfig.auto_fallback = true;
    g_sensorConfig.read_interval = TEMP_READ_INTERVAL;
    g_sensorConfig.temp_offset = 0.0;
    
    g_sensorConfig.checksum = calculateConfigChecksum(&g_sensorConfig);
    
    DEBUG_PRINTLN("[OK] Sensor configuration reset to defaults");
}

bool validateSensorConfig(const SensorConfig* config) {
    // Check magic number
    if (config->magic != SENSOR_CONFIG_MAGIC) {
        DEBUG_PRINTF("❌ Invalid config magic: 0x%04X (expected 0x%04X)\n", config->magic, SENSOR_CONFIG_MAGIC);
        return false;
    }
    
    // Check checksum
    uint8_t expectedChecksum = calculateConfigChecksum(config);
    if (config->checksum != expectedChecksum) {
        DEBUG_PRINTF("❌ Invalid config checksum: 0x%02X (expected 0x%02X)\n", config->checksum, expectedChecksum);
        return false;
    }
    
    // Validate GPIO pins (ESP32 specific)
    const uint8_t validPins[] = {2, 4, 5, 13, 14, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33, 34, 35, 36, 39};
    const size_t numValidPins = sizeof(validPins) / sizeof(validPins[0]);
    
    auto isValidPin = [&](uint8_t pin) {
        for (size_t i = 0; i < numValidPins; i++) {
            if (validPins[i] == pin) return true;
        }
        return false;
    };
    
    if (config->ds18b20_enabled && !isValidPin(config->ds18b20_pin)) {
        DEBUG_PRINTF("❌ Invalid DS18B20 pin: %d\n", config->ds18b20_pin);
        return false;
    }
    
    if (config->am2302_enabled && !isValidPin(config->am2302_pin)) {
        DEBUG_PRINTF("❌ Invalid AM2302 pin: %d\n", config->am2302_pin);
        return false;
    }
    
    if (config->lm35_enabled && !isValidPin(config->lm35_pin)) {
        DEBUG_PRINTF("❌ Invalid LM35 pin: %d\n", config->lm35_pin);
        return false;
    }
    
    // Check for pin conflicts
    if (config->ds18b20_enabled && config->am2302_enabled && config->ds18b20_pin == config->am2302_pin) {
        DEBUG_PRINTF("❌ Pin conflict: DS18B20 and AM2302 both using GPIO%d\n", config->ds18b20_pin);
        return false;
    }
    
    DEBUG_PRINTLN("[OK] Sensor configuration validation passed");
    return true;
}

void loadSensorConfig() {
    DEBUG_PRINTLN("📂 Loading sensor configuration from EEPROM...");
    
    EEPROM.get(SENSOR_CONFIG_EEPROM_ADDR, g_sensorConfig);
    
    if (!validateSensorConfig(&g_sensorConfig)) {
        DEBUG_PRINTLN("[WARNING] Invalid configuration found, resetting to defaults");
        resetSensorConfigToDefaults();
        saveSensorConfig(); // Save the defaults
    } else {
        DEBUG_PRINTLN("[OK] Valid configuration loaded from EEPROM");
        DEBUG_PRINTF("   DS18B20: %s on GPIO%d (priority %d)\n", 
                     g_sensorConfig.ds18b20_enabled ? "Enabled" : "Disabled",
                     g_sensorConfig.ds18b20_pin, g_sensorConfig.ds18b20_priority);
        DEBUG_PRINTF("   AM2302:  %s on GPIO%d (priority %d)\n", 
                     g_sensorConfig.am2302_enabled ? "Enabled" : "Disabled",
                     g_sensorConfig.am2302_pin, g_sensorConfig.am2302_priority);
        DEBUG_PRINTF("   LM35:    %s on GPIO%d (priority %d)\n", 
                     g_sensorConfig.lm35_enabled ? "Enabled" : "Disabled",
                     g_sensorConfig.lm35_pin, g_sensorConfig.lm35_priority);
    }
}

void saveSensorConfig() {
    DEBUG_PRINTLN("💾 Saving sensor configuration to EEPROM...");
    
    g_sensorConfig.checksum = calculateConfigChecksum(&g_sensorConfig);
    EEPROM.put(SENSOR_CONFIG_EEPROM_ADDR, g_sensorConfig);
    EEPROM.commit();
    
    DEBUG_PRINTLN("[OK] Sensor configuration saved");
}

SensorConfig* getSensorConfig() {
    return &g_sensorConfig;
}

void applySensorConfiguration() {
    DEBUG_PRINTLN("🔄 Applying sensor configuration...");
    
    // Update dynamic pin assignments
    DS18B20_PIN = g_sensorConfig.ds18b20_pin;
    AM2302_PIN = g_sensorConfig.am2302_pin;
    LM35_PIN = g_sensorConfig.lm35_pin;
    
    // Reset availability flags
    ds18b20Available = false;
    am2302Available = false;
    lm35Available = false;
    
    // Reinitialize sensors with new configuration
    if (g_sensorConfig.ds18b20_enabled) {
        // Reinitialize DS18B20 with new pin
        if (ds18b20Wire) delete ds18b20Wire;
        if (ds18b20Sensor) delete ds18b20Sensor;
        
        ds18b20Wire = new OneWire(DS18B20_PIN);
        ds18b20Sensor = new DallasTemperature(ds18b20Wire);
        ds18b20Sensor->begin();
        DEBUG_PRINTF("[SENSOR] DS18B20 initialized on GPIO%d\n", DS18B20_PIN);
    }
    
    if (g_sensorConfig.am2302_enabled) {
        am2302Sensor.setup(AM2302_PIN, DHTesp::DHT22);
        DEBUG_PRINTF("[SENSOR] AM2302 initialized on GPIO%d\n", AM2302_PIN);
    }
    
    if (g_sensorConfig.lm35_enabled) {
        lm35Available = true; // LM35 is always available (analog)
        DEBUG_PRINTF("[SENSOR] LM35 enabled on GPIO%d\n", LM35_PIN);
    }
    
    // Update sensor priorities
    updateSensorPriorities();
    
    DEBUG_PRINTLN("[OK] Sensor configuration applied");
}

void updateSensorPriorities() {
    DEBUG_PRINTLN("📊 Updating sensor priorities...");
    
    // Create array of enabled sensors with their priorities
    struct SensorPriority {
        uint8_t type;
        uint8_t priority;
        bool enabled;
    };
    
    SensorPriority sensors[] = {
        {TEMP_SENSOR_DS18B20, g_sensorConfig.ds18b20_priority, g_sensorConfig.ds18b20_enabled},
        {TEMP_SENSOR_AM2302, g_sensorConfig.am2302_priority, g_sensorConfig.am2302_enabled},
        {TEMP_SENSOR_LM35, g_sensorConfig.lm35_priority, g_sensorConfig.lm35_enabled}
    };
    
    // Find the highest priority enabled sensor
    uint8_t bestSensor = TEMP_SENSOR_NONE;
    uint8_t bestPriority = 255; // Lowest possible priority
    
    for (int i = 0; i < 3; i++) {
        if (sensors[i].enabled && sensors[i].priority < bestPriority) {
            bestPriority = sensors[i].priority;
            bestSensor = sensors[i].type;
        }
    }
    
    if (bestSensor != TEMP_SENSOR_NONE) {
        currentTempSensorType = bestSensor;
        DEBUG_PRINTF("🎯 Primary sensor set to: %s (priority %d)\n", 
                     getTemperatureSensorName(bestSensor), bestPriority);
    } else {
        currentTempSensorType = TEMP_SENSOR_NONE;
        DEBUG_PRINTLN("[WARNING] No sensors enabled!");
    }
}

String getSensorTestResult(uint8_t sensorType, uint8_t pin) {
    String result = "";
    bool testPassed = false;
    float temperature = NAN;
    float humidity = NAN;
    
    switch (sensorType) {
        case TEMP_SENSOR_DS18B20: {
            result += "DS18B20 on GPIO " + String(pin) + ": ";
            
            // Create temporary OneWire and DallasTemperature objects
            OneWire* testOneWire = new OneWire(pin);
            DallasTemperature* testSensor = new DallasTemperature(testOneWire);
            
            testSensor->begin();
            delay(100); // Allow sensor to initialize
            
            int deviceCount = testSensor->getDeviceCount();
            if (deviceCount > 0) {
                testSensor->requestTemperatures();
                delay(750); // Wait for conversion
                temperature = testSensor->getTempCByIndex(0);
                
                if (temperature != DEVICE_DISCONNECTED_C && temperature > -50 && temperature < 100) {
                    result += "[OK] " + String(temperature, 1) + "°C";
                    if (deviceCount > 1) {
                        result += " (" + String(deviceCount) + " devices)";
                    }
                    testPassed = true;
                } else {
                    result += "❌ Invalid reading: " + String(temperature, 1) + "°C";
                }
            } else {
                result += "❌ No DS18B20 devices found";
            }
            
            // Clean up
            delete testSensor;
            delete testOneWire;
            break;
        }
        
        case TEMP_SENSOR_AM2302: {
            result += "AM2302 on GPIO " + String(pin) + ": ";
            
            // Create temporary DHTesp object
            DHTesp* testDHT = new DHTesp();
            testDHT->setup(pin, DHTesp::DHT22);
            
            delay(2000); // DHT22 needs time to stabilize
            
            TempAndHumidity reading = testDHT->getTempAndHumidity();
            
            if (testDHT->getStatus() == DHTesp::ERROR_NONE) {
                temperature = reading.temperature;
                humidity = reading.humidity;
                
                if (!isnan(temperature) && !isnan(humidity) && 
                    temperature > -40 && temperature < 80 &&
                    humidity >= 0 && humidity <= 100) {
                    result += "[OK] " + String(temperature, 1) + "°C, " + String(humidity, 1) + "%RH";
                    testPassed = true;
                } else {
                    result += "❌ Invalid reading: " + String(temperature, 1) + "°C, " + String(humidity, 1) + "%RH";
                }
            } else {
                result += "❌ Sensor error: " + String(testDHT->getStatusString());
            }
            
            // Clean up
            delete testDHT;
            break;
        }
        
        case TEMP_SENSOR_LM35: {
            result += "LM35 on GPIO " + String(pin) + ": ";
            
            if (pin < 32 || pin > 39) {
                result += "❌ Invalid ADC pin (use 32-39)";
                break;
            }
            
            // Take multiple ADC readings for stability
            int totalReading = 0;
            int validReadings = 0;
            
            for (int i = 0; i < 10; i++) {
                int adcValue = analogRead(pin);
                if (adcValue > 0) {
                    totalReading += adcValue;
                    validReadings++;
                }
                delay(50);
            }
            
            if (validReadings > 0) {
                int avgReading = totalReading / validReadings;
                // LM35: 10mV/°C, ESP32 ADC: 0-4095 for 0-3.3V
                temperature = (avgReading * 3.3 * 100.0) / 4095.0;
                
                if (temperature >= 0 && temperature <= 100) {
                    result += "[OK] " + String(temperature, 1) + "°C (ADC: " + String(avgReading) + ")";
                    testPassed = true;
                } else {
                    result += "[WARNING] " + String(temperature, 1) + "°C (ADC: " + String(avgReading) + ") - Check wiring";
                }
            } else {
                result += "❌ No ADC readings obtained";
            }
            break;
        }
        
        default:
            result = "❌ Unknown sensor type: " + String(sensorType);
            break;
    }
    
    return result;
}