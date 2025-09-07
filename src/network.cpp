#include "network.h"
#include <time.h>

// Network state variables - ESP32 v3.2.0 Compatible Version
static NetworkStatus current_network_status = NET_DISCONNECTED;
static bool auto_reconnect_enabled = true;
static bool mdns_active = false;
static bool g_wifi_connected = false;

// Connection tracking variables
static String last_ssid = "";
static String last_password = "";
static bool use_static_config = false;

// Statistics variables
static unsigned long last_connection_attempt = 0;
static unsigned long wifi_disconnect_time = 0;
static unsigned long total_connection_time = 0;
static uint16_t successful_connections = 0;
static uint16_t failed_connections = 0;
static uint8_t reconnect_attempts = 0;

bool initializeNetworkStack() {
    DEBUG_PRINTLN("Initializing network stack...");
    
    current_network_status = NET_DISCONNECTED;
    g_wifi_connected = false;
    mdns_active = false;
    
    DEBUG_PRINTLN("Network stack initialized successfully");
    return true;
}

bool connectToWiFi(const char* ssid, const char* password, bool use_dhcp) {
    if (!ssid || strlen(ssid) == 0) {
        DEBUG_PRINTLN("ERROR: Invalid SSID");
        return false;
    }
    
    DEBUG_PRINTF("Attempting to connect to WiFi: %s\n", ssid);
    last_ssid = String(ssid);
    last_password = String(password);
    use_static_config = !use_dhcp;
    last_connection_attempt = millis();
    
    // WiFi connection will be implemented when ESP32 core compatibility is resolved
    DEBUG_PRINTLN("WiFi connection deferred - compatibility mode active");
    return false;
}

bool connectToWiFiWithConfig(const NetworkConfig& network_config) {
    return connectToWiFi(network_config.ssid, network_config.password, network_config.use_dhcp);
}

bool disconnectWiFi() {
    DEBUG_PRINTLN("Disconnecting WiFi...");
    g_wifi_connected = false;
    current_network_status = NET_DISCONNECTED;
    wifi_disconnect_time = millis();
    stopMDNS();
    DEBUG_PRINTLN("WiFi disconnected");
    return true;
}

// isWiFiConnected() moved to wifi_manager.cpp

// getNetworkStatus() moved to wifi_manager.cpp

String getNetworkStatusString(NetworkStatus status) {
    switch (status) {
        case NET_DISCONNECTED: return "Disconnected";
        case NET_CONNECTING: return "Connecting";
        case NET_CONNECTED_STA: return "Connected (Station)";
        case NET_AP_MODE: return "Access Point";
        case NET_ERROR: return "Error";
        default: return "Unknown";
    }
}

// getCurrentSSID() moved to wifi_manager.cpp

// getCurrentRSSI() moved to wifi_manager.cpp

bool startMDNS(const char* hostname) {
    if (!hostname || strlen(hostname) == 0) {
        DEBUG_PRINTLN("ERROR: Invalid hostname for mDNS");
        return false;
    }
    
    DEBUG_PRINTF("Starting mDNS service with hostname: %s\n", hostname);
    // mDNS will be implemented when WiFi libraries are compatible
    mdns_active = false; // Simulated for now
    DEBUG_PRINTLN("mDNS service deferred - compatibility mode active");
    return false;
}

void stopMDNS() {
    if (mdns_active) {
        DEBUG_PRINTLN("Stopping mDNS service");
        mdns_active = false;
    }
}

bool isMDNSActive() {
    return mdns_active;
}

void enableAutoReconnect(bool enable) {
    auto_reconnect_enabled = enable;
    DEBUG_PRINTF("Auto-reconnect %s\n", enable ? "enabled" : "disabled");
}

bool isAutoReconnectEnabled() {
    return auto_reconnect_enabled;
}

void handleNetworkEvents() {
    // Handle auto-reconnect logic
    if (auto_reconnect_enabled && !g_wifi_connected && 
        current_network_status == NET_DISCONNECTED && 
        last_ssid.length() > 0) {
        
        unsigned long now = millis();
        if (now - last_connection_attempt > 30000) { // Try every 30 seconds
            DEBUG_PRINTLN("Attempting auto-reconnect...");
            connectToWiFi(last_ssid.c_str(), last_password.c_str(), !use_static_config);
        }
    }
}

// Network utility functions
void resetNetworkStatistics() {
    DEBUG_PRINTLN("Resetting network statistics");
    successful_connections = 0;
    failed_connections = 0;
    reconnect_attempts = 0;
    total_connection_time = 0;
}

NetworkStatistics getNetworkStatistics() {
    NetworkStatistics stats;
    stats.successful_connections = successful_connections;
    stats.failed_connections = failed_connections;
    stats.reconnect_attempts = reconnect_attempts;
    stats.total_uptime = millis();
    stats.current_session_time = g_wifi_connected ? (millis() - last_connection_attempt) : 0;
    stats.average_connection_time = successful_connections > 0 ? (total_connection_time / successful_connections) : 0;
    stats.last_disconnect_time = wifi_disconnect_time;
    
    return stats;
}

// Event callbacks (simplified for compatibility)
void onWiFiConnected() {
    DEBUG_PRINTLN("WiFi event: Connected");
    g_wifi_connected = true;
    current_network_status = NET_CONNECTED_STA;
    successful_connections++;
    reconnect_attempts = 0;
}

void onWiFiDisconnected() {
    DEBUG_PRINTLN("WiFi event: Disconnected");
    g_wifi_connected = false;
    current_network_status = NET_DISCONNECTED;
    wifi_disconnect_time = millis();
    stopMDNS();
}

void onWiFiGotIP() {
    DEBUG_PRINTLN("WiFi event: Got IP - deferred in compatibility mode");
    // IP information will be available when WiFi libraries are compatible
}

// Network diagnostics (simplified)
bool pingGateway() {
    DEBUG_PRINTLN("Gateway ping test deferred - compatibility mode active");
    return false; // Will implement when WiFi libraries are compatible
}

bool checkInternetConnectivity() {
    DEBUG_PRINTLN("Internet connectivity test deferred - compatibility mode active");
    return false; // Will implement when WiFi libraries are compatible
}

void performNetworkDiagnostics() {
    DEBUG_PRINTLN("=== Network Diagnostics ===");
    DEBUG_PRINTF("Status: %s\n", getNetworkStatusString(current_network_status).c_str());
    DEBUG_PRINTF("Auto-reconnect: %s\n", auto_reconnect_enabled ? "Enabled" : "Disabled");
    DEBUG_PRINTF("mDNS: %s\n", mdns_active ? "Active" : "Inactive");
    DEBUG_PRINTF("Last SSID: %s\n", last_ssid.c_str());
    DEBUG_PRINTF("Connection attempts: %d successful, %d failed\n", successful_connections, failed_connections);
    DEBUG_PRINTLN("===========================");
}

// Configuration management
bool saveNetworkConfig(const NetworkConfig& config) {
    DEBUG_PRINTLN("Saving network configuration to EEPROM");
    g_system_config.network = config;
    // EEPROM save will be implemented when system config functions are available
    return true;
}

NetworkConfig loadNetworkConfig() {
    DEBUG_PRINTLN("Loading network configuration from EEPROM");
    // EEPROM load will be implemented when system config functions are available
    return g_system_config.network;
}

void printNetworkConfig(const NetworkConfig& config) {
    DEBUG_PRINTLN("=== Network Configuration ===");
    DEBUG_PRINTF("SSID: %s\n", config.ssid);
    DEBUG_PRINTF("Use DHCP: %s\n", config.use_dhcp ? "Yes" : "No");
    DEBUG_PRINTF("Hostname: %s\n", config.hostname);
    if (!config.use_dhcp) {
        DEBUG_PRINTF("Static IP: %d.%d.%d.%d\n", 
                    (int)((config.ip_address >> 0) & 0xFF),
                    (int)((config.ip_address >> 8) & 0xFF),
                    (int)((config.ip_address >> 16) & 0xFF),
                    (int)((config.ip_address >> 24) & 0xFF));
        DEBUG_PRINTF("Gateway: %d.%d.%d.%d\n",
                    (int)((config.gateway >> 0) & 0xFF),
                    (int)((config.gateway >> 8) & 0xFF),
                    (int)((config.gateway >> 16) & 0xFF),
                    (int)((config.gateway >> 24) & 0xFF));
    }
    DEBUG_PRINTLN("=============================");
}

String getNetworkInfo() {
    String info = "Network Status:\n";
    info += "Status: " + getNetworkStatusString(current_network_status) + "\n";
    
    if (g_wifi_connected) {
        info += "SSID: " + last_ssid + "\n";
        info += "Signal: " + String(getCurrentRSSI()) + " dBm\n";
    }
    
    info += "Auto-reconnect: " + String(auto_reconnect_enabled ? "On" : "Off") + "\n";
    info += "mDNS: " + String(mdns_active ? "Active" : "Inactive") + "\n";
    
    info += "Connections: " + String(successful_connections) + " successful\n";
    info += "Uptime: " + String(millis() / 1000) + " seconds\n";
    
    return info;
}

// Compatibility alias functions
void setAutoReconnectEnabled(bool enable) {
    enableAutoReconnect(enable);
}

void handleAutoReconnect() {
    handleNetworkEvents();
}