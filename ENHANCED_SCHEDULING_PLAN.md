# 🕐 **Enhanced Scheduling System - Implementation Plan**

## 📋 **System Overview**

This is a **smart energy-saving AC controller** that sits between the indoor and outdoor units, providing intelligent temperature control with RTC-based scheduling capabilities.

### **Physical Setup**
- **Relay controls signal** between indoor unit ↔ outdoor unit
- **Indoor unit** remains powered (display, sensors, remote control functional)
- **Outdoor unit** (compressor) controlled by our relay system
- **Temperature sensors** monitor room conditions with calibration support
- **RTC module** provides time-based scheduling independent of internet connectivity

---

## 🎛️ **Control Modes**

### **1. Mode Toggle: CENTRAL | LOCAL**
```cpp
enum ControlMode {
    CENTRAL,  // Our system controls the AC intelligently
    LOCAL     // User's remote control works normally (bypass mode)
};
```

**LOCAL Mode:**
- ✅ Relay stays ON permanently
- ✅ User remote control works normally  
- ✅ AC operates as user sets it (no restrictions)
- ✅ System monitors only (no active control)

**CENTRAL Mode:**
- 🎯 System controls relay based on intelligent logic
- 🎯 Monitors room temperature vs setpoint with delta control
- 🎯 Enforces user-configurable minimum temperature policy
- 🎯 Provides energy savings through smart ON/OFF control

### **2. Setpoint Mode: DIRECT | SCHEDULE**

**DIRECT Setpoint Mode:**
- 🎯 Single user-defined target temperature
- 🎯 System maintains this temperature continuously
- 🎯 Ignores any scheduled changes

**SCHEDULE Mode:**
- 📅 Follows RTC-based weekly schedule
- 📅 3 configurable entries per day (Morning/Afternoon/Evening)
- 📅 Automatic temperature changes based on time
- 📅 Individual entry enable/disable flags

---

## ⚡ **Smart Control Logic**

### **Temperature Control Algorithm:**
```cpp
float room_temperature = getCalibratedTemperature();
float target_setpoint = getCurrentSetpoint(); // From Direct or Schedule
float delta = user_defined_delta; // Can be positive or negative

if (room_temperature <= (target_setpoint + delta)) {
    relay.turnOff(); // Stop outdoor unit, save energy
    status = "Target Reached - Outdoor Unit OFF";
} else {
    relay.turnOn();  // Allow cooling
    status = "Cooling Active - Outdoor Unit ON";
}
```

### **Delta Control Examples:**
- **Setpoint: 25°C, Delta: -1°C**
  - Outdoor unit OFF at: 24°C (25 + (-1) = 24)
  - Outdoor unit ON at: 25°C and above
  
- **Setpoint: 25°C, Delta: +1°C**  
  - Outdoor unit OFF at: 26°C (25 + 1 = 26)
  - Outdoor unit ON at: 27°C and above

### **Temperature Calibration System**
```cpp
struct TemperatureCalibration {
    float sensor_offset;     // e.g., +2.5°C or -1.0°C
    char description[32];    // "Sensor above AC unit"
};

float getCalibratedTemperature() {
    float raw_sensor = sensor.readTemperature();
    float calibrated = raw_sensor + calibration.sensor_offset;
    return calibrated;
}
```

**Calibration Use Cases:**
- **Sensor above AC**: +2°C offset (sensor reads warmer than room)
- **Sensor far from AC**: -1°C offset (sensor reads cooler than target area)  
- **Sensor near heat source**: Variable offset based on installation conditions

---

## 🕐 **RTC-Based Scheduling System**

### **Schedule Data Structure**
```cpp
struct DailyScheduleEntry {
    uint8_t hour_start;        // 0-23
    uint8_t minute_start;      // 0-59
    uint8_t hour_stop;         // 0-23 
    uint8_t minute_stop;       // 0-59
    float target_temperature;  // Target temp for this period
    bool entry_active;         // Enable/disable this specific entry
};

struct DaySchedule {
    DailyScheduleEntry morning;   // Entry 1: e.g., 06:00-12:00
    DailyScheduleEntry afternoon; // Entry 2: e.g., 12:00-18:00
    DailyScheduleEntry evening;   // Entry 3: e.g., 18:00-22:00
    bool day_enabled;             // Enable/disable entire day
};

struct WeeklySchedule {
    DaySchedule monday;
    DaySchedule tuesday;
    DaySchedule wednesday;
    DaySchedule thursday;
    DaySchedule friday;
    DaySchedule saturday;
    DaySchedule sunday;
    bool schedule_active;  // Master enable/disable
};
```

### **RTC Schedule Execution**
```cpp
class RTCScheduler {
private:
    RTC_DS1307 rtc;
    WeeklySchedule schedule;
    
public:
    float getCurrentScheduledSetpoint() {
        DateTime now = rtc.now();
        uint8_t dayOfWeek = now.dayOfTheWeek(); // 0=Sunday, 1=Monday...
        uint8_t hour = now.hour();
        uint8_t minute = now.minute();
        
        DaySchedule* todaySchedule = getDaySchedule(dayOfWeek);
        DailyScheduleEntry* activeEntry = findActiveEntry(todaySchedule, hour, minute);
        
        if (activeEntry && activeEntry->entry_active) {
            return activeEntry->target_temperature;
        }
        return direct_setpoint; // Fallback to direct mode
    }
};
```

### **Time Management System**
```cpp
struct TimeConfig {
    bool internet_available;
    bool ntp_sync_enabled;
    DateTime last_ntp_sync;
    DateTime commissioning_time;    // Set by engineer during installation
    bool rtc_manually_configured;   // Flag for manual time setting
};
```

---

## 🔧 **System Commissioning**

### **Installation Time Setup**
1. **Internet Available:**
   - Automatic NTP synchronization
   - RTC updated from internet time
   - Schedule runs on accurate time

2. **No Internet (Field Installation):**
   - Configuration engineer manually sets RTC time
   - Schedule runs on manually configured time
   - System operates completely independently

### **Commissioning Interface**
- Manual RTC time setting capability
- Schedule configuration and testing
- Sensor calibration setup
- System validation and testing tools

---

## 🌐 **Web Interface Design**

### **Main Control Dashboard**
```
┌─────────────────────────────────────────────────────┐
│                AC Control System                    │
├─────────────────────────────────────────────────────┤
│ System Time: 2025-09-07 19:30:45 (RTC)            │
│ Time Source: Manual Set | Last Sync: Never         │
│                                                     │
│ Mode: [●] CENTRAL  [ ] LOCAL                       │
│ Control: [ ] DIRECT SETPOINT  [●] SCHEDULE         │
│                                                     │
│ Current Schedule: Evening Entry                     │
│ Active Period: 18:00 - 22:00                      │
│ Target Temperature: 24.0°C                         │
│ Next Change: Tomorrow 08:00 → 25.0°C               │
│                                                     │
│ Sensor Reading: 26.5°C                            │
│ Calibration: +1.5°C                               │
│ Room Temperature: 25.0°C (Calibrated)             │
│                                                     │
│ Control Logic: 24.0°C + (-1.0°C) = 23.0°C         │
│ Status: 🔴 Cooling Active                          │
│ Relay: ON | Outdoor Unit: ON                       │
└─────────────────────────────────────────────────────┘
```

### **Weekly Schedule Editor**
```
┌─────────────────────────────────────────────────────────────┐
│                    📅 Weekly Schedule                        │
├─────────────────────────────────────────────────────────────┤
│ Master Enable: [✓] Schedule Active                         │
├─────────────────────────────────────────────────────────────┤
│        │ Morning      │ Afternoon    │ Evening      │ Day   │
│        │ 06:00-12:00  │ 12:00-18:00  │ 18:00-22:00  │ On/Off│
├────────┼──────────────┼──────────────┼──────────────┼───────┤
│ Mon    │ [✓] 25°C     │ [✓] 27°C     │ [✓] 24°C     │ [✓]   │
│ Tue    │ [✓] 25°C     │ [✗] --       │ [✓] 24°C     │ [✓]   │
│ Wed    │ [✓] 25°C     │ [✓] 27°C     │ [✓] 24°C     │ [✓]   │
│ Thu    │ [✓] 25°C     │ [✓] 27°C     │ [✓] 24°C     │ [✓]   │
│ Fri    │ [✓] 25°C     │ [✓] 27°C     │ [✓] 24°C     │ [✓]   │
│ Sat    │ [✓] 26°C     │ [✓] 28°C     │ [✓] 25°C     │ [✓]   │
│ Sun    │ [✓] 26°C     │ [✓] 28°C     │ [✓] 25°C     │ [✓]   │
└─────────────────────────────────────────────────────────────┘
         [Save Schedule] [Reset to Defaults] [Copy Week]
```

---

## 🛠️ **Implementation Plan (1 Week)**

### **Day 1-2: Core Schedule Engine**
- Create RTC-based schedule data structures
- Implement EEPROM storage/loading for schedules
- Build schedule evaluation logic with time matching
- Test basic RTC time integration

### **Day 3-4: Web Interface Development**
- Create schedule setup page with 7-day grid
- Implement 3 entries per day with individual enable/disable
- Add save/load functionality for schedules
- Create real-time status display

### **Day 5-6: System Integration & Testing**
- Connect to existing temperature control system
- Implement real-time schedule execution
- Add delta control logic and calibration
- Debug and optimize performance

### **Day 7: Final Polish & Validation**
- UI improvements and error handling
- System testing and validation
- Documentation and commissioning guides

---

## 💾 **Data Storage Structure**

```cpp
struct SystemConfig {
    // Control Configuration
    ControlMode mode;              // CENTRAL or LOCAL
    bool use_schedule;             // true = SCHEDULE, false = DIRECT
    float direct_setpoint;         // Direct mode temperature
    float delta_threshold;         // Positive or negative delta
    float minimum_allowed_temp;    // User-defined minimum temperature
    
    // Temperature Calibration
    float sensor_calibration;      // Sensor offset correction
    
    // Time Management  
    TimeConfig time_config;
    bool rtc_calibrated;
    
    // Schedule Data
    WeeklySchedule weekly_schedule;
    
    // Commissioning Information
    char engineer_name[32];
    DateTime installation_date;
    char site_location[64];
    
    // System State
    bool relay_state;             // Current relay ON/OFF
    float raw_sensor_temp;        // Direct sensor reading
    float calibrated_room_temp;   // Sensor + calibration
    float current_target;         // Active setpoint
    
    uint16_t checksum;
};
```

### **EEPROM Memory Map**
```cpp
#define SCHEDULE_EEPROM_ADDR 400
#define SCHEDULE_MAGIC 0x5344  // 'SD' - Schedule Data
#define TIME_CONFIG_ADDR 600
#define CALIBRATION_ADDR 700
```

---

## 📡 **API Endpoints**

### **Schedule Management**
```cpp
GET    /api/schedule          // Get current weekly schedule
POST   /api/schedule          // Save weekly schedule  
GET    /api/schedule/status   // Current active entry and next transition
POST   /api/schedule/enable   // Enable/disable master schedule switch
GET    /api/schedule/current  // Currently active schedule entry

// Time Management
GET    /api/time              // Current RTC time and sync status
POST   /api/time/set          // Manually set RTC time
POST   /api/time/sync         // Attempt NTP synchronization

// Control System
GET    /api/control           // Current control mode and status
POST   /api/control/mode      // Set CENTRAL/LOCAL mode
POST   /api/control/setpoint  // Set direct setpoint mode
GET    /api/control/status    // Detailed system status

// Calibration
GET    /api/calibration       // Temperature calibration settings
POST   /api/calibration       // Update calibration offset
```

---

## ✅ **Success Criteria**

### **Functional Requirements**
1. ✅ RTC-based schedule execution works offline
2. ✅ Individual schedule entries can be enabled/disabled
3. ✅ Delta control provides precise temperature management
4. ✅ Sensor calibration compensates for installation variations
5. ✅ CENTRAL/LOCAL mode switching works seamlessly
6. ✅ Manual time setting supports field installations

### **Performance Requirements**
- **Schedule Evaluation**: < 100ms per minute check
- **Memory Usage**: < 60KB for complete system
- **EEPROM Endurance**: Smart write cycles to prevent wear
- **Real-time Response**: Schedule changes active within 1 minute
- **Offline Operation**: 100% functionality without internet

### **User Experience**
- **Simple Interface**: Non-technical users can configure schedules
- **Visual Feedback**: Clear status indicators for system state
- **Reliable Operation**: System works autonomously after commissioning
- **Flexible Configuration**: Adapts to various installation scenarios

---

## 🎯 **Example Usage Scenarios**

### **Typical Office Schedule**
- **Morning (6:00-12:00)**: 25°C - Pre-cool before staff arrives
- **Afternoon (12:00-18:00)**: 27°C - Comfortable during peak hours  
- **Evening (18:00-22:00)**: 24°C - Energy saving after hours

### **Weekend/Holiday Schedule**
- **Morning**: 26°C - Less aggressive cooling
- **Afternoon**: 28°C - Minimal cooling during low occupancy
- **Evening**: 25°C - Comfortable for security/cleaning staff

### **Energy Saving Example**
- **Setpoint**: 25°C, **Delta**: -1°C
- **Room reaches**: 24°C → Outdoor unit OFF (energy saved)
- **Room rises to**: 25°C → Outdoor unit ON (cooling resumes)

This system provides intelligent, autonomous AC control with significant energy savings while maintaining user comfort and operational flexibility.