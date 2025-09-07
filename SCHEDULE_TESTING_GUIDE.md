# ESP32 Schedule System Testing Guide

## Overview

This document provides comprehensive testing procedures for the MAC-SYS firmware schedule system. The system has been completely redesigned to use a simplified ON/OFF scheduling approach with robust conflict detection and resolution.

## System Architecture

### Core Components

- **Schedule Engine** (`scheduler.cpp`): 5-second interval checking, simplified ON/OFF logic
- **Web API** (`web_server.cpp`): RESTful endpoints with conflict validation
- **Frontend** (`app.js`, `style.css`): Real-time calendar interface with visual conflict detection
- **Storage** (`config.h`): EEPROM persistence with max 7 schedule entries
- **Hardware Integration**: OLED display updates, relay control, notifications

### Key Features Implemented

✅ **Simplified Logic**: AC ON during scheduled times, OFF otherwise (no priority system)  
✅ **5-Second Responsiveness**: Reduced from 30-second intervals  
✅ **Conflict Detection**: Visual warnings, API validation, resolution suggestions  
✅ **Mobile Responsive**: Touch-friendly interface with mobile navigation  
✅ **Real-time Updates**: 2-second refresh intervals, immediate notifications  
✅ **OLED Integration**: Live schedule status display  
✅ **User Permissions**: Role-based access control (Admin/Operator/Viewer)

## Testing Scenarios

### 1. Basic Schedule Creation

#### Test Case 1.1: Single Day Schedule

```
Objective: Create a basic weekday schedule
Steps:
1. Navigate to Schedule page
2. Click "Add Schedule Rule"
3. Set: Monday, 09:00-17:00, 22°C
4. Save schedule

Expected Results:
- Schedule saves successfully
- Calendar shows colored block for Monday 9-17
- Schedule Rules list shows new entry
- Notification: "Schedule saved successfully"
- OLED displays "Status: OFF" (if outside schedule time)
```

#### Test Case 1.2: Multi-Day Schedule

```
Objective: Create schedule spanning multiple days
Steps:
1. Create schedule: Mon-Fri, 08:30-18:00, 23°C
2. Verify calendar display
3. Check individual day entries in device memory

Expected Results:
- 5 separate entries created (one per day)
- All weekdays show consistent schedule blocks
- Total entries ≤ 7 (system limit)
```

#### Test Case 1.3: Overnight Schedule

```
Objective: Test midnight-spanning schedules
Steps:
1. Create schedule: Friday, 22:00-06:00, 20°C
2. Verify time calculation logic
3. Test during active period

Expected Results:
- Calendar shows blocks from 22:00 Friday to 06:00 Saturday
- Schedule activates correctly at 22:00
- Schedule deactivates at 06:00 next day
```

### 2. Conflict Detection and Resolution

#### Test Case 2.1: Time Overlap Detection

```
Objective: Verify conflict detection works
Steps:
1. Create: Monday, 09:00-13:00, 22°C
2. Try to create: Monday, 11:00-15:00, 24°C
3. Observe conflict warning

Expected Results:
- Conflict banner appears: "Schedule Conflicts Detected (1)"
- Conflicted calendar cells highlighted with red border
- Warning details: "11:00-13:00: overlap detected"
- Suggestion: "Consider adjusting end time of earlier schedule"
```

#### Test Case 2.2: Identical Schedule Detection

```
Objective: Test duplicate schedule handling
Steps:
1. Create: Tuesday, 10:00-16:00, 23°C
2. Try to create: Tuesday, 10:00-16:00, 23°C

Expected Results:
- API returns conflict validation error
- Suggestion: "Identical schedule already exists"
- Option to force override with "force": true parameter
```

#### Test Case 2.3: Temperature Conflict Resolution

```
Objective: Test same-time different-temperature conflicts
Steps:
1. Create: Wednesday, 12:00-14:00, 20°C
2. Create: Wednesday, 13:00-15:00, 26°C

Expected Results:
- Visual conflict highlighting 13:00-14:00 overlap
- Suggestion: "Temperature conflict: 20°C vs 26°C"
- Recommended time adjustments provided
```

### 3. Schedule Execution and Control

#### Test Case 3.1: Schedule Activation

```
Objective: Verify schedule triggers AC control
Steps:
1. Create schedule for current time + 2 minutes
2. Wait for activation
3. Monitor relay state and OLED display

Expected Results:
- At scheduled time: "✅ SCHEDULE ACTIVATED" in serial log
- OLED shows: "Status: ACTIVE", target temperature
- Physical relay switches if temperature threshold met
- Dashboard shows schedule active indicator
```

#### Test Case 3.2: Schedule Deactivation

```
Objective: Test schedule end behavior
Steps:
1. Create 5-minute schedule starting now
2. Monitor through completion
3. Verify AC OFF behavior

Expected Results:
- At end time: "📴 SCHEDULE ENDED - AC OFF" message
- OLED updates to "Status: OFF"
- Relay switches OFF regardless of temperature
- Setpoint restored to original value
```

#### Test Case 3.3: Multiple Schedule Transition

```
Objective: Test back-to-back schedules
Steps:
1. Create: 14:00-15:00, 22°C
2. Create: 15:00-16:00, 24°C
3. Monitor transition at 15:00

Expected Results:
- Seamless transition without AC OFF period
- Temperature setpoint changes immediately
- No conflict warnings (non-overlapping)
- OLED updates target temperature
```

### 4. User Interface and Mobile Testing

#### Test Case 4.1: Mobile Responsive Layout

```
Objective: Test mobile interface
Steps:
1. Open interface on mobile device (or resize browser < 768px)
2. Verify mobile header appears
3. Test hamburger menu navigation
4. Create schedule using mobile interface

Expected Results:
- Mobile header with hamburger menu appears
- Sidebar slides in/out correctly
- Calendar scrolls horizontally on mobile
- Form controls are 44px minimum height
- Touch interactions work smoothly
```

#### Test Case 4.2: Calendar Interaction

```
Objective: Test calendar cell interactions
Steps:
1. Click empty calendar cell
2. Click occupied calendar cell
3. Test right-click context menu

Expected Results:
- Empty cell: Temperature prompt appears
- Occupied cell: Edit/Delete options
- Context menu works (if implemented)
- Viewer role: interactions disabled with appropriate message
```

#### Test Case 4.3: Real-time Updates

```
Objective: Verify live data updates
Steps:
1. Open dashboard and schedule page
2. Create schedule via API/another browser
3. Monitor automatic refresh

Expected Results:
- New schedules appear within 2 seconds
- Calendar re-renders automatically
- Status updates reflect current state
- No manual refresh required
```

### 5. Error Handling and Edge Cases

#### Test Case 5.1: Maximum Schedule Limit

```
Objective: Test 7-schedule entry limit
Steps:
1. Create 7 different schedules
2. Attempt to create 8th schedule
3. Verify error handling

Expected Results:
- First 7 schedules save successfully
- 8th schedule rejected with error message
- No system crash or data corruption
- Clear error message to user
```

#### Test Case 5.2: Invalid Time Formats

```
Objective: Test time validation
Steps:
1. Try invalid start time: "25:00"
2. Try invalid end time: "abc"
3. Try end time before start time

Expected Results:
- Input validation prevents invalid times
- Clear error messages displayed
- Form doesn't submit with invalid data
- User guidance provided
```

#### Test Case 5.3: Network Disconnection

```
Objective: Test offline behavior
Steps:
1. Create schedule while connected
2. Disconnect WiFi/network
3. Verify schedule continues running
4. Reconnect and check synchronization

Expected Results:
- Existing schedules continue running offline
- Schedule changes cached until reconnection
- No schedule execution interruption
- Data consistency maintained
```

### 6. Performance and Reliability

#### Test Case 6.1: 5-Second Response Test

```
Objective: Verify improved responsiveness
Steps:
1. Create schedule to start in 10 seconds
2. Monitor exact activation timing
3. Measure schedule check intervals

Expected Results:
- Schedule activates within 5 seconds of start time
- Serial log shows 5-second check intervals
- Improved from previous 30-second delays
- System remains responsive during checks
```

#### Test Case 6.2: Memory and Storage Test

```
Objective: Test EEPROM persistence
Steps:
1. Create multiple schedules
2. Restart ESP32 device
3. Verify schedules persist
4. Monitor memory usage

Expected Results:
- All schedules restored after restart
- No memory leaks observed
- EEPROM integrity maintained
- System stable under normal load
```

#### Test Case 6.3: Long-term Stability

```
Objective: Test extended operation
Steps:
1. Set up repeating daily schedules
2. Run system for 24+ hours
3. Monitor for any degradation

Expected Results:
- Schedules execute consistently
- No memory leaks or crashes
- Time synchronization maintained
- OLED and relay operations stable
```

## API Testing

### Schedule Creation API

```bash
# Test basic schedule creation
curl -X POST http://esp32-ip/api/schedule \
  -H "Authorization: Bearer YOUR_TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "schedules": [{
      "days": [1,2,3,4,5],
      "startTime": "09:00",
      "endTime": "17:00",
      "targetTemp": 22.5,
      "enabled": true
    }]
  }'

# Test conflict validation
curl -X POST http://esp32-ip/api/schedule/validate \
  -H "Authorization: Bearer YOUR_TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "schedules": [
      {"days": [1], "startTime": "09:00", "endTime": "12:00", "targetTemp": 22},
      {"days": [1], "startTime": "11:00", "endTime": "14:00", "targetTemp": 24}
    ]
  }'

# Test force override
curl -X POST http://esp32-ip/api/schedule \
  -H "Authorization: Bearer YOUR_TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "schedules": [...],
    "force": true
  }'
```

## User Acceptance Testing Checklist

### Admin User Testing

- [ ] Create schedules successfully
- [ ] Modify existing schedules
- [ ] Delete schedules
- [ ] View conflict warnings
- [ ] Override conflicts when needed
- [ ] Access all system features

### Operator User Testing

- [ ] Create and modify schedules
- [ ] Control relays and setpoints
- [ ] View system status
- [ ] Cannot access admin settings
- [ ] Proper permission messages

### Viewer User Testing

- [ ] View schedules (read-only)
- [ ] View system status
- [ ] Cannot modify schedules
- [ ] Cannot control system
- [ ] Clear "contact admin" messages

## Known Issues and Limitations

1. **Maximum 7 Schedule Entries**: EEPROM size limitation
2. **5-Second Minimum Accuracy**: Trade-off between responsiveness and system load
3. **Time Zone Dependency**: Requires NTP sync for accurate scheduling
4. **Single Priority Level**: Simplified system doesn't support complex priority rules

## Testing Environment Setup

### Hardware Requirements

- MAC-SYS board with ESP32
- SSD1306 OLED display
- Temperature sensor (AM2302/DS18B20/LM35)
- Relay load for testing

### Software Requirements

- Arduino IDE with ESP32 core
- SPIFFS file system uploaded
- NTP time synchronization
- WiFi connection for web interface

### Test Data Reset

```cpp
// Reset all schedules (add to setup() for clean testing)
config.numScheduleEntries = 0;
saveConfiguration();
```

## Success Criteria

The schedule system passes testing when:

1. All basic schedule operations work reliably
2. Conflict detection catches overlapping schedules
3. 5-second responsiveness maintained
4. Mobile interface fully functional
5. User permissions properly enforced
6. System stable under extended operation
7. No data loss or corruption
8. Professional user experience maintained

## Conclusion

This testing guide ensures the schedule system meets production requirements for customer deployment. All critical scenarios must pass before firmware release to maintain company reputation and customer satisfaction.
