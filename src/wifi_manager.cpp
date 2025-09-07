#include "wifi_manager.h"
#include "display.h"
#include "hardware.h"
#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>
#include <DNSServer.h>

// WiFi Manager - Complete Implementation

// State variables
static bool wifi_manager_active = false;
static bool config_mode_active = false;
static bool wifi_connected = false;
static unsigned long config_mode_start_time = 0;
static unsigned long last_reconnect_attempt = 0;
static unsigned long last_connection_check = 0;
static int reconnect_attempts = 0;

// Configuration constants
static const unsigned long RECONNECT_INTERVAL = 30000; // 30 seconds
static const unsigned long CONFIG_TIMEOUT = 300000;   // 5 minutes
static const unsigned long CONNECTION_CHECK_INTERVAL = 10000; // 10 seconds
static const int MAX_RECONNECT_ATTEMPTS = 5;

// Web server and DNS for captive portal
WebServer* config_server = nullptr;  // Made global for portal access
static DNSServer* dns_server = nullptr;

// WiFi credentials storage
struct WiFiCredentials {
    char ssid[64];
    char password[64];
    uint16_t checksum;
};

// EEPROM addresses for WiFi credentials
#define WIFI_CREDENTIALS_ADDR 100

// External function from main.cpp
extern uint16_t calculateChecksum(const void* data, size_t len);

bool saveWiFiCredentials(const char* ssid, const char* password) {
    WiFiCredentials creds;
    memset(&creds, 0, sizeof(creds));
    
    strncpy(creds.ssid, ssid, sizeof(creds.ssid) - 1);
    strncpy(creds.password, password, sizeof(creds.password) - 1);
    creds.checksum = calculateChecksum(&creds, sizeof(creds) - sizeof(creds.checksum));
    
    EEPROM.put(WIFI_CREDENTIALS_ADDR, creds);
    bool success = EEPROM.commit();
    
    if (!success) DEBUG_PRINTLN("ERROR: WiFi credentials save failed");
    return success;
}

bool loadWiFiCredentials(char* ssid, char* password) {
    WiFiCredentials creds;
    EEPROM.get(WIFI_CREDENTIALS_ADDR, creds);
    
    // Verify checksum
    uint16_t calculated = calculateChecksum(&creds, sizeof(creds) - sizeof(creds.checksum));
    if (calculated != creds.checksum) {
        // No saved credentials
        return false;
    }
    
    strcpy(ssid, creds.ssid);
    strcpy(password, creds.password);
    // Credentials loaded
    return true;
}

bool initializeWiFiManager() {
    DEBUG_PRINTLN("WiFi Manager starting...");
    
    // Set WiFi mode
    WiFi.mode(WIFI_STA);
    wifi_manager_active = true;
    
    // Try to connect with saved credentials
    char saved_ssid[64], saved_password[64];
    if (loadWiFiCredentials(saved_ssid, saved_password)) {
        DEBUG_PRINTF("Connecting to: %s\n", saved_ssid);
        if (attemptWiFiConnection(saved_ssid, saved_password)) {
            wifi_connected = true;
            DEBUG_PRINTLN("WiFi connected");
            return true;
        }
    }
    
    // No saved credentials or connection failed
    DEBUG_PRINTLN("Starting WiFi config mode");
    autoStartConfigMode();
    
    return true;
}

void startConfigurationMode() {
    if (!wifi_manager_active || config_mode_active) {
        return;
    }
    
    DEBUG_PRINTLN("Starting WiFi configuration mode...");
    config_mode_active = true;
    config_mode_start_time = millis();
    
    // Start Access Point
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(DEFAULT_AP_SSID, DEFAULT_AP_PASSWORD);
    
    IPAddress ap_ip = WiFi.softAPIP();
    DEBUG_PRINTF("Access Point started: %s\n", DEFAULT_AP_SSID);
    DEBUG_PRINTF("AP IP address: %s\n", ap_ip.toString().c_str());
    
    // Start DNS server for captive portal
    if (!dns_server) {
        dns_server = new DNSServer();
    }
    dns_server->start(53, "*", ap_ip);
    
    // Start web server
    if (!config_server) {
        config_server = new WebServer(80);
        setupConfigPortalRoutes();
    }
    config_server->begin();
    
    // Update display
    setDisplayScreen(SCREEN_WIFI_SETUP);
    showWiFiSetup(DEFAULT_AP_SSID, ap_ip.toString().c_str());
    
    DEBUG_PRINTLN("Configuration mode started - connect to WiFi and browse to setup");
}

void stopConfigurationMode() {
    if (!config_mode_active) return;
    
    DEBUG_PRINTLN("Stopping WiFi configuration mode...");
    config_mode_active = false;
    
    // Stop servers
    if (config_server) {
        config_server->stop();
    }
    if (dns_server) {
        dns_server->stop();
    }
    
    // Stop Access Point
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    
    // Update display
    setDisplayScreen(SCREEN_WIFI_STATUS);
    DEBUG_PRINTLN("Configuration mode stopped");
}

void handleWiFiManager() {
    if (!wifi_manager_active) return;
    
    // Handle configuration mode
    if (config_mode_active) {
        // Handle client requests
        if (dns_server) {
            dns_server->processNextRequest();
        }
        if (config_server) {
            config_server->handleClient();
        }
        
        // Handle timeout
        if (millis() - config_mode_start_time > CONFIG_TIMEOUT) {
            DEBUG_PRINTLN("Configuration mode timeout");
            stopConfigurationMode();
        }
        return;
    }
    
    // Check WiFi connection status
    if (millis() - last_connection_check > CONNECTION_CHECK_INTERVAL) {
        last_connection_check = millis();
        
        if (WiFi.status() == WL_CONNECTED) {
            if (!wifi_connected) {
                wifi_connected = true;
                reconnect_attempts = 0;
                DEBUG_PRINTF("WiFi connected: %s (IP: %s, RSSI: %d dBm)\n", 
                           WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(), WiFi.RSSI());
                setDisplayScreen(SCREEN_WIFI_STATUS);
            }
        } else {
            if (wifi_connected) {
                wifi_connected = false;
                DEBUG_PRINTLN("WiFi connection lost");
            }
            
            // Attempt reconnection
            if (millis() - last_reconnect_attempt > RECONNECT_INTERVAL) {
                last_reconnect_attempt = millis();
                reconnect_attempts++;
                
                if (reconnect_attempts <= MAX_RECONNECT_ATTEMPTS) {
                    DEBUG_PRINTF("WiFi reconnection attempt %d/%d\n", reconnect_attempts, MAX_RECONNECT_ATTEMPTS);
                    handleWiFiReconnection();
                } else {
                    DEBUG_PRINTLN("Max reconnection attempts reached - starting config mode");
                    reconnect_attempts = 0;
                    autoStartConfigMode();
                }
            }
        }
    }
}

bool isConfigurationModeActive() {
    return config_mode_active;
}

void autoStartConfigMode() {
    // Auto-start configuration mode since we can't check actual WiFi status
    if (!wifi_connected) {
        DEBUG_PRINTLN("Auto-starting configuration mode (compatibility mode)");
        startConfigurationMode();
    }
}

// Network utility functions - compatibility stubs
bool isWiFiConnected() {
    return wifi_connected; // Always false in compatibility mode
}

int getCurrentRSSI() {
    return -100; // Weak signal indicator for compatibility mode
}

String getCurrentSSID() {
    return String("N/A"); // No SSID in compatibility mode
}

NetworkStatus getNetworkStatus() {
    if (config_mode_active) {
        return NET_AP_MODE;
    } else {
        return NET_DISCONNECTED;
    }
}

// Configuration portal routes
void setupConfigPortalRoutes() {
    if (!config_server) return;
    
    config_server->on("/", handleConfigRoot);
    config_server->on("/scan", handleWiFiScan);
    config_server->on("/save", HTTP_POST, handleConfigSave);
    config_server->on("/status", handleConfigStatus);
    config_server->on("/reset", handleConfigReset);
    config_server->onNotFound(handleConfigRoot); // Captive portal
    
    DEBUG_PRINTLN("Configuration portal routes setup complete");
}

bool attemptWiFiConnection(const char* ssid, const char* password) {
    DEBUG_PRINTF("Connecting to WiFi: %s\\n", ssid);
    
    WiFi.begin(ssid, password);
    
    // Wait for connection with timeout
    unsigned long connect_start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - connect_start < WIFI_CONNECT_TIMEOUT) {
        delay(500);
        DEBUG_PRINT(".");
    }
    DEBUG_PRINTLN();
    
    if (WiFi.status() == WL_CONNECTED) {
        DEBUG_PRINTF("WiFi connected successfully: %s (IP: %s)\\n", 
                   WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
        return true;
    } else {
        DEBUG_PRINTF("WiFi connection failed: status=%d\\n", WiFi.status());
        return false;
    }
}

void handleWiFiReconnection() {
    char saved_ssid[64], saved_password[64];
    if (loadWiFiCredentials(saved_ssid, saved_password)) {
        DEBUG_PRINTF("Reconnecting to: %s\\n", saved_ssid);
        WiFi.begin(saved_ssid, saved_password);
    } else {
        DEBUG_PRINTLN("No saved credentials for reconnection");
    }
}

// WiFi initialization
void startWiFi() {
    DEBUG_PRINTLN("Starting WiFi system...");
    initializeWiFiManager();
}