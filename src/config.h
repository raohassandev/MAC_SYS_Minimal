#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <EEPROM.h>
#include <Wire.h>
#include <RTClib.h>
#include <esp_task_wdt.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
// WiFi includes removed for ESP32 v3.2.0 compatibility
// Full WiFi functionality will be restored when ESP32 core compatibility is resolved

// System Information
#define FIRMWARE_VERSION "1.0.0"
#define SYSTEM_NAME "MAC-SYS-MINIMAL"
#define MANUFACTURER "MAC-SYS"

// Hardware Configuration - MAC-SYS Board (Corrected)
#define I2C_SDA_PIN 4
#define I2C_SCL_PIN 15
#define I2C_FREQUENCY 100000

// SSD1306 OLED Display Configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

// PCF8574 I2C Addresses - MAC-SYS Standard
#define PCF8574_OUTPUT_ADDR 0x24  // Relay outputs (MAC-SYS standard)
#define PCF8574_INPUT_ADDR  0x22  // Digital inputs (MAC-SYS standard)

// Status LED Configuration
#define STATUS_LED_PIN 2
#define LED_BLINK_FAST 200
#define LED_BLINK_SLOW 1000

// Temperature Sensor Configuration
#define TEMP_SENSOR_PIN 36        // ADC1_CH0 for LM35
#define ADC_RESOLUTION 4096       // 12-bit ADC
#define ADC_VREF 3.3             // Reference voltage
#define LM35_SCALE 0.01          // 10mV per degree
#define TEMP_FILTER_SAMPLES 10   // Moving average samples

// RTC Configuration
#define RTC_I2C_ADDRESS 0x68     // DS1307 address

// Network Configuration
#define DEFAULT_AP_SSID "MACSYS-CONFIG"
#define DEFAULT_AP_PASSWORD "admin123"
#define DEFAULT_HOSTNAME "macsys-minimal"
#define WIFI_CONNECT_TIMEOUT 20000
#define WIFI_RECONNECT_INTERVAL 60000

// WiFi Manager Configuration
#define WIFI_MANAGER_TIMEOUT 300000  // 5 minutes timeout for config portal
#define CUSTOM_DNS_PORT 53
#define CONFIG_PORTAL_SSID "MAC-SYS-Setup"
#define CONFIG_PORTAL_PASSWORD ""    // Open AP for easy setup

// Modbus TCP Configuration
#define MODBUS_TCP_PORT 502
#define MODBUS_MAX_MASTERS 5
#define MODBUS_TIMEOUT 1000
#define MODBUS_MAX_REGISTERS 100

// System Timing
#define SYSTEM_LOOP_INTERVAL 1000    // Main loop interval (ms)
#define TEMP_READ_INTERVAL 5000      // Temperature reading interval
#define STATUS_UPDATE_INTERVAL 2000   // Status update interval
#define WATCHDOG_TIMEOUT 30000       // Watchdog timeout (ms)

// Memory Management
#define CONFIG_EEPROM_SIZE 4096
#define CONFIG_EEPROM_ADDR 0
#define CONFIG_MAGIC_NUMBER 0xAC55   // Configuration validity check

// HVAC Control Parameters
#define MIN_TEMP_SETPOINT 16.0
#define MAX_TEMP_SETPOINT 40.0
#define DEFAULT_TEMP_SETPOINT 24.0
#define DEFAULT_HYSTERESIS 1.0
#define MIN_COMPRESSOR_CYCLE_TIME 300  // 5 minutes
#define MAX_COMPRESSOR_RUN_TIME 7200   // 2 hours

// Error Handling
#define MAX_ERROR_COUNT 10
#define ERROR_RESET_INTERVAL 300000    // 5 minutes

// Schedule Configuration
#define MAX_SCHEDULE_ENTRIES 7         // One per day of week
#define INVALID_TIME 0xFFFF

// Authentication Configuration
#define MAX_CONCURRENT_SESSIONS 10
#define MAX_FAILED_ATTEMPTS 10
#define DEFAULT_SESSION_TIMEOUT 3600      // 1 hour in seconds
#define DEFAULT_MAX_LOGIN_ATTEMPTS 5
#define DEFAULT_LOCKOUT_DURATION 900      // 15 minutes in seconds
#define MIN_PASSWORD_LENGTH 6
#define MAX_PASSWORD_LENGTH 32

// System States
enum SystemState {
    STATE_INITIALIZING = 0,
    STATE_CONNECTING = 1,
    STATE_READY = 2,
    STATE_RUNNING = 3,
    STATE_ERROR = 4,
    STATE_MAINTENANCE = 5
};

// HVAC Operation Modes
enum OperationMode {
    MODE_OFF = 0,
    MODE_MANUAL = 1,
    MODE_AUTO = 2,
    MODE_SCHEDULE = 3,
    MODE_MAINTENANCE = 4
};

// Error Codes
enum ErrorCode {
    ERROR_NONE = 0,
    ERROR_WIFI = 1,
    ERROR_SENSOR = 2,
    ERROR_RTC = 3,
    ERROR_I2C = 4,
    ERROR_MEMORY = 5,
    ERROR_MODBUS = 6,
    ERROR_CONFIG = 7
};

// Network Configuration Structure
struct NetworkConfig {
    char ssid[32];
    char password[64];
    bool use_dhcp;
    uint32_t ip_address;
    uint32_t gateway;
    uint32_t subnet;
    uint32_t dns1;
    uint32_t dns2;
    char hostname[32];
    bool enable_ap;
    char ap_ssid[32];
    char ap_password[64];
    uint16_t checksum;
};

// HVAC Configuration Structure  
struct HVACConfig {
    float setpoint_temperature;
    float hysteresis;
    float delta_temperature;
    uint16_t min_cycle_time;
    uint16_t max_run_time;
    OperationMode operation_mode;
    bool enable_schedule;
    uint16_t checksum;
};

// Schedule Entry Structure
struct ScheduleEntry {
    uint8_t day_of_week;     // 0=Sunday, 1=Monday, etc.
    uint16_t start_time;     // Minutes from midnight
    uint16_t stop_time;      // Minutes from midnight  
    bool enabled;
    float setpoint;
    uint16_t checksum;
};

// System Status Structure
struct SystemStatus {
    SystemState state;
    uint32_t uptime;
    float current_temperature;
    float setpoint_temperature;
    bool compressor_running;
    bool schedule_active;
    uint32_t last_error;
    uint8_t error_count;
    uint32_t free_memory;
    int8_t wifi_rssi;
    uint8_t modbus_connections;
};

// User Credentials Structure
struct UserCredentials {
    char username[32];
    char password_hash[64];
    uint32_t last_login;
    uint16_t login_attempts;
    bool account_locked;
    uint16_t checksum;
};

// Main Configuration Structure
struct SystemConfig {
    uint16_t magic_number;
    uint16_t version;
    NetworkConfig network;
    HVACConfig hvac;
    ScheduleEntry schedule[MAX_SCHEDULE_ENTRIES];
    UserCredentials user;
    
    // Thermal Control Parameters (MAC_SYS compatible)
    float ac_setpoint;                // Manual setpoint temperature
    float delta_temperature;          // Hysteresis delta for control
    float delivery_compensation;      // Sensor compensation offset
    uint8_t operation_mode;          // 0=Direct, 1=Schedule
    bool central_control_mode;       // true=Central, false=Local
    bool ac_control_enabled;         // true=Auto, false=Force-Off
    uint8_t primary_temp_sensor;     // 1=AM2302, 2=DS18B20, 3=LM35
    
    uint32_t last_update;
    uint16_t global_checksum;
};

// Modbus Register Map (as per SCADA integration requirements)
#define MB_REG_SYSTEM_STATUS        1000
#define MB_REG_CURRENT_TEMP         1001
#define MB_REG_SETPOINT_TEMP        1002
#define MB_REG_OPERATION_MODE       1003
#define MB_REG_COMPRESSOR_STATE     1004
#define MB_REG_SCHEDULE_ACTIVE      1005
#define MB_REG_ERROR_CODE           1006
#define MB_REG_UPTIME_LOW           1007
#define MB_REG_UPTIME_HIGH          1008
#define MB_REG_WIFI_RSSI            1009
#define MB_REG_FREE_MEMORY          1010

// Control Registers (Writable)
#define MB_REG_SET_SETPOINT         2001
#define MB_REG_SET_MODE             2002
#define MB_REG_SET_SCHEDULE_ENABLE  2003
#define MB_REG_SYSTEM_RESET         2004
#define MB_REG_CALIBRATION_OFFSET   2005

// Schedule Registers (Day 0-6, Start/Stop times)
#define MB_REG_SCHEDULE_BASE        3000  // Base + (day*4) + offset
// Offset 0: enabled, 1: start_time, 2: stop_time, 3: setpoint*10

// Configuration Registers
#define MB_REG_HYSTERESIS           4001
#define MB_REG_MIN_CYCLE_TIME       4002
#define MB_REG_MAX_RUN_TIME         4003
#define MB_REG_DELTA_TEMP           4004

// Function Prototypes
extern SystemConfig g_system_config;
extern SystemStatus g_system_status;

// Utility Macros
#define CONSTRAIN_TEMP(temp) constrain(temp, MIN_TEMP_SETPOINT, MAX_TEMP_SETPOINT)
#define TEMP_TO_MODBUS(temp) ((uint16_t)(temp * 100))
#define MODBUS_TO_TEMP(reg) ((float)(reg / 100.0))

// Debug Macros
#ifdef CORE_DEBUG_LEVEL
  #define DEBUG_PRINT(x) Serial.print(x)
  #define DEBUG_PRINTLN(x) Serial.println(x)
  #define DEBUG_PRINTF(format, ...) Serial.printf(format, ##__VA_ARGS__)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)  
  #define DEBUG_PRINTF(format, ...)
#endif

#endif // CONFIG_H
