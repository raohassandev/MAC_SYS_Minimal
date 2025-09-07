# MAC-SYS Control Panel

A comprehensive ESP32-based HVAC control system with advanced scheduling, user management, and IoT capabilities.

## 🎯 Overview

MAC-SYS is a professional-grade temperature control system designed for intelligent building automation. It features web-based management, multi-user authentication, advanced scheduling capabilities, and comprehensive thermal control with both local and remote operation modes.

## 📋 Table of Contents

- [Features](#features)
- [Hardware Requirements](#hardware-requirements)
- [System Architecture](#system-architecture)
- [Operation Modes](#operation-modes)
- [Thermal Control System](#thermal-control-system)
- [Schedule Management](#schedule-management)
- [User Management & Security](#user-management--security)
- [Network Configuration](#network-configuration)
- [API Documentation](#api-documentation)
- [Installation & Setup](#installation--setup)
- [Troubleshooting](#troubleshooting)
- [Technical Specifications](#technical-specifications)

## ✨ Features

### Core Functionality

- **Dual Operation Modes**: Direct Mode (traditional thermostat) and Schedule Mode (automated energy-saving)
- **Advanced Thermal Control**: Configurable hysteresis and delivery compensation
- **Multi-Sensor Support**: AM2302, DS18B20, and LM35 temperature sensors
- **Real-time Monitoring**: Live temperature, humidity, and system status
- **Schedule Conflict Resolution**: Intelligent priority-based conflict handling
- **Persistent Storage**: EEPROM-based configuration storage survives power cycles

### User Interface

- **Responsive Web UI**: Works on desktop, tablet, and mobile devices
- **Multi-User Support**: Role-based access control (Admin, Operator, Viewer)
- **Real-time Updates**: Live status updates without page refresh
- **Comprehensive Help System**: Built-in user manual and contextual help

### Connectivity

- **WiFi Management**: Auto-connect to "Automatrix" with fallback to AP mode
- **Static IP Support**: Configurable network settings (default: 192.168.1.71)
- **Modbus Integration**: RTU and TCP support for industrial systems
- **RESTful API**: Complete API for system integration

## 🔧 Hardware Requirements

### ESP32 Development Board (MAC-SYS Recommended)

- **MCU**: ESP32-WROOM-32 (240MHz dual-core)
- **Flash**: Minimum 4MB
- **RAM**: 520KB SRAM
- **GPIO**: 6 relay outputs, 6 digital inputs minimum

### Sensors

- **AM2302**: GPIO33 (Temperature + Humidity)
- **DS18B20**: GPIO32 (Temperature, waterproof)
- **LM35**: GPIO35 (Analog temperature)

### I/O Configuration

- **Relays**: PCF8574 @ 0x24 (6 channels)
- **Digital Inputs**: PCF8574 @ 0x22 (6 channels)
- **Display**: SSD1306 OLED 128x64 @ 0x3C
- **Communication**: RS485 (GPIO14/27), RS232 (GPIO16/17)

### Wiring Diagram

```
ESP32-WROOM-32
├── I2C Bus (SDA: GPIO4, SCL: GPIO15)
│   ├── PCF8574 (0x24) → Relay Control
│   ├── PCF8574 (0x22) → Digital Inputs
│   └── SSD1306 (0x3C) → OLED Display
├── Temperature Sensors
│   ├── AM2302 → GPIO33
│   ├── DS18B20 → GPIO32
│   └── LM35 → GPIO35 (ADC)
└── Communication
    ├── RS485 → GPIO14(RX)/GPIO27(TX)
    └── RS232 → GPIO16(RX)/GPIO17(TX)
```

## 🏗️ System Architecture

### Software Stack

```
┌─────────────────────────────────────┐
│           Web Interface             │
│     (HTML/CSS/JavaScript)           │
├─────────────────────────────────────┤
│          RESTful API                │
│      (AsyncWebServer)               │
├─────────────────────────────────────┤
│       Application Layer             │
│  ┌─────────┬─────────┬─────────┐    │
│  │Schedule │ User    │ Smart   │    │
│  │Manager  │ Auth    │ Control │    │
│  └─────────┴─────────┴─────────┘    │
├─────────────────────────────────────┤
│         Hardware Layer              │
│  ┌─────────┬─────────┬─────────┐    │
│  │ PCF8574 │ Sensors │ Modbus  │    │
│  │ I/O     │         │ RTU/TCP │    │
│  └─────────┴─────────┴─────────┘    │
├─────────────────────────────────────┤
│       Storage Layer                 │
│  ┌─────────┬─────────────────────┐   │
│  │ EEPROM  │      SPIFFS         │   │
│  │ Config  │   User/Session      │   │
│  └─────────┴─────────────────────┘   │
└─────────────────────────────────────┘
```

### Data Flow

1. **Sensor Reading** → Temperature sensors → Compensation → Smart Control
2. **User Input** → Web UI → Authentication → API → Configuration Storage
3. **Schedule Engine** → Time Check → Active Schedule → Temperature Control
4. **Control Logic** → Decision Engine → Relay Control → Hardware Output

## 🔄 Operation Modes

The system provides two distinct operation modes that fundamentally change how the AC responds to schedules and manual input.

### Direct Mode

- **Behavior**: AC maintains user setpoint regardless of schedules
- **Use Case**: Traditional thermostat operation for manual control
- **Schedule Impact**: All schedules are ignored, manual setpoint always used
- **Energy Profile**: Standard consumption based on manual setpoint
- **Example**: User sets 25°C → AC always tries to maintain 25°C

**When to Use Direct Mode:**

- Manual override situations
- Testing and commissioning
- Spaces with irregular occupancy
- When precise user control is required

### Schedule Mode

- **Behavior**: AC follows schedules only, turns OFF when no schedule is active
- **Use Case**: Energy-saving automated operation for commercial buildings
- **Schedule Impact**: AC only operates during scheduled periods
- **Energy Profile**: Optimized consumption with automatic OFF periods
- **Example**: Schedule 9AM-5PM at 24°C → AC OFF outside these hours

**When to Use Schedule Mode:**

- Commercial office buildings
- Energy optimization requirements
- Automated building management
- Compliance with energy regulations

### Mode Selection Logic

```cpp
if (operationMode == DIRECT_MODE) {
    // Always use manual setpoint
    targetSetpoint = config.acSetpoint;
    shouldProceedWithControl = true;
} else { // SCHEDULE_MODE
    if (scheduleActive) {
        // Use schedule setpoint
        targetSetpoint = getCurrentScheduleSetpoint();
        shouldProceedWithControl = true;
    } else {
        // No schedule active - AC remains OFF
        shouldProceedWithControl = false;
    }
}
```

## 🌡️ Thermal Control System

### Delta Temperature Control

Delta temperature controls AC cycling behavior and hysteresis to prevent rapid on/off switching that can damage equipment.

#### Positive Delta (Wider Range)

- **Logic**: Creates temperature band above setpoint
- **Example**: Setpoint=25°C, Delta=+2°C
  - AC starts when temperature ≥ 27°C
  - AC stops when temperature ≤ 25°C
  - Maintains temperature between 25-27°C
- **Benefits**: Energy efficient, less frequent cycling
- **Trade-off**: Less precise temperature control
- **Use Case**: General comfort cooling, energy-conscious operation

#### Negative Delta (Aggressive Cooling)

- **Logic**: Pre-cools below setpoint for heat load anticipation
- **Example**: Setpoint=25°C, Delta=-2°C
  - AC starts when temperature ≥ 25°C
  - AC stops when temperature ≤ 23°C
  - Provides 2°C buffer below setpoint
- **Benefits**: Better heat load handling, thermal mass utilization
- **Trade-off**: Higher energy consumption
- **Use Case**: High heat load environments, thermal mass cooling

#### Minimum Delta Validation

```cpp
// Auto-adjustment to prevent rapid cycling
if (deltaValue >= -0.4 && deltaValue <= 0.4) {
    config.deltaTemperature = deltaValue >= 0 ? 0.5 : -0.5;
    Serial.printf("Delta adjusted to %.1f°C for stability\n", config.deltaTemperature);
}
```

### Delivery Compensation

Delivery compensation is an offset/calibration value that compensates for temperature differences between sensor mounting point and target control point.

#### Compensation Calculation

```
Effective Temperature = Sensor Reading - Delivery Compensation
```

#### Practical Applications

**Positive Compensation (+3°C)**

- **Scenario**: Sensor mounted near heat source (server rack, sunny wall)
- **Example**: Sensor=28°C, Target Area=25°C, Compensation=+3°C
- **Result**: System uses 25°C for control decisions
- **Setup**: Measure both sensor and target area, set compensation to difference

**Negative Compensation (-3°C)**

- **Scenario**: Sensor mounted in cool location (AC vent, shaded area)
- **Example**: Sensor=22°C, Target Area=25°C, Compensation=-3°C
- **Result**: System uses 25°C for control decisions
- **Setup**: Compensation corrects for cool sensor location

**Zero Compensation (0°C)**

- **Scenario**: Sensor optimally positioned in representative location
- **Result**: Direct sensor reading used for control
- **Best Practice**: Aim for zero compensation through proper sensor placement

#### Compensation Setup Process

1. **Baseline Measurement**: Measure temperature at sensor location
2. **Target Measurement**: Measure temperature at desired control point
3. **Calculate Compensation**: Compensation = Sensor Temperature - Target Temperature
4. **Apply Setting**: Enter calculated value in system
5. **Validation**: Monitor system performance and fine-tune as needed

### Control Algorithm

```cpp
// Correct delta logic for cooling mode
if (config.deltaTemperature >= 0) {
    // Positive delta: Wider range above setpoint
    turnOnThreshold = targetSetpoint + config.deltaTemperature;
    turnOffThreshold = targetSetpoint;
} else {
    // Negative delta: Aggressive cooling below setpoint
    turnOnThreshold = targetSetpoint;
    turnOffThreshold = targetSetpoint + config.deltaTemperature; // (setpoint + negative = lower temp)
}

// Apply delivery compensation
float compensatedTemp = rawSensorTemp - config.deliveryCompensation;

// Control logic
if (!acCompressorIsOn) {
    if (compensatedTemp > turnOnThreshold) {
        shouldCool = true; // Start cooling
    }
} else {
    if (compensatedTemp <= turnOffThreshold) {
        shouldCool = false; // Stop cooling
    }
}
```

## 📅 Schedule Management

### Schedule Data Structure

```cpp
struct ScheduleEntry {
    byte dayOfWeek;           // 0=Sunday, 1=Monday, etc.
    byte startHour, startMinute;
    byte endHour, endMinute;
    float scheduledSetpoint;  // Target temperature
    bool enabled;             // Schedule active flag
};
```

### Schedule Conflict Resolution

When multiple schedules overlap, the system uses intelligent priority logic:

#### Resolution Rules

1. **Priority Logic**: First active schedule takes precedence
2. **Single Active**: Only one schedule controls the system at any time
3. **Conflict Logging**: All conflicts logged for troubleshooting
4. **UI Indication**: Only controlling schedule shows "RUNNING NOW"

#### Implementation

```cpp
// Enhanced Schedule Logic with conflict resolution
for (int i = 0; i < config.numScheduleEntries; i++) {
    if (isTimeInSchedule(config.weeklySchedule[i])) {
        conflictCount++;

        // Priority logic: First active schedule wins
        if (!scheduleActive) {
            scheduleActive = true;
            activeScheduleIndex = i;
            scheduledSetpoint = config.weeklySchedule[i].scheduledSetpoint;
        } else {
            // Log conflicting schedule but don't activate
            Serial.printf("⚠️ CONFLICT: Ignored schedule %d\n", i);
        }
    }
}
```

### Schedule Persistence

- **Storage**: EEPROM-based persistent storage
- **Backup**: Configuration survives power cycles and resets
- **Validation**: Automatic validation prevents invalid schedules
- **Recovery**: Factory reset option for corrupted configurations

### Best Practices

1. **Avoid Overlaps**: Design schedules with time gaps
2. **Priority Planning**: Create critical schedules first
3. **Monitoring**: Regularly check active schedule status
4. **Documentation**: Maintain schedule documentation for troubleshooting

## 👥 User Management & Security

### User Roles & Permissions

| Role             | Dashboard | Relays       | Schedules    | Settings     | Users        | Description            |
| ---------------- | --------- | ------------ | ------------ | ------------ | ------------ | ---------------------- |
| **Admin (0)**    | ✅ Full   | ✅ Control   | ✅ Modify    | ✅ Configure | ✅ Manage    | Complete system access |
| **Operator (1)** | ✅ View   | ✅ Control   | ✅ Modify    | ❌ Read-only | ❌ No access | Operational control    |
| **Viewer (2)**   | ✅ View   | ❌ Read-only | ❌ Read-only | ❌ Read-only | ❌ No access | Monitoring only        |

### Authentication System

- **Session Management**: 30-minute timeout with remember-me option
- **Password Security**: SHA256 hashing with secure storage
- **Token-based**: Bearer tokens for API authentication
- **Brute Force Protection**: Rate limiting on failed attempts

### Security Features

- **HTTPS Ready**: SSL/TLS support for encrypted communication
- **CORS Protection**: Cross-origin request security
- **Input Validation**: All user inputs validated and sanitized
- **Session Expiry**: Automatic logout for security
- **Audit Logging**: User actions logged for security audit

### Default Credentials

```
Username: admin
Password: admin123
Role: Administrator

Note: Change default credentials immediately after first login
```

## 🌐 Network Configuration

### WiFi Configuration

The system automatically connects to predefined networks with intelligent fallback:

#### Primary Network

- **SSID**: "Automatrix"
- **Password**: "Automatrix"
- **IP Configuration**: Static IP 192.168.1.71
- **Gateway**: 192.168.1.1
- **DNS**: 8.8.8.8, 8.8.4.4

#### Fallback Behavior

```cpp
void startWiFi() {
    // Try connecting to Automatrix network
    WiFi.begin("Automatrix", "Automatrix");

    // Wait up to 10 seconds for connection
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Connected to Automatrix WiFi!");
        currentMode = MODE_STA_STATIC;
    } else {
        Serial.println("Failed to connect. Starting AP mode...");
        currentMode = MODE_AP;
        startAccessPoint();
    }
}
```

#### Access Point Mode

- **SSID**: MAC-SYS-[MAC_SUFFIX]
- **Password**: MAC-SYS[MAC_DIGITS]!
- **IP Range**: 192.168.4.1/24
- **DHCP**: Enabled for client devices

### Network Services

- **Web Server**: Port 80 (HTTP)
- **Modbus TCP**: Port 502 (configurable)
- **mDNS**: Hostname resolution support
- **NTP**: Time synchronization with internet time servers

## 📡 API Documentation

### Authentication Endpoints

#### Login

```http
POST /api/auth/login
Content-Type: application/json

{
    "username": "admin",
    "password": "admin123",
    "remember": true
}

Response:
{
    "success": true,
    "token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...",
    "user": {
        "username": "admin",
        "role": 0
    }
}
```

#### Validate Session

```http
POST /api/auth/validate
Authorization: Bearer [token]

Response:
{
    "valid": true,
    "user": {
        "username": "admin",
        "role": 0
    }
}
```

### System Status

```http
GET /api/status
Authorization: Bearer [token]

Response:
{
    "temperature": 25.4,
    "humidity": 60.2,
    "setpoint": 25.0,
    "compressor": true,
    "relays": [
        {"id": 0, "state": true, "label": "AC Compressor"},
        {"id": 1, "state": false, "label": "Relay 2"}
    ],
    "schedule": {
        "active": true,
        "name": "Office Hours",
        "setpoint": 24.0
    }
}
```

### Control Endpoints

#### Update Operation Mode

```http
POST /api/operation-mode
Authorization: Bearer [token]
Content-Type: application/json

{
    "operationMode": 1  // 0=Direct, 1=Schedule
}
```

#### Thermal Configuration

```http
POST /api/thermal-config
Authorization: Bearer [token]
Content-Type: application/json

{
    "deltaTemperature": 2.0,
    "deliveryCompensation": -1.5
}
```

#### Relay Control

```http
POST /api/relays/1/toggle
Authorization: Bearer [token]

Response:
{
    "relay": 1,
    "state": true,
    "timestamp": "2024-07-04T13:57:58Z"
}
```

### Schedule Management

#### Get Schedules

```http
GET /api/schedule
Authorization: Bearer [token]

Response:
{
    "schedules": [
        {
            "id": "sched_001",
            "name": "Office Hours",
            "days": [1, 2, 3, 4, 5],
            "startTime": "09:00",
            "endTime": "17:00",
            "targetTemp": 24.0,
            "enabled": true
        }
    ]
}
```

#### Create/Update Schedule

```http
POST /api/schedule
Authorization: Bearer [token]
Content-Type: application/json

{
    "schedules": [
        {
            "name": "Morning Session",
            "days": [1, 2, 3, 4, 5],
            "startTime": "08:30",
            "endTime": "12:00",
            "targetTemp": 23.5,
            "enabled": true
        }
    ]
}
```

## 🚀 Installation & Setup

### Prerequisites

- PlatformIO IDE or Arduino IDE
- ESP32 development board (MAC-SYS recommended)
- Required sensors and I/O modules
- WiFi network access

### Software Installation

1. **Clone Repository**

```bash
git clone https://github.com/raohassandev/esp32_firmware.git
cd esp32_firmware
```

2. **Install Dependencies**

```bash
# Using PlatformIO
pio lib install

# Dependencies will be automatically installed:
# - ArduinoJson
# - AsyncTCP
# - ESPAsyncWebServer
# - Adafruit GFX Library
# - Adafruit SSD1306
# - PCF8574 library
# - OneWire
# - DallasTemperature
# - DHT sensor library
```

3. **Configure Hardware**

   - Update pin definitions in `config.h`
   - Verify I2C addresses for PCF8574 modules
   - Configure sensor types and pins

4. **Build and Upload**

```bash
# PlatformIO
pio run --target upload

# Arduino IDE
# Open MAC_SYS.ino and upload via IDE
```

### Initial Configuration

1. **First Boot**

   - System starts in AP mode if no saved WiFi configuration
   - Connect to MAC-SYS-[MAC] access point
   - Navigate to http://192.168.4.1

2. **WiFi Setup**

   - Configure network settings
   - Set static IP if required
   - Save configuration (stored in EEPROM)

3. **User Setup**

   - Login with default credentials (admin/admin123)
   - Create additional users as needed
   - Change default passwords

4. **Sensor Calibration**
   - Verify sensor readings
   - Configure delivery compensation
   - Test thermal control operation

### Hardware Setup

1. **I2C Connections**

```
ESP32    PCF8574 (Relays)    PCF8574 (Inputs)    SSD1306
GPIO4  → SDA               → SDA               → SDA
GPIO15 → SCL               → SCL               → SCL
3.3V   → VCC               → VCC               → VCC
GND    → GND               → GND               → GND
```

2. **Temperature Sensors**

```
AM2302:  Data → GPIO33, VCC → 3.3V, GND → GND
DS18B20: Data → GPIO32, VCC → 3.3V, GND → GND
LM35:    Out  → GPIO35, VCC → 3.3V, GND → GND
```

3. **Communication Interfaces**

```
RS485: A+ → Terminal A+, B- → Terminal B-
RS232: TX → GPIO17, RX → GPIO16
```

## 🔧 Troubleshooting

### Common Issues

#### WiFi Connection Problems

**Symptoms**: Cannot connect to web interface
**Solutions**:

1. Check WiFi credentials in code
2. Verify network availability
3. Reset to AP mode if needed
4. Check static IP configuration

#### Schedule Not Working

**Symptoms**: AC not following schedules
**Solutions**:

1. Verify operation mode is set to "Schedule Mode"
2. Check schedule time settings
3. Confirm schedule is enabled
4. Review conflict resolution logs

#### Temperature Reading Issues

**Symptoms**: Incorrect temperature display
**Solutions**:

1. Check sensor wiring and power
2. Verify sensor type configuration
3. Calibrate delivery compensation
4. Test with known reference temperature

#### User Authentication Problems

**Symptoms**: Cannot login or session expires
**Solutions**:

1. Clear browser cache and cookies
2. Check user credentials
3. Verify session timeout settings
4. Reset to default admin account if needed

### Diagnostic Tools

#### Serial Monitor Output

```
🕐 Time: 14:07:30 Day:5 TZ:UTC
🎯 DIRECT MODE: Using manual setpoint 25.0°C
🔄 HYSTERESIS: Positive delta (2.0°C) - Wider range: 25.0-27.0°C
🌡️ TEMP: Current=26.2°C | Turn-ON=27.0°C | Turn-OFF=25.0°C | Delta=2.0°C
❄️ MAINTAIN: AC stays OFF - 26.2°C ≤ 27.0°C (below turn-on)
```

#### System Information Page

- Hardware status and versions
- Network configuration details
- Memory usage and performance metrics
- Sensor status and readings

#### Log Analysis

- Authentication events
- Schedule conflicts and resolutions
- Thermal control decisions
- System errors and warnings

### Factory Reset

```cpp
// Emergency reset procedure
1. Hold BOOT button while powering on
2. Connect to AP mode network
3. Navigate to System Info page
4. Click "Factory Reset" button
5. Confirm reset operation
```

## 📊 Technical Specifications

### Performance Specifications

- **Response Time**: < 100ms for web interface
- **Temperature Accuracy**: ±0.5°C (sensor dependent)
- **Schedule Resolution**: 1-minute precision
- **Maximum Schedules**: 7 entries (configurable)
- **User Sessions**: 5 concurrent users
- **Memory Usage**: ~60% RAM, ~70% Flash (typical)

### Environmental Specifications

- **Operating Temperature**: 0°C to 50°C
- **Humidity Range**: 10% to 90% RH (non-condensing)
- **Power Supply**: 5V DC, 2A minimum
- **Enclosure Rating**: IP20 (indoor use)

### Communication Specifications

- **WiFi Standards**: 802.11 b/g/n (2.4GHz)
- **Modbus RTU**: 1200-115200 baud, configurable parity
- **Modbus TCP**: Standard port 502, up to 10 concurrent connections
- **Web Interface**: HTTP/HTTPS, responsive design
- **API**: RESTful JSON, authentication required

### Compliance & Standards

- **Safety**: Low voltage design, isolated I/O
- **EMC**: CE marking compatible design
- **Software**: Open source components, GPL v3 license
- **Documentation**: Complete technical documentation provided

## 📝 License

This project is licensed under the GPL v3 License - see the [LICENSE](LICENSE) file for details.

## 🤝 Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## 📞 Support

For technical support and documentation:

- **GitHub Issues**: [Create an issue](https://github.com/raohassandev/esp32_firmware/issues)
- **Documentation**: See built-in help system in web interface
- **Community**: Join discussions in repository

## 📈 Version History

- **v1.0.0** - Initial release with basic HVAC control
- **v1.1.0** - Added user management and authentication
- **v1.2.0** - Implemented advanced scheduling system
- **v1.3.0** - Added operation modes and thermal control enhancements
- **v1.4.0** - Current version with conflict resolution and improved UI

---

**Built with ❤️ for intelligent building automation**
