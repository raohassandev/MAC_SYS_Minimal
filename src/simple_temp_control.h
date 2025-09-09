#ifndef SIMPLE_TEMP_CONTROL_H
#define SIMPLE_TEMP_CONTROL_H

#include <Arduino.h>
#include "relay_control.h"
#include "temperature_control.h"

struct SimpleTempConfig {
    char location_name[32];          // Room/Area/Office name
    float setpoint;                  // Target temperature
    float delta_temp;                // Hysteresis (deadband)
    float delivery_compensation;     // Sensor location compensation
    TempControlMode mode;            // Current operation mode
    bool enabled;                    // System enabled/disabled
    
    // Safety limits
    float emergency_high;            // Emergency stop high temp
    float emergency_low;             // Emergency stop low temp
    
    // Timing protection
    unsigned long min_on_time;       // Minimum compressor on time (ms)
    unsigned long min_off_time;      // Minimum compressor off time (ms)
    
    // Relay assignments (0-5 for 6 relays)
    uint8_t compressor_relay;        // Compressor/cooling relay
    uint8_t heater_relay;           // Heater relay
    uint8_t fan_relay;              // Fan relay
    uint8_t aux_relay;              // Auxiliary relay
    
    // State tracking
    unsigned long last_compressor_change;
    unsigned long last_heater_change;
    bool compressor_state;
    bool heater_state;
    bool fan_state;
    bool emergency_stop;
    
    // Statistics
    unsigned long total_runtime;
    unsigned long compressor_cycles;
    float min_temp_recorded;
    float max_temp_recorded;
};

class SimpleTempController {
private:
    SimpleTempConfig config;
    float current_temp;
    float compensated_temp;
    bool sensor_valid;
    unsigned long last_update;
    
    // Control logic
    bool canChangeCompressor();
    bool canChangeHeater();
    bool shouldStartCooling();
    bool shouldStopCooling();
    bool shouldStartHeating();
    bool shouldStopHeating();
    void updateRelays();
    void checkEmergencyLimits();
    
public:
    SimpleTempController();
    
    // Initialization
    void begin();
    void loadConfig();
    void saveConfig();
    void resetToDefaults();
    
    // Temperature updates
    void updateTemperature(float temp);
    float getCurrentTemp() { return current_temp; }
    float getCompensatedTemp() { return compensated_temp; }
    bool isSensorValid() { return sensor_valid; }
    
    // Configuration
    void setLocationName(const char* name);
    void setSetpoint(float temp);
    void setDelta(float delta);
    void setCompensation(float comp);
    void setMode(TempControlMode mode);
    void setEnabled(bool enable);
    void setEmergencyLimits(float low, float high);
    void setTimingProtection(unsigned long min_on, unsigned long min_off);
    void setRelayAssignments(uint8_t comp, uint8_t heat, uint8_t fan, uint8_t aux);
    
    // Control
    void process();  // Main control loop
    void emergencyStop();
    void clearEmergency();
    void forceOff();
    
    // Status
    SimpleTempConfig& getConfig() { return config; }
    String getStatus();
    String getStatistics();
    bool isCompressorOn() { return config.compressor_state; }
    bool isHeaterOn() { return config.heater_state; }
    bool isFanOn() { return config.fan_state; }
    bool isEmergencyStopped() { return config.emergency_stop; }
    float getEmergencyHighLimit() { return config.emergency_high; }
    float getEmergencyLowLimit() { return config.emergency_low; }
    
    // API helpers
    String getJsonStatus();
    String getJsonConfig();
    bool updateFromJson(String json);
};

extern SimpleTempController simple_temp;

#endif // SIMPLE_TEMP_CONTROL_H