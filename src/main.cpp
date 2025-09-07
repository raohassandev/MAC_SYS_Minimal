#include "config.h"
#include "hardware.h"
#include "network.h"
#include "auth.h"
#include "display.h"
#include "wifi_manager.h"
#include "temperature.h"
#include "relay_control.h"
#include "temperature_control.h"
#include "rtc_manager.h"
#include "schedule_manager.h"
#include <EEPROM.h>
#include <esp_task_wdt.h>
#include <WiFi.h>
#include <WebServer.h>

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
unsigned long last_io_update = 0;
unsigned long system_start_time = 0;

// Hardware state variables
bool i2c_initialized = false;
bool g_wifi_connected = false;
bool status_led_state = false;
unsigned long last_led_toggle = 0;

// Web server for device management
WebServer* device_server = nullptr;

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
void setupDeviceWebServer();
void handleDeviceWebServer();
void updateIOSystem();

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
    
    // Initialize relay and I/O system
    if (!relay_controller.begin()) {
        DEBUG_PRINTLN("WARNING: Relay controller initialization failed");
        g_system_status.state = STATE_ERROR;
    } else {
        DEBUG_PRINTLN("✅ Relay and I/O system initialized successfully");
    }
    
    // Initialize temperature control system
    temp_controller.begin();
    
    // Set up initial zone configuration (Zone 0 as example)
    temp_controller.getZoneConfig(0).enabled = true;
    temp_controller.getZoneConfig(0).mode = TEMP_MODE_HEATING;
    temp_controller.getZoneConfig(0).setpoint = 22.0;
    temp_controller.getZoneConfig(0).delta = 1.0;
    DEBUG_PRINTLN("✅ Temperature control system initialized");
    
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
    
    // I/O system update loop
    if (current_time - last_io_update >= 100) {  // Update I/O every 100ms
        updateIOSystem();
        last_io_update = current_time;
    }
    
    // Update RTC manager (for NTP sync)
    rtc_manager.updateTime();
    
    // Process schedule manager
    schedule_manager.process();
    
    // Handle WiFi Manager
    handleWiFiManager();
    
    // Handle device web server
    handleDeviceWebServer();
    
    // Small delay to prevent watchdog issues
    delay(10);
}

void initializeSystem() {
    DEBUG_PRINTLN("Initializing system components...");
    
    // Initialize authentication system
    initializeAuthSystem();
    
    // Initialize hardware components
    initializeHardware();
    
    // Initialize RTC manager for scheduling
    rtc_manager.begin();
    
    // Initialize schedule manager
    schedule_manager.begin();
    
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
            
            // Synchronize time with NTP servers
            rtc_manager.syncWithNTP();
            
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
    
    String statusMsg = "System Status - State: " + String(g_system_status.state) + 
                      ", Uptime: " + String(g_system_status.uptime) + 
                      ", Free Memory: " + String(g_system_status.free_memory) + 
                      ", Temp: " + String(g_system_status.current_temperature, 1) + "°C";
    
    if (g_wifi_connected) {
        statusMsg += ", WiFi: OK (IP: " + WiFi.localIP().toString() + ")";
    } else {
        statusMsg += ", WiFi: FAIL";
    }
    
    DEBUG_PRINTLN(statusMsg);
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

void setupDeviceWebServer() {
    if (!g_wifi_connected || device_server != nullptr) {
        return;  // Only start when WiFi is connected and not already running
    }
    
    DEBUG_PRINTLN("Starting device web server...");
    device_server = new WebServer(80);
    
    // Main device status page
    device_server->on("/", []() {
        String html = "<!DOCTYPE html><html><head><title>MAC-SYS Device Status</title>";
        html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
        html += "<meta charset='UTF-8'>";
        html += "<style>body{font-family:Arial,sans-serif;margin:20px;background:#f0f0f0}";
        html += ".container{background:white;padding:20px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1);max-width:1200px;margin:0 auto}";
        html += ".status{background:#e8f5e8;padding:15px;border-radius:5px;margin:10px 0}";
        html += ".temp{font-size:24px;color:#2c5234;font-weight:bold}";
        html += ".info{display:flex;justify-content:space-between;margin:10px 0;padding:5px 0;border-bottom:1px solid #eee}";
        html += ".label{font-weight:bold;color:#666}";
        html += ".section{background:#f8f9fa;border-radius:8px;margin:20px 0;overflow:hidden;border:1px solid #dee2e6}";
        html += ".section-header{background:#007bff;color:white;padding:12px 20px;cursor:pointer;user-select:none;display:flex;justify-content:space-between;align-items:center}";
        html += ".section-header:hover{background:#0056b3}";
        html += ".section-content{padding:20px;display:none}";
        html += ".section-content.active{display:block}";
        html += ".arrow{transition:transform 0.3s}";
        html += ".arrow.down{transform:rotate(90deg)}";
        html += "button{transition:all 0.3s}";
        html += "button:hover{opacity:0.8;transform:translateY(-2px)}";
        html += "</style></head><body>";
        
        html += "<div class='container'>";
        html += "<h1>MAC-SYS Industrial Controller</h1>";
        
        html += "<div class='status'>";
        html += "<div class='temp'>Temperature: " + String(g_system_status.current_temperature, 1) + "&deg;C</div>";
        html += "</div>";
        
        // System Status Section
        html += "<div class='section'>";
        html += "<div class='section-header' onclick='toggleSection(\"system\")'>";
        html += "<span>&#x1F4CA; System Status</span>";
        html += "<span class='arrow' id='system-arrow'>&#x25B6;</span>";
        html += "</div>";
        html += "<div class='section-content active' id='system-content'>";
        html += "<div class='info'><span class='label'>System State:</span><span>" + String(g_system_status.state) + "</span></div>";
        html += "<div class='info'><span class='label'>Uptime:</span><span>" + String(g_system_status.uptime) + "s</span></div>";
        html += "<div class='info'><span class='label'>Free Memory:</span><span>" + String(g_system_status.free_memory) + " bytes</span></div>";
        html += "<div class='info'><span class='label'>WiFi Network:</span><span>" + WiFi.SSID() + "</span></div>";
        html += "<div class='info'><span class='label'>IP Address:</span><span>" + WiFi.localIP().toString() + "</span></div>";
        html += "<div class='info'><span class='label'>Signal Strength:</span><span>" + String(WiFi.RSSI()) + " dBm</span></div>";
        html += "</div>";
        html += "</div>";
        
        // Relay Control Section
        html += "<div class='section'>";
        html += "<div class='section-header' onclick='toggleSection(\"relay\")'>";
        html += "<span>&#x1F50C; Relay Control - Manual Testing</span>";
        html += "<span class='arrow' id='relay-arrow'>&#x25B6;</span>";
        html += "</div>";
        html += "<div class='section-content' id='relay-content'>";
        html += "<p style='margin:0 0 15px 0;color:#666;font-style:italic'>Click buttons below to manually control each relay for testing</p>";
        
        // Add individual relay control cards
        html += "<div style='display:grid;grid-template-columns:repeat(auto-fit,minmax(300px,1fr));gap:15px;margin:20px 0'>";
        
        for (int i = 0; i < 6; i++) {
            bool relayState = relay_controller.getRelayState(i);
            String stateColor = relayState ? "#28a745" : "#dc3545";
            String stateText = relayState ? "ON" : "OFF";
            String cardBorder = relayState ? "border-left:4px solid #28a745" : "border-left:4px solid #dc3545";
            
            html += "<div style='background:white;padding:15px;border-radius:6px;box-shadow:0 2px 4px rgba(0,0,0,0.1);" + cardBorder + "'>";
            html += "<h4 style='margin:0 0 10px 0;color:#333'>🔌 Relay " + String(i + 1) + "</h4>";
            html += "<div style='display:flex;align-items:center;justify-content:space-between;margin:10px 0'>";
            html += "<span style='font-weight:bold'>Status:</span>";
            html += "<span style='color:" + stateColor + ";font-weight:bold;font-size:16px'>" + stateText + "</span>";
            html += "</div>";
            
            // Individual control buttons
            html += "<div style='display:flex;gap:8px;margin-top:15px'>";
            html += "<button onclick='setRelay(" + String(i) + ", true)' style='flex:1;padding:8px 12px;border:none;border-radius:4px;background:#28a745;color:white;cursor:pointer;font-weight:bold'>";
            html += "🟢 Turn ON</button>";
            html += "<button onclick='setRelay(" + String(i) + ", false)' style='flex:1;padding:8px 12px;border:none;border-radius:4px;background:#dc3545;color:white;cursor:pointer;font-weight:bold'>";
            html += "🔴 Turn OFF</button>";
            html += "</div>";
            html += "</div>";
        }
        
        html += "</div>";
        
        // Add test all relays buttons
        html += "<div style='margin:20px 0;text-align:center'>";
        html += "<button onclick='testAllRelays(true)' style='padding:8px 16px;margin:5px;border:none;border-radius:4px;background:#28a745;color:white;cursor:pointer;font-weight:bold'>🟢 Turn All ON</button>";
        html += "<button onclick='testAllRelays(false)' style='padding:8px 16px;margin:5px;border:none;border-radius:4px;background:#dc3545;color:white;cursor:pointer;font-weight:bold'>🔴 Turn All OFF</button>";
        html += "</div>";
        html += "</div>";
        html += "</div>";
        
        // Temperature Control Section
        html += "<div class='section'>";
        html += "<div class='section-header' onclick='toggleSection(\"temp\")'>";
        html += "<span>&#x1F321; Temperature Control System</span>";
        html += "<span class='arrow' id='temp-arrow'>&#x25B6;</span>";
        html += "</div>";
        html += "<div class='section-content' id='temp-content'>";
        
        // Zone 0 control
        ZoneConfig& zone0 = temp_controller.getZoneConfig(0);
        float currentTemp = temp_controller.getCompensatedTemp(0);
        String modeStr = "";
        switch(zone0.mode) {
            case TEMP_MODE_OFF: modeStr = "OFF"; break;
            case TEMP_MODE_HEATING: modeStr = "HEATING"; break;
            case TEMP_MODE_COOLING: modeStr = "COOLING"; break;
            case TEMP_MODE_AUTO: modeStr = "AUTO"; break;
            case TEMP_MODE_MANUAL: modeStr = "MANUAL"; break;
        }
        
        html += "<div style='background:white;padding:15px;border-radius:6px;margin:10px 0'>";
        html += "<h3>Zone 1 Control</h3>";
        html += "<div class='info'><span class='label'>Current Temperature:</span><span style='font-weight:bold;color:#007bff'>" + String(currentTemp, 1) + "°C</span></div>";
        html += "<div class='info'><span class='label'>Setpoint:</span><span>" + String(zone0.setpoint, 1) + "°C</span></div>";
        html += "<div class='info'><span class='label'>Mode:</span><span>" + modeStr + "</span></div>";
        html += "<div class='info'><span class='label'>Status:</span><span style='color:" + String(zone0.current_state ? "#28a745" : "#dc3545") + "'>" + String(zone0.current_state ? "ACTIVE" : "IDLE") + "</span></div>";
        
        // Temperature control form
        html += "<form method='post' action='/tempcontrol' style='margin-top:15px'>";
        html += "<input type='hidden' name='zone' value='0'>";
        
        html += "<div style='margin:10px 0'>";
        html += "<label>Setpoint (°C):</label><br>";
        html += "<input type='number' name='setpoint' value='" + String(zone0.setpoint, 1) + "' min='5' max='40' step='0.5' style='width:100px;padding:5px'>";
        html += "</div>";
        
        html += "<div style='margin:10px 0'>";
        html += "<label>Delta (±°C):</label><br>";
        html += "<input type='number' name='delta' value='" + String(zone0.delta, 1) + "' min='0.5' max='5' step='0.5' style='width:100px;padding:5px'>";
        html += "</div>";
        
        html += "<div style='margin:10px 0'>";
        html += "<label>Mode:</label><br>";
        html += "<select name='mode' style='padding:5px'>";
        html += "<option value='0'" + String(zone0.mode == TEMP_MODE_OFF ? " selected" : "") + ">OFF</option>";
        html += "<option value='1'" + String(zone0.mode == TEMP_MODE_HEATING ? " selected" : "") + ">HEATING</option>";
        html += "<option value='2'" + String(zone0.mode == TEMP_MODE_COOLING ? " selected" : "") + ">COOLING</option>";
        html += "<option value='3'" + String(zone0.mode == TEMP_MODE_AUTO ? " selected" : "") + ">AUTO</option>";
        html += "</select>";
        html += "</div>";
        
        html += "<button type='submit' style='background:#007bff;color:white;padding:8px 16px;border:none;border-radius:4px;cursor:pointer'>Apply Settings</button>";
        html += "</form>";
        html += "</div>";
        html += "</div>";
        html += "</div>";
        
        // Digital Inputs Section
        html += "<div class='section'>";
        html += "<div class='section-header' onclick='toggleSection(\"inputs\")'>";
        html += "<span>&#x1F4CA; Digital Inputs Status</span>";
        html += "<span class='arrow' id='inputs-arrow'>&#x25B6;</span>";
        html += "</div>";
        html += "<div class='section-content' id='inputs-content'>";
        html += "<p style='margin:0 0 15px 0;color:#666;font-style:italic'>Real-time status of 6 digital input channels</p>";
        
        // Add input status indicators
        for (int i = 0; i < 6; i++) {
            bool inputState = relay_controller.getInputState(i);
            String stateColor = inputState ? "#dc3545" : "#28a745";
            String stateText = inputState ? "ACTIVE" : "INACTIVE";
            
            html += "<div class='info' style='margin:8px 0'>";
            html += "<span class='label'>Input " + String(i + 1) + ":</span>";
            html += "<span style='color:" + stateColor + ";font-weight:bold'>" + stateText + "</span></div>";
        }
        html += "</div>";
        html += "</div>";
        
        // WiFi Management Section  
        html += "<div class='section'>";
        html += "<div class='section-header' onclick='toggleSection(\"wifi\")'>";
        html += "<span>&#x1F4F6; WiFi Management</span>";
        html += "<span class='arrow' id='wifi-arrow'>&#x25B6;</span>";
        html += "</div>";
        html += "<div class='section-content' id='wifi-content'>";
        html += "<h3>Network Configuration</h3>";
        html += "<form method='post' action='/wifi'>";
        html += "<div style='margin:10px 0'><label><strong>Network Mode:</strong></label><br>";
        html += "<label><input type='radio' name='mode' value='dhcp' checked> DHCP (Automatic)</label><br>";
        html += "<label><input type='radio' name='mode' value='static'> Static IP</label></div>";
        
        html += "<div id='static-config' style='display:none;margin:15px 0;padding:15px;background:#e9ecef;border-radius:5px'>";
        html += "<div style='margin:8px 0'><label>IP Address:</label><br><input type='text' name='static_ip' placeholder='192.168.1.100' style='width:200px;padding:5px'></div>";
        html += "<div style='margin:8px 0'><label>Gateway:</label><br><input type='text' name='gateway' placeholder='192.168.1.1' style='width:200px;padding:5px'></div>";
        html += "<div style='margin:8px 0'><label>Subnet Mask:</label><br><input type='text' name='subnet' placeholder='255.255.255.0' style='width:200px;padding:5px'></div>";
        html += "<div style='margin:8px 0'><label>DNS Server:</label><br><input type='text' name='dns' placeholder='8.8.8.8' style='width:200px;padding:5px'></div>";
        html += "</div>";
        
        html += "<div style='margin:15px 0'><button type='submit' style='background:#007bff;color:white;padding:10px 20px;border:none;border-radius:5px;cursor:pointer'>Apply Network Settings</button></div>";
        html += "</form>";
        
        html += "<h3>Change WiFi Network</h3>";
        html += "<button onclick=\"location.href='/wifi-config'\" style='background:#28a745;color:white;padding:10px 20px;border:none;border-radius:5px;cursor:pointer'>Configure WiFi Network</button>";
        html += "</div>";
        html += "</div>";
        html += "</div>";
        
        // Device Information Section
        html += "<div class='section'>";
        html += "<div class='section-header' onclick='toggleSection(\"device\")'>";
        html += "<span>&#x1F4BB; Device Information</span>";
        html += "<span class='arrow' id='device-arrow'>&#x25B6;</span>";
        html += "</div>";
        html += "<div class='section-content' id='device-content'>";
        html += "<div class='info'><span class='label'>Chip Model:</span><span>ESP32</span></div>";
        html += "<div class='info'><span class='label'>MAC Address:</span><span>" + WiFi.macAddress() + "</span></div>";
        html += "<div class='info'><span class='label'>Flash Size:</span><span>" + String(ESP.getFlashChipSize() / 1024) + " KB</span></div>";
        html += "</div>";
        html += "</div>";
        
        html += "<script>";
        html += "document.querySelectorAll('input[name=mode]').forEach(function(radio) {";
        html += "  radio.addEventListener('change', function() {";
        html += "    document.getElementById('static-config').style.display = this.value === 'static' ? 'block' : 'none';";
        html += "  });";
        html += "});";
        html += "function setRelay(relayNum, state) {";
        html += "  fetch('/relay', {";
        html += "    method: 'POST',";
        html += "    headers: {'Content-Type': 'application/x-www-form-urlencoded'},";
        html += "    body: 'relay=' + relayNum + '&action=set&state=' + (state ? '1' : '0')";
        html += "  }).then(response => response.json()).then(data => {";
        html += "    if(data.success) {";
        html += "      location.reload();";
        html += "    } else {";
        html += "      alert('Failed to control relay: ' + data.message);";
        html += "    }";
        html += "  }).catch(err => alert('Network error: ' + err));";
        html += "}";
        html += "function toggleRelay(relayNum) {";
        html += "  fetch('/relay', {";
        html += "    method: 'POST',";
        html += "    headers: {'Content-Type': 'application/x-www-form-urlencoded'},";
        html += "    body: 'relay=' + relayNum + '&action=toggle'";
        html += "  }).then(response => response.json()).then(data => {";
        html += "    if(data.success) {";
        html += "      location.reload();";
        html += "    } else {";
        html += "      alert('Failed to control relay: ' + data.message);";
        html += "    }";
        html += "  }).catch(err => alert('Network error: ' + err));";
        html += "}";
        html += "function testAllRelays(state) {";
        html += "  fetch('/relay', {";
        html += "    method: 'POST',";
        html += "    headers: {'Content-Type': 'application/x-www-form-urlencoded'},";
        html += "    body: 'action=all&state=' + (state ? '1' : '0')";
        html += "  }).then(response => response.json()).then(data => {";
        html += "    if(data.success) {";
        html += "      location.reload();";
        html += "    } else {";
        html += "      alert('Failed to control relays: ' + data.message);";
        html += "    }";
        html += "  }).catch(err => alert('Network error: ' + err));";
        html += "}";
        html += "function toggleSection(sectionId) {";
        html += "  var content = document.getElementById(sectionId + '-content');";
        html += "  var arrow = document.getElementById(sectionId + '-arrow');";
        html += "  if (content.classList.contains('active')) {";
        html += "    content.classList.remove('active');";
        html += "    arrow.classList.remove('down');";
        html += "  } else {";
        html += "    content.classList.add('active');";
        html += "    arrow.classList.add('down');";
        html += "  }";
        html += "}";
        html += "</script>";
        
        // Navigation section
        html += "<div class='section' style='margin-top:30px;text-align:center'>";
        html += "<div class='section-header' style='background:#17a2b8'>System Navigation</div>";
        html += "<div class='section-content' style='padding:20px'>";
        html += "<button onclick=\"location.href='/schedule'\" style='background:#28a745;color:white;padding:15px 30px;border:none;border-radius:5px;cursor:pointer;margin:10px;font-size:16px'>📅 Schedule Configuration</button><br>";
        html += "<button onclick=\"location.href='/wifi-config'\" style='background:#007bff;color:white;padding:15px 30px;border:none;border-radius:5px;cursor:pointer;margin:10px;font-size:16px'>🌐 WiFi Configuration</button>";
        html += "</div>";
        html += "</div>";
        
        html += "</div></body></html>";
        
        device_server->send(200, "text/html", html);
    });
    
    // JSON API endpoint
    device_server->on("/api/status", []() {
        String json = "{";
        json += "\"temperature\":" + String(g_system_status.current_temperature, 1) + ",";
        json += "\"state\":" + String(g_system_status.state) + ",";
        json += "\"uptime\":" + String(g_system_status.uptime) + ",";
        json += "\"free_memory\":" + String(g_system_status.free_memory) + ",";
        json += "\"wifi_ssid\":\"" + WiFi.SSID() + "\",";
        json += "\"ip_address\":\"" + WiFi.localIP().toString() + "\",";
        json += "\"rssi\":" + String(WiFi.RSSI());
        json += "}";
        
        device_server->send(200, "application/json", json);
    });
    
    // Relay control endpoint
    device_server->on("/relay", HTTP_POST, []() {
        String relayStr = device_server->arg("relay");
        String action = device_server->arg("action");
        String stateStr = device_server->arg("state");
        
        String response = "{";
        
        if (action == "all") {
            // Control all relays
            bool state = (stateStr == "1");
            uint8_t allStates = state ? 0x3F : 0x00; // 0x3F = all 6 relays ON, 0x00 = all OFF
            relay_controller.setAllRelays(allStates);
            
            response += "\"success\":true,";
            response += "\"action\":\"all\",";
            response += "\"state\":" + String(state ? "true" : "false") + ",";
            response += "\"message\":\"All relays " + String(state ? "ON" : "OFF") + "\"";
            
        } else if (action == "set") {
            // Direct relay control
            int relayNum = relayStr.toInt();
            bool state = (stateStr == "1");
            
            if (relayNum >= 0 && relayNum < 6) {
                bool success = relay_controller.setRelay(relayNum, state);
                
                response += "\"success\":" + String(success ? "true" : "false") + ",";
                response += "\"relay\":" + String(relayNum) + ",";
                response += "\"state\":" + String(state ? "true" : "false") + ",";
                response += "\"message\":\"";
                response += (success ? ("Relay " + String(relayNum + 1) + " turned " + String(state ? "ON" : "OFF")) : "Failed to control relay");
                response += "\"";
            } else {
                response += "\"success\":false,\"message\":\"Invalid relay number\"";
            }
            
        } else if (action == "toggle") {
            // Single relay control
            int relayNum = relayStr.toInt();
            
            if (relayNum >= 0 && relayNum < 6) {
                bool currentState = relay_controller.getRelayState(relayNum);
                bool success = relay_controller.setRelay(relayNum, !currentState);
                
                response += "\"success\":" + String(success ? "true" : "false") + ",";
                response += "\"relay\":" + String(relayNum) + ",";
                response += "\"state\":" + String(!currentState ? "true" : "false") + ",";
                response += "\"message\":\"";
                response += (success ? "Relay toggled successfully" : "Failed to control relay");
                response += "\"";
            } else {
                response += "\"success\":false,\"message\":\"Invalid relay number\"";
            }
        } else {
            response += "\"success\":false,\"message\":\"Invalid action\"";
        }
        
        response += "}";
        device_server->send(200, "application/json", response);
    });
    
    // WiFi configuration endpoint
    device_server->on("/wifi", HTTP_POST, []() {
        String mode = device_server->arg("mode");
        String message = "";
        
        if (mode == "static") {
            String static_ip = device_server->arg("static_ip");
            String gateway = device_server->arg("gateway");
            String subnet = device_server->arg("subnet");
            String dns = device_server->arg("dns");
            
            // Validate IP addresses
            IPAddress ip, gw, sn, dn;
            if (ip.fromString(static_ip) && gw.fromString(gateway) && 
                sn.fromString(subnet) && dn.fromString(dns)) {
                
                if (WiFi.config(ip, gw, sn, dn)) {
                    message = "✅ Static IP configuration applied successfully!<br>Reconnecting...";
                    DEBUG_PRINTF("Static IP configured: %s\n", static_ip.c_str());
                } else {
                    message = "❌ Failed to apply static IP configuration";
                }
            } else {
                message = "❌ Invalid IP address format";
            }
        } else {
            // Switch to DHCP
            if (WiFi.config(0U, 0U, 0U)) {  // Reset to DHCP
                message = "✅ DHCP mode enabled successfully!<br>Reconnecting...";
                DEBUG_PRINTLN("DHCP mode configured");
            } else {
                message = "❌ Failed to enable DHCP mode";
            }
        }
        
        String html = "<!DOCTYPE html><html><head><title>Network Configuration</title>";
        html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
        html += "<meta http-equiv='refresh' content='5;url=/'>";
        html += "<style>body{font-family:Arial;margin:40px;background:#f0f0f0;text-align:center}";
        html += ".container{background:white;padding:30px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1);display:inline-block}</style></head><body>";
        html += "<div class='container'>";
        html += "<h2>Network Configuration</h2>";
        html += "<div style='margin:20px 0;font-size:18px'>" + message + "</div>";
        html += "<p>Redirecting back to main page in 5 seconds...</p>";
        html += "<a href='/' style='color:#007bff;text-decoration:none'>← Back to Main Page</a>";
        html += "</div></body></html>";
        
        device_server->send(200, "text/html", html);
        
        // Reconnect WiFi with new settings after response
        if (message.indexOf("✅") >= 0) {
            delay(2000);  // Give time for response to be sent
            WiFi.reconnect();
        }
    });
    
    // WiFi network change page
    device_server->on("/wifi-config", []() {
        String html = "<!DOCTYPE html><html><head><title>WiFi Configuration</title>";
        html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
        html += "<style>body{font-family:Arial;margin:40px;background:#f0f0f0}";
        html += ".container{background:white;padding:30px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1)}";
        html += ".warning{background:#fff3cd;border:1px solid #ffeaa7;color:#856404;padding:15px;border-radius:5px;margin:15px 0}";
        html += "button{background:#dc3545;color:white;padding:10px 20px;border:none;border-radius:5px;cursor:pointer;margin:10px 5px}</style></head><body>";
        
        html += "<div class='container'>";
        html += "<h2>WiFi Network Configuration</h2>";
        
        html += "<div class='warning'>⚠️ <strong>Warning:</strong> Changing WiFi settings will disconnect the current connection and start configuration mode.</div>";
        
        html += "<h3>Current Connection</h3>";
        html += "<p><strong>Network:</strong> " + WiFi.SSID() + "</p>";
        html += "<p><strong>IP Address:</strong> " + WiFi.localIP().toString() + "</p>";
        html += "<p><strong>Signal:</strong> " + String(WiFi.RSSI()) + " dBm</p>";
        
        html += "<h3>Change Network</h3>";
        html += "<p>To change the WiFi network:</p>";
        html += "<ol>";
        html += "<li>Click <strong>Reset WiFi Settings</strong> below</li>";
        html += "<li>Device will restart in configuration mode</li>";
        html += "<li>Connect to <strong>MAC-SYS-CONFIG</strong> network (password: admin123)</li>";
        html += "<li>Configure new network settings</li>";
        html += "</ol>";
        
        html += "<div style='margin:30px 0;text-align:center'>";
        html += "<button onclick=\"location.href='/'\" style='background:#6c757d'>← Back to Main</button>";
        html += "<button onclick='resetWiFi()'>Reset WiFi Settings</button>";
        html += "</div>";
        
        html += "<script>";
        html += "function resetWiFi() {";
        html += "  if(confirm('Are you sure? This will disconnect the current WiFi and restart configuration mode.')) {";
        html += "    fetch('/reset-wifi', {method:'POST'}).then(()=>{";
        html += "      alert('WiFi settings reset! Device restarting in configuration mode...');";
        html += "    });";
        html += "  }";
        html += "}";
        html += "</script>";
        
        html += "</div></body></html>";
        
        device_server->send(200, "text/html", html);
    });
    
    // Temperature control endpoint
    device_server->on("/tempcontrol", HTTP_POST, []() {
        String zoneStr = device_server->arg("zone");
        String setpointStr = device_server->arg("setpoint");
        String deltaStr = device_server->arg("delta");
        String modeStr = device_server->arg("mode");
        
        int zone = zoneStr.toInt();
        float setpoint = setpointStr.toFloat();
        float delta = deltaStr.toFloat();
        int mode = modeStr.toInt();
        
        // Apply settings
        temp_controller.setSetpoint(zone, setpoint);
        temp_controller.setDelta(zone, delta);
        temp_controller.setMode(zone, (TempControlMode)mode);
        
        String html = "<!DOCTYPE html><html><head><title>Temperature Control Updated</title>";
        html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
        html += "<meta http-equiv='refresh' content='3;url=/'>";
        html += "<style>body{font-family:Arial;margin:40px;background:#f0f0f0;text-align:center}";
        html += ".container{background:white;padding:30px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1);display:inline-block}</style></head><body>";
        html += "<div class='container'>";
        html += "<h2>✅ Temperature Settings Updated</h2>";
        html += "<p>Zone " + String(zone + 1) + " configuration saved</p>";
        html += "<p>Setpoint: " + String(setpoint, 1) + "°C, Delta: ±" + String(delta, 1) + "°C</p>";
        html += "<p>Redirecting back in 3 seconds...</p>";
        html += "<a href='/' style='color:#007bff'>← Back to Main</a>";
        html += "</div></body></html>";
        
        device_server->send(200, "text/html", html);
    });
    
    // WiFi reset endpoint
    device_server->on("/reset-wifi", HTTP_POST, []() {
        device_server->send(200, "text/plain", "OK");
        DEBUG_PRINTLN("WiFi reset requested via web interface");
        delay(1000);
        
        // Clear stored credentials and restart
        EEPROM.write(0, 0);  // Clear checksum
        EEPROM.commit();
        DEBUG_PRINTLN("WiFi credentials cleared, restarting...");
        ESP.restart();
    });

    // Schedule management endpoints
    device_server->on("/schedule", []() {
        String html = "<!DOCTYPE html><html><head><title>MAC-SYS Schedule Configuration</title>";
        html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
        html += "<meta charset='UTF-8'>";
        html += "<style>body{font-family:Arial,sans-serif;margin:20px;background:#f0f0f0}";
        html += ".container{background:white;padding:20px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1);max-width:1000px;margin:0 auto}";
        html += ".section{background:#f8f9fa;border-radius:8px;margin:20px 0;overflow:hidden;border:1px solid #dee2e6}";
        html += ".section-header{background:#007bff;color:white;padding:15px;font-weight:bold;cursor:pointer;user-select:none}";
        html += ".section-content{padding:20px;display:block}";
        html += ".form-group{margin:15px 0}";
        html += "label{display:block;margin-bottom:5px;font-weight:bold}";
        html += "input,select{width:100%;padding:8px;border:1px solid #ddd;border-radius:4px;box-sizing:border-box}";
        html += "button{background:#007bff;color:white;padding:10px 15px;border:none;border-radius:4px;cursor:pointer;margin:5px}";
        html += "button:hover{background:#0056b3}";
        html += ".schedule-event{background:#e9ecef;padding:10px;margin:10px 0;border-radius:4px}";
        html += ".status{background:#e8f5e8;padding:15px;border-radius:5px;margin:10px 0}";
        html += "</style></head><body>";
        
        html += "<div class='container'>";
        html += "<h1>MAC-SYS Schedule Configuration</h1>";
        
        // Current time and status
        html += "<div class='status'>";
        html += "<strong>📅 Current Time:</strong> " + rtc_manager.getFormattedDateTime() + "<br>";
        html += "<strong>📊 Schedule Status:</strong> " + String(schedule_manager.isScheduleActive() ? "ACTIVE" : "INACTIVE") + "<br>";
        html += "<strong>🌐 RTC Status:</strong> " + String(rtc_manager.isRTCAvailable() ? "Available" : "Not Available") + "<br>";
        html += "<strong>⏰ NTP Status:</strong> " + String(rtc_manager.isNTPSynced() ? "Synchronized" : "Not Synchronized");
        html += "</div>";
        
        // Global controls
        html += "<div class='section'>";
        html += "<div class='section-header'>Global Schedule Controls</div>";
        html += "<div class='section-content'>";
        html += "<div class='form-group'>";
        html += "<button onclick=\"setGlobalSchedule(true)\">Enable Global Schedule</button>";
        html += "<button onclick=\"setGlobalSchedule(false)\">Disable Global Schedule</button>";
        html += "<button onclick=\"setHolidayMode(true)\">Enable Holiday Mode</button>";
        html += "<button onclick=\"setHolidayMode(false)\">Disable Holiday Mode</button>";
        html += "</div>";
        html += "<h3>Backup & Restore</h3>";
        html += "<div class='form-group'>";
        html += "<button onclick=\"exportSchedules()\" style='background:#17a2b8'>📥 Export All Schedules</button>";
        html += "<button onclick=\"document.getElementById('importFile').click()\" style='background:#28a745'>📤 Import Schedules</button>";
        html += "<input type='file' id='importFile' accept='.json' style='display:none' onchange='importSchedules(this)'>";
        html += "</div>";
        html += "</div>";
        html += "</div>";
        
        // Zone schedules
        for (int zone = 0; zone < 4; zone++) {
            WeeklySchedule& schedule = schedule_manager.getZoneSchedule(zone);
            html += "<div class='section'>";
            html += "<div class='section-header'>Zone " + String(zone + 1) + " Schedule (" + String(schedule.zone_name) + ")</div>";
            html += "<div class='section-content'>";
            
            // Zone enable/disable
            html += "<div class='form-group'>";
            html += "<button onclick=\"setZoneSchedule(" + String(zone) + ", true)\">Enable Zone</button>";
            html += "<button onclick=\"setZoneSchedule(" + String(zone) + ", false)\">Disable Zone</button>";
            html += "</div>";
            
            // Current events
            html += "<h3>Current Events (" + String(schedule.active_events) + "/" + String(MAX_SCHEDULE_EVENTS) + ")</h3>";
            for (int i = 0; i < schedule.active_events; i++) {
                ScheduleEvent& event = schedule.events[i];
                html += "<div class='schedule-event'>";
                html += "<strong>" + String(event.description) + "</strong><br>";
                html += "Time: " + schedule_manager.formatTimeFromMinutes(event.time_minutes);
                html += " | Days: " + schedule_manager.formatDayMask(event.day_mask);
                html += " | " + String(event.enabled ? "ENABLED" : "DISABLED");
                if (event.event_type == SCHEDULE_TEMP_SETPOINT) {
                    html += " | Setpoint: " + String(event.value1, 1) + "°C";
                }
                html += "<br><button onclick=\"removeEvent(" + String(zone) + ", " + String(i) + ")\">Remove</button>";
                html += "</div>";
            }
            
            // Add new event form
            html += "<h3>Add New Event</h3>";
            html += "<div class='form-group'>";
            html += "<label>Description:</label>";
            html += "<input type='text' id='desc_" + String(zone) + "' placeholder='Event description' maxlength='30'>";
            html += "</div>";
            html += "<div class='form-group'>";
            html += "<label>Time (HH:MM):</label>";
            html += "<input type='time' id='time_" + String(zone) + "' value='08:00'>";
            html += "</div>";
            html += "<div class='form-group'>";
            html += "<label>Days:</label>";
            html += "<div>";
            html += "<label><input type='checkbox' id='sun_" + String(zone) + "'> Sunday</label>";
            html += "<label><input type='checkbox' id='mon_" + String(zone) + "' checked> Monday</label>";
            html += "<label><input type='checkbox' id='tue_" + String(zone) + "' checked> Tuesday</label>";
            html += "<label><input type='checkbox' id='wed_" + String(zone) + "' checked> Wednesday</label>";
            html += "<label><input type='checkbox' id='thu_" + String(zone) + "' checked> Thursday</label>";
            html += "<label><input type='checkbox' id='fri_" + String(zone) + "' checked> Friday</label>";
            html += "<label><input type='checkbox' id='sat_" + String(zone) + "'> Saturday</label>";
            html += "</div>";
            html += "</div>";
            html += "<div class='form-group'>";
            html += "<label>Temperature Setpoint (°C):</label>";
            html += "<input type='number' id='temp_" + String(zone) + "' min='5' max='40' step='0.5' value='22'>";
            html += "</div>";
            html += "<div class='form-group'>";
            html += "<label>Delta (°C):</label>";
            html += "<input type='number' id='delta_" + String(zone) + "' min='0.5' max='5' step='0.1' value='1'>";
            html += "</div>";
            html += "<div class='form-group'>";
            html += "<label>Mode:</label>";
            html += "<select id='mode_" + String(zone) + "'>";
            html += "<option value='0'>OFF</option>";
            html += "<option value='1' selected>HEATING</option>";
            html += "<option value='2'>COOLING</option>";
            html += "<option value='3'>AUTO</option>";
            html += "</select>";
            html += "</div>";
            html += "<button onclick=\"addEvent(" + String(zone) + ")\">Add Event</button>";
            
            html += "</div>";
            html += "</div>";
        }
        
        // Navigation
        html += "<div style='text-align:center;margin-top:20px'>";
        html += "<button onclick=\"window.location.href='/'\">← Back to Main</button>";
        html += "<button onclick=\"window.location.reload()\">🔄 Refresh</button>";
        html += "</div>";
        
        html += "</div>";
        
        // JavaScript functions
        html += "<script>";
        html += "function setGlobalSchedule(enabled) {";
        html += "  fetch('/api/schedule/global', {method: 'POST', body: 'enabled=' + enabled})";
        html += "    .then(() => window.location.reload());";
        html += "}";
        html += "function setHolidayMode(enabled) {";
        html += "  fetch('/api/schedule/holiday', {method: 'POST', body: 'enabled=' + enabled})";
        html += "    .then(() => window.location.reload());";
        html += "}";
        html += "function setZoneSchedule(zone, enabled) {";
        html += "  fetch('/api/schedule/zone', {method: 'POST', body: 'zone=' + zone + '&enabled=' + enabled})";
        html += "    .then(() => window.location.reload());";
        html += "}";
        html += "function removeEvent(zone, index) {";
        html += "  fetch('/api/schedule/event', {method: 'DELETE', body: 'zone=' + zone + '&index=' + index})";
        html += "    .then(() => window.location.reload());";
        html += "}";
        html += "function addEvent(zone) {";
        html += "  const desc = document.getElementById('desc_' + zone).value;";
        html += "  const time = document.getElementById('time_' + zone).value;";
        html += "  const temp = document.getElementById('temp_' + zone).value;";
        html += "  const delta = document.getElementById('delta_' + zone).value;";
        html += "  const mode = document.getElementById('mode_' + zone).value;";
        html += "  let days = 0;";
        html += "  if (document.getElementById('sun_' + zone).checked) days |= 1;";
        html += "  if (document.getElementById('mon_' + zone).checked) days |= 2;";
        html += "  if (document.getElementById('tue_' + zone).checked) days |= 4;";
        html += "  if (document.getElementById('wed_' + zone).checked) days |= 8;";
        html += "  if (document.getElementById('thu_' + zone).checked) days |= 16;";
        html += "  if (document.getElementById('fri_' + zone).checked) days |= 32;";
        html += "  if (document.getElementById('sat_' + zone).checked) days |= 64;";
        html += "  const data = 'zone=' + zone + '&desc=' + desc + '&time=' + time + '&temp=' + temp + '&delta=' + delta + '&mode=' + mode + '&days=' + days;";
        html += "  fetch('/api/schedule/event', {method: 'POST', body: data})";
        html += "    .then(() => window.location.reload());";
        html += "}";
        html += "function exportSchedules() {";
        html += "  fetch('/api/schedule/export')";
        html += "    .then(response => response.json())";
        html += "    .then(data => {";
        html += "      const blob = new Blob([JSON.stringify(data, null, 2)], {type: 'application/json'});";
        html += "      const url = URL.createObjectURL(blob);";
        html += "      const a = document.createElement('a');";
        html += "      a.href = url;";
        html += "      a.download = 'mac-sys-schedules-' + new Date().toISOString().split('T')[0] + '.json';";
        html += "      document.body.appendChild(a);";
        html += "      a.click();";
        html += "      document.body.removeChild(a);";
        html += "      URL.revokeObjectURL(url);";
        html += "    });";
        html += "}";
        html += "function importSchedules(input) {";
        html += "  const file = input.files[0];";
        html += "  if (!file) return;";
        html += "  const reader = new FileReader();";
        html += "  reader.onload = function(e) {";
        html += "    fetch('/api/schedule/import', {";
        html += "      method: 'POST',";
        html += "      headers: {'Content-Type': 'application/json'},";
        html += "      body: e.target.result";
        html += "    })";
        html += "    .then(response => response.text())";
        html += "    .then(result => {";
        html += "      alert(result);";
        html += "      window.location.reload();";
        html += "    });";
        html += "  };";
        html += "  reader.readAsText(file);";
        html += "}";
        html += "</script>";
        
        html += "</body></html>";
        device_server->send(200, "text/html", html);
    });
    
    // Schedule API endpoints
    device_server->on("/api/schedule/global", HTTP_POST, []() {
        if (device_server->hasArg("enabled")) {
            bool enabled = device_server->arg("enabled") == "true";
            schedule_manager.setGlobalEnabled(enabled);
            device_server->send(200, "text/plain", "OK");
        } else {
            device_server->send(400, "text/plain", "Missing enabled parameter");
        }
    });
    
    device_server->on("/api/schedule/holiday", HTTP_POST, []() {
        if (device_server->hasArg("enabled")) {
            bool enabled = device_server->arg("enabled") == "true";
            schedule_manager.setHolidayMode(enabled);
            device_server->send(200, "text/plain", "OK");
        } else {
            device_server->send(400, "text/plain", "Missing enabled parameter");
        }
    });
    
    device_server->on("/api/schedule/zone", HTTP_POST, []() {
        if (device_server->hasArg("zone") && device_server->hasArg("enabled")) {
            uint8_t zone = device_server->arg("zone").toInt();
            bool enabled = device_server->arg("enabled") == "true";
            schedule_manager.setZoneEnabled(zone, enabled);
            device_server->send(200, "text/plain", "OK");
        } else {
            device_server->send(400, "text/plain", "Missing parameters");
        }
    });
    
    device_server->on("/api/schedule/event", HTTP_POST, []() {
        if (device_server->hasArg("zone") && device_server->hasArg("time") && 
            device_server->hasArg("temp") && device_server->hasArg("days")) {
            
            ScheduleEvent event;
            event.enabled = true;
            event.zone_id = device_server->arg("zone").toInt();
            event.day_mask = device_server->arg("days").toInt();
            event.time_minutes = schedule_manager.parseTimeToMinutes(device_server->arg("time"));
            event.event_type = SCHEDULE_TEMP_SETPOINT;
            event.value1 = device_server->arg("temp").toFloat();
            event.value2 = device_server->hasArg("delta") ? device_server->arg("delta").toFloat() : 1.0;
            event.temp_mode = (TempControlMode)(device_server->hasArg("mode") ? device_server->arg("mode").toInt() : 1);
            
            String desc = device_server->hasArg("desc") ? device_server->arg("desc") : "Custom Event";
            strncpy(event.description, desc.c_str(), sizeof(event.description) - 1);
            event.description[sizeof(event.description) - 1] = '\0';
            
            if (schedule_manager.addEvent(event.zone_id, event)) {
                device_server->send(200, "text/plain", "Event added");
            } else {
                device_server->send(400, "text/plain", "Failed to add event");
            }
        } else {
            device_server->send(400, "text/plain", "Missing parameters");
        }
    });
    
    device_server->on("/api/schedule/event", HTTP_DELETE, []() {
        if (device_server->hasArg("zone") && device_server->hasArg("index")) {
            uint8_t zone = device_server->arg("zone").toInt();
            uint8_t index = device_server->arg("index").toInt();
            
            if (schedule_manager.removeEvent(zone, index)) {
                device_server->send(200, "text/plain", "Event removed");
            } else {
                device_server->send(400, "text/plain", "Failed to remove event");
            }
        } else {
            device_server->send(400, "text/plain", "Missing parameters");
        }
    });
    
    // Schedule backup and restore endpoints
    device_server->on("/api/schedule/export", []() {
        if (device_server->hasArg("zone")) {
            uint8_t zone = device_server->arg("zone").toInt();
            String json = schedule_manager.exportScheduleJSON(zone);
            device_server->send(200, "application/json", json);
        } else {
            String json = schedule_manager.exportAllSchedulesJSON();
            device_server->send(200, "application/json", json);
        }
    });
    
    device_server->on("/api/schedule/import", HTTP_POST, []() {
        String json_data = device_server->arg("plain");  // Get raw POST body
        if (json_data.length() == 0) {
            json_data = device_server->arg("data");  // Try form parameter
        }
        
        if (json_data.length() > 0) {
            if (device_server->hasArg("zone")) {
                uint8_t zone = device_server->arg("zone").toInt();
                if (schedule_manager.importScheduleJSON(zone, json_data)) {
                    device_server->send(200, "text/plain", "Schedule imported successfully");
                } else {
                    device_server->send(400, "text/plain", "Failed to import schedule");
                }
            } else {
                if (schedule_manager.importAllSchedulesJSON(json_data)) {
                    device_server->send(200, "text/plain", "All schedules imported successfully");
                } else {
                    device_server->send(400, "text/plain", "Failed to import schedules");
                }
            }
        } else {
            device_server->send(400, "text/plain", "No data provided");
        }
    });
    
    device_server->begin();
    DEBUG_PRINTLN("✅ Device web server started on port 80");
    DEBUG_PRINTF("🌐 Access at: http://%s/\n", WiFi.localIP().toString().c_str());
}

void handleDeviceWebServer() {
    // Start server if WiFi connected and not running
    if (g_wifi_connected && device_server == nullptr) {
        setupDeviceWebServer();
    }
    
    // Stop server if WiFi disconnected
    if (!g_wifi_connected && device_server != nullptr) {
        DEBUG_PRINTLN("Stopping device web server (WiFi disconnected)");
        device_server->stop();
        delete device_server;
        device_server = nullptr;
    }
    
    // Handle client requests
    if (device_server != nullptr) {
        device_server->handleClient();
    }
}

void updateIOSystem() {
    // Update digital inputs with debouncing
    relay_controller.updateInputs();
    
    // Log input state changes
    static uint8_t last_logged_inputs = 0xFF;
    uint8_t current_inputs = relay_controller.getAllInputStates();
    
    if (current_inputs != last_logged_inputs) {
        DEBUG_PRINTF("📊 I/O Status Changed: 0x%02X\n", current_inputs);
        DEBUG_PRINTLN(relay_controller.getInputStatusString());
        last_logged_inputs = current_inputs;
    }
    
    // Update temperature controller with current temperature
    float current_temp = g_system_status.current_temperature;
    temp_controller.updateTemperature(0, current_temp);  // Update zone 0
    
    // Process temperature control logic
    temp_controller.process();
}

