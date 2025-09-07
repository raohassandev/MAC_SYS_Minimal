#include "config.h"
#include "hardware.h"
#include "network.h"
#include "auth.h"
#include "display.h"
#include "wifi_manager.h"
#include "temperature.h"
#include <EEPROM.h>
#include <esp_task_wdt.h>

// Global system variables
SystemConfig g_system_config;
SystemStatus g_system_status;

// Utility functions
uint16_t calculateChecksum(const void* data, size_t len) {
    uint16_t checksum = 0;
    const uint8_t* bytes = (const uint8_t*)data;
    for (size_t i = 0; i < len; i++) {
        checksum += bytes[i];
    }
    return checksum;
}

// Timing variables
unsigned long last_system_update = 0;
unsigned long last_temp_reading = 0;
unsigned long last_status_update = 0;
unsigned long last_display_update = 0;
unsigned long system_start_time = 0;

// Hardware state variables
bool i2c_initialized = false;
bool g_wifi_connected = false;
bool status_led_state = false;
unsigned long last_led_toggle = 0;

// Forward declarations
void initializeSystem();
bool initializeHardware();
void initializeNetwork();
void loadConfiguration();
void loadDefaultConfiguration();
void saveConfiguration();
bool validateConfiguration();
void updateSystemStatus();
void handleStatusLED();
void systemLoop();
void readTemperatureSensors();

void setup() {
    // Initialize serial communication
    Serial.begin(115200);
    delay(1000);
    
    DEBUG_PRINTLN("=================================");
    DEBUG_PRINTLN("MAC-SYS Minimal v" FIRMWARE_VERSION);
    DEBUG_PRINTLN("Initializing Industrial HVAC Controller");
    DEBUG_PRINTLN("=================================");
    
    // Record system start time
    system_start_time = millis();
    
    // Initialize watchdog timer (ESP32 v3.x compatible)
    // Initialize watchdog timer (simplified for ESP32 v3.2.0 compatibility)
    esp_task_wdt_init(WATCHDOG_TIMEOUT / 1000, true);
    esp_task_wdt_add(NULL);
    
    // Initialize system status
    memset(&g_system_status, 0, sizeof(SystemStatus));
    g_system_status.state = STATE_INITIALIZING;
    g_system_status.last_error = ERROR_NONE;
    
    // Initialize EEPROM
    if (!EEPROM.begin(CONFIG_EEPROM_SIZE)) {
        DEBUG_PRINTLN("ERROR: Failed to initialize EEPROM");
        g_system_status.last_error = ERROR_MEMORY;
    }
    
    // Load system configuration
    loadConfiguration();
    
    // Initialize system components
    initializeSystem();
    
    // Initialize display
    if (initializeDisplay()) {
        DEBUG_PRINTLN("Display initialized successfully");
    } else {
        DEBUG_PRINTLN("WARNING: Display initialization failed");
    }
    
    // Initialize WiFi Manager
    if (initializeWiFiManager()) {
        DEBUG_PRINTLN("WiFi Manager initialized successfully");
        
        // Auto-start config mode if needed
        autoStartConfigMode();
    } else {
        DEBUG_PRINTLN("WARNING: WiFi Manager initialization failed");
    }
    
    DEBUG_PRINTLN("System initialization complete");
    g_system_status.state = STATE_READY;
    
    // Feed watchdog
    esp_task_wdt_reset();
}

void loop() {
    unsigned long current_time = millis();
    
    // Feed watchdog timer
    esp_task_wdt_reset();
    
    // Handle status LED
    handleStatusLED();
    
    // Main system loop (runs every second)
    if (current_time - last_system_update >= SYSTEM_LOOP_INTERVAL) {
        systemLoop();
        last_system_update = current_time;
    }
    
    // Temperature reading loop
    if (current_time - last_temp_reading >= TEMP_READ_INTERVAL) {
        readTemperatureSensors();
        last_temp_reading = current_time;
    }
    
    // Status update loop
    if (current_time - last_status_update >= STATUS_UPDATE_INTERVAL) {
        updateSystemStatus();
        last_status_update = current_time;
    }
    
    // Display update loop
    updateDisplay();
    
    // Handle WiFi Manager
    handleWiFiManager();
    
    // Small delay to prevent watchdog issues
    delay(10);
}

void initializeSystem() {
    DEBUG_PRINTLN("Initializing system components...");
    
    // Initialize authentication system
    initializeAuthSystem();
    
    // Initialize hardware components
    initializeHardware();
    
    // Initialize network components
    initializeNetwork();
    
    // Set initial system state
    g_system_status.state = STATE_CONNECTING;
    
    DEBUG_PRINTLN("System components initialized");
}

// initializeHardware() function is implemented in hardware.cpp

void initializeNetwork() {
    DEBUG_PRINTLN("Initializing network interface...");
    
    // Initialize network stack
    if (!initializeNetworkStack()) {
        DEBUG_PRINTLN("ERROR: Network stack initialization failed");
        g_system_status.last_error = ERROR_WIFI;
        return;
    }
    
    // Start Access Point if enabled
    if (g_system_config.network.enable_ap) {
        DEBUG_PRINTLN("Starting Access Point mode...");
        startConfigurationMode();
    }
    
    // Connect to WiFi network if configured
    if (strlen(g_system_config.network.ssid) > 0) {
        DEBUG_PRINTF("Connecting to WiFi: %s\n", g_system_config.network.ssid);
        
        bool connection_success = false;
        
        if (!g_system_config.network.use_dhcp) {
            // Use static IP configuration
            IPAddress ip(g_system_config.network.ip_address);
            IPAddress gateway(g_system_config.network.gateway);
            IPAddress subnet(g_system_config.network.subnet);
            IPAddress dns1(g_system_config.network.dns1);
            IPAddress dns2(g_system_config.network.dns2);
            
            connection_success = connectToWiFi(g_system_config.network.ssid,
                                              g_system_config.network.password, false);
        } else {
            // Use DHCP
            connection_success = connectToWiFi(g_system_config.network.ssid,
                                             g_system_config.network.password);
        }
        
        if (connection_success) {
            g_wifi_connected = true;
            DEBUG_PRINTF("WiFi connected: IP %s, RSSI %d dBm\n", 
                        "N/A", getCurrentRSSI());
            
            // Start mDNS service
            if (startMDNS(g_system_config.network.hostname)) {
                DEBUG_PRINTF("mDNS service started: %s.local\n", g_system_config.network.hostname);
            }
        } else {
            DEBUG_PRINTLN("WiFi connection failed");
            g_system_status.last_error = ERROR_WIFI;
        }
    }
    
    // Enable auto-reconnect
    setAutoReconnectEnabled(true);
    
    DEBUG_PRINTLN("Network initialization complete");
}

void loadConfiguration() {
    DEBUG_PRINTLN("Loading system configuration...");
    
    // Read configuration from EEPROM
    EEPROM.readBytes(CONFIG_EEPROM_ADDR, &g_system_config, sizeof(SystemConfig));
    
    // Validate configuration
    if (!validateConfiguration()) {
        DEBUG_PRINTLN("Invalid configuration, loading defaults");
        loadDefaultConfiguration();
        saveConfiguration();
    } else {
        DEBUG_PRINTLN("Valid configuration loaded");
    }
}

void loadDefaultConfiguration() {
    DEBUG_PRINTLN("Loading default configuration...");
    
    // Clear configuration structure
    memset(&g_system_config, 0, sizeof(SystemConfig));
    
    // Set magic number and version
    g_system_config.magic_number = CONFIG_MAGIC_NUMBER;
    g_system_config.version = 1;
    
    // Network defaults
    strcpy(g_system_config.network.ssid, "");
    strcpy(g_system_config.network.password, "");
    g_system_config.network.use_dhcp = true;
    strcpy(g_system_config.network.hostname, DEFAULT_HOSTNAME);
    g_system_config.network.enable_ap = true;
    strcpy(g_system_config.network.ap_ssid, DEFAULT_AP_SSID);
    strcpy(g_system_config.network.ap_password, DEFAULT_AP_PASSWORD);
    
    // HVAC defaults
    g_system_config.hvac.setpoint_temperature = DEFAULT_TEMP_SETPOINT;
    g_system_config.hvac.hysteresis = DEFAULT_HYSTERESIS;
    g_system_config.hvac.delta_temperature = 0.5;
    g_system_config.hvac.min_cycle_time = MIN_COMPRESSOR_CYCLE_TIME;
    
    // Thermal Control defaults (MAC_SYS compatible)
    g_system_config.ac_setpoint = 25.0;
    g_system_config.delta_temperature = 1.0;
    g_system_config.delivery_compensation = 0.0;
    g_system_config.operation_mode = 0; // Direct mode
    g_system_config.central_control_mode = true;
    g_system_config.ac_control_enabled = true;
    g_system_config.primary_temp_sensor = 3; // LM35 default
    g_system_config.hvac.max_run_time = MAX_COMPRESSOR_RUN_TIME;
    g_system_config.hvac.operation_mode = MODE_AUTO;
    g_system_config.hvac.enable_schedule = false;
    
    // User defaults
    strcpy(g_system_config.user.username, "admin");
    strcpy(g_system_config.user.password_hash, "21232f297a57a5a743894a0e4a801fc3"); // "admin"
    g_system_config.user.account_locked = false;
    
    // Initialize schedule with default off state
    for (int i = 0; i < MAX_SCHEDULE_ENTRIES; i++) {
        g_system_config.schedule[i].day_of_week = i;
        g_system_config.schedule[i].start_time = INVALID_TIME;
        g_system_config.schedule[i].stop_time = INVALID_TIME;
        g_system_config.schedule[i].enabled = false;
        g_system_config.schedule[i].setpoint = DEFAULT_TEMP_SETPOINT;
    }
    
    g_system_config.last_update = millis();
    
    // Calculate checksums
    g_system_config.network.checksum = calculateChecksum(&g_system_config.network, sizeof(NetworkConfig) - sizeof(uint16_t));
    g_system_config.hvac.checksum = calculateChecksum(&g_system_config.hvac, sizeof(HVACConfig) - sizeof(uint16_t));
    g_system_config.user.checksum = calculateChecksum(&g_system_config.user, sizeof(UserCredentials) - sizeof(uint16_t));
    
    for (int i = 0; i < MAX_SCHEDULE_ENTRIES; i++) {
        g_system_config.schedule[i].checksum = calculateChecksum(&g_system_config.schedule[i], sizeof(ScheduleEntry) - sizeof(uint16_t));
    }
    
    g_system_config.global_checksum = calculateChecksum(&g_system_config, sizeof(SystemConfig) - sizeof(uint16_t));
    
    DEBUG_PRINTLN("Default configuration loaded");
}

void saveConfiguration() {
    DEBUG_PRINTLN("Saving system configuration...");
    
    g_system_config.last_update = millis();
    g_system_config.global_checksum = calculateChecksum(&g_system_config, sizeof(SystemConfig) - sizeof(uint16_t));
    
    EEPROM.writeBytes(CONFIG_EEPROM_ADDR, &g_system_config, sizeof(SystemConfig));
    EEPROM.commit();
    
    DEBUG_PRINTLN("Configuration saved to EEPROM");
}

void saveSystemConfig() {
    saveConfiguration();
}

bool validateConfiguration() {
    // Check magic number
    if (g_system_config.magic_number != CONFIG_MAGIC_NUMBER) {
        DEBUG_PRINTLN("Configuration magic number mismatch");
        return false;
    }
    
    // Check global checksum
    uint16_t calculated_checksum = calculateChecksum(&g_system_config, sizeof(SystemConfig) - sizeof(uint16_t));
    if (calculated_checksum != g_system_config.global_checksum) {
        DEBUG_PRINTLN("Configuration checksum mismatch");
        return false;
    }
    
    return true;
}

void updateSystemStatus() {
    g_system_status.uptime = (millis() - system_start_time) / 1000;
    g_system_status.free_memory = ESP.getFreeHeap();
    
    // Update network status
    if (isWiFiConnected()) {
        g_system_status.wifi_rssi = getCurrentRSSI();
    } else {
        g_system_status.wifi_rssi = -100;
    }
    
    // Check system state based on network status
    NetworkStatus net_status = getNetworkStatus();
    if (g_system_status.last_error != ERROR_NONE && g_system_status.error_count > MAX_ERROR_COUNT) {
        g_system_status.state = STATE_ERROR;
    } else {
        switch (net_status) {
            case NET_CONNECTED_STA:
                g_system_status.state = STATE_RUNNING;
                break;
            case NET_CONNECTING:
                g_system_status.state = STATE_CONNECTING;
                break;
            case NET_AP_MODE:
                g_system_status.state = STATE_READY;  // Ready for configuration
                break;
            case NET_ERROR:
                g_system_status.state = STATE_ERROR;
                break;
            default:
                g_system_status.state = STATE_CONNECTING;
                break;
        }
    }
}

void handleStatusLED() {
    unsigned long current_time = millis();
    unsigned long blink_interval;
    
    // Determine blink pattern based on system state
    switch (g_system_status.state) {
        case STATE_INITIALIZING:
        case STATE_CONNECTING:
            blink_interval = LED_BLINK_FAST;
            break;
        case STATE_RUNNING:
            blink_interval = LED_BLINK_SLOW;
            break;
        case STATE_ERROR:
            blink_interval = LED_BLINK_FAST / 2;  // Very fast blink
            break;
        default:
            blink_interval = LED_BLINK_SLOW;
            break;
    }
    
    // Toggle LED at appropriate interval
    if (current_time - last_led_toggle >= blink_interval) {
        status_led_state = !status_led_state;
        digitalWrite(STATUS_LED_PIN, status_led_state);
        last_led_toggle = current_time;
    }
}

void systemLoop() {
    // Update hardware status
    updateHardwareStatus();
    
    // Update network status and handle auto-reconnect
    handleNetworkEvents();
    handleAutoReconnect();
    
    // Update mDNS services with current system information
    if (isMDNSActive()) {
        // mDNS updates deferred - compatibility mode
    }
    
    // Update WiFi connection status
    g_wifi_connected = isWiFiConnected();
    
    // Main system processing - Run temperature control
    runTemperatureControl();
    
    DEBUG_PRINTF("System Status - State: %d, Uptime: %lu, Free Memory: %lu, Temp: %.1f°C, WiFi: %s\n", 
                 g_system_status.state, g_system_status.uptime, g_system_status.free_memory,
                 g_system_status.current_temperature, g_wifi_connected ? "OK" : "FAIL");
}

void readTemperatureSensors() {
    // Use our new temperature system
    float temperature = readTemperature();
    g_system_status.current_temperature = temperature;
    
    // Debug output for temperature reading
    DEBUG_PRINTF("📊 Temperature update: %.2f°C\n", temperature);
    
    // Apply basic filtering (simple moving average)
    static float temp_readings[TEMP_FILTER_SAMPLES] = {0};
    static int temp_index = 0;
    
    temp_readings[temp_index] = temperature;
    temp_index = (temp_index + 1) % TEMP_FILTER_SAMPLES;
    
    float filtered_temp = 0;
    for (int i = 0; i < TEMP_FILTER_SAMPLES; i++) {
        filtered_temp += temp_readings[i];
    }
    filtered_temp /= TEMP_FILTER_SAMPLES;
    
    g_system_status.current_temperature = filtered_temp;
    g_system_status.setpoint_temperature = g_system_config.hvac.setpoint_temperature;
    
    DEBUG_PRINTF("Temperature: %.2f°C\n", filtered_temp);
}

