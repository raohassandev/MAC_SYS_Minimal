#include "temperature_control.h"
#include "config.h"
#include <EEPROM.h>

// EEPROM address for temperature control config
#define TEMP_CONFIG_ADDR 256

// Global temperature controller instance
TemperatureController temp_controller;

TemperatureController::TemperatureController() {
    // Initialize with default values
    config.emergency_high = 50.0;  // 50°C emergency high
    config.emergency_low = -10.0;  // -10°C emergency low
    config.emergency_stop = false;
    config.last_update = 0;
    
    // Initialize zones with defaults
    for (int i = 0; i < 4; i++) {
        config.zones[i].enabled = false;
        config.zones[i].setpoint = 22.0;  // 22°C default
        config.zones[i].delta = 1.0;      // ±1°C hysteresis
        config.zones[i].compensation = 0.0;
        config.zones[i].mode = TEMP_MODE_OFF;
        config.zones[i].relay_mask = (1 << i);  // Default: zone controls its relay
        config.zones[i].min_on_time = 60000;    // 1 minute minimum on
        config.zones[i].min_off_time = 60000;   // 1 minute minimum off
        config.zones[i].last_change = 0;
        config.zones[i].current_state = false;
        snprintf(config.zones[i].name, sizeof(config.zones[i].name), "Zone %d", i + 1);
        
        current_temps[i] = 20.0;
        sensor_valid[i] = false;
    }
}

void TemperatureController::begin() {
    DEBUG_PRINTLN("Initializing Temperature Controller...");
    loadConfig();
    DEBUG_PRINTLN("[OK] Temperature Controller initialized");
}

void TemperatureController::loadConfig() {
    // Check if EEPROM has valid config
    uint16_t checksum;
    EEPROM.get(TEMP_CONFIG_ADDR, checksum);
    
    if (checksum == 0x5A5A) {  // Valid config marker
        EEPROM.get(TEMP_CONFIG_ADDR + 2, config);
        DEBUG_PRINTLN("[OK] Temperature config loaded from EEPROM");
    } else {
        DEBUG_PRINTLN("[WARNING] No valid temperature config in EEPROM, using defaults");
        saveConfig();  // Save defaults
    }
}

void TemperatureController::saveConfig() {
    uint16_t checksum = 0x5A5A;  // Config validity marker
    EEPROM.put(TEMP_CONFIG_ADDR, checksum);
    EEPROM.put(TEMP_CONFIG_ADDR + 2, config);
    EEPROM.commit();
    DEBUG_PRINTLN("[OK] Temperature config saved to EEPROM");
}

void TemperatureController::updateTemperature(uint8_t zone, float temp) {
    if (zone >= 4) return;
    
    current_temps[zone] = temp;
    sensor_valid[zone] = (temp > -50.0 && temp < 100.0);  // Basic validity check
    
    // Check emergency limits
    if (sensor_valid[zone]) {
        if (temp >= config.emergency_high || temp <= config.emergency_low) {
            emergencyStop();
            DEBUG_PRINTF("🚨 EMERGENCY STOP! Zone %d temp: %.1f°C\n", zone, temp);
        }
    }
}

void TemperatureController::updateAllTemperatures(float* temps) {
    for (int i = 0; i < 4; i++) {
        updateTemperature(i, temps[i]);
    }
}

bool TemperatureController::canChangeState(uint8_t zone) {
    if (zone >= 4) return false;
    
    ZoneConfig& z = config.zones[zone];
    unsigned long now = millis();
    unsigned long elapsed = now - z.last_change;
    
    if (z.current_state) {
        // Currently ON - check minimum on time
        return elapsed >= z.min_on_time;
    } else {
        // Currently OFF - check minimum off time
        return elapsed >= z.min_off_time;
    }
}

bool TemperatureController::shouldHeat(uint8_t zone) {
    if (zone >= 4 || !config.zones[zone].enabled || !sensor_valid[zone]) {
        return false;
    }
    
    ZoneConfig& z = config.zones[zone];
    float temp = getCompensatedTemp(zone);
    
    if (z.current_state) {
        // Currently heating - turn off if above setpoint + delta
        return temp < (z.setpoint + z.delta);
    } else {
        // Currently not heating - turn on if below setpoint - delta
        return temp < (z.setpoint - z.delta);
    }
}

bool TemperatureController::shouldCool(uint8_t zone) {
    if (zone >= 4 || !config.zones[zone].enabled || !sensor_valid[zone]) {
        return false;
    }
    
    ZoneConfig& z = config.zones[zone];
    float temp = getCompensatedTemp(zone);
    
    if (z.current_state) {
        // Currently cooling - turn off if below setpoint - delta
        return temp > (z.setpoint - z.delta);
    } else {
        // Currently not cooling - turn on if above setpoint + delta
        return temp > (z.setpoint + z.delta);
    }
}

bool TemperatureController::evaluateAutoMode(uint8_t zone) {
    if (zone >= 4 || !config.zones[zone].enabled || !sensor_valid[zone]) {
        return false;
    }
    
    float temp = getCompensatedTemp(zone);
    float setpoint = config.zones[zone].setpoint;
    
    // Simple auto mode: heat if too cold, cool if too hot
    if (temp < setpoint - 2.0) {
        // Needs heating
        return shouldHeat(zone);
    } else if (temp > setpoint + 2.0) {
        // Needs cooling
        return shouldCool(zone);
    }
    
    return false;  // In deadband - no action needed
}

void TemperatureController::process() {
    if (config.emergency_stop) {
        // Emergency stop - turn off all relays
        relay_controller.setAllRelays(0x00);
        return;
    }
    
    unsigned long now = millis();
    
    // Process each zone
    for (uint8_t zone = 0; zone < 4; zone++) {
        ZoneConfig& z = config.zones[zone];
        
        if (!z.enabled || !sensor_valid[zone]) {
            continue;
        }
        
        bool should_activate = false;
        
        // Evaluate based on mode
        switch (z.mode) {
            case TEMP_MODE_OFF:
                should_activate = false;
                break;
                
            case TEMP_MODE_HEATING:
                should_activate = shouldHeat(zone);
                break;
                
            case TEMP_MODE_COOLING:
                should_activate = shouldCool(zone);
                break;
                
            case TEMP_MODE_AUTO:
                should_activate = evaluateAutoMode(zone);
                break;
                
            case TEMP_MODE_MANUAL:
                // Manual mode - don't change state automatically
                should_activate = z.current_state;
                break;
        }
        
        // Apply state change if needed and allowed
        if (should_activate != z.current_state && canChangeState(zone)) {
            z.current_state = should_activate;
            z.last_change = now;
            
            // Control relays based on zone's relay mask
            for (uint8_t relay = 0; relay < 6; relay++) {
                if (z.relay_mask & (1 << relay)) {
                    relay_controller.setRelay(relay, should_activate);
                }
            }
            
            DEBUG_PRINTF("[TEMP] Zone %d: %s (Temp: %.1f°C, Setpoint: %.1f°C)\n", 
                        zone, should_activate ? "ON" : "OFF", 
                        getCompensatedTemp(zone), z.setpoint);
        }
    }
    
    config.last_update = now;
}

void TemperatureController::emergencyStop() {
    config.emergency_stop = true;
    relay_controller.setAllRelays(0x00);
    DEBUG_PRINTLN("🚨 EMERGENCY STOP ACTIVATED!");
}

void TemperatureController::clearEmergency() {
    config.emergency_stop = false;
    DEBUG_PRINTLN("[OK] Emergency stop cleared");
}

void TemperatureController::setZoneConfig(uint8_t zone, ZoneConfig& cfg) {
    if (zone >= 4) return;
    config.zones[zone] = cfg;
    saveConfig();
}

ZoneConfig& TemperatureController::getZoneConfig(uint8_t zone) {
    if (zone >= 4) zone = 0;
    return config.zones[zone];
}

void TemperatureController::setSetpoint(uint8_t zone, float setpoint) {
    if (zone >= 4) return;
    config.zones[zone].setpoint = constrain(setpoint, 5.0, 40.0);
    saveConfig();
}

void TemperatureController::setDelta(uint8_t zone, float delta) {
    if (zone >= 4) return;
    config.zones[zone].delta = constrain(delta, 0.5, 5.0);
    saveConfig();
}

void TemperatureController::setMode(uint8_t zone, TempControlMode mode) {
    if (zone >= 4) return;
    config.zones[zone].mode = mode;
    config.zones[zone].last_change = millis();  // Reset timer on mode change
    saveConfig();
}

void TemperatureController::setCompensation(uint8_t zone, float comp) {
    if (zone >= 4) return;
    config.zones[zone].compensation = constrain(comp, -10.0, 10.0);
    saveConfig();
}

float TemperatureController::getCurrentTemp(uint8_t zone) {
    if (zone >= 4) return 0.0;
    return current_temps[zone];
}

float TemperatureController::getCompensatedTemp(uint8_t zone) {
    if (zone >= 4) return 0.0;
    return current_temps[zone] + config.zones[zone].compensation;
}

bool TemperatureController::isSensorValid(uint8_t zone) {
    if (zone >= 4) return false;
    return sensor_valid[zone];
}

String TemperatureController::getZoneStatus(uint8_t zone) {
    if (zone >= 4) return "Invalid";
    
    ZoneConfig& z = config.zones[zone];
    String status = String(z.name) + ": ";
    
    if (!z.enabled) {
        status += "Disabled";
    } else if (!sensor_valid[zone]) {
        status += "Sensor Error";
    } else {
        status += "Temp=" + String(getCompensatedTemp(zone), 1) + "°C";
        status += ", Set=" + String(z.setpoint, 1) + "°C";
        status += ", Mode=";
        
        switch (z.mode) {
            case TEMP_MODE_OFF: status += "OFF"; break;
            case TEMP_MODE_HEATING: status += "HEAT"; break;
            case TEMP_MODE_COOLING: status += "COOL"; break;
            case TEMP_MODE_AUTO: status += "AUTO"; break;
            case TEMP_MODE_MANUAL: status += "MANUAL"; break;
        }
        
        status += ", State=" + String(z.current_state ? "ON" : "OFF");
    }
    
    return status;
}

String TemperatureController::getSystemStatus() {
    String status = "Temperature Control System\n";
    
    if (config.emergency_stop) {
        status += "EMERGENCY STOP ACTIVE!\n";
    }
    
    for (uint8_t i = 0; i < 4; i++) {
        status += getZoneStatus(i) + "\n";
    }
    
    return status;
}

void TemperatureController::setEmergencyLimits(float low, float high) {
    config.emergency_low = low;
    config.emergency_high = high;
    saveConfig();
}

void TemperatureController::setMinTimes(uint8_t zone, unsigned long min_on, unsigned long min_off) {
    if (zone >= 4) return;
    config.zones[zone].min_on_time = min_on;
    config.zones[zone].min_off_time = min_off;
    saveConfig();
}