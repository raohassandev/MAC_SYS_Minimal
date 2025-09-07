#ifndef SCHEDULE_MANAGER_H
#define SCHEDULE_MANAGER_H

#include <Arduino.h>
#include "rtc_manager.h"
#include "temperature_control.h"

// Schedule configuration
#define MAX_SCHEDULE_EVENTS 24    // Maximum events per day per zone
#define MAX_ZONES 4              // Maximum temperature zones
#define SCHEDULE_CONFIG_ADDR 512  // EEPROM address for schedule config

// Schedule event types
enum ScheduleEventType {
    SCHEDULE_TEMP_SETPOINT = 0,  // Change temperature setpoint
    SCHEDULE_TEMP_MODE = 1,      // Change temperature mode  
    SCHEDULE_RELAY_CONTROL = 2,  // Direct relay control
    SCHEDULE_SYSTEM_MODE = 3     // System-wide mode changes
};

// Individual schedule event
struct ScheduleEvent {
    bool enabled;                    // Event enabled/disabled
    uint8_t day_mask;               // Days of week (bit 0=Sunday, 1=Monday, etc.)
    uint16_t time_minutes;          // Time in minutes since midnight (0-1439)
    ScheduleEventType event_type;   // Type of scheduled event
    uint8_t zone_id;               // Target zone (0-3) or relay number
    float value1;                  // Primary value (temperature, relay state, etc.)
    float value2;                  // Secondary value (delta, duration, etc.)
    TempControlMode temp_mode;     // Temperature mode (if applicable)
    char description[32];          // Human readable description
};

// Weekly schedule for one zone
struct WeeklySchedule {
    bool enabled;                           // Schedule enabled for this zone
    uint8_t active_events;                 // Number of active events
    ScheduleEvent events[MAX_SCHEDULE_EVENTS]; // Schedule events
    char zone_name[20];                    // Zone name
    
    // Override settings
    bool override_active;                  // Manual override active
    unsigned long override_end_time;       // When override expires (millis)
    float override_setpoint;               // Override temperature
    TempControlMode override_mode;         // Override mode
};

// Complete schedule configuration
struct ScheduleConfig {
    WeeklySchedule zones[MAX_ZONES];       // Per-zone schedules
    bool global_enabled;                   // Master schedule enable/disable
    bool holiday_mode;                     // Holiday mode (disable all schedules)
    uint16_t checksum;                     // Configuration integrity check
    unsigned long last_update;             // Last modification timestamp
};

class ScheduleManager {
private:
    ScheduleConfig config;
    bool config_loaded;
    unsigned long last_execution_check;
    
    // Internal helper functions
    bool isEventActive(const ScheduleEvent& event, int current_day, int current_minutes);
    void executeEvent(const ScheduleEvent& event);
    ScheduleEvent* findNextEvent(uint8_t zone, int& next_day, int& next_minutes);
    uint16_t calculateChecksum();
    void sortEvents(uint8_t zone);
    
public:
    ScheduleManager();
    
    // Initialization and configuration
    bool begin();
    void loadConfig();
    void saveConfig();
    void resetToDefaults();
    
    // Schedule management
    bool addEvent(uint8_t zone, const ScheduleEvent& event);
    bool removeEvent(uint8_t zone, uint8_t event_index);
    bool updateEvent(uint8_t zone, uint8_t event_index, const ScheduleEvent& event);
    void clearSchedule(uint8_t zone);
    void clearAllSchedules();
    
    // Execution control
    void process();  // Main execution loop - call regularly
    void setGlobalEnabled(bool enabled);
    void setZoneEnabled(uint8_t zone, bool enabled);
    void setHolidayMode(bool enabled);
    
    // Override functions
    void setOverride(uint8_t zone, float setpoint, TempControlMode mode, unsigned long duration_minutes = 0);
    void clearOverride(uint8_t zone);
    bool isOverrideActive(uint8_t zone);
    
    // Query functions
    bool isScheduleActive() { return config.global_enabled && !config.holiday_mode; }
    WeeklySchedule& getZoneSchedule(uint8_t zone);
    ScheduleEvent* getEvent(uint8_t zone, uint8_t event_index);
    uint8_t getEventCount(uint8_t zone);
    String getNextEventDescription(uint8_t zone);
    
    // Status and information
    String getScheduleStatus();
    String getZoneScheduleStatus(uint8_t zone);
    String getDaySchedule(uint8_t zone, int day_of_week);
    void printSchedule(uint8_t zone);
    
    // Time utilities specific to scheduling
    String formatTimeFromMinutes(uint16_t minutes);
    uint16_t parseTimeToMinutes(const String& time_str);
    String formatDayMask(uint8_t day_mask);
    uint8_t parseDayMask(const String& days_str);
    
    // Import/Export functionality
    String exportScheduleJSON(uint8_t zone);
    bool importScheduleJSON(uint8_t zone, const String& json_str);
    String exportAllSchedulesJSON();
    bool importAllSchedulesJSON(const String& json_str);
    
    // Validation functions
    bool validateEvent(const ScheduleEvent& event);
    bool validateTimeRange(uint16_t start_minutes, uint16_t end_minutes);
    bool hasConflicts(uint8_t zone, const ScheduleEvent& new_event, int exclude_index = -1);
};

// Global schedule manager instance
extern ScheduleManager schedule_manager;

// Helper functions
String scheduleEventTypeToString(ScheduleEventType type);
ScheduleEventType stringToScheduleEventType(const String& type_str);
String tempModeToString(TempControlMode mode);
TempControlMode stringToTempMode(const String& mode_str);

#endif // SCHEDULE_MANAGER_H