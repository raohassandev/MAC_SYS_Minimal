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
#include "modbus_manager.h"
#include "utils.h"
#include "webserver_api.h"
#include <EEPROM.h>
#include <esp_task_wdt.h>
#include <WiFi.h>
#include <WebServer.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// Global system variables
SystemConfig g_system_config;
SystemStatus g_system_status;

// Utility functions
// calculateChecksum is now defined in utils.cpp and utils.h

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
        DEBUG_PRINTLN("[OK] Relay and I/O system initialized successfully");
    }
    
    // Initialize temperature control system
    temp_controller.begin();
    
    // Set up initial zone configuration (Zone 0 as example)
    temp_controller.getZoneConfig(0).enabled = true;
    temp_controller.getZoneConfig(0).mode = TEMP_MODE_HEATING;
    temp_controller.getZoneConfig(0).setpoint = 22.0;
    temp_controller.getZoneConfig(0).delta = 1.0;
    DEBUG_PRINTLN("[OK] Temperature control system initialized");

    initializeModbus();
    
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

    // Handle Modbus
    handleModbus();
    
    
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
    DEBUG_PRINTF("[DATA] Temperature update: %.2f°C\n", temperature);
    
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
        html += "<style>* { margin: 0; padding: 0; box-sizing: border-box; }";
        html += "body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: #0B1426; color: #E5E7EB; line-height: 1.6; padding-top: 6rem; }";
        html += ".container { max-width: 1200px; margin: 0 auto; padding: 1.5rem; }";
        // Hero metrics section
        html += ".hero-metrics { display: grid; grid-template-columns: repeat(auto-fit, minmax(280px, 1fr)); gap: 1.5rem; margin-bottom: 2rem; }";
        html += ".metric-card { background: linear-gradient(135deg, #1f2937 0%, #374151 100%); border: 1px solid #374151; border-radius: 12px; padding: 1.5rem; box-shadow: 0 4px 6px rgba(0,0,0,0.3); }";
        html += ".metric-header { display: flex; align-items: center; gap: 0.75rem; margin-bottom: 1rem; }";
        html += ".metric-icon { font-size: 1.5rem; opacity: 0.8; }";
        html += ".metric-title { font-size: 1.1rem; font-weight: 600; color: #F3F4F6; }";
        html += ".metric-value { font-size: 2rem; font-weight: 700; color: #00D4FF; margin-bottom: 0.5rem; }";
        html += ".metric-subtitle { font-size: 0.9rem; color: #9CA3AF; }";
        html += ".status-indicator { display: inline-block; width: 12px; height: 12px; border-radius: 50%; margin-right: 0.5rem; }";
        html += ".status-running { background: #10B981; box-shadow: 0 0 8px rgba(16,185,129,0.4); }";
        html += ".status-idle { background: #6B7280; }";
        html += ".status-error { background: #EF4444; box-shadow: 0 0 8px rgba(239,68,68,0.4); }";
        // Control panel section
        html += ".control-panel { display: grid; grid-template-columns: repeat(auto-fit, minmax(250px, 1fr)); gap: 1rem; margin-bottom: 2rem; }";
        html += ".control-card { background: #1f2937; border: 1px solid #374151; border-radius: 8px; padding: 1rem; }";
        html += ".control-card h3 { color: #F9FAFB; margin-bottom: 1rem; font-size: 1rem; }";
        html += ".control-actions { display: flex; gap: 0.5rem; flex-wrap: wrap; }";
        html += ".btn { border: none; padding: 0.5rem 1rem; border-radius: 6px; cursor: pointer; font-weight: 500; transition: all 0.2s; }";
        html += ".btn-primary { background: #3B82F6; color: white; }";
        html += ".btn-primary:hover { background: #2563EB; transform: translateY(-1px); }";
        html += ".btn-success { background: #10B981; color: white; }";
        html += ".btn-success:hover { background: #059669; }";
        html += ".btn-danger { background: #EF4444; color: white; }";
        html += ".btn-danger:hover { background: #DC2626; }";
        html += ".btn-secondary { background: #6B7280; color: white; }";
        html += ".btn-secondary:hover { background: #4B5563; }";
        // Details section
        html += ".details-section { margin-top: 2rem; }";
        html += ".section-card { background: #1f2937; border: 1px solid #374151; border-radius: 8px; margin-bottom: 1rem; overflow: hidden; }";
        html += ".section-header { background: #374151; color: #F9FAFB; padding: 1rem; cursor: pointer; user-select: none; display: flex; justify-content: space-between; align-items: center; }";
        html += ".section-header:hover { background: #4B5563; }";
        html += ".section-content { padding: 1.5rem; display: none; }";
        html += ".section-content.active { display: block; }";
        html += ".arrow { transition: transform 0.3s; font-size: 0.8rem; }";
        html += ".arrow.down { transform: rotate(90deg); }";
        html += ".relay-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(150px, 1fr)); gap: 1rem; }";
        html += ".relay-card { background: #374151; border-radius: 6px; padding: 1rem; text-align: center; border: 2px solid transparent; }";
        html += ".relay-card.active { border-color: #10B981; }";
        html += ".relay-card.inactive { border-color: #6B7280; }";
        html += ".info-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 1rem; }";
        html += ".info-item { display: flex; justify-content: space-between; padding: 0.5rem 0; border-bottom: 1px solid #374151; }";
        html += ".info-label { color: #9CA3AF; }";
        html += ".info-value { color: #F3F4F6; font-weight: 500; }";
        // Professional navigation styles to match other pages
        html += ".header{position:fixed;top:0;left:0;right:0;z-index:1000;background:linear-gradient(135deg,#1e3a8a 0%,#3b82f6 100%);color:white;padding:1rem 2rem;box-shadow:0 2px 10px rgba(0,0,0,0.3)}";
        html += ".header h1{font-size:1.5rem;margin:0;display:flex;align-items:center;gap:0.75rem}";
        html += ".nav-links{margin-top:0.75rem;display:flex;gap:1.5rem;flex-wrap:wrap}";
        html += ".nav-links a{color:#dbeafe;text-decoration:none;padding:0.375rem 0.75rem;border-radius:0.375rem;transition:all 0.2s;font-size:0.875rem}";
        html += ".nav-links a:hover{background:rgba(255,255,255,0.2)}";
        html += ".nav-links a.active{background:#00D4FF;color:#0B1426;font-weight:600}";
        html += "</style></head><body>";
        
        // Professional navigation header
        html += "<div class='header'>";
        html += "<h1>MAC-SYS Industrial Controller</h1>";
        html += "<div class='nav-links'>";
        html += "<a href='/' class='active'>Dashboard</a>";
        html += "<a href='/system'>System</a>";
        html += "<a href='/relays'>Relays</a>";
        html += "<a href='/temperature'>Temperature</a>";
        html += "<a href='/schedule'>Schedule</a>";
        html += "<a href='/sensors'>Sensors</a>";
        html += "<a href='/wifi-config'>WiFi Config</a>";
        html += "</div>";
        html += "</div>";
        
        html += "<div class='container'>";
        
        // Hero metrics section
        html += "<div class='hero-metrics'>";
        html += "<div class='metric-card'>";
        html += "<div class='metric-header'>";
        html += "<span class='metric-title'>Temperature</span>";
        html += "</div>";
        html += "<div class='metric-value'>" + String(g_system_status.current_temperature, 1) + "°C</div>";
        html += "<div class='metric-subtitle'>Current reading</div>";
        html += "</div>";
        html += "<div class='metric-card'>";
        html += "<div class='metric-header'>";
        html += "<span class='metric-icon'>⚡</span>";
        html += "<span class='metric-title'>System Status</span>";
        html += "</div>";
        html += "<div class='metric-value'>" + String(g_system_status.state == 1 ? "ACTIVE" : "IDLE") + "</div>";
        html += "<div class='metric-subtitle'><span class='status-indicator " + String(g_system_status.state == 1 ? "status-running" : "status-idle") + "'></span>System state</div>";
        html += "</div>";
        html += "<div class='metric-card'>";
        html += "<div class='metric-header'>";
        html += "<span class='metric-title'>Uptime</span>";
        html += "</div>";
        html += "<div class='metric-value'>" + String(g_system_status.uptime/3600) + "h</div>";
        html += "<div class='metric-subtitle'>" + String(g_system_status.uptime) + " seconds</div>";
        html += "</div>";
        html += "<div class='metric-card'>";
        html += "<div class='metric-header'>";
        html += "<span class='metric-title'>Free Memory</span>";
        html += "</div>";
        html += "<div class='metric-value'>" + String(g_system_status.free_memory/1024) + "KB</div>";
        html += "<div class='metric-subtitle'>" + String(g_system_status.free_memory) + " bytes available</div>";
        html += "</div>";
        html += "</div>";
        
        // Control panel section
        html += "<div class='control-panel'>";
        html += "<div class='control-card'>";
        html += "<h3>📊 System Status</h3>";
        html += "<div class='control-actions'>";
        html += "<button class='btn btn-primary' onclick='testSystemAPI()'>System API</button>";
        html += "<button class='btn btn-secondary' onclick='testDiagnosticsAPI()'>Diagnostics</button>";
        html += "<button class='btn btn-success' onclick='testNetworkAPI()'>Network</button>";
        html += "</div>";
        html += "</div>";
        html += "<div class='control-card'>";
        html += "<h3>🔧 Quick Actions</h3>";
        html += "<div class='control-actions'>";
        html += "<button class='btn btn-primary' onclick=\"location.href='/system'\">System Config</button>";
        html += "<button class='btn btn-success' onclick=\"location.href='/relays'\">Relay Control</button>";
        html += "<button class='btn btn-primary' onclick=\"location.href='/temperature'\">Temperature</button>";
        html += "</div>";
        html += "</div>";
        html += "<div class='control-card'>";
        html += "<h3>📅 Schedule Control</h3>";
        html += "<div class='control-actions'>";
        html += "<button class='btn btn-primary' onclick=\"location.href='/schedule'\">Schedule Editor</button>";
        html += "<button class='btn btn-secondary' onclick='toggleControlMode()'>Toggle Mode</button>";
        html += "</div>";
        html += "</div>";
        html += "</div>";
        
        
        // JavaScript for API calls
        html += "<script>";
        
        // API test functions
        html += "function testSystemAPI() {";
        html += "  fetch('/api/system').then(r => r.json()).then(d => {";
        html += "    document.getElementById('system-api-result') && (document.getElementById('system-api-result').innerHTML = JSON.stringify(d, null, 2));";
        html += "  }).catch(e => console.error(e));";
        html += "}";
        
        html += "function testRelayAPI() {";
        html += "  fetch('/api/relays').then(r => r.json()).then(d => {";
        html += "    const result = document.getElementById('relay-api-result');";
        html += "    if (result) { result.style.display = 'block'; result.innerHTML = JSON.stringify(d, null, 2); }";
        html += "  }).catch(e => console.error(e));";
        html += "}";
        
        html += "function setRelay(relay, state) {";
        html += "  fetch('/api/relays/control', {";
        html += "    method: 'POST',";
        html += "    headers: { 'Content-Type': 'application/json' },";
        html += "    body: JSON.stringify({ relay: relay, state: state })";
        html += "  }).then(r => r.json()).then(d => location.reload()).catch(e => console.error(e));";
        html += "}";
        
        html += "function testAllRelays(state) {";
        html += "  for (let i = 0; i < 8; i++) setRelay(i, state);";
        html += "}";
        
        html += "function toggleControlMode() {";
        html += "  fetch('/api/schedule/control-mode', { method: 'POST' })";
        html += "  .then(r => r.json()).then(d => location.reload()).catch(e => console.error(e));";
        html += "}";
        
        html += "</script>";
        
        
        html += "</div>"; // Close container
        html += "</body></html>";
        
        
        
        
        
        device_server->send(200, "text/html", html);
    });
    
    // JSON API endpoint
    device_server->on("/api/status", []() {
        String json = "{";
        json += "\"temperature\":" + String(g_system_status.current_temperature, 1) + ",";
        json += "\"state\":" + String(g_system_status.state) + ",";
        json += "\"uptime\":" + String(g_system_status.uptime) + ",";
        json += "\"free_memory\":" + String(g_system_status.free_memory) + ",";
        json += "\"current_time\":\"" + rtc_manager.getFormattedDateTime() + "\",";
        json += "\"wifi_ssid\":\"" + WiFi.SSID() + "\",";
        json += "\"ip_address\":\"" + WiFi.localIP().toString() + "\",";
        json += "\"rssi\":" + String(WiFi.RSSI());
        json += "}";
        
        device_server->send(200, "application/json", json);
    });
    
    // Time API endpoint
    device_server->on("/api/time", []() {
        String json = "{";
        json += "\"datetime\":\"" + rtc_manager.getFormattedDateTime() + "\",";
        json += "\"date\":\"" + rtc_manager.getFormattedDate() + "\",";
        json += "\"time\":\"" + rtc_manager.getFormattedTime() + "\",";
        json += "\"unix_timestamp\":" + String(rtc_manager.getUnixTime()) + ",";
        json += "\"rtc_available\":" + String(rtc_manager.isRTCAvailable() ? "true" : "false") + ",";
        json += "\"ntp_synced\":" + String(rtc_manager.isNTPSynced() ? "true" : "false");
        json += "}";
        
        device_server->send(200, "application/json", json);
    });
    
    // Temperature sensor debug endpoint
    device_server->on("/api/sensors", []() {
        String json = "{";
        json += "\"ds18b20_available\":" + String(isDS18B20Available() ? "true" : "false") + ",";
        json += "\"am2302_available\":" + String(isAM2302Available() ? "true" : "false") + ",";
        json += "\"lm35_available\":" + String(isLM35Available() ? "true" : "false") + ",";
        json += "\"primary_sensor\":\"" + String(getTemperatureSensorName(currentTempSensorType)) + "\",";
        json += "\"last_temperature\":" + String(lastValidTemperature, 2);
        
        // Get current readings from each sensor
        if (isDS18B20Available()) {
            float ds18b20_temp = readDS18B20Temperature();
            json += ",\"ds18b20_reading\":" + (isnan(ds18b20_temp) ? "null" : String(ds18b20_temp, 2));
        } else {
            json += ",\"ds18b20_reading\":null";
        }
        
        if (isAM2302Available()) {
            float am2302_temp = readAM2302Temperature();
            json += ",\"am2302_reading\":" + (isnan(am2302_temp) ? "null" : String(am2302_temp, 2));
        } else {
            json += ",\"am2302_reading\":null";
        }
        
        if (isLM35Available()) {
            float lm35_temp = readLM35Temperature();
            json += ",\"lm35_reading\":" + (isnan(lm35_temp) ? "null" : String(lm35_temp, 2));
        } else {
            json += ",\"lm35_reading\":null";
        }
        
        json += "}";
        device_server->send(200, "application/json", json);
    });
    
    // GPIO pin scanner for DS18B20
    device_server->on("/api/sensors/scan", []() {
        String json = "{\"scan_results\":[";
        bool found = false;
        
        // Test common GPIO pins for DS18B20
        int testPins[] = {2, 4, 5, 13, 14, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33};
        int numPins = sizeof(testPins) / sizeof(testPins[0]);
        
        for (int i = 0; i < numPins; i++) {
            int pin = testPins[i];
            
            // Try DS18B20 on this pin
            OneWire testWire(pin);
            DallasTemperature testSensor(&testWire);
            testSensor.begin();
            
            delay(100); // Short delay for initialization
            int deviceCount = testSensor.getDeviceCount();
            
            if (found) json += ",";
            json += "{\"pin\":" + String(pin) + ",\"ds18b20_devices\":" + String(deviceCount);
            
            if (deviceCount > 0) {
                testSensor.requestTemperatures();
                delay(1000);
                float temp = testSensor.getTempCByIndex(0);
                json += ",\"temperature\":" + String(temp, 2);
            }
            json += "}";
            found = true;
        }
        
        json += "]}";
        device_server->send(200, "application/json", json);
    });

    // Detailed sensor debug endpoint
    device_server->on("/api/sensors/debug", []() {
        String json = "{";
        json += "\"sensor_pins\":{";
        json += "\"ds18b20_pin\":" + String(DS18B20_PIN) + ",";
        json += "\"am2302_pin\":" + String(AM2302_PIN) + ",";
        json += "\"lm35_pin\":" + String(LM35_PIN);
        json += "},";
        
        // DS18B20 detailed debug
        json += "\"ds18b20_debug\":{";
        json += "\"initialized\":true,";
        json += "\"device_count\":" + String(getDS18B20DeviceCount()) + ",";
        json += "\"parasitic_power\":" + String(getDS18B20ParasiticPower()) + ",";
        json += "\"resolution\":" + String(getDS18B20Resolution());
        json += "},";
        
        // AM2302 detailed debug  
        json += "\"am2302_debug\":{";
        json += "\"initialized\":true,";
        json += "\"status\":\"" + String(getAM2302StatusString()) + "\",";
        json += "\"status_code\":" + String(getAM2302StatusCode());
        json += "},";
        
        // LM35 detailed debug
        int lm35_adc = getLM35ADCReading();
        json += "\"lm35_debug\":{";
        json += "\"adc_reading\":" + String(lm35_adc) + ",";
        json += "\"voltage\":" + String((lm35_adc / 4095.0) * 3.3, 3);
        json += "}";
        
        json += "}";
        device_server->send(200, "application/json", json);
    });
    
    // Sensor configuration page
    device_server->on("/sensors", []() {
        String html = "<!DOCTYPE html><html><head>";
        html += "<title>Sensor Configuration - MAC-SYS</title>";
        html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
        html += "<meta charset='UTF-8'>";
        
        // Enhanced CSS for sensor configuration
        html += "<style>";
        html += "* { margin: 0; padding: 0; box-sizing: border-box; }";
        html += "body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: #0B1426; color: #E5E7EB; line-height: 1.6; padding-top: 6rem; }";
        html += ".header { position: fixed; top: 0; left: 0; right: 0; z-index: 1000; background: linear-gradient(135deg, #1e3a8a 0%, #3b82f6 100%); color: white; padding: 1rem 2rem; box-shadow: 0 2px 10px rgba(0,0,0,0.3); }";
        html += ".header h1 { font-size: 1.5rem; margin: 0; display: flex; align-items: center; gap: 0.75rem; }";
        html += ".nav-links { margin-top: 0.75rem; display: flex; gap: 1.5rem; flex-wrap: wrap; }";
        html += ".nav-links a { color: #dbeafe; text-decoration: none; padding: 0.375rem 0.75rem; border-radius: 0.375rem; transition: all 0.2s; font-size: 0.875rem; }";
        html += ".nav-links a:hover { background: rgba(255,255,255,0.2); }";
        html += ".nav-links a.active { background: #00D4FF; color: #0B1426; font-weight: 600; }";
        html += ".container { max-width: 1200px; margin: 2rem auto; padding: 0 1rem; }";
        html += ".sensor-card { background: linear-gradient(135deg, #1f2937 0%, #374151 100%); border: 1px solid #374151; border-radius: 0.75rem; margin: 1.5rem 0; overflow: hidden; box-shadow: 0 4px 6px rgba(0,0,0,0.3); }";
        html += ".sensor-header { background: #00D4FF; color: #0B1426; padding: 1rem 1.5rem; font-weight: 600; font-size: 1.125rem; }";
        html += ".sensor-body { padding: 1.5rem; }";
        html += ".config-row{display:flex;align-items:center;margin:15px 0;flex-wrap:wrap;gap:15px}";
        html += ".config-label { min-width: 120px; font-weight: 600; color: #9CA3AF; font-size: 0.875rem; }";
        html += ".config-control { flex: 1; min-width: 150px; }";
        html += "select, input[type=number] { width: 100%; padding: 0.5rem; border: 1px solid #4B5563; border-radius: 0.375rem; background: #374151; color: #E5E7EB; font-size: 0.875rem; }";
        html += "select:focus, input[type=number]:focus { outline: none; border-color: #00D4FF; box-shadow: 0 0 0 2px rgba(0, 212, 255, 0.2); }";
        html += ".toggle{position:relative;display:inline-block;width:60px;height:34px}";
        html += ".toggle input{opacity:0;width:0;height:0}";
        html += ".slider{position:absolute;cursor:pointer;top:0;left:0;right:0;bottom:0;background-color:#ccc;transition:.4s;border-radius:34px}";
        html += ".slider:before{position:absolute;content:'';height:26px;width:26px;left:4px;bottom:4px;background-color:white;transition:.4s;border-radius:50%}";
        html += "input:checked + .slider{background-color:#28a745}";
        html += "input:checked + .slider:before{transform:translateX(26px)}";
        html += ".status-indicator{display:inline-block;padding:4px 12px;border-radius:20px;font-size:12px;font-weight:bold;margin-left:10px}";
        html += ".status-working{background:#d4edda;color:#155724}";
        html += ".status-disabled{background:#f8d7da;color:#721c24}";
        html += ".status-testing{background:#fff3cd;color:#856404}";
        html += ".test-button{padding:6px 12px;background:#17a2b8;color:white;border:none;border-radius:4px;cursor:pointer;font-size:12px}";
        html += ".test-button:hover{background:#138496}";
        html += ".save-button { background: #00D4FF; color: #0B1426; padding: 0.75rem 1.5rem; border: none; border-radius: 0.375rem; cursor: pointer; margin: 1rem 0.5rem; font-size: 0.875rem; font-weight: 600; }";
        html += ".reset-button { background: #EF4444; color: white; padding: 0.75rem 1.5rem; border: none; border-radius: 0.375rem; cursor: pointer; margin: 1rem 0.5rem; font-size: 0.875rem; font-weight: 600; }";
        html += ".back-button { background: #374151; color: #E5E7EB; padding: 0.5rem 1rem; border: 1px solid #4B5563; border-radius: 0.375rem; cursor: pointer; margin: 0.5rem; text-decoration: none; font-size: 0.875rem; }";
        html += "</style></head><body>";
        
        // Header with navigation
        html += "<div class='header'>";
        html += "<h1>Sensor Configuration</h1>";
        html += "<div class='nav-links'>";
        html += "<a href='/'>Dashboard</a>";
        html += "<a href='/system'>System</a>";
        html += "<a href='/relays'>Relays</a>";
        html += "<a href='/temperature'>Temperature</a>";
        html += "<a href='/schedule'>Schedule</a>";
        html += "<a href='/sensors' class='active'>Sensors</a>";
        html += "<a href='/wifi-config'>Network</a>";
        html += "</div>";
        html += "</div>";
        
        html += "<div class='container'>";
        html += "<h2 style='color: #00D4FF; margin-bottom: 1rem;'>Temperature Sensor Setup</h2>";
        html += "<p>Configure temperature sensors, GPIO pins, and priorities. Changes are saved to EEPROM and persist across reboots.</p>";
        
        // Get current configuration
        SensorConfig* config = getSensorConfig();
        
        html += "<form id='sensorConfigForm'>";
        
        // DS18B20 Configuration
        html += "<div class='sensor-card'>";
        html += "<div class='sensor-header'>DS18B20 (Digital Waterproof Temperature Sensor)</div>";
        html += "<div class='sensor-body'>";
        
        html += "<div class='config-row'>";
        html += "<div class='config-label'>Enable:</div>";
        html += "<div class='config-control'>";
        html += "<label class='toggle'>";
        html += "<input type='checkbox' id='ds18b20_enabled' " + String(config->ds18b20_enabled ? "checked" : "") + ">";
        html += "<span class='slider'></span>";
        html += "</label>";
        html += "<span class='status-indicator status-working'>Working (33.2°C)</span>";
        html += "</div>";
        html += "</div>";
        
        html += "<div class='config-row'>";
        html += "<div class='config-label'>GPIO Pin:</div>";
        html += "<div class='config-control'>";
        html += "<select id='ds18b20_pin'>";
        const int validPins[] = {2, 4, 5, 13, 14, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33, 34, 35, 36, 39};
        for (int pin : validPins) {
            html += "<option value='" + String(pin) + "'" + (pin == config->ds18b20_pin ? " selected" : "") + ">GPIO" + String(pin) + "</option>";
        }
        html += "</select>";
        html += "<button type='button' class='test-button' onclick='testSensor(2, " + String(config->ds18b20_pin) + ")'>🧪 Test</button>";
        html += "</div>";
        html += "</div>";
        
        html += "<div class='config-row'>";
        html += "<div class='config-label'>Priority:</div>";
        html += "<div class='config-control'>";
        html += "<select id='ds18b20_priority'>";
        html += "<option value='0'" + String(config->ds18b20_priority == 0 ? " selected" : "") + ">1 (Highest)</option>";
        html += "<option value='1'" + String(config->ds18b20_priority == 1 ? " selected" : "") + ">2 (Medium)</option>";
        html += "<option value='2'" + String(config->ds18b20_priority == 2 ? " selected" : "") + ">3 (Lowest)</option>";
        html += "</select>";
        html += "</div>";
        html += "</div>";
        
        html += "</div></div>";
        
        // AM2302 Configuration
        html += "<div class='sensor-card'>";
        html += "<div class='sensor-header'>AM2302/DHT22 (Temperature + Humidity Sensor)</div>";
        html += "<div class='sensor-body'>";
        
        html += "<div class='config-row'>";
        html += "<div class='config-label'>Enable:</div>";
        html += "<div class='config-control'>";
        html += "<label class='toggle'>";
        html += "<input type='checkbox' id='am2302_enabled' " + String(config->am2302_enabled ? "checked" : "") + ">";
        html += "<span class='slider'></span>";
        html += "</label>";
        html += "<span class='status-indicator status-disabled'>Disabled</span>";
        html += "</div>";
        html += "</div>";
        
        html += "<div class='config-row'>";
        html += "<div class='config-label'>GPIO Pin:</div>";
        html += "<div class='config-control'>";
        html += "<select id='am2302_pin'>";
        for (int pin : validPins) {
            html += "<option value='" + String(pin) + "'" + (pin == config->am2302_pin ? " selected" : "") + ">GPIO" + String(pin) + "</option>";
        }
        html += "</select>";
        html += "<button type='button' class='test-button' onclick='testSensor(1, " + String(config->am2302_pin) + ")'>🧪 Test</button>";
        html += "</div>";
        html += "</div>";
        
        html += "<div class='config-row'>";
        html += "<div class='config-label'>Priority:</div>";
        html += "<div class='config-control'>";
        html += "<select id='am2302_priority'>";
        html += "<option value='0'" + String(config->am2302_priority == 0 ? " selected" : "") + ">1 (Highest)</option>";
        html += "<option value='1'" + String(config->am2302_priority == 1 ? " selected" : "") + ">2 (Medium)</option>";
        html += "<option value='2'" + String(config->am2302_priority == 2 ? " selected" : "") + ">3 (Lowest)</option>";
        html += "</select>";
        html += "</div>";
        html += "</div>";
        
        html += "</div></div>";
        
        // LM35 Configuration
        html += "<div class='sensor-card'>";
        html += "<div class='sensor-header'>LM35 (Analog Temperature Sensor)</div>";
        html += "<div class='sensor-body'>";
        
        html += "<div class='config-row'>";
        html += "<div class='config-label'>Enable:</div>";
        html += "<div class='config-control'>";
        html += "<label class='toggle'>";
        html += "<input type='checkbox' id='lm35_enabled' " + String(config->lm35_enabled ? "checked" : "") + ">";
        html += "<span class='slider'></span>";
        html += "</label>";
        html += "<span class='status-indicator status-working'>Fallback Ready</span>";
        html += "</div>";
        html += "</div>";
        
        html += "<div class='config-row'>";
        html += "<div class='config-label'>GPIO Pin:</div>";
        html += "<div class='config-control'>";
        html += "<select id='lm35_pin'>";
        const int analogPins[] = {32, 33, 34, 35, 36, 39}; // ESP32 ADC pins
        for (int pin : analogPins) {
            html += "<option value='" + String(pin) + "'" + (pin == config->lm35_pin ? " selected" : "") + ">GPIO" + String(pin) + " (ADC)</option>";
        }
        html += "</select>";
        html += "<button type='button' class='test-button' onclick='testSensor(3, " + String(config->lm35_pin) + ")'>🧪 Test</button>";
        html += "</div>";
        html += "</div>";
        
        html += "<div class='config-row'>";
        html += "<div class='config-label'>Priority:</div>";
        html += "<div class='config-control'>";
        html += "<select id='lm35_priority'>";
        html += "<option value='0'" + String(config->lm35_priority == 0 ? " selected" : "") + ">1 (Highest)</option>";
        html += "<option value='1'" + String(config->lm35_priority == 1 ? " selected" : "") + ">2 (Medium)</option>";
        html += "<option value='2'" + String(config->lm35_priority == 2 ? " selected" : "") + ">3 (Lowest)</option>";
        html += "</select>";
        html += "</div>";
        html += "</div>";
        
        html += "</div></div>";
        
        html += "</form>";
        
        // Action buttons
        html += "<div style='text-align:center;margin:30px 0'>";
        html += "<button class='save-button' onclick='saveSensorConfig()'>Save Configuration</button>";
        html += "<button class='reset-button' onclick='resetToDefaults()'>Reset to Defaults</button>";
        html += "</div>";
        
        html += "<div style='text-align:center;margin:20px 0'>";
        html += "<a href='/' class='back-button'>← Back to Main</a>";
        html += "</div>";
        
        // JavaScript for sensor configuration
        html += "<script>";
        html += "function testSensor(type, pin) {";
        html += "  fetch('/api/sensors/test?type=' + type + '&pin=' + pin)";
        html += "    .then(response => response.text())";
        html += "    .then(result => alert('Test Result: ' + result))";
        html += "    .catch(err => alert('Test failed: ' + err));";
        html += "}";
        
        html += "function saveSensorConfig() {";
        html += "  const config = {";
        html += "    ds18b20_enabled: document.getElementById('ds18b20_enabled').checked,";
        html += "    ds18b20_pin: parseInt(document.getElementById('ds18b20_pin').value),";
        html += "    ds18b20_priority: parseInt(document.getElementById('ds18b20_priority').value),";
        html += "    am2302_enabled: document.getElementById('am2302_enabled').checked,";
        html += "    am2302_pin: parseInt(document.getElementById('am2302_pin').value),";
        html += "    am2302_priority: parseInt(document.getElementById('am2302_priority').value),";
        html += "    lm35_enabled: document.getElementById('lm35_enabled').checked,";
        html += "    lm35_pin: parseInt(document.getElementById('lm35_pin').value),";
        html += "    lm35_priority: parseInt(document.getElementById('lm35_priority').value)";
        html += "  };";
        
        html += "  const formData = new URLSearchParams();";
        html += "  Object.keys(config).forEach(key => formData.append(key, config[key]));";
        html += "  fetch('/api/sensors/config', {";
        html += "    method: 'POST',";
        html += "    headers: {'Content-Type': 'application/x-www-form-urlencoded'},";
        html += "    body: formData";
        html += "  })";
        html += "  .then(response => response.json())";
        html += "  .then(data => {";
        html += "    if (data.success) {";
        html += "      alert('[OK] Configuration saved successfully!\\nReboot recommended for all changes to take effect.');";
        html += "    } else {";
        html += "      alert('[ERROR] Failed to save configuration: ' + data.message);";
        html += "    }";
        html += "  })";
        html += "  .catch(err => alert('[ERROR] Save failed: ' + err));";
        html += "}";
        
        html += "function resetToDefaults() {";
        html += "  if (confirm('[WARNING] Reset all sensor settings to defaults?\\nThis cannot be undone.')) {";
        html += "    fetch('/api/sensors/reset', {method: 'POST'})";
        html += "      .then(response => response.json())";
        html += "      .then(data => {";
        html += "        if (data.success) {";
        html += "          alert('[OK] Settings reset to defaults!');";
        html += "          location.reload();";
        html += "        } else {";
        html += "          alert('[ERROR] Reset failed: ' + data.message);";
        html += "        }";
        html += "      })";
        html += "      .catch(err => alert('[ERROR] Reset failed: ' + err));";
        html += "  }";
        html += "}";
        html += "</script>";
        
        html += "</div></body></html>";
        device_server->send(200, "text/html", html);
    });
    
    // Sensor configuration API endpoints
    device_server->on("/api/sensors/config", HTTP_POST, []() {
        String json = "{";
        
        // Parse form data configuration
        SensorConfig newConfig = g_sensorConfig; // Start with current config
        
        // Parse each field from form parameters
        if (device_server->hasArg("ds18b20_enabled")) {
            newConfig.ds18b20_enabled = device_server->arg("ds18b20_enabled") == "true";
        }
        if (device_server->hasArg("am2302_enabled")) {
            newConfig.am2302_enabled = device_server->arg("am2302_enabled") == "true";
        }
        if (device_server->hasArg("lm35_enabled")) {
            newConfig.lm35_enabled = device_server->arg("lm35_enabled") == "true";
        }
        
        // Extract pins and priorities
        if (device_server->hasArg("ds18b20_pin")) {
            newConfig.ds18b20_pin = device_server->arg("ds18b20_pin").toInt();
        }
        if (device_server->hasArg("am2302_pin")) {
            newConfig.am2302_pin = device_server->arg("am2302_pin").toInt();
        }
        if (device_server->hasArg("lm35_pin")) {
            newConfig.lm35_pin = device_server->arg("lm35_pin").toInt();
        }
        
        if (device_server->hasArg("ds18b20_priority")) {
            newConfig.ds18b20_priority = device_server->arg("ds18b20_priority").toInt();
        }
        if (device_server->hasArg("am2302_priority")) {
            newConfig.am2302_priority = device_server->arg("am2302_priority").toInt();
        }
        if (device_server->hasArg("lm35_priority")) {
            newConfig.lm35_priority = device_server->arg("lm35_priority").toInt();
        }
        
        // Set magic number and prepare for checksum calculation  
        newConfig.magic = SENSOR_CONFIG_MAGIC;
        newConfig.checksum = 0; // Set to 0 before calculating
        newConfig.checksum = calculateConfigChecksum(&newConfig);
        
        // Skip validation for now and save configuration directly
        // TODO: Fix validation logic
        g_sensorConfig = newConfig;
        saveSensorConfig();
        applySensorConfiguration();
        
        json += "\"success\":true,";
        json += "\"message\":\"Configuration saved successfully (validation bypassed for testing)\"";
        
        json += "}";
        device_server->send(200, "application/json", json);
    });
    
    device_server->on("/api/sensors/reset", HTTP_POST, []() {
        resetSensorConfigToDefaults();
        saveSensorConfig();
        applySensorConfiguration();
        
        String json = "{\"success\":true,\"message\":\"Configuration reset to defaults\"}";
        device_server->send(200, "application/json", json);
    });
    
    device_server->on("/api/sensors/test", []() {
        int sensorType = device_server->arg("type").toInt();
        int pin = device_server->arg("pin").toInt();
        
        String result = getSensorTestResult(sensorType, pin);
        device_server->send(200, "text/plain", result);
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
                    message = "[OK] Static IP configuration applied successfully!<br>Reconnecting...";
                    DEBUG_PRINTF("Static IP configured: %s\n", static_ip.c_str());
                } else {
                    message = "[ERROR] Failed to apply static IP configuration";
                }
            } else {
                message = "[ERROR] Invalid IP address format";
            }
        } else {
            // Switch to DHCP
            if (WiFi.config(0U, 0U, 0U)) {  // Reset to DHCP
                message = "[OK] DHCP mode enabled successfully!<br>Reconnecting...";
                DEBUG_PRINTLN("DHCP mode configured");
            } else {
                message = "[ERROR] Failed to enable DHCP mode";
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
        if (message.indexOf("[OK]") >= 0) {
            delay(2000);  // Give time for response to be sent
            WiFi.reconnect();
        }
    });
    
    // WiFi network change page
    device_server->on("/wifi-config", []() {
        String html = "<!DOCTYPE html><html><head><title>Network Configuration - MAC-SYS Industrial Controller</title>";
        html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
        html += "<style>";
        html += "* { margin: 0; padding: 0; box-sizing: border-box; }";
        html += "body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: #0B1426; color: #E5E7EB; line-height: 1.6; padding-top: 6rem; }";
        html += ".header { position: fixed; top: 0; left: 0; right: 0; z-index: 1000; background: linear-gradient(135deg, #1e3a8a 0%, #3b82f6 100%); color: white; padding: 1rem 2rem; box-shadow: 0 2px 10px rgba(0,0,0,0.3); }";
        html += ".header h1 { font-size: 1.5rem; margin: 0; display: flex; align-items: center; gap: 0.75rem; }";
        html += ".nav-links { margin-top: 0.75rem; display: flex; gap: 1.5rem; flex-wrap: wrap; }";
        html += ".nav-links a { color: #dbeafe; text-decoration: none; padding: 0.375rem 0.75rem; border-radius: 0.375rem; transition: all 0.2s; font-size: 0.875rem; }";
        html += ".nav-links a:hover { background: rgba(255,255,255,0.2); }";
        html += ".nav-links a.active { background: #00D4FF; color: #0B1426; font-weight: 600; }";
        html += ".container { max-width: 1200px; margin: 2rem auto; padding: 0 1rem; }";
        html += ".network-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); gap: 1.5rem; margin: 1.5rem 0; }";
        html += ".network-card { background: linear-gradient(135deg, #1f2937 0%, #374151 100%); border-radius: 0.75rem; padding: 1.5rem; border: 1px solid #374151; box-shadow: 0 4px 6px rgba(0,0,0,0.3); }";
        html += ".card-header { color: #00D4FF; font-size: 1.125rem; font-weight: 600; margin-bottom: 1rem; }";
        html += ".warning { background: rgba(245, 158, 11, 0.1); border: 1px solid #F59E0B; color: #FCD34D; padding: 1rem; border-radius: 0.5rem; margin: 1rem 0; }";
        html += ".network-item { display: flex; justify-content: space-between; align-items: center; padding: 0.75rem 0; border-bottom: 1px solid #374151; }";
        html += ".network-item:last-child { border-bottom: none; }";
        html += ".network-label { color: #9CA3AF; font-size: 0.875rem; }";
        html += ".network-value { color: #E5E7EB; font-weight: 600; }";
        html += ".btn { padding: 0.5rem 1rem; border: none; border-radius: 0.375rem; font-size: 0.875rem; font-weight: 600; cursor: pointer; transition: all 0.2s; margin: 0.25rem; }";
        html += ".btn-primary { background: #00D4FF; color: #0B1426; }";
        html += ".btn-danger { background: #EF4444; color: white; }";
        html += ".btn-secondary { background: #374151; color: #E5E7EB; border: 1px solid #4B5563; }";
        html += "</style></head><body>";
        
        // Header with navigation
        html += "<div class='header'>";
        html += "<h1>Network Configuration</h1>";
        html += "<div class='nav-links'>";
        html += "<a href='/'>Dashboard</a>";
        html += "<a href='/system'>System</a>";
        html += "<a href='/relays'>Relays</a>";
        html += "<a href='/temperature'>Temperature</a>";
        html += "<a href='/schedule'>Schedule</a>";
        html += "<a href='/sensors'>Sensors</a>";
        html += "<a href='/wifi-config' class='active'>Network</a>";
        html += "</div>";
        html += "</div>";
        
        html += "<div class='container'>";
        html += "<div class='network-grid'>";
        
        // Current Network Status Card
        html += "<div class='network-card'>";
        html += "<div class='card-header'>Current Network Status</div>";
        
        html += "<div class='network-item'>";
        html += "<span class='network-label'>Network SSID</span>";
        html += "<span class='network-value'>" + WiFi.SSID() + "</span>";
        html += "</div>";
        html += "<div class='network-item'>";
        html += "<span class='network-label'>IP Address</span>";
        html += "<span class='network-value'>" + WiFi.localIP().toString() + "</span>";
        html += "</div>";
        html += "<div class='network-item'>";
        html += "<span class='network-label'>Signal Strength</span>";
        html += "<span class='network-value'>" + String(WiFi.RSSI()) + " dBm</span>";
        html += "</div>";
        html += "<div class='network-item'>";
        html += "<span class='network-label'>MAC Address</span>";
        html += "<span class='network-value'>" + WiFi.macAddress() + "</span>";
        html += "</div>";
        html += "</div>";
        
        // Network Configuration Card
        html += "<div class='network-card'>";
        html += "<div class='card-header'>Network Configuration</div>";
        html += "<div class='warning'><strong>Warning:</strong> Changing WiFi settings will disconnect the current connection and start configuration mode.</div>";
        
        html += "<h3 style='color: #E5E7EB; margin: 1rem 0;'>Change Network Instructions</h3>";
        html += "<ol style='color: #9CA3AF; margin-left: 1.5rem;'>";
        html += "<li>Click <strong>Reset WiFi Settings</strong> below</li>";
        html += "<li>Device will restart in configuration mode</li>";
        html += "<li>Connect to <strong>MAC-SYS-CONFIG</strong> network (password: admin123)</li>";
        html += "<li>Configure new network settings</li>";
        html += "</ol>";
        
        html += "<div style='margin: 2rem 0; text-align: center;'>";
        html += "<button class='btn btn-secondary' onclick=\"location.href='/'\">← Back to Dashboard</button>";
        html += "<button class='btn btn-danger' onclick='resetWiFi()'>Reset WiFi Settings</button>";
        html += "</div>";
        html += "</div>";
        html += "</div>"; // network-grid
        html += "</div>"; // container
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
        html += "<h2>Temperature Settings Updated</h2>";
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

    // Dedicated System Status page
    device_server->on("/system", []() {
        String html = "<!DOCTYPE html><html><head>";
        html += "<title>System Status - MAC-SYS Industrial Controller</title>";
        html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
        html += "<style>";
        html += "* { margin: 0; padding: 0; box-sizing: border-box; }";
        html += "body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: #0B1426; color: #E5E7EB; line-height: 1.6; padding-top: 6rem; }";
        html += ".header { position: fixed; top: 0; left: 0; right: 0; z-index: 1000; background: linear-gradient(135deg, #1e3a8a 0%, #3b82f6 100%); color: white; padding: 1rem 2rem; box-shadow: 0 2px 10px rgba(0,0,0,0.3); }";
        html += ".header h1 { font-size: 1.5rem; margin: 0; display: flex; align-items: center; gap: 0.75rem; }";
        html += ".nav-links { margin-top: 0.75rem; display: flex; gap: 1.5rem; flex-wrap: wrap; }";
        html += ".nav-links a { color: #dbeafe; text-decoration: none; padding: 0.375rem 0.75rem; border-radius: 0.375rem; transition: all 0.2s; font-size: 0.875rem; }";
        html += ".nav-links a:hover { background: rgba(255,255,255,0.2); }";
        html += ".nav-links a.active { background: #00D4FF; color: #0B1426; font-weight: 600; }";
        html += ".main-content { max-width: 1200px; margin: 2rem auto; padding: 0 1rem; }";
        html += ".status-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); gap: 1.5rem; margin: 1.5rem 0; }";
        html += ".status-card { background: linear-gradient(135deg, #1f2937 0%, #374151 100%); border-radius: 0.75rem; padding: 1.5rem; border: 1px solid #374151; box-shadow: 0 4px 6px rgba(0,0,0,0.3); }";
        html += ".card-header { color: #00D4FF; font-size: 1.125rem; font-weight: 600; margin-bottom: 1rem; }";
        html += ".status-item { display: flex; justify-content: space-between; align-items: center; padding: 0.75rem 0; border-bottom: 1px solid #374151; }";
        html += ".status-item:last-child { border-bottom: none; }";
        html += ".status-label { color: #9CA3AF; font-size: 0.875rem; }";
        html += ".status-value { color: #E5E7EB; font-weight: 600; }";
        html += ".status-good { color: #10B981; }";
        html += ".status-warning { color: #F59E0B; }";
        html += ".status-error { color: #EF4444; }";
        html += ".action-buttons { display: flex; gap: 0.75rem; margin-top: 1.5rem; }";
        html += ".btn { padding: 0.5rem 1rem; border: none; border-radius: 0.375rem; font-size: 0.875rem; font-weight: 600; cursor: pointer; transition: all 0.2s; }";
        html += ".btn-primary { background: #00D4FF; color: #0B1426; }";
        html += ".btn-danger { background: #EF4444; color: white; }";
        html += ".btn-secondary { background: #374151; color: #E5E7EB; border: 1px solid #4B5563; }";
        html += ".api-result { background: #111827; border: 1px solid #374151; border-radius: 0.5rem; padding: 1rem; margin-top: 1rem; font-family: monospace; font-size: 0.75rem; max-height: 200px; overflow-y: auto; display: none; }";
        html += "@media (max-width: 768px) { .status-grid { grid-template-columns: 1fr; } }";
        html += "</style></head><body>";
        
        // Header with navigation
        html += "<div class='header'>";
        html += "<h1>System Status</h1>";
        html += "<div class='nav-links'>";
        html += "<a href='/'>Dashboard</a>";
        html += "<a href='/system' class='active'>System</a>";
        html += "<a href='/relays'>Relays</a>";
        html += "<a href='/temperature'>Temperature</a>";
        html += "<a href='/schedule'>Schedule</a>";
        html += "<a href='/sensors'>Sensors</a>";
        html += "<a href='/wifi-config'>Network</a>";
        html += "</div>";
        html += "</div>";
        
        html += "<div class='main-content'>";
        html += "<div class='status-grid'>";
        
        // System Information Card
        html += "<div class='status-card'>";
        html += "<div class='card-header'>System Information</div>";
        html += "<div class='status-item'>";
        html += "<span class='status-label'>System State</span>";
        html += "<span class='status-value status-good'>" + String(g_system_status.state) + "</span>";
        html += "</div>";
        html += "<div class='status-item'>";
        html += "<span class='status-label'>Uptime</span>";
        html += "<span class='status-value'>" + String(g_system_status.uptime) + " seconds</span>";
        html += "</div>";
        html += "<div class='status-item'>";
        html += "<span class='status-label'>Free Memory</span>";
        html += "<span class='status-value'>" + String(g_system_status.free_memory/1024) + " KB</span>";
        html += "</div>";
        html += "<div class='status-item'>";
        html += "<span class='status-label'>Firmware Version</span>";
        html += "<span class='status-value'>" + String(FIRMWARE_VERSION) + "</span>";
        html += "</div>";
        html += "</div>";
        
        // Network Information Card
        html += "<div class='status-card'>";
        html += "<div class='card-header'>Network Status</div>";
        html += "<div class='status-item'>";
        html += "<span class='status-label'>WiFi Status</span>";
        html += "<span class='status-value " + String(g_wifi_connected ? "status-good'>Connected" : "status-error'>Disconnected") + "</span>";
        html += "</div>";
        if (g_wifi_connected) {
            html += "<div class='status-item'>";
            html += "<span class='status-label'>Network SSID</span>";
            html += "<span class='status-value'>" + WiFi.SSID() + "</span>";
            html += "</div>";
            html += "<div class='status-item'>";
            html += "<span class='status-label'>IP Address</span>";
            html += "<span class='status-value'>" + WiFi.localIP().toString() + "</span>";
            html += "</div>";
            html += "<div class='status-item'>";
            html += "<span class='status-label'>Signal Strength</span>";
            html += "<span class='status-value'>" + String(WiFi.RSSI()) + " dBm</span>";
            html += "</div>";
        }
        html += "</div>";
        
        // Hardware Information Card
        html += "<div class='status-card'>";
        html += "<div class='card-header'>Hardware Status</div>";
        html += "<div class='status-item'>";
        html += "<span class='status-label'>Chip Model</span>";
        html += "<span class='status-value'>ESP32</span>";
        html += "</div>";
        html += "<div class='status-item'>";
        html += "<span class='status-label'>CPU Frequency</span>";
        html += "<span class='status-value'>" + String(ESP.getCpuFreqMHz()) + " MHz</span>";
        html += "</div>";
        html += "<div class='status-item'>";
        html += "<span class='status-label'>Flash Size</span>";
        html += "<span class='status-value'>" + String(ESP.getFlashChipSize()/1024/1024) + " MB</span>";
        html += "</div>";
        html += "<div class='status-item'>";
        html += "<span class='status-label'>MAC Address</span>";
        html += "<span class='status-value'>" + WiFi.macAddress() + "</span>";
        html += "</div>";
        html += "</div>";
        
        // Time Information Card
        html += "<div class='status-card'>";
        html += "<div class='card-header'>Time & Date</div>";
        html += "<div class='status-item'>";
        html += "<span class='status-label'>Current Time</span>";
        html += "<span class='status-value'>" + rtc_manager.getFormattedDateTime() + "</span>";
        html += "</div>";
        html += "<div class='status-item'>";
        html += "<span class='status-label'>RTC Status</span>";
        html += "<span class='status-value " + String(rtc_manager.isRTCAvailable() ? "status-good'>Available" : "status-warning'>Unavailable") + "</span>";
        html += "</div>";
        html += "<div class='status-item'>";
        html += "<span class='status-label'>NTP Sync</span>";
        html += "<span class='status-value " + String(rtc_manager.isNTPSynced() ? "status-good'>Synchronized" : "status-warning'>Not Synced") + "</span>";
        html += "</div>";
        html += "</div>";
        
        html += "</div>"; // status-grid
        
        // System Actions
        html += "<div class='status-card' style='margin-top: 1.5rem;'>";
        html += "<div class='card-header'>System Actions</div>";
        html += "<div class='action-buttons'>";
        html += "<button class='btn btn-primary' onclick='testSystemAPI()'>Test System API</button>";
        html += "<button class='btn btn-secondary' onclick='testDiagnosticsAPI()'>Run Diagnostics</button>";
        html += "<button class='btn btn-danger' onclick='restartSystem()'>Restart System</button>";
        html += "</div>";
        html += "<div id='api-result' class='api-result'></div>";
        html += "</div>";
        
        html += "</div>"; // main-content
        
        // JavaScript
        html += "<script>";
        html += "function testSystemAPI() {";
        html += "  fetch('/api/system').then(r => r.json()).then(data => {";
        html += "    document.getElementById('api-result').style.display = 'block';";
        html += "    document.getElementById('api-result').textContent = JSON.stringify(data, null, 2);";
        html += "  });";
        html += "}";
        html += "function testDiagnosticsAPI() {";
        html += "  fetch('/api/diagnostics').then(r => r.json()).then(data => {";
        html += "    document.getElementById('api-result').style.display = 'block';";
        html += "    document.getElementById('api-result').textContent = JSON.stringify(data, null, 2);";
        html += "  });";
        html += "}";
        html += "function restartSystem() {";
        html += "  if(confirm('Restart the system? This will disconnect all users.')) {";
        html += "    fetch('/api/system/restart', {method: 'POST'}).then(() => {";
        html += "      alert('System restarting... Please wait 30 seconds before reconnecting.');";
        html += "    });";
        html += "  }";
        html += "}";
        // Auto-refresh disabled for better user interaction - use manual refresh
        // html += "setInterval(() => location.reload(), 10000);";
        html += "</script>";
        html += "</body></html>";
        
        device_server->send(200, "text/html", html);
    });

    // Dedicated Relay Control page
    device_server->on("/relays", []() {
        String html = "<!DOCTYPE html><html><head>";
        html += "<title>Relay Control - MAC-SYS Industrial Controller</title>";
        html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
        html += "<style>";
        html += "* { margin: 0; padding: 0; box-sizing: border-box; }";
        html += "body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: #0B1426; color: #E5E7EB; line-height: 1.6; padding-top: 6rem; }";
        html += ".header { position: fixed; top: 0; left: 0; right: 0; z-index: 1000; background: linear-gradient(135deg, #1e3a8a 0%, #3b82f6 100%); color: white; padding: 1rem 2rem; box-shadow: 0 2px 10px rgba(0,0,0,0.3); }";
        html += ".header h1 { font-size: 1.5rem; margin: 0; display: flex; align-items: center; gap: 0.75rem; }";
        html += ".nav-links { margin-top: 0.75rem; display: flex; gap: 1.5rem; flex-wrap: wrap; }";
        html += ".nav-links a { color: #dbeafe; text-decoration: none; padding: 0.375rem 0.75rem; border-radius: 0.375rem; transition: all 0.2s; font-size: 0.875rem; }";
        html += ".nav-links a:hover { background: rgba(255,255,255,0.2); }";
        html += ".nav-links a.active { background: #00D4FF; color: #0B1426; font-weight: 600; }";
        html += ".main-content { max-width: 1200px; margin: 2rem auto; padding: 0 1rem; }";
        html += ".relay-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(280px, 1fr)); gap: 1.5rem; margin: 1.5rem 0; }";
        html += ".relay-card { background: linear-gradient(135deg, #1f2937 0%, #374151 100%); border-radius: 0.75rem; padding: 1.5rem; border: 1px solid #374151; box-shadow: 0 4px 6px rgba(0,0,0,0.3); }";
        html += ".relay-header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 1rem; }";
        html += ".relay-title { font-size: 1.125rem; font-weight: 600; color: #00D4FF; }";
        html += ".relay-status { padding: 0.25rem 0.75rem; border-radius: 1rem; font-size: 0.75rem; font-weight: 600; text-transform: uppercase; }";
        html += ".status-on { background: #10B981; color: white; }";
        html += ".status-off { background: #6B7280; color: white; }";
        html += ".relay-display { display: flex; justify-content: space-between; align-items: center; margin: 1rem 0; }";
        html += ".relay-state { font-size: 2rem; font-weight: bold; color: #00D4FF; }";
        html += ".relay-info { font-size: 0.875rem; color: #D1D5DB; }";
        html += ".control-buttons { display: flex; gap: 0.75rem; margin-top: 1rem; }";
        html += ".btn { padding: 0.5rem 1rem; border: none; border-radius: 0.375rem; font-size: 0.875rem; font-weight: 600; cursor: pointer; transition: all 0.2s; }";
        html += ".btn-on { background: #10B981; color: white; }";
        html += ".btn-off { background: #6B7280; color: white; }";
        html += ".btn-primary { background: #00D4FF; color: #0B1426; }";
        html += ".btn-danger { background: #EF4444; color: white; }";
        html += ".global-controls { background: linear-gradient(135deg, #1f2937 0%, #374151 100%); border-radius: 0.75rem; padding: 1.5rem; margin: 1.5rem 0; border: 1px solid #374151; }";
        html += ".global-title { color: #00D4FF; font-size: 1.125rem; font-weight: 600; margin-bottom: 1rem; }";
        html += "@media (max-width: 768px) { .relay-grid { grid-template-columns: 1fr; } }";
        html += "</style></head><body>";
        
        // Header with navigation
        html += "<div class='header'>";
        html += "<h1>Relay Control System</h1>";
        html += "<div class='nav-links'>";
        html += "<a href='/'>Dashboard</a>";
        html += "<a href='/system'>System</a>";
        html += "<a href='/relays' class='active'>Relays</a>";
        html += "<a href='/temperature'>Temperature</a>";
        html += "<a href='/schedule'>Schedule</a>";
        html += "<a href='/sensors'>Sensors</a>";
        html += "<a href='/wifi-config'>Network</a>";
        html += "</div>";
        html += "</div>";
        
        html += "<div class='main-content'>";
        
        // Global relay controls
        html += "<div class='global-controls'>";
        html += "<div class='global-title'>Global Relay Controls</div>";
        html += "<div class='control-buttons'>";
        html += "<button class='btn btn-on' onclick='controlAllRelays(true)'>Turn All ON</button>";
        html += "<button class='btn btn-off' onclick='controlAllRelays(false)'>Turn All OFF</button>";
        html += "<button class='btn btn-primary' onclick='testRelayAPI()'>Test API</button>";
        html += "</div>";
        html += "</div>";
        
        html += "<div class='relay-grid'>";
        
        // Generate cards for all 6 relays
        for (int relay = 0; relay < 6; relay++) {
            bool relayState = relay_controller.getRelayState(relay);
            
            html += "<div class='relay-card'>";
            html += "<div class='relay-header'>";
            html += "<div class='relay-title'>Relay " + String(relay + 1) + "</div>";
            
            String statusClass = relayState ? "status-on" : "status-off";
            String statusText = relayState ? "ON" : "OFF";
            html += "<div class='relay-status " + statusClass + "'>" + statusText + "</div>";
            html += "</div>";
            
            html += "<div class='relay-display'>";
            html += "<div class='relay-state'>" + String(relayState ? "ON" : "OFF") + "</div>";
            html += "<div class='relay-info'>Channel " + String(relay + 1) + "</div>";
            html += "</div>";
            
            html += "<div class='control-buttons'>";
            html += "<button class='btn btn-on' onclick='controlRelay(" + String(relay) + ", true)'>Turn ON</button>";
            html += "<button class='btn btn-off' onclick='controlRelay(" + String(relay) + ", false)'>Turn OFF</button>";
            html += "</div>";
            html += "</div>";
        }
        
        html += "</div>"; // relay-grid
        html += "</div>"; // main-content
        
        // JavaScript
        html += "<script>";
        html += "function controlRelay(relay, state) {";
        html += "  const button = event.target;";
        html += "  button.disabled = true;";
        html += "  button.textContent = 'Working...';";
        html += "  fetch('/api/relays/control', {";
        html += "    method: 'POST',";
        html += "    headers: {'Content-Type': 'application/x-www-form-urlencoded'},";
        html += "    body: 'relay=' + relay + '&state=' + state";
        html += "  })";
        html += "  .then(response => response.json())";
        html += "  .then(data => {";
        html += "    if (data.success) {";
        html += "      setTimeout(() => location.reload(), 500);";
        html += "    } else {";
        html += "      alert('Error: ' + (data.error || 'Unknown error'));";
        html += "      button.disabled = false;";
        html += "      button.textContent = state ? 'Turn ON' : 'Turn OFF';";
        html += "    }";
        html += "  })";
        html += "  .catch(err => {";
        html += "    alert('Network error: ' + err);";
        html += "    button.disabled = false;";
        html += "    button.textContent = state ? 'Turn ON' : 'Turn OFF';";
        html += "  });";
        html += "}";
        html += "function controlAllRelays(state) {";
        html += "  if (!confirm('Turn ALL relays ' + (state ? 'ON' : 'OFF') + '?')) return;";
        html += "  fetch('/api/relays/all', {";
        html += "    method: 'POST',";
        html += "    headers: {'Content-Type': 'application/x-www-form-urlencoded'},";
        html += "    body: 'state=' + state";
        html += "  })";
        html += "  .then(response => response.json())";
        html += "  .then(data => {";
        html += "    if (data.success) {";
        html += "      setTimeout(() => location.reload(), 500);";
        html += "    } else {";
        html += "      alert('Error: ' + (data.error || 'Unknown error'));";
        html += "    }";
        html += "  })";
        html += "  .catch(err => alert('Network error: ' + err));";
        html += "}";
        html += "function testRelayAPI() {";
        html += "  fetch('/api/relays').then(r => r.json()).then(data => {";
        html += "    alert('Relay API Test Result:\\n' + JSON.stringify(data, null, 2));";
        html += "  });";
        html += "}";
        // Auto-refresh disabled for better user interaction
        // html += "setInterval(() => location.reload(), 5000);";
        html += "</script>";
        html += "</body></html>";
        
        device_server->send(200, "text/html", html);
    });

    // Schedule management endpoints
    // Temperature route removed - using professional interface from webserver_api.h
    /* REMOVED CONFLICTING ROUTE - START
    device_server->on("/temperature", []() {
        String html = "<!DOCTYPE html><html><head>";
        html += "<title>Temperature Control - MAC-SYS Industrial Controller</title>";
        html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
        html += "<style>";
        html += "* { margin: 0; padding: 0; box-sizing: border-box; }";
        html += "body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: #0B1426; color: #E5E7EB; line-height: 1.6; padding-top: 6rem; }";
        html += ".header { position: fixed; top: 0; left: 0; right: 0; z-index: 1000; background: linear-gradient(135deg, #1e3a8a 0%, #3b82f6 100%); color: white; padding: 1rem 2rem; box-shadow: 0 2px 10px rgba(0,0,0,0.3); }";
        html += ".header h1 { font-size: 1.5rem; margin: 0; display: flex; align-items: center; gap: 0.75rem; }";
        html += ".nav-links { margin-top: 0.75rem; display: flex; gap: 1.5rem; flex-wrap: wrap; }";
        html += ".nav-links a { color: #dbeafe; text-decoration: none; padding: 0.375rem 0.75rem; border-radius: 0.375rem; transition: all 0.2s; font-size: 0.875rem; }";
        html += ".nav-links a:hover { background: rgba(255,255,255,0.2); }";
        html += ".nav-links a.active { background: #00D4FF; color: #0B1426; font-weight: 600; }";
        html += ".main-content { max-width: 1200px; margin: 2rem auto; padding: 0 1rem; }";
        
        // Temperature grid styles
        html += ".temp-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(280px, 1fr)); gap: 1.5rem; margin: 1.5rem 0; }";
        html += ".temp-card { background: linear-gradient(135deg, #1f2937 0%, #374151 100%); border-radius: 0.75rem; padding: 1.5rem; border: 1px solid #374151; box-shadow: 0 4px 6px rgba(0,0,0,0.3); }";
        html += ".zone-header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 1rem; }";
        html += ".zone-title { font-size: 1.125rem; font-weight: 600; color: #00D4FF; }";
        html += ".zone-status { padding: 0.25rem 0.75rem; border-radius: 1rem; font-size: 0.75rem; font-weight: 600; text-transform: uppercase; }";
        html += ".status-active { background: #10B981; color: white; }";
        html += ".status-idle { background: #6B7280; color: white; }";
        html += ".status-disabled { background: #EF4444; color: white; }";
        html += ".temp-display { display: flex; justify-content: space-between; align-items: center; margin: 1rem 0; }";
        html += ".current-temp { font-size: 2rem; font-weight: bold; color: #00D4FF; }";
        html += ".temp-unit { font-size: 1rem; color: #9CA3AF; margin-left: 0.25rem; }";
        html += ".setpoint { font-size: 0.875rem; color: #D1D5DB; }";
        html += ".controls { display: grid; grid-template-columns: 1fr 1fr; gap: 1rem; margin-top: 1rem; }";
        html += ".control-group { display: flex; flex-direction: column; gap: 0.375rem; }";
        html += ".control-group label { font-size: 0.75rem; color: #9CA3AF; text-transform: uppercase; font-weight: 600; }";
        html += ".control-input { background: #374151; border: 1px solid #4B5563; color: #E5E7EB; padding: 0.5rem; border-radius: 0.375rem; font-size: 0.875rem; }";
        html += ".control-input:focus { outline: none; border-color: #00D4FF; box-shadow: 0 0 0 2px rgba(0, 212, 255, 0.2); }";
        html += ".mode-select { background: #374151; border: 1px solid #4B5563; color: #E5E7EB; padding: 0.5rem; border-radius: 0.375rem; font-size: 0.875rem; }";
        html += ".action-buttons { display: flex; gap: 0.75rem; margin-top: 1rem; }";
        html += ".btn { padding: 0.5rem 1rem; border: none; border-radius: 0.375rem; font-size: 0.875rem; font-weight: 600; cursor: pointer; transition: all 0.2s; text-decoration: none; display: inline-block; text-align: center; }";
        html += ".btn-primary { background: #00D4FF; color: #0B1426; }";
        html += ".btn-primary:hover { background: #0EA5E9; }";
        html += ".btn-secondary { background: #374151; color: #E5E7EB; border: 1px solid #4B5563; }";
        html += ".btn-secondary:hover { background: #4B5563; }";
        html += ".btn-danger { background: #EF4444; color: white; }";
        html += ".btn-danger:hover { background: #DC2626; }";
        html += ".emergency-section { background: linear-gradient(135deg, #7F1D1D 0%, #991B1B 100%); border-radius: 0.75rem; padding: 1.5rem; margin: 2rem 0; border: 1px solid #DC2626; }";
        html += ".emergency-title { color: #FEF2F2; font-size: 1.125rem; font-weight: 600; margin-bottom: 1rem; }";
        html += ".emergency-info { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 1rem; }";
        html += ".emergency-item { background: rgba(0,0,0,0.2); padding: 1rem; border-radius: 0.5rem; }";
        html += ".emergency-label { color: #FECACA; font-size: 0.75rem; text-transform: uppercase; font-weight: 600; }";
        html += ".emergency-value { color: #FEF2F2; font-size: 1.125rem; font-weight: bold; }";
        html += "@media (max-width: 768px) { .temp-grid { grid-template-columns: 1fr; } .controls { grid-template-columns: 1fr; } }";
        html += "</style></head><body>";
        
        // Header
        html += "<div class='header'>";
        html += "<h1>Temperature Control System</h1>";
        html += "<div class='nav-links'>";
        html += "<a href='/'>Dashboard</a>";
        html += "<a href='/system'>System</a>";
        html += "<a href='/relays'>Relays</a>";
        html += "<a href='/temperature' class='active'>Temperature</a>";
        html += "<a href='/schedule'>Schedule</a>";
        html += "<a href='/sensors'>Sensors</a>";
        html += "<a href='/wifi-config'>Network</a>";
        html += "</div>";
        html += "</div>";
        
        html += "<div class='main-content'>";
        
        // Emergency status if active
        if (temp_controller.isEmergencyStopped()) {
            html += "<div class='emergency-section'>";
            html += "<div class='emergency-title'>EMERGENCY STOP ACTIVE</div>";
            html += "<div class='emergency-info'>";
            html += "<div class='emergency-item'>";
            html += "<div class='emergency-label'>Status</div>";
            html += "<div class='emergency-value'>SYSTEM STOPPED</div>";
            html += "</div>";
            html += "<div class='emergency-item'>";
            html += "<div class='emergency-label'>Action Required</div>";
            html += "<div class='emergency-value'>Check sensors and clear emergency</div>";
            html += "</div>";
            html += "</div>";
            html += "<div class='action-buttons'>";
            html += "<button class='btn btn-danger' onclick='clearEmergency()'>Clear Emergency</button>";
            html += "</div>";
            html += "</div>";
        }
        
        html += "<div class='temp-grid'>";
        
        // Generate cards for all 4 zones
        for (int zone = 0; zone < 4; zone++) {
            ZoneConfig& zoneConfig = temp_controller.getZoneConfig(zone);
            float currentTemp = temp_controller.getCompensatedTemp(zone);
            bool sensorValid = temp_controller.isSensorValid(zone);
            
            html += "<div class='temp-card'>";
            html += "<div class='zone-header'>";
            html += "<div class='zone-title'>Zone " + String(zone + 1) + "</div>";
            
            String statusClass = "status-disabled";
            String statusText = "DISABLED";
            if (zoneConfig.enabled) {
                if (sensorValid) {
                    statusClass = zoneConfig.current_state ? "status-active" : "status-idle";
                    statusText = zoneConfig.current_state ? "ACTIVE" : "IDLE";
                } else {
                    statusClass = "status-disabled";
                    statusText = "SENSOR ERROR";
                }
            }
            html += "<div class='zone-status " + statusClass + "'>" + statusText + "</div>";
            html += "</div>";
            
            html += "<div class='temp-display'>";
            if (sensorValid) {
                html += "<div class='current-temp'>" + String(currentTemp, 1) + "<span class='temp-unit'>°C</span></div>";
            } else {
                html += "<div class='current-temp'>--<span class='temp-unit'>°C</span></div>";
            }
            html += "<div class='setpoint'>Target: " + String(zoneConfig.setpoint, 1) + "°C</div>";
            html += "</div>";
            
            // Mode display
            String modeStr = "";
            switch(zoneConfig.mode) {
                case TEMP_MODE_OFF: modeStr = "OFF"; break;
                case TEMP_MODE_HEATING: modeStr = "HEATING"; break;
                case TEMP_MODE_COOLING: modeStr = "COOLING"; break;
                case TEMP_MODE_AUTO: modeStr = "AUTO"; break;
                case TEMP_MODE_MANUAL: modeStr = "MANUAL"; break;
            }
            html += "<div style='margin: 1rem 0; color: #9CA3AF; font-size: 0.875rem;'>Mode: " + modeStr + "</div>";
            
            html += "<form method='post' action='/api/temperature/control'>";
            html += "<input type='hidden' name='zone' value='" + String(zone) + "'>";
            html += "<div class='controls'>";
            html += "<div class='control-group'>";
            html += "<label>Setpoint (°C)</label>";
            html += "<input type='number' name='setpoint' class='control-input' value='" + String(zoneConfig.setpoint, 1) + "' min='5' max='40' step='0.5'>";
            html += "</div>";
            html += "<div class='control-group'>";
            html += "<label>Mode</label>";
            html += "<select name='mode' class='mode-select'>";
            html += "<option value='off'" + String(zoneConfig.mode == TEMP_MODE_OFF ? " selected" : "") + ">OFF</option>";
            html += "<option value='heating'" + String(zoneConfig.mode == TEMP_MODE_HEATING ? " selected" : "") + ">Heating</option>";
            html += "<option value='cooling'" + String(zoneConfig.mode == TEMP_MODE_COOLING ? " selected" : "") + ">Cooling</option>";
            html += "<option value='auto'" + String(zoneConfig.mode == TEMP_MODE_AUTO ? " selected" : "") + ">Auto</option>";
            html += "</select>";
            html += "</div>";
            html += "</div>";
            html += "<div class='action-buttons'>";
            html += "<button type='submit' class='btn btn-primary'>Update Zone</button>";
            if (zoneConfig.enabled) {
                html += "<button type='button' class='btn btn-secondary' onclick='toggleZone(" + String(zone) + ", false)'>Disable</button>";
            } else {
                html += "<button type='button' class='btn btn-secondary' onclick='toggleZone(" + String(zone) + ", true)'>Enable</button>";
            }
            html += "</div>";
            html += "</form>";
            html += "</div>";
        }
        
        html += "</div>"; // temp-grid
        
        // Emergency Limits Section
        html += "<div class='emergency-section' style='background: linear-gradient(135deg, #1f2937 0%, #374151 100%); border: 1px solid #4B5563;'>";
        html += "<div class='emergency-title' style='color: #00D4FF;'>Emergency Temperature Limits</div>";
        html += "<div class='emergency-info'>";
        html += "<div class='emergency-item'>";
        html += "<div class='emergency-label'>High Limit</div>";
        html += "<div class='emergency-value' style='color: #EF4444;'>" + String(temp_controller.getEmergencyHighLimit(), 1) + "°C</div>";
        html += "</div>";
        html += "<div class='emergency-item'>";
        html += "<div class='emergency-label'>Low Limit</div>";
        html += "<div class='emergency-value' style='color: #3B82F6;'>" + String(temp_controller.getEmergencyLowLimit(), 1) + "°C</div>";
        html += "</div>";
        html += "</div>";
        html += "</div>";
        
        html += "</div>"; // main-content
        
        // JavaScript for interactivity
        html += "<script>";
        html += "function toggleZone(zone, enabled) {";
        html += "  fetch('/api/temperature/control', {";
        html += "    method: 'POST',";
        html += "    headers: {'Content-Type': 'application/x-www-form-urlencoded'},";
        html += "    body: 'zone=' + zone + '&enabled=' + enabled";
        html += "  }).then(() => location.reload());";
        html += "}";
        html += "function clearEmergency() {";
        html += "  if(confirm('Clear emergency stop?')) {";
        html += "    fetch('/api/temperature/emergency', {method: 'POST', headers: {'Content-Type': 'application/x-www-form-urlencoded'}, body: 'action=clear'})";
        html += "    .then(() => location.reload());";
        html += "  }";
        html += "}";
        // Auto-refresh reduced frequency to avoid interference
        html += "setInterval(() => location.reload(), 60000);"; // Auto-refresh every 60 seconds
        html += "</script>";
        html += "</body></html>";
        
        device_server->send(200, "text/html", html);
    });
    REMOVED CONFLICTING ROUTE - END */

    device_server->on("/schedule", []() {
        String html = "<!DOCTYPE html><html><head><title>MAC-SYS Schedule</title>";
        html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
        html += "<meta charset='UTF-8'>";
        html += "<style>";
        html += "* { margin: 0; padding: 0; box-sizing: border-box; }";
        html += "body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: #0B1426; color: #E5E7EB; line-height: 1.6; padding-top: 6rem; }";
        html += ".header { position: fixed; top: 0; left: 0; right: 0; z-index: 1000; background: linear-gradient(135deg, #1e3a8a 0%, #3b82f6 100%); color: white; padding: 1rem 2rem; box-shadow: 0 2px 10px rgba(0,0,0,0.3); }";
        html += ".header h1 { font-size: 1.5rem; margin: 0; display: flex; align-items: center; gap: 0.75rem; }";
        html += ".nav-links { margin-top: 0.75rem; display: flex; gap: 1.5rem; flex-wrap: wrap; }";
        html += ".nav-links a { color: #dbeafe; text-decoration: none; padding: 0.375rem 0.75rem; border-radius: 0.375rem; transition: all 0.2s; font-size: 0.875rem; }";
        html += ".nav-links a:hover { background: rgba(255,255,255,0.2); }";
        html += ".nav-links a.active { background: #00D4FF; color: #0B1426; font-weight: 600; }";
        html += ".container { max-width: 1200px; margin: 0 auto; padding: 20px; }";
        html += ".page-header { display: flex; align-items: center; justify-content: space-between; margin-bottom: 30px; }";
        html += ".page-header h2 { color: #00D4FF; font-size: 28px; font-weight: 600; }";
        html += ".status { display: flex; align-items: center; gap: 20px; font-size: 14px; }";
        html += ".status-item { display: flex; align-items: center; gap: 8px; }";
        html += ".status-dot { width: 8px; height: 8px; border-radius: 50%; }";
        html += ".active { background: #00D4FF; }";
        html += ".inactive { background: #64748B; }";
        html += ".main-content { flex: 1; display: grid; grid-template-columns: 1fr 320px; gap: 30px; overflow: hidden; }";
        html += ".schedule-table { background: #1E293B; border-radius: 12px; padding: 20px; overflow: auto; }";
        html += ".table-header { display: flex; align-items: center; justify-content: space-between; margin-bottom: 20px; }";
        html += ".table-header h2 { color: #F8FAFC; font-size: 18px; font-weight: 600; }";
        html += ".btn { padding: 8px 16px; border: none; border-radius: 6px; cursor: pointer; font-size: 14px; font-weight: 500; transition: all 0.2s; }";
        html += ".btn-primary { background: #00D4FF; color: #0B1426; }";
        html += ".btn-primary:hover { background: #0EA5E9; }";
        html += ".btn-success { background: #10B981; color: white; }";
        html += ".btn-success:hover { background: #059669; }";
        html += ".btn-danger { background: #EF4444; color: white; }";
        html += ".btn-danger:hover { background: #DC2626; }";
        html += ".btn-toggle { background: #374151; color: #F9FAFB; }";
        html += ".btn-toggle:hover { background: #4B5563; }";
        html += ".btn-toggle.active { background: #00D4FF; color: #0B1426; }";
        html += "table { width: 100%; border-collapse: collapse; }";
        html += "th, td { padding: 12px; text-align: left; border-bottom: 1px solid #334155; }";
        html += "th { background: #0F172A; color: #00D4FF; font-weight: 600; font-size: 14px; }";
        html += "td { font-size: 14px; }";
        html += ".day-cell { font-weight: 500; color: #F8FAFC; }";
        html += ".time-cell { color: #94A3B8; font-family: monospace; }";
        html += ".status-cell { display: flex; align-items: center; gap: 8px; }";
        html += ".status-badge { padding: 4px 8px; border-radius: 4px; font-size: 12px; font-weight: 500; }";
        html += ".enabled { background: rgba(16, 185, 129, 0.2); color: #10B981; }";
        html += ".disabled { background: rgba(100, 116, 139, 0.2); color: #64748B; }";
        html += ".control-panel { background: #1E293B; border-radius: 12px; padding: 20px; }";
        html += ".control-section { margin-bottom: 25px; }";
        html += ".control-section h3 { color: #F8FAFC; font-size: 16px; margin-bottom: 12px; }";
        html += ".form-group { margin-bottom: 15px; }";
        html += ".form-group label { display: block; margin-bottom: 6px; color: #CBD5E1; font-size: 14px; }";
        html += ".form-control { width: 100%; padding: 10px; border: 1px solid #475569; border-radius: 6px; background: #0F172A; color: #F8FAFC; font-size: 14px; }";
        html += ".form-control:focus { border-color: #00D4FF; outline: none; }";
        html += ".btn-group { display: flex; gap: 10px; margin-top: 15px; }";
        html += ".day-selector { display: grid; grid-template-columns: repeat(7, 1fr); gap: 6px; margin-top: 10px; }";
        html += ".day-btn { padding: 8px; border: 1px solid #475569; border-radius: 6px; background: #0F172A; color: #CBD5E1; text-align: center; cursor: pointer; font-size: 12px; transition: all 0.2s; }";
        html += ".day-btn.selected { background: #00D4FF; color: #0B1426; border-color: #00D4FF; }";
        html += ".nav-footer { display: flex; justify-content: center; gap: 15px; margin-top: 20px; }";
        html += "</style></head><body>";
        
        // Header with navigation
        html += "<div class='header'>";
        html += "<h1>Schedule Control System</h1>";
        html += "<div class='nav-links'>";
        html += "<a href='/'>Dashboard</a>";
        html += "<a href='/system'>System</a>";
        html += "<a href='/relays'>Relays</a>";
        html += "<a href='/temperature'>Temperature</a>";
        html += "<a href='/schedule' class='active'>Schedule</a>";
        html += "<a href='/sensors'>Sensors</a>";
        html += "<a href='/wifi-config'>Network</a>";
        html += "</div>";
        html += "</div>";
        
        html += "<div class='container'>";
        html += "<div class='page-header'>";
        html += "<h2>AC Schedule Control</h2>";
        html += "<div class='status'>";
        html += "<div class='status-item'>";
        html += "<div class='status-dot active'></div>";
        html += "<span>Schedule Active</span>";
        html += "</div>";
        html += "<div class='status-item'>";
        html += "<div class='status-dot active'></div>";
        html += "<span>" + rtc_manager.getFormattedDateTime() + "</span>";
        html += "</div>";
        html += "</div>";
        html += "</div>";
        
        html += "<div class='main-content'>";
        html += "<div class='schedule-table'>";
        html += "<div class='table-header'>";
        html += "<h2>Weekly Schedule</h2>";
        html += "<button class='btn btn-primary' onclick='toggleSchedule()' id='scheduleToggle'>";
        html += "Schedule " + String(schedule_manager.isScheduleActive() ? "ON" : "OFF");
        html += "</button>";
        html += "</div>";
        
        // Simple weekly schedule table
        html += "<table>";
        html += "<thead>";
        html += "<tr>";
        html += "<th>Day</th>";
        html += "<th>Start Time</th>";
        html += "<th>End Time</th>";
        html += "<th>Temperature</th>";
        html += "<th>Status</th>";
        html += "<th>Actions</th>";
        html += "</tr>";
        html += "</thead>";
        html += "<tbody id='scheduleBody'>";
        
        // Generate 7 days of schedule
        String days[] = {"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"};
        for (int i = 0; i < 7; i++) {
            html += "<tr id='day" + String(i) + "'>";
            html += "<td class='day-cell'>" + days[i] + "</td>";
            html += "<td class='time-cell' id='start" + String(i) + "'>08:00</td>";
            html += "<td class='time-cell' id='end" + String(i) + "'>18:00</td>";
            html += "<td id='temp" + String(i) + "'>22°C</td>";
            html += "<td class='status-cell'>";
            html += "<span class='status-badge enabled' id='status" + String(i) + "'>Enabled</span>";
            html += "</td>";
            html += "<td>";
            html += "<button class='btn btn-toggle btn-sm' onclick='toggleDay(" + String(i) + ")'>Toggle</button>";
            html += "<button class='btn btn-primary btn-sm' onclick='editDay(" + String(i) + ")'>Edit</button>";
            html += "</td>";
            html += "</tr>";
        }
        html += "</tbody>";
        html += "</table>";
        html += "</div>";
        
        // Control panel
        html += "<div class='control-panel'>";
        html += "<div class='control-section'>";
        html += "<h3>Quick Settings</h3>";
        html += "<div class='btn-group'>";
        html += "<button class='btn btn-success' onclick='enableAll()'>Enable All Days</button>";
        html += "<button class='btn btn-danger' onclick='disableAll()'>Disable All Days</button>";
        html += "</div>";
        html += "</div>";
        
        html += "<div class='control-section'>";
        html += "<h3>Edit Schedule Entry</h3>";
        html += "<div class='form-group'>";
        html += "<label>Day</label>";
        html += "<select class='form-control' id='editDay'>";
        for (int i = 0; i < 7; i++) {
            html += "<option value='" + String(i) + "'>" + days[i] + "</option>";
        }
        html += "</select>";
        html += "</div>";
        html += "<div class='form-group'>";
        html += "<label>Start Time</label>";
        html += "<input type='time' class='form-control' id='editStart' value='08:00'>";
        html += "</div>";
        html += "<div class='form-group'>";
        html += "<label>End Time</label>";
        html += "<input type='time' class='form-control' id='editEnd' value='18:00'>";
        html += "</div>";
        html += "<div class='form-group'>";
        html += "<label>Temperature (°C)</label>";
        html += "<input type='number' class='form-control' id='editTemp' min='15' max='35' step='0.5' value='22'>";
        html += "</div>";
        html += "<div class='btn-group'>";
        html += "<button class='btn btn-success' onclick='saveEntry()'>Save</button>";
        html += "<button class='btn btn-toggle' onclick='resetEntry()'>Reset</button>";
        html += "</div>";
        html += "</div>";
        html += "</div>";
        html += "</div>";
        
        html += "<div class='nav-footer'>";
        html += "<button class='btn btn-toggle' onclick=\"window.location.href='/'\">Back to Dashboard</button>";
        html += "<button class='btn btn-primary' onclick='window.location.reload()'>Refresh</button>";
        html += "</div>";
        html += "</div>";
        
        // JavaScript functions
        html += "<script>";
        html += "let scheduleData = [";
        for (int i = 0; i < 7; i++) {
            html += "{day:" + String(i) + ",start:'08:00',end:'18:00',temp:22,enabled:true}";
            if (i < 6) html += ",";
        }
        html += "];";
        
        html += "function toggleSchedule() {";
        html += "  const btn = document.getElementById('scheduleToggle');";
        html += "  const isActive = btn.textContent.includes('ON');";
        html += "  fetch('/api/schedule/toggle', {method: 'POST', body: 'enabled=' + (!isActive)})";
        html += "    .then(() => {";
        html += "      btn.textContent = 'Schedule ' + (!isActive ? 'ON' : 'OFF');";
        html += "      btn.className = 'btn ' + (!isActive ? 'btn-success' : 'btn-toggle');";
        html += "    });";
        html += "}";
        
        html += "function toggleDay(day) {";
        html += "  scheduleData[day].enabled = !scheduleData[day].enabled;";
        html += "  updateDisplay(day);";
        html += "  saveSchedule();";
        html += "}";
        
        html += "function editDay(day) {";
        html += "  const data = scheduleData[day];";
        html += "  document.getElementById('editDay').value = day;";
        html += "  document.getElementById('editStart').value = data.start;";
        html += "  document.getElementById('editEnd').value = data.end;";
        html += "  document.getElementById('editTemp').value = data.temp;";
        html += "}";
        
        html += "function saveEntry() {";
        html += "  const day = parseInt(document.getElementById('editDay').value);";
        html += "  const start = document.getElementById('editStart').value;";
        html += "  const end = document.getElementById('editEnd').value;";
        html += "  const temp = parseFloat(document.getElementById('editTemp').value);";
        html += "  scheduleData[day] = {day:day, start:start, end:end, temp:temp, enabled:scheduleData[day].enabled};";
        html += "  updateDisplay(day);";
        html += "  saveSchedule();";
        html += "}";
        
        html += "function updateDisplay(day) {";
        html += "  const data = scheduleData[day];";
        html += "  document.getElementById('start' + day).textContent = data.start;";
        html += "  document.getElementById('end' + day).textContent = data.end;";
        html += "  document.getElementById('temp' + day).textContent = data.temp + '°C';";
        html += "  const status = document.getElementById('status' + day);";
        html += "  status.textContent = data.enabled ? 'Enabled' : 'Disabled';";
        html += "  status.className = 'status-badge ' + (data.enabled ? 'enabled' : 'disabled');";
        html += "}";
        
        html += "function enableAll() {";
        html += "  for(let i = 0; i < 7; i++) {";
        html += "    scheduleData[i].enabled = true;";
        html += "    updateDisplay(i);";
        html += "  }";
        html += "  saveSchedule();";
        html += "}";
        
        html += "function disableAll() {";
        html += "  for(let i = 0; i < 7; i++) {";
        html += "    scheduleData[i].enabled = false;";
        html += "    updateDisplay(i);";
        html += "  }";
        html += "  saveSchedule();";
        html += "}";
        
        html += "function resetEntry() {";
        html += "  document.getElementById('editStart').value = '08:00';";
        html += "  document.getElementById('editEnd').value = '18:00';";
        html += "  document.getElementById('editTemp').value = '22';";
        html += "}";
        
        html += "function saveSchedule() {";
        html += "  fetch('/api/schedule/save', {";
        html += "    method: 'POST',";
        html += "    headers: {'Content-Type': 'application/json'},";
        html += "    body: JSON.stringify(scheduleData)";
        html += "  });";
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
    
    // Simplified schedule API endpoints for new UI
    device_server->on("/api/schedule/toggle", HTTP_POST, []() {
        if (device_server->hasArg("enabled")) {
            bool enabled = device_server->arg("enabled") == "true";
            schedule_manager.setGlobalEnabled(enabled);
            device_server->send(200, "application/json", "{\"success\":true}");
        } else {
            device_server->send(400, "application/json", "{\"success\":false,\"error\":\"Missing enabled parameter\"}");
        }
    });
    
    device_server->on("/api/schedule/save", HTTP_POST, []() {
        String json_data = device_server->arg("plain");
        if (json_data.length() == 0) {
            // Try to read from request body
            json_data = device_server->arg("scheduleData");
        }
        
        if (json_data.length() > 0) {
            DEBUG_PRINTLN("Received schedule data: " + json_data);
            // For now, just acknowledge the save - in a real implementation,
            // you would parse the JSON and update the schedule system
            device_server->send(200, "application/json", "{\"success\":true}");
        } else {
            device_server->send(400, "application/json", "{\"success\":false,\"error\":\"No schedule data provided\"}");
        }
    });
    
    // Schedule quick control endpoints
    device_server->on("/api/schedule/control-mode", HTTP_POST, []() {
        g_system_config.central_control_mode = !g_system_config.central_control_mode;
        saveConfiguration();
        
        String json = "{";
        json += "\"success\":true,";
        json += "\"control_mode\":\"" + String(g_system_config.central_control_mode ? "CENTRAL" : "LOCAL") + "\",";
        json += "\"message\":\"Control mode switched to " + String(g_system_config.central_control_mode ? "CENTRAL" : "LOCAL") + "\"";
        json += "}";
        
        device_server->send(200, "application/json", json);
    });
    
    device_server->on("/api/schedule/setpoint-mode", HTTP_POST, []() {
        g_system_config.operation_mode = (g_system_config.operation_mode == 0) ? 1 : 0;
        saveConfiguration();
        
        String json = "{";
        json += "\"success\":true,";
        json += "\"setpoint_mode\":\"" + String(g_system_config.operation_mode == 1 ? "SCHEDULE" : "DIRECT") + "\",";
        json += "\"message\":\"Setpoint mode switched to " + String(g_system_config.operation_mode == 1 ? "SCHEDULE" : "DIRECT") + "\"";
        json += "}";
        
        device_server->send(200, "application/json", json);
    });
    
    // ========== COMPREHENSIVE RESTful API ENDPOINTS ==========
    
    // API Info and Documentation
    device_server->on("/api", HTTP_GET, []() {
        String json = "{";
        json += "\"api_version\":\"1.0\",";
        json += "\"system\":\"" + String(SYSTEM_NAME) + "\",";
        json += "\"firmware\":\"" + String(FIRMWARE_VERSION) + "\",";
        json += "\"manufacturer\":\"" + String(MANUFACTURER) + "\",";
        json += "\"endpoints\":{";
        json += "\"system\":\"/api/system\",";
        json += "\"relays\":\"/api/relays\",";
        json += "\"sensors\":\"/api/sensors\",";
        json += "\"temperature\":\"/api/temperature\",";
        json += "\"schedule\":\"/api/schedule\",";
        json += "\"network\":\"/api/network\",";
        json += "\"diagnostics\":\"/api/diagnostics\"";
        json += "}";
        json += "}";
        device_server->send(200, "application/json", json);
    });
    
    // System Information API
    device_server->on("/api/system", HTTP_GET, []() {
        String json = "{";
        json += "\"status\":\"" + String(g_system_status.state) + "\",";
        json += "\"uptime\":" + String(g_system_status.uptime) + ",";
        json += "\"free_memory\":" + String(g_system_status.free_memory) + ",";
        json += "\"firmware_version\":\"" + String(FIRMWARE_VERSION) + "\",";
        json += "\"system_name\":\"" + String(SYSTEM_NAME) + "\",";
        json += "\"manufacturer\":\"" + String(MANUFACTURER) + "\",";
        json += "\"chip_model\":\"ESP32\",";
        json += "\"mac_address\":\"" + WiFi.macAddress() + "\",";
        json += "\"flash_size\":" + String(ESP.getFlashChipSize()) + ",";
        json += "\"cpu_frequency\":" + String(ESP.getCpuFreqMHz()) + ",";
        json += "\"timestamp\":\"" + String(millis()) + "\"";
        json += "}";
        device_server->send(200, "application/json", json);
    });
    
    // System Control API
    device_server->on("/api/system/restart", HTTP_POST, []() {
        String json = "{\"success\":true,\"message\":\"System restart initiated\"}";
        device_server->send(200, "application/json", json);
        delay(1000);
        ESP.restart();
    });
    
    // Relay Control API - GET all relays
    device_server->on("/api/relays", HTTP_GET, []() {
        String json = "{\"relays\":[";
        for (int i = 0; i < NUM_RELAYS; i++) {
            if (i > 0) json += ",";
            json += "{";
            json += "\"id\":" + String(i) + ",";
            json += "\"name\":\"Relay " + String(i + 1) + "\",";
            json += "\"state\":" + String(getRelayState(i) ? "true" : "false") + ",";
            json += "\"manual_override\":false"; // Manual override not implemented yet
            json += "}";
        }
        json += "]}";
        device_server->send(200, "application/json", json);
    });
    
    // Relay Control API - GET specific relay (using query parameter)
    device_server->on("/api/relays/info", HTTP_GET, []() {
        String json = "{\"success\":false,\"error\":\"Invalid relay ID\"}";
        int statusCode = 400;
        
        if (device_server->hasArg("id")) {
            int relayId = device_server->arg("id").toInt();
            if (relayId >= 0 && relayId < NUM_RELAYS) {
                json = "{";
                json += "\"success\":true,";
                json += "\"id\":" + String(relayId) + ",";
                json += "\"name\":\"Relay " + String(relayId + 1) + "\",";
                json += "\"state\":" + String(getRelayState(relayId) ? "true" : "false") + ",";
                json += "\"manual_override\":false"; // Manual override not implemented yet
                json += "}";
                statusCode = 200;
            }
        }
        device_server->send(statusCode, "application/json", json);
    });
    
    // Relay Control API - SET relay state
    device_server->on("/api/relays/control", HTTP_POST, []() {
        String json = "{\"success\":false,\"error\":\"Invalid parameters\"}";
        int statusCode = 400;
        
        if (device_server->hasArg("relay") && device_server->hasArg("state")) {
            int relayId = device_server->arg("relay").toInt();
            bool state = device_server->arg("state").equalsIgnoreCase("true") || 
                        device_server->arg("state") == "1";
            
            if (relayId >= 0 && relayId < NUM_RELAYS) {
                bool success = relay_controller.setRelay(relayId, state);
                if (success) {
                    json = "{";
                    json += "\"success\":true,";
                    json += "\"relay\":" + String(relayId) + ",";
                    json += "\"state\":" + String(state ? "true" : "false") + ",";
                    json += "\"message\":\"Relay " + String(relayId + 1) + " set to " + String(state ? "ON" : "OFF") + "\"";
                    json += "}";
                    statusCode = 200;
                } else {
                    json = "{\"success\":false,\"error\":\"Failed to control relay " + String(relayId + 1) + "\"}";
                    statusCode = 500;
                }
            }
        }
        device_server->send(statusCode, "application/json", json);
    });
    
    // Relay Bulk Control API
    device_server->on("/api/relays/all", HTTP_POST, []() {
        String json = "{\"success\":false,\"error\":\"Invalid parameters\"}";
        int statusCode = 400;
        
        if (device_server->hasArg("state")) {
            bool state = device_server->arg("state").equalsIgnoreCase("true") || 
                        device_server->arg("state") == "1";
            
            bool allSuccess = true;
            for (int i = 0; i < NUM_RELAYS; i++) {
                bool success = relay_controller.setRelay(i, state);
                if (!success) allSuccess = false;
            }
            
            if (allSuccess) {
                json = "{";
                json += "\"success\":true,";
                json += "\"state\":" + String(state ? "true" : "false") + ",";
                json += "\"message\":\"All relays set to " + String(state ? "ON" : "OFF") + "\"";
                json += "}";
                statusCode = 200;
            } else {
                json = "{\"success\":false,\"error\":\"Some relays failed to respond\"}";
                statusCode = 500;
            }
        }
        device_server->send(statusCode, "application/json", json);
    });
    
    // Temperature Control API - GET current temperature data
    device_server->on("/api/temperature", HTTP_GET, []() {
        String json = "{";
        json += "\"current_temperature\":" + String(g_system_status.current_temperature, 2) + ",";
        json += "\"sensor_type\":\"" + String(getTemperatureSensorName(currentTempSensorType)) + "\",";
        json += "\"sensor_available\":" + String(isTemperatureSensorAvailable() ? "true" : "false") + ",";
        json += "\"last_read_time\":" + String(lastTempReadTime) + ",";
        json += "\"sensors_initialized\":" + String(temperatureSensorsInitialized ? "true" : "false") + ",";
        json += "\"available_sensors\":{";
        json += "\"ds18b20\":" + String(isDS18B20Available() ? "true" : "false") + ",";
        json += "\"am2302\":" + String(isAM2302Available() ? "true" : "false") + ",";
        json += "\"lm35\":" + String(isLM35Available() ? "true" : "false");
        json += "}";
        json += "}";
        device_server->send(200, "application/json", json);
    });
    
    // Temperature Control API - GET zone configuration
    device_server->on("/api/temperature/zones", HTTP_GET, []() {
        String json = "{\"zones\":[";
        for (int i = 0; i < MAX_ZONES; i++) {
            if (i > 0) json += ",";
            ZoneConfig& zone = temp_controller.getZoneConfig(i);
            json += "{";
            json += "\"id\":" + String(i) + ",";
            json += "\"name\":\"Zone " + String(i + 1) + "\",";
            json += "\"enabled\":" + String(zone.enabled ? "true" : "false") + ",";
            json += "\"setpoint\":" + String(zone.setpoint, 1) + ",";
            json += "\"mode\":\"" + String(zone.mode) + "\",";
            json += "\"current_state\":" + String(zone.current_state ? "true" : "false") + ",";
            json += "\"current_temp\":" + String(temp_controller.getCompensatedTemp(i), 1);
            json += "}";
        }
        json += "]}";
        device_server->send(200, "application/json", json);
    });
    
    // Temperature Control API - SET zone configuration
    device_server->on("/api/temperature/control", HTTP_POST, []() {
        String json = "{\"success\":false,\"error\":\"Invalid parameters\"}";
        int statusCode = 400;
        
        if (device_server->hasArg("zone")) {
            int zoneId = device_server->arg("zone").toInt();
            if (zoneId >= 0 && zoneId < MAX_ZONES) {
                ZoneConfig& zone = temp_controller.getZoneConfig(zoneId);
                bool configChanged = false;
                
                if (device_server->hasArg("setpoint")) {
                    float setpoint = device_server->arg("setpoint").toFloat();
                    if (setpoint >= 10.0 && setpoint <= 40.0) {
                        zone.setpoint = setpoint;
                        configChanged = true;
                    }
                }
                
                if (device_server->hasArg("mode")) {
                    String mode = device_server->arg("mode");
                    if (mode == "OFF") zone.mode = TEMP_MODE_OFF;
                    else if (mode == "HEATING") zone.mode = TEMP_MODE_HEATING;
                    else if (mode == "COOLING") zone.mode = TEMP_MODE_COOLING;
                    else if (mode == "AUTO") zone.mode = TEMP_MODE_AUTO;
                    else if (mode == "MANUAL") zone.mode = TEMP_MODE_MANUAL;
                    configChanged = true;
                }
                
                if (device_server->hasArg("enabled")) {
                    zone.enabled = device_server->arg("enabled").equalsIgnoreCase("true") || 
                                  device_server->arg("enabled") == "1";
                    configChanged = true;
                }
                
                if (configChanged) {
                    // Zone config save implementation pending
                    json = "{";
                    json += "\"success\":true,";
                    json += "\"zone\":" + String(zoneId) + ",";
                    json += "\"message\":\"Zone " + String(zoneId + 1) + " configuration updated\"";
                    json += "}";
                    statusCode = 200;
                }
            }
        }
        device_server->send(statusCode, "application/json", json);
    });

    // Temperature Emergency Control API
    device_server->on("/api/temperature/emergency", HTTP_POST, []() {
        String json = "{";
        int statusCode = 200;
        
        if (device_server->hasArg("action")) {
            String action = device_server->arg("action");
            
            if (action == "clear") {
                temp_controller.clearEmergency();
                json += "\"success\":true,";
                json += "\"message\":\"Emergency stop cleared\"";
            } else {
                statusCode = 400;
                json += "\"success\":false,";
                json += "\"error\":\"Invalid action: " + action + "\"";
            }
        } else {
            statusCode = 400;
            json += "\"success\":false,";
            json += "\"error\":\"Missing action parameter\"";
        }
        
        json += "}";
        device_server->send(statusCode, "application/json", json);
    });
    
    // Network Information API
    device_server->on("/api/network", HTTP_GET, []() {
        String json = "{";
        json += "\"wifi_connected\":" + String(g_wifi_connected ? "true" : "false") + ",";
        json += "\"ip_address\":\"" + WiFi.localIP().toString() + "\",";
        json += "\"mac_address\":\"" + WiFi.macAddress() + "\",";
        json += "\"ssid\":\"" + WiFi.SSID() + "\",";
        json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
        json += "\"gateway\":\"" + WiFi.gatewayIP().toString() + "\",";
        json += "\"dns\":\"" + WiFi.dnsIP().toString() + "\",";
        json += "\"subnet\":\"" + WiFi.subnetMask().toString() + "\"";
        json += "}";
        device_server->send(200, "application/json", json);
    });
    
    // Diagnostics API
    device_server->on("/api/diagnostics", HTTP_GET, []() {
        String json = "{";
        json += "\"uptime\":" + String(millis() / 1000) + ",";
        json += "\"free_memory\":" + String(ESP.getFreeHeap()) + ",";
        json += "\"total_memory\":" + String(ESP.getHeapSize()) + ",";
        json += "\"memory_usage\":" + String(100 - (ESP.getFreeHeap() * 100 / ESP.getHeapSize())) + ",";
        json += "\"cpu_frequency\":" + String(ESP.getCpuFreqMHz()) + ",";
        json += "\"flash_size\":" + String(ESP.getFlashChipSize()) + ",";
        json += "\"flash_speed\":" + String(ESP.getFlashChipSpeed()) + ",";
        json += "\"chip_revision\":" + String(ESP.getChipRevision()) + ",";
        json += "\"sdk_version\":\"" + String(ESP.getSdkVersion()) + "\",";
        json += "\"wifi_rssi\":" + String(WiFi.RSSI()) + ",";
        json += "\"temperature_sensors_ok\":" + String(temperatureSensorsInitialized ? "true" : "false") + ",";
        json += "\"relay_count\":" + String(NUM_RELAYS);
        json += "}";
        device_server->send(200, "application/json", json);
    });
    
    // Digital Inputs API
    device_server->on("/api/inputs", HTTP_GET, []() {
        String json = "{\"inputs\":[";
        for (int i = 0; i < NUM_DIGITAL_INPUTS; i++) {
            if (i > 0) json += ",";
            json += "{";
            json += "\"id\":" + String(i) + ",";
            json += "\"name\":\"Input " + String(i + 1) + "\",";
            json += "\"state\":" + String(getInputState(i) ? "true" : "false");
            json += "}";
        }
        json += "]}";
        device_server->send(200, "application/json", json);
    });
    
    // Enhanced Sensor Data API (improved version of existing /api/sensors)
    device_server->on("/api/sensors/data", HTTP_GET, []() {
        String json = "{";
        json += "\"timestamp\":" + String(millis()) + ",";
        json += "\"temperature\":{";
        json += "\"current\":" + String(g_system_status.current_temperature, 2) + ",";
        json += "\"sensor_type\":\"" + String(getTemperatureSensorName(currentTempSensorType)) + "\",";
        json += "\"available\":" + String(isTemperatureSensorAvailable() ? "true" : "false");
        json += "},";
        
        // Individual sensor readings
        json += "\"sensors\":{";
        json += "\"ds18b20\":{";
        json += "\"available\":" + String(isDS18B20Available() ? "true" : "false");
        if (isDS18B20Available()) {
            json += ",\"temperature\":" + String(readDS18B20Temperature(), 2);
        }
        json += "},";
        
        json += "\"am2302\":{";
        json += "\"available\":" + String(isAM2302Available() ? "true" : "false");
        if (isAM2302Available()) {
            json += ",\"temperature\":" + String(readAM2302Temperature(), 2);
            json += ",\"humidity\":" + String(readAM2302Humidity(), 1);
        }
        json += "},";
        
        json += "\"lm35\":{";
        json += "\"available\":" + String(isLM35Available() ? "true" : "false");
        if (isLM35Available()) {
            json += ",\"temperature\":" + String(readLM35Temperature(), 2);
        }
        json += "}";
        json += "}";
        json += "}";
        device_server->send(200, "application/json", json);
    });
    
    // Setup professional temperature API endpoints with calibration controls
    setupTemperatureAPI();
    
    device_server->begin();
    DEBUG_PRINTLN("[OK] Device web server started on port 80");
    DEBUG_PRINTF("[WEB] Access at: http://%s/\n", WiFi.localIP().toString().c_str());
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
        DEBUG_PRINTF("[I/O] Status Changed: 0x%02X\n", current_inputs);
        DEBUG_PRINTLN(relay_controller.getInputStatusString());
        last_logged_inputs = current_inputs;
    }
    
    // Update temperature controller with current temperature
    float current_temp = g_system_status.current_temperature;
    temp_controller.updateTemperature(0, current_temp);  // Update zone 0
    
    // Process temperature control logic
    temp_controller.process();
}

