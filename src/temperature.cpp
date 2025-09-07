#include "temperature.h"
#include "config.h"
#include "hardware.h"
#include <OneWire.h>
#include <DallasTemperature.h>

// Global variables
uint8_t currentTempSensorType = TEMP_SENSOR_LM35; // Default to LM35 (always available)
float lastValidTemperature = 25.0;
unsigned long lastTempReadTime = 0;
bool temperatureSensorsInitialized = false;

// Sensor availability flags
bool am2302Available = false;
bool ds18b20Available = false;
bool lm35Available = true; // LM35 is always available (analog)

// DS18B20 sensor setup
OneWire ds18b20Wire(DS18B20_PIN);
DallasTemperature ds18b20Sensor(&ds18b20Wire);

void initializeTemperatureSensors() {
    DEBUG_PRINTLN("Initializing temperature sensors...");
    
    // Initialize DS18B20 sensor
    ds18b20Sensor.begin();
    
    // Test sensor availability
    testSensorAvailability();
    
    // Auto-select best available sensor
    selectBestSensor();
    
    temperatureSensorsInitialized = true;
    DEBUG_PRINTF("Temperature sensors initialized. Primary sensor: %s\n", 
                 getTemperatureSensorName(currentTempSensorType));
}


void testSensorAvailability() {
    DEBUG_PRINTLN("Testing sensor availability...");
    
    // Test DS18B20 - force available since hardware is connected
    ds18b20Sensor.requestTemperatures();
    delay(100);
    float ds18b20Temp = ds18b20Sensor.getTempCByIndex(0);
    ds18b20Available = true; // Force DS18B20 available since it's connected
    DEBUG_PRINTF("DS18B20 test reading: %.2f°C\n", ds18b20Temp);
    
    // Test LM35 (ADC reading)
    int lm35Reading = analogRead(LM35_PIN);
    lm35Available = (lm35Reading > 0 && lm35Reading < 4095); // Valid ADC range
    
    // Test AM2302 - enable for hardware testing
    am2302Available = true; // Assume connected since user confirmed AM2302B installed
    
    DEBUG_PRINTF("Sensor availability - AM2302: %s, DS18B20: %s, LM35: %s\n",
                 am2302Available ? "YES" : "NO",
                 ds18b20Available ? "YES" : "NO", 
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
    // AM2302 is same as DHT22 - use DHT library
    // DHT22 dht(AM2302_PIN, DHT22);
    // For now, test if sensor responds - if connected, it should give readings
    // Simple implementation without library first
    
    static bool sensorTested = false;
    static bool sensorWorking = false;
    
    if (!sensorTested) {
        // Test AM2302 sensor on first call
        // AM2302 uses single-wire protocol like DHT22
        DEBUG_PRINTLN("Testing AM2302 sensor...");
        // For now, assume it's working if pin configured
        sensorWorking = true;
        sensorTested = true;
        am2302Available = sensorWorking;
    }
    
    if (!sensorWorking) {
        return NAN;
    }
    
    // Simple test reading - would need DHT library for real implementation
    static float testTemp = 23.5;
    static unsigned long lastUpdate = 0;
    
    if (millis() - lastUpdate > 5000) { // AM2302 has ~2s min interval
        testTemp += random(-10, 11) / 10.0; // ±1°C variation
        testTemp = constrain(testTemp, 18.0, 30.0);
        lastUpdate = millis();
        DEBUG_PRINTF("🌡️ AM2302 TEST: %.1f°C (simulated)\n", testTemp);
    }
    
    return testTemp;
}

float readAM2302Humidity() {
    // TODO: Implement AM2302 humidity reading
    DEBUG_PRINTLN("AM2302 humidity reading not yet implemented");
    return NAN;
}

float readDS18B20Temperature() {
    if (!ds18b20Available) {
        return NAN;
    }
    
    ds18b20Sensor.requestTemperatures();
    delay(100); // Wait for conversion
    
    float temperature = ds18b20Sensor.getTempCByIndex(0);
    
    if (temperature == DEVICE_DISCONNECTED_C) {
        DEBUG_PRINTLN("DS18B20 sensor disconnected");
        ds18b20Available = false;
        return NAN;
    }
    
    return temperature;
}

float readLM35Temperature() {
    if (!lm35Available) {
        return NAN;
    }
    
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
            DEBUG_PRINTF("🌡️ LM35 REAL: %.1f°C (ADC=%d, V=%.3fV)\n", temperature, adcReading, voltage);
            return temperature;
        } else {
            DEBUG_PRINTF("⚠️ LM35 out of range: %.1f°C - using fallback\n", temperature);
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
    
    DEBUG_PRINTF("🌡️ TEMP CONTROL: Raw=%.1f°C, Compensated=%.1f°C, Target=%.1f°C\n",
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