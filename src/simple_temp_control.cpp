#include "simple_temp_control.h"
#include "config.h"
#include <EEPROM.h>
#include <ArduinoJson.h>

#define SIMPLE_TEMP_CONFIG_ADDR 640

SimpleTempController simple_temp;

SimpleTempController::SimpleTempController() {
    resetToDefaults();
}

void SimpleTempController::resetToDefaults() {
    strcpy(config.location_name, "Main Unit");
    config.setpoint = 24.0;
    config.delta_temp = 1.0;
    config.delivery_compensation = 0.0;
    config.mode = TEMP_MODE_OFF;
    config.enabled = false;
    
    config.emergency_high = 50.0;
    config.emergency_low = -10.0;
    
    config.min_on_time = 180000;  // 3 minutes
    config.min_off_time = 180000; // 3 minutes
    
    config.compressor_relay = 0;
    config.heater_relay = 1;
    config.fan_relay = 2;
    config.aux_relay = 3;
    
    config.last_compressor_change = 0;
    config.last_heater_change = 0;
    config.compressor_state = false;
    config.heater_state = false;
    config.fan_state = false;
    config.emergency_stop = false;
    
    config.total_runtime = 0;
    config.compressor_cycles = 0;
    config.min_temp_recorded = 100.0;
    config.max_temp_recorded = -100.0;
    
    current_temp = 20.0;
    compensated_temp = 20.0;
    sensor_valid = false;
    last_update = 0;
}

void SimpleTempController::begin() {
    DEBUG_PRINTLN("Initializing Simple Temperature Controller...");
    loadConfig();
    DEBUG_PRINTLN("[OK] Simple Temperature Controller initialized");
}

void SimpleTempController::loadConfig() {
    uint16_t checksum;
    EEPROM.get(SIMPLE_TEMP_CONFIG_ADDR, checksum);
    
    if (checksum == 0x7A7A) {
        EEPROM.get(SIMPLE_TEMP_CONFIG_ADDR + 2, config);
        DEBUG_PRINTLN("[OK] Simple temp config loaded from EEPROM");
    } else {
        DEBUG_PRINTLN("[WARNING] No valid config, using defaults");
        saveConfig();
    }
}

void SimpleTempController::saveConfig() {
    uint16_t checksum = 0x7A7A;
    EEPROM.put(SIMPLE_TEMP_CONFIG_ADDR, checksum);
    EEPROM.put(SIMPLE_TEMP_CONFIG_ADDR + 2, config);
    EEPROM.commit();
    DEBUG_PRINTLN("[OK] Simple temp config saved to EEPROM");
}

void SimpleTempController::updateTemperature(float temp) {
    current_temp = temp;
    sensor_valid = (temp > -50.0 && temp < 100.0);
    
    if (sensor_valid) {
        compensated_temp = temp + config.delivery_compensation;
        
        // Update statistics
        if (compensated_temp < config.min_temp_recorded) {
            config.min_temp_recorded = compensated_temp;
        }
        if (compensated_temp > config.max_temp_recorded) {
            config.max_temp_recorded = compensated_temp;
        }
        
        checkEmergencyLimits();
    }
}

void SimpleTempController::checkEmergencyLimits() {
    if (compensated_temp >= config.emergency_high || compensated_temp <= config.emergency_low) {
        emergencyStop();
        DEBUG_PRINTF("🚨 EMERGENCY! Temp: %.1f°C\n", compensated_temp);
    }
}

bool SimpleTempController::canChangeCompressor() {
    unsigned long now = millis();
    unsigned long elapsed = now - config.last_compressor_change;
    
    if (config.compressor_state) {
        return elapsed >= config.min_on_time;
    } else {
        return elapsed >= config.min_off_time;
    }
}

bool SimpleTempController::canChangeHeater() {
    unsigned long now = millis();
    unsigned long elapsed = now - config.last_heater_change;
    
    if (config.heater_state) {
        return elapsed >= config.min_on_time;
    } else {
        return elapsed >= config.min_off_time;
    }
}

bool SimpleTempController::shouldStartCooling() {
    return compensated_temp > config.setpoint;
}

bool SimpleTempController::shouldStopCooling() {
    return compensated_temp <= (config.setpoint - config.delta_temp);
}

bool SimpleTempController::shouldStartHeating() {
    return compensated_temp < config.setpoint;
}

bool SimpleTempController::shouldStopHeating() {
    return compensated_temp >= (config.setpoint + config.delta_temp);
}

void SimpleTempController::process() {
    if (!config.enabled || config.emergency_stop || !sensor_valid) {
        if (config.emergency_stop) {
            forceOff();
        }
        return;
    }
    
    unsigned long now = millis();
    last_update = now;
    
    bool want_compressor = false;
    bool want_heater = false;
    bool want_fan = false;
    
    switch (config.mode) {
        case TEMP_MODE_OFF:
            want_compressor = false;
            want_heater = false;
            want_fan = false;
            break;
            
        case TEMP_MODE_COOLING:
            if (config.compressor_state) {
                want_compressor = !shouldStopCooling();
            } else {
                want_compressor = shouldStartCooling();
            }
            want_fan = want_compressor;
            break;
            
        case TEMP_MODE_HEATING:
            if (config.heater_state) {
                want_heater = !shouldStopHeating();
            } else {
                want_heater = shouldStartHeating();
            }
            want_fan = want_heater;
            break;
            
        case TEMP_MODE_AUTO:
            if (compensated_temp > config.setpoint + 2.0) {
                // Too hot - need cooling
                if (config.compressor_state) {
                    want_compressor = !shouldStopCooling();
                } else {
                    want_compressor = shouldStartCooling();
                }
                want_fan = want_compressor;
            } else if (compensated_temp < config.setpoint - 2.0) {
                // Too cold - need heating
                if (config.heater_state) {
                    want_heater = !shouldStopHeating();
                } else {
                    want_heater = shouldStartHeating();
                }
                want_fan = want_heater;
            }
            break;
            
        case TEMP_MODE_FAN_ONLY:
            want_fan = true;
            break;
            
        case TEMP_MODE_MANUAL:
            // Manual mode - maintain current states
            want_compressor = config.compressor_state;
            want_heater = config.heater_state;
            want_fan = config.fan_state;
            break;
    }
    
    // Apply changes with timing protection
    if (want_compressor != config.compressor_state && canChangeCompressor()) {
        config.compressor_state = want_compressor;
        config.last_compressor_change = now;
        if (want_compressor) {
            config.compressor_cycles++;
        }
        DEBUG_PRINTF("Compressor: %s\n", want_compressor ? "ON" : "OFF");
    }
    
    if (want_heater != config.heater_state && canChangeHeater()) {
        config.heater_state = want_heater;
        config.last_heater_change = now;
        DEBUG_PRINTF("Heater: %s\n", want_heater ? "ON" : "OFF");
    }
    
    config.fan_state = want_fan;
    
    // Update runtime statistics
    if (config.compressor_state || config.heater_state) {
        config.total_runtime += (now - last_update);
    }
    
    updateRelays();
}

void SimpleTempController::updateRelays() {
    relay_controller.setRelay(config.compressor_relay, config.compressor_state);
    relay_controller.setRelay(config.heater_relay, config.heater_state);
    relay_controller.setRelay(config.fan_relay, config.fan_state);
}

void SimpleTempController::emergencyStop() {
    config.emergency_stop = true;
    forceOff();
    DEBUG_PRINTLN("🚨 EMERGENCY STOP ACTIVATED!");
}

void SimpleTempController::clearEmergency() {
    config.emergency_stop = false;
    DEBUG_PRINTLN("[OK] Emergency stop cleared");
}

void SimpleTempController::forceOff() {
    config.compressor_state = false;
    config.heater_state = false;
    config.fan_state = false;
    updateRelays();
}

void SimpleTempController::setLocationName(const char* name) {
    strncpy(config.location_name, name, sizeof(config.location_name) - 1);
    config.location_name[sizeof(config.location_name) - 1] = '\0';
    saveConfig();
}

void SimpleTempController::setSetpoint(float temp) {
    config.setpoint = constrain(temp, 5.0, 40.0);
    saveConfig();
}

void SimpleTempController::setDelta(float delta) {
    config.delta_temp = constrain(delta, 0.5, 5.0);
    saveConfig();
}

void SimpleTempController::setCompensation(float comp) {
    config.delivery_compensation = constrain(comp, -10.0, 10.0);
    saveConfig();
}

void SimpleTempController::setMode(TempControlMode mode) {
    config.mode = mode;
    config.last_compressor_change = millis();
    config.last_heater_change = millis();
    saveConfig();
}

void SimpleTempController::setEnabled(bool enable) {
    config.enabled = enable;
    if (!enable) {
        forceOff();
    }
    saveConfig();
}

void SimpleTempController::setEmergencyLimits(float low, float high) {
    config.emergency_low = low;
    config.emergency_high = high;
    saveConfig();
}

void SimpleTempController::setTimingProtection(unsigned long min_on, unsigned long min_off) {
    config.min_on_time = min_on;
    config.min_off_time = min_off;
    saveConfig();
}

void SimpleTempController::setRelayAssignments(uint8_t comp, uint8_t heat, uint8_t fan, uint8_t aux) {
    config.compressor_relay = comp;
    config.heater_relay = heat;
    config.fan_relay = fan;
    config.aux_relay = aux;
    saveConfig();
}

String SimpleTempController::getStatus() {
    String status = "Location: " + String(config.location_name) + "\n";
    status += "Mode: ";
    switch (config.mode) {
        case TEMP_MODE_OFF: status += "OFF"; break;
        case TEMP_MODE_HEATING: status += "HEATING"; break;
        case TEMP_MODE_COOLING: status += "COOLING"; break;
        case TEMP_MODE_AUTO: status += "AUTO"; break;
        case TEMP_MODE_FAN_ONLY: status += "FAN ONLY"; break;
    }
    status += "\n";
    status += "Enabled: " + String(config.enabled ? "Yes" : "No") + "\n";
    status += "Current Temp: " + String(current_temp, 1) + "°C\n";
    status += "Compensated: " + String(compensated_temp, 1) + "°C\n";
    status += "Setpoint: " + String(config.setpoint, 1) + "°C\n";
    status += "Compressor: " + String(config.compressor_state ? "ON" : "OFF") + "\n";
    status += "Heater: " + String(config.heater_state ? "ON" : "OFF") + "\n";
    status += "Fan: " + String(config.fan_state ? "ON" : "OFF") + "\n";
    
    if (config.emergency_stop) {
        status += "⚠️ EMERGENCY STOP ACTIVE!\n";
    }
    
    return status;
}

String SimpleTempController::getStatistics() {
    String stats = "Statistics for " + String(config.location_name) + "\n";
    stats += "Total Runtime: " + String(config.total_runtime / 3600000) + " hours\n";
    stats += "Compressor Cycles: " + String(config.compressor_cycles) + "\n";
    stats += "Min Temp: " + String(config.min_temp_recorded, 1) + "°C\n";
    stats += "Max Temp: " + String(config.max_temp_recorded, 1) + "°C\n";
    return stats;
}

String SimpleTempController::getJsonStatus() {
    StaticJsonDocument<512> doc;
    
    doc["location"] = config.location_name;
    doc["enabled"] = config.enabled;
    doc["mode"] = config.mode;
    doc["current_temp"] = current_temp;
    doc["compensated_temp"] = compensated_temp;
    doc["setpoint"] = config.setpoint;
    doc["delta"] = config.delta_temp;
    doc["compensation"] = config.delivery_compensation;
    doc["sensor_valid"] = sensor_valid;
    doc["emergency_stop"] = config.emergency_stop;
    
    JsonObject state = doc.createNestedObject("state");
    state["compressor"] = config.compressor_state;
    state["heater"] = config.heater_state;
    state["fan"] = config.fan_state;
    
    JsonObject stats = doc.createNestedObject("stats");
    stats["runtime_hours"] = config.total_runtime / 3600000.0;
    stats["cycles"] = config.compressor_cycles;
    stats["min_temp"] = config.min_temp_recorded;
    stats["max_temp"] = config.max_temp_recorded;
    
    String output;
    serializeJson(doc, output);
    return output;
}

String SimpleTempController::getJsonConfig() {
    StaticJsonDocument<512> doc;
    
    doc["location"] = config.location_name;
    doc["setpoint"] = config.setpoint;
    doc["delta"] = config.delta_temp;
    doc["compensation"] = config.delivery_compensation;
    doc["mode"] = config.mode;
    doc["enabled"] = config.enabled;
    
    JsonObject limits = doc.createNestedObject("limits");
    limits["emergency_high"] = config.emergency_high;
    limits["emergency_low"] = config.emergency_low;
    
    JsonObject timing = doc.createNestedObject("timing");
    timing["min_on_time"] = config.min_on_time;
    timing["min_off_time"] = config.min_off_time;
    
    JsonObject relays = doc.createNestedObject("relays");
    relays["compressor"] = config.compressor_relay;
    relays["heater"] = config.heater_relay;
    relays["fan"] = config.fan_relay;
    relays["aux"] = config.aux_relay;
    
    String output;
    serializeJson(doc, output);
    return output;
}

bool SimpleTempController::updateFromJson(String json) {
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, json);
    
    if (error) {
        DEBUG_PRINTLN("JSON parse error");
        return false;
    }
    
    if (doc.containsKey("location")) {
        setLocationName(doc["location"]);
    }
    if (doc.containsKey("setpoint")) {
        setSetpoint(doc["setpoint"]);
    }
    if (doc.containsKey("delta")) {
        setDelta(doc["delta"]);
    }
    if (doc.containsKey("compensation")) {
        setCompensation(doc["compensation"]);
    }
    if (doc.containsKey("mode")) {
        setMode((TempControlMode)doc["mode"].as<int>());
    }
    if (doc.containsKey("enabled")) {
        setEnabled(doc["enabled"]);
    }
    
    if (doc.containsKey("limits")) {
        JsonObject limits = doc["limits"];
        if (limits.containsKey("emergency_high") && limits.containsKey("emergency_low")) {
            setEmergencyLimits(limits["emergency_low"], limits["emergency_high"]);
        }
    }
    
    if (doc.containsKey("timing")) {
        JsonObject timing = doc["timing"];
        if (timing.containsKey("min_on_time") && timing.containsKey("min_off_time")) {
            setTimingProtection(timing["min_on_time"], timing["min_off_time"]);
        }
    }
    
    if (doc.containsKey("relays")) {
        JsonObject relays = doc["relays"];
        if (relays.containsKey("compressor") && relays.containsKey("heater") && 
            relays.containsKey("fan") && relays.containsKey("aux")) {
            setRelayAssignments(
                relays["compressor"],
                relays["heater"],
                relays["fan"],
                relays["aux"]
            );
        }
    }
    
    return true;
}
