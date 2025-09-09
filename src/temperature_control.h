#ifndef TEMPERATURE_CONTROL_H
#define TEMPERATURE_CONTROL_H

#include <Arduino.h>
#include "relay_control.h"

// Temperature control modes
enum TempControlMode {
    TEMP_MODE_OFF = 0,
    TEMP_MODE_HEATING = 1,
    TEMP_MODE_COOLING = 2,
    TEMP_MODE_AUTO = 3,
    TEMP_MODE_MANUAL = 4,
    TEMP_MODE_FAN_ONLY = 5
};

// Zone configuration
struct ZoneConfig {
    bool enabled;
    float setpoint;         // Target temperature
    float delta;            // Hysteresis value (±delta)
    float compensation;     // Temperature compensation/offset
    TempControlMode mode;   // Control mode for this zone
    uint8_t relay_mask;     // Which relays this zone controls (bit mask)
    unsigned long min_on_time;  // Minimum on time in ms
    unsigned long min_off_time; // Minimum off time in ms
    unsigned long last_change;  // Last state change timestamp
    bool current_state;     // Current relay state
    char name[20];         // Zone name
};

// Temperature control configuration
struct TempControlConfig {
    ZoneConfig zones[4];    // Support up to 4 zones
    float emergency_high;   // Emergency shutdown high temp
    float emergency_low;    // Emergency shutdown low temp
    bool emergency_stop;    // Emergency stop active
    unsigned long last_update;
};

class TemperatureController {
private:
    TempControlConfig config;
    float current_temps[4];  // Current temperature readings per zone
    bool sensor_valid[4];    // Sensor validity flags
    
    // Anti-short-cycle protection
    bool canChangeState(uint8_t zone);
    
    // Control logic
    bool shouldHeat(uint8_t zone);
    bool shouldCool(uint8_t zone);
    bool evaluateAutoMode(uint8_t zone);
    
public:
    TemperatureController();
    
    // Initialization
    void begin();
    void loadConfig();
    void saveConfig();
    
    // Temperature updates
    void updateTemperature(uint8_t zone, float temp);
    void updateAllTemperatures(float* temps);
    
    // Zone configuration
    void setZoneConfig(uint8_t zone, ZoneConfig& cfg);
    ZoneConfig& getZoneConfig(uint8_t zone);
    void setSetpoint(uint8_t zone, float setpoint);
    void setDelta(uint8_t zone, float delta);
    void setMode(uint8_t zone, TempControlMode mode);
    void setCompensation(uint8_t zone, float comp);
    
    // Control functions
    void process();  // Main control loop - call regularly
    void emergencyStop();
    void clearEmergency();
    bool isEmergencyStopped() { return config.emergency_stop; }
    
    // Status functions
    float getCurrentTemp(uint8_t zone);
    float getCompensatedTemp(uint8_t zone);
    bool isSensorValid(uint8_t zone);
    String getZoneStatus(uint8_t zone);
    String getSystemStatus();
    
    // Safety settings
    void setEmergencyLimits(float low, float high);
    void setMinTimes(uint8_t zone, unsigned long min_on, unsigned long min_off);
    float getEmergencyHighLimit() { return config.emergency_high; }
    float getEmergencyLowLimit() { return config.emergency_low; }
};

// Global temperature controller instance
extern TemperatureController temp_controller;

#endif // TEMPERATURE_CONTROL_H