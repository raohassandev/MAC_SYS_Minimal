#include "schedule_manager.h"
#include "config.h"
#include <EEPROM.h>

// Global schedule manager instance
ScheduleManager schedule_manager;

ScheduleManager::ScheduleManager() {
    config_loaded = false;
    last_execution_check = 0;
    
    // Initialize with defaults
    config.global_enabled = false;
    config.holiday_mode = false;
    config.last_update = 0;
    config.checksum = 0;
    
    // Initialize zones
    for (int i = 0; i < MAX_ZONES; i++) {
        config.zones[i].enabled = false;
        config.zones[i].active_events = 0;
        config.zones[i].override_active = false;
        config.zones[i].override_end_time = 0;
        config.zones[i].override_setpoint = 22.0;
        config.zones[i].override_mode = TEMP_MODE_OFF;
        snprintf(config.zones[i].zone_name, sizeof(config.zones[i].zone_name), "Zone %d", i + 1);
        
        // Initialize events
        for (int j = 0; j < MAX_SCHEDULE_EVENTS; j++) {
            memset(&config.zones[i].events[j], 0, sizeof(ScheduleEvent));
        }
    }
}

bool ScheduleManager::begin() {
    DEBUG_PRINTLN("Initializing Schedule Manager...");
    loadConfig();
    DEBUG_PRINTF("✅ Schedule Manager initialized (%d zones)\n", MAX_ZONES);
    return true;
}

void ScheduleManager::loadConfig() {
    // Check if EEPROM has valid config
    uint16_t stored_checksum;
    EEPROM.get(SCHEDULE_CONFIG_ADDR, stored_checksum);
    
    if (stored_checksum != 0) {  // Config exists
        EEPROM.get(SCHEDULE_CONFIG_ADDR, config);
        uint16_t calculated_checksum = calculateChecksum();
        
        if (config.checksum == calculated_checksum) {
            config_loaded = true;
            DEBUG_PRINTLN("✅ Schedule config loaded from EEPROM");
            DEBUG_PRINTF("📅 %d zones configured, global: %s\n", 
                        MAX_ZONES, config.global_enabled ? "ON" : "OFF");
        } else {
            DEBUG_PRINTLN("⚠️ Schedule config checksum mismatch, using defaults");
            resetToDefaults();
        }
    } else {
        DEBUG_PRINTLN("⚠️ No schedule config in EEPROM, using defaults");
        resetToDefaults();
    }
}

void ScheduleManager::saveConfig() {
    config.last_update = rtc_manager.getUnixTime();
    config.checksum = calculateChecksum();
    
    EEPROM.put(SCHEDULE_CONFIG_ADDR, config);
    EEPROM.commit();
    
    DEBUG_PRINTLN("✅ Schedule config saved to EEPROM");
}

void ScheduleManager::resetToDefaults() {
    // Create some default schedule events for demonstration
    
    // Zone 0: Office temperature schedule
    config.zones[0].enabled = true;
    config.zones[0].active_events = 4;
    strcpy(config.zones[0].zone_name, "Office");
    
    // Morning warmup (6:00 AM, weekdays)
    config.zones[0].events[0].enabled = true;
    config.zones[0].events[0].day_mask = 0x3E; // Monday-Friday (bits 1-5)
    config.zones[0].events[0].time_minutes = 6 * 60; // 6:00 AM
    config.zones[0].events[0].event_type = SCHEDULE_TEMP_SETPOINT;
    config.zones[0].events[0].zone_id = 0;
    config.zones[0].events[0].value1 = 22.0; // 22°C
    config.zones[0].events[0].value2 = 1.0;  // ±1°C delta
    config.zones[0].events[0].temp_mode = TEMP_MODE_HEATING;
    strcpy(config.zones[0].events[0].description, "Morning Warmup");
    
    // Daytime comfort (8:00 AM, weekdays)
    config.zones[0].events[1].enabled = true;
    config.zones[0].events[1].day_mask = 0x3E; // Monday-Friday
    config.zones[0].events[1].time_minutes = 8 * 60; // 8:00 AM
    config.zones[0].events[1].event_type = SCHEDULE_TEMP_SETPOINT;
    config.zones[0].events[1].zone_id = 0;
    config.zones[0].events[1].value1 = 24.0; // 24°C
    config.zones[0].events[1].value2 = 1.0;
    config.zones[0].events[1].temp_mode = TEMP_MODE_AUTO;
    strcpy(config.zones[0].events[1].description, "Daytime Comfort");
    
    // Evening reduction (6:00 PM, weekdays)
    config.zones[0].events[2].enabled = true;
    config.zones[0].events[2].day_mask = 0x3E; // Monday-Friday
    config.zones[0].events[2].time_minutes = 18 * 60; // 6:00 PM
    config.zones[0].events[2].event_type = SCHEDULE_TEMP_SETPOINT;
    config.zones[0].events[2].zone_id = 0;
    config.zones[0].events[2].value1 = 21.0; // 21°C
    config.zones[0].events[2].value2 = 1.0;
    config.zones[0].events[2].temp_mode = TEMP_MODE_HEATING;
    strcpy(config.zones[0].events[2].description, "Evening Reduction");
    
    // Night setback (10:00 PM, all days)
    config.zones[0].events[3].enabled = true;
    config.zones[0].events[3].day_mask = 0x7F; // All days (bits 0-6)
    config.zones[0].events[3].time_minutes = 22 * 60; // 10:00 PM
    config.zones[0].events[3].event_type = SCHEDULE_TEMP_SETPOINT;
    config.zones[0].events[3].zone_id = 0;
    config.zones[0].events[3].value1 = 18.0; // 18°C
    config.zones[0].events[3].value2 = 1.5;
    config.zones[0].events[3].temp_mode = TEMP_MODE_HEATING;
    strcpy(config.zones[0].events[3].description, "Night Setback");
    
    config.global_enabled = true;
    saveConfig();
}

uint16_t ScheduleManager::calculateChecksum() {
    uint16_t checksum = 0;
    uint8_t* data = (uint8_t*)&config;
    size_t size = sizeof(ScheduleConfig) - sizeof(uint16_t); // Exclude checksum field
    
    for (size_t i = 0; i < size; i++) {
        checksum ^= data[i];
        checksum = (checksum << 1) | (checksum >> 15); // Rotate left
    }
    
    return checksum;
}

bool ScheduleManager::addEvent(uint8_t zone, const ScheduleEvent& event) {
    if (zone >= MAX_ZONES || !validateEvent(event)) {
        return false;
    }
    
    WeeklySchedule& schedule = config.zones[zone];
    
    if (schedule.active_events >= MAX_SCHEDULE_EVENTS) {
        DEBUG_PRINTLN("❌ Schedule full, cannot add event");
        return false;
    }
    
    // Check for conflicts
    if (hasConflicts(zone, event)) {
        DEBUG_PRINTLN("❌ Event conflicts with existing schedule");
        return false;
    }
    
    // Add event
    schedule.events[schedule.active_events] = event;
    schedule.active_events++;
    
    // Sort events by time
    sortEvents(zone);
    
    saveConfig();
    DEBUG_PRINTF("✅ Added schedule event to zone %d: %s\n", zone, event.description);
    return true;
}

bool ScheduleManager::removeEvent(uint8_t zone, uint8_t event_index) {
    if (zone >= MAX_ZONES || event_index >= config.zones[zone].active_events) {
        return false;
    }
    
    WeeklySchedule& schedule = config.zones[zone];
    
    // Shift events down
    for (uint8_t i = event_index; i < schedule.active_events - 1; i++) {
        schedule.events[i] = schedule.events[i + 1];
    }
    
    schedule.active_events--;
    
    // Clear the last event
    memset(&schedule.events[schedule.active_events], 0, sizeof(ScheduleEvent));
    
    saveConfig();
    DEBUG_PRINTF("✅ Removed schedule event from zone %d\n", zone);
    return true;
}

bool ScheduleManager::updateEvent(uint8_t zone, uint8_t event_index, const ScheduleEvent& event) {
    if (zone >= MAX_ZONES || event_index >= config.zones[zone].active_events || !validateEvent(event)) {
        return false;
    }
    
    // Check for conflicts (excluding the event being updated)
    if (hasConflicts(zone, event, event_index)) {
        DEBUG_PRINTLN("❌ Updated event conflicts with existing schedule");
        return false;
    }
    
    config.zones[zone].events[event_index] = event;
    
    // Re-sort events by time
    sortEvents(zone);
    
    saveConfig();
    DEBUG_PRINTF("✅ Updated schedule event in zone %d\n", zone);
    return true;
}

void ScheduleManager::sortEvents(uint8_t zone) {
    if (zone >= MAX_ZONES) return;
    
    WeeklySchedule& schedule = config.zones[zone];
    
    // Simple bubble sort by time_minutes
    for (int i = 0; i < schedule.active_events - 1; i++) {
        for (int j = 0; j < schedule.active_events - i - 1; j++) {
            if (schedule.events[j].time_minutes > schedule.events[j + 1].time_minutes) {
                ScheduleEvent temp = schedule.events[j];
                schedule.events[j] = schedule.events[j + 1];
                schedule.events[j + 1] = temp;
            }
        }
    }
}

void ScheduleManager::process() {
    if (!isScheduleActive()) {
        return;
    }
    
    unsigned long now = millis();
    
    // Only check once per minute to avoid excessive processing
    if (now - last_execution_check < 60000) {
        return;
    }
    
    last_execution_check = now;
    
    int current_day = rtc_manager.getCurrentDayOfWeek();
    int current_minutes = rtc_manager.getCurrentTimeMinutes();
    
    // Process each zone
    for (uint8_t zone = 0; zone < MAX_ZONES; zone++) {
        WeeklySchedule& schedule = config.zones[zone];
        
        if (!schedule.enabled) continue;
        
        // Check for expired overrides
        if (schedule.override_active && schedule.override_end_time > 0) {
            if (now >= schedule.override_end_time) {
                clearOverride(zone);
                DEBUG_PRINTF("⏰ Override expired for zone %d\n", zone);
            }
        }
        
        // Skip scheduling if override is active
        if (schedule.override_active) continue;
        
        // Check each event
        for (uint8_t i = 0; i < schedule.active_events; i++) {
            ScheduleEvent& event = schedule.events[i];
            
            if (isEventActive(event, current_day, current_minutes)) {
                executeEvent(event);
            }
        }
    }
}

bool ScheduleManager::isEventActive(const ScheduleEvent& event, int current_day, int current_minutes) {
    if (!event.enabled) return false;
    
    // Check if today is in the day mask
    if (!(event.day_mask & (1 << current_day))) {
        return false;
    }
    
    // Check if current time matches event time (within 1 minute window)
    return (abs(current_minutes - (int)event.time_minutes) <= 1);
}

void ScheduleManager::executeEvent(const ScheduleEvent& event) {
    DEBUG_PRINTF("🕒 Executing schedule event: %s\n", event.description);
    
    switch (event.event_type) {
        case SCHEDULE_TEMP_SETPOINT:
            temp_controller.setSetpoint(event.zone_id, event.value1);
            temp_controller.setDelta(event.zone_id, event.value2);
            if (event.temp_mode != TEMP_MODE_OFF) {
                temp_controller.setMode(event.zone_id, event.temp_mode);
            }
            DEBUG_PRINTF("🌡️ Zone %d: Setpoint=%.1f°C, Delta=%.1f°C, Mode=%d\n", 
                        event.zone_id, event.value1, event.value2, event.temp_mode);
            break;
            
        case SCHEDULE_TEMP_MODE:
            temp_controller.setMode(event.zone_id, event.temp_mode);
            DEBUG_PRINTF("🔄 Zone %d: Mode changed to %d\n", event.zone_id, event.temp_mode);
            break;
            
        case SCHEDULE_RELAY_CONTROL:
            // Direct relay control (value1: 1=ON, 0=OFF)
            relay_controller.setRelay(event.zone_id, event.value1 > 0.5);
            DEBUG_PRINTF("🔌 Relay %d: %s\n", event.zone_id, event.value1 > 0.5 ? "ON" : "OFF");
            break;
            
        case SCHEDULE_SYSTEM_MODE:
            // System-wide mode changes (could extend this)
            DEBUG_PRINTF("⚙️ System mode event: %.0f\n", event.value1);
            break;
    }
}

void ScheduleManager::setOverride(uint8_t zone, float setpoint, TempControlMode mode, unsigned long duration_minutes) {
    if (zone >= MAX_ZONES) return;
    
    WeeklySchedule& schedule = config.zones[zone];
    schedule.override_active = true;
    schedule.override_setpoint = setpoint;
    schedule.override_mode = mode;
    
    if (duration_minutes > 0) {
        schedule.override_end_time = millis() + (duration_minutes * 60000);
    } else {
        schedule.override_end_time = 0; // Permanent override
    }
    
    // Apply override immediately
    temp_controller.setSetpoint(zone, setpoint);
    temp_controller.setMode(zone, mode);
    
    DEBUG_PRINTF("🔧 Override set for zone %d: %.1f°C, Mode=%d, Duration=%lu min\n", 
                zone, setpoint, mode, duration_minutes);
}

void ScheduleManager::clearOverride(uint8_t zone) {
    if (zone >= MAX_ZONES) return;
    
    config.zones[zone].override_active = false;
    config.zones[zone].override_end_time = 0;
    
    DEBUG_PRINTF("🔧 Override cleared for zone %d\n", zone);
}

String ScheduleManager::getScheduleStatus() {
    String status = "Schedule Manager Status\n";
    status += "========================\n";
    
    if (!isScheduleActive()) {
        if (config.holiday_mode) {
            status += "🏖️ Holiday Mode: ACTIVE\n";
        } else {
            status += "❌ Global Schedule: DISABLED\n";
        }
    } else {
        status += "✅ Global Schedule: ACTIVE\n";
    }
    
    status += "⏰ Current Time: " + rtc_manager.getFormattedDateTime() + "\n\n";
    
    // Zone summaries
    for (uint8_t i = 0; i < MAX_ZONES; i++) {
        WeeklySchedule& schedule = config.zones[i];
        status += String(schedule.zone_name) + ": ";
        
        if (!schedule.enabled) {
            status += "Disabled";
        } else if (schedule.override_active) {
            status += "Override Active";
        } else {
            status += String(schedule.active_events) + " events";
        }
        status += "\n";
    }
    
    return status;
}

String ScheduleManager::formatTimeFromMinutes(uint16_t minutes) {
    if (minutes >= 1440) return "Invalid";
    
    int hour = minutes / 60;
    int minute = minutes % 60;
    
    String time_str = "";
    if (hour < 10) time_str += "0";
    time_str += String(hour) + ":";
    if (minute < 10) time_str += "0";
    time_str += String(minute);
    
    return time_str;
}

uint16_t ScheduleManager::parseTimeToMinutes(const String& time_str) {
    int colonPos = time_str.indexOf(':');
    if (colonPos == -1 || time_str.length() != 5) {
        return 0; // Invalid format, return midnight
    }
    
    int hour = time_str.substring(0, colonPos).toInt();
    int minute = time_str.substring(colonPos + 1).toInt();
    
    if (hour < 0 || hour > 23 || minute < 0 || minute > 59) {
        return 0; // Invalid time, return midnight
    }
    
    return (hour * 60) + minute;
}

bool ScheduleManager::validateEvent(const ScheduleEvent& event) {
    // Check time range
    if (event.time_minutes >= 1440) {
        return false;
    }
    
    // Check zone ID
    if (event.zone_id >= MAX_ZONES && event.event_type != SCHEDULE_RELAY_CONTROL) {
        return false;
    }
    
    // Check relay number for relay control events
    if (event.event_type == SCHEDULE_RELAY_CONTROL && event.zone_id >= 6) {
        return false;
    }
    
    // Check day mask (at least one day must be selected)
    if (event.day_mask == 0) {
        return false;
    }
    
    // Check temperature values
    if (event.event_type == SCHEDULE_TEMP_SETPOINT) {
        if (event.value1 < 5.0 || event.value1 > 40.0) { // Setpoint range
            return false;
        }
        if (event.value2 < 0.5 || event.value2 > 5.0) { // Delta range
            return false;
        }
    }
    
    return true;
}

bool ScheduleManager::hasConflicts(uint8_t zone, const ScheduleEvent& new_event, int exclude_index) {
    if (zone >= MAX_ZONES) return true;
    
    WeeklySchedule& schedule = config.zones[zone];
    
    for (int i = 0; i < schedule.active_events; i++) {
        if (i == exclude_index) continue; // Skip the event being updated
        
        ScheduleEvent& existing = schedule.events[i];
        
        // Check for overlapping days and times
        if ((new_event.day_mask & existing.day_mask) && 
            (abs((int)new_event.time_minutes - (int)existing.time_minutes) < 5)) {
            return true; // Conflict: same days and within 5 minutes
        }
    }
    
    return false;
}

WeeklySchedule& ScheduleManager::getZoneSchedule(uint8_t zone) {
    if (zone >= MAX_ZONES) zone = 0;
    return config.zones[zone];
}

void ScheduleManager::setGlobalEnabled(bool enabled) {
    config.global_enabled = enabled;
    saveConfig();
    DEBUG_PRINTF("🌍 Global schedule: %s\n", enabled ? "ENABLED" : "DISABLED");
}

void ScheduleManager::setZoneEnabled(uint8_t zone, bool enabled) {
    if (zone >= MAX_ZONES) return;
    config.zones[zone].enabled = enabled;
    saveConfig();
    DEBUG_PRINTF("🏠 Zone %d schedule: %s\n", zone, enabled ? "ENABLED" : "DISABLED");
}

void ScheduleManager::setHolidayMode(bool enabled) {
    config.holiday_mode = enabled;
    saveConfig();
    DEBUG_PRINTF("🏖️ Holiday mode: %s\n", enabled ? "ENABLED" : "DISABLED");
}

String ScheduleManager::formatDayMask(uint8_t day_mask) {
    String days = "";
    const char* day_names[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    
    bool first = true;
    for (int i = 0; i < 7; i++) {
        if (day_mask & (1 << i)) {
            if (!first) days += ", ";
            days += day_names[i];
            first = false;
        }
    }
    
    return days.length() > 0 ? days : "None";
}

uint8_t ScheduleManager::parseDayMask(const String& days_str) {
    uint8_t mask = 0;
    
    if (days_str.indexOf("Sun") >= 0) mask |= 1;
    if (days_str.indexOf("Mon") >= 0) mask |= 2;
    if (days_str.indexOf("Tue") >= 0) mask |= 4;
    if (days_str.indexOf("Wed") >= 0) mask |= 8;
    if (days_str.indexOf("Thu") >= 0) mask |= 16;
    if (days_str.indexOf("Fri") >= 0) mask |= 32;
    if (days_str.indexOf("Sat") >= 0) mask |= 64;
    
    return mask;
}

// Helper functions for schedule event types and temperature modes
String scheduleEventTypeToString(ScheduleEventType type) {
    switch (type) {
        case SCHEDULE_TEMP_SETPOINT: return "Temperature Setpoint";
        case SCHEDULE_TEMP_MODE: return "Temperature Mode";
        case SCHEDULE_RELAY_CONTROL: return "Relay Control";
        case SCHEDULE_SYSTEM_MODE: return "System Mode";
        default: return "Unknown";
    }
}

ScheduleEventType stringToScheduleEventType(const String& type_str) {
    if (type_str == "temp_setpoint") return SCHEDULE_TEMP_SETPOINT;
    if (type_str == "temp_mode") return SCHEDULE_TEMP_MODE;
    if (type_str == "relay") return SCHEDULE_RELAY_CONTROL;
    if (type_str == "system") return SCHEDULE_SYSTEM_MODE;
    return SCHEDULE_TEMP_SETPOINT; // Default
}

String tempModeToString(TempControlMode mode) {
    switch (mode) {
        case TEMP_MODE_OFF: return "OFF";
        case TEMP_MODE_HEATING: return "HEATING";
        case TEMP_MODE_COOLING: return "COOLING";
        case TEMP_MODE_AUTO: return "AUTO";
        case TEMP_MODE_MANUAL: return "MANUAL";
        default: return "UNKNOWN";
    }
}

TempControlMode stringToTempMode(const String& mode_str) {
    if (mode_str == "OFF" || mode_str == "0") return TEMP_MODE_OFF;
    if (mode_str == "HEATING" || mode_str == "1") return TEMP_MODE_HEATING;
    if (mode_str == "COOLING" || mode_str == "2") return TEMP_MODE_COOLING;
    if (mode_str == "AUTO" || mode_str == "3") return TEMP_MODE_AUTO;
    if (mode_str == "MANUAL" || mode_str == "4") return TEMP_MODE_MANUAL;
    return TEMP_MODE_OFF; // Default
}