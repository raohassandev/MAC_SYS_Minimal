#include "wifi_professional.h"
#include "display.h"
#include <esp_ping.h>
#include <ESPmDNS.h>

// Global instance
ProfessionalWiFi wifiPro;

// Static callback functions for WiFiManager integration
static void configModeCallback(WiFiManager *myWiFiManager) {
    Serial.println("[WiFiPro] Entered config mode");
    Serial.print("[WiFiPro] AP IP: ");
    Serial.println(WiFi.softAPIP());
    Serial.print("[WiFiPro] AP SSID: ");
    Serial.println(myWiFiManager->getConfigPortalSSID());
    
    // Update display if available
    setDisplayScreen(SCREEN_WIFI_SETUP);
    showWiFiSetup(myWiFiManager->getConfigPortalSSID().c_str(), 
                  WiFi.softAPIP().toString().c_str());
    
    // Call user callback if set
    if (wifiPro.on_config_mode_callback) {
        wifiPro.on_config_mode_callback(myWiFiManager);
    }
}

static void saveConfigCallback() {
    Serial.println("[WiFiPro] Configuration saved, connection established");
    wifiPro.onWiFiConnected();
}

// Constructor and initialization
bool ProfessionalWiFi::begin() {
    Serial.println("[WiFiPro] Initializing Professional WiFi Manager");
    
    portal_active = false;
    connection_established = false;
    last_connection_check = 0;
    connection_start_time = 0;
    connection_established_time = 0;
    
    // Configure WiFiManager callbacks
    wm.setAPCallback(configModeCallback);
    wm.setSaveConfigCallback(saveConfigCallback);
    
    // Configure WiFiManager parameters
    wm.setConfigPortalTimeout(300);        // 5 minute timeout
    wm.setConnectTimeout(20);              // 20 second connect timeout
    wm.setMinimumSignalQuality(15);        // Minimum 15% signal quality
    wm.setBreakAfterConfig(true);          // Exit after config
    // wm.setScanDispOptions(true);           // Show SSID, RSSI, encryption - method may not exist in this version
    wm.setShowInfoUpdate(false);           // Don't show info page
    wm.setShowInfoErase(false);            // Don't show erase button on info
    
    // Try to connect with saved credentials or start portal
    connection_start_time = millis();
    
    Serial.println("[WiFiPro] Attempting auto-connect...");
    if (!wm.autoConnect("MAC-SYS-Setup")) {
        Serial.println("[WiFiPro] Failed to connect - continuing in AP mode");
        portal_active = true;
        return false;  // Indicates AP mode
    }
    
    Serial.println("[WiFiPro] Connected successfully!");
    Serial.print("[WiFiPro] IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("[WiFiPro] SSID: ");
    Serial.println(WiFi.SSID());
    Serial.print("[WiFiPro] Signal Strength: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
    
    connection_established = true;
    connection_established_time = millis();
    // Reduce latency for HTTP by disabling modem sleep
    WiFi.setSleep(false);
    onWiFiConnected();
    
    return true;  // Indicates STA mode
}

void ProfessionalWiFi::process() {
    // Handle WiFiManager processing
    if (portal_active) {
        wm.process();
    }
    
    // Check connection status periodically
    if (millis() - last_connection_check > 10000) {  // Every 10 seconds
        updateConnectionStatus();
        last_connection_check = millis();
    }
}

void ProfessionalWiFi::updateConnectionStatus() {
    bool currently_connected = (WiFi.status() == WL_CONNECTED);
    
    if (currently_connected && !connection_established) {
        // Just connected
        connection_established = true;
        connection_established_time = millis();
        onWiFiConnected();
    } else if (!currently_connected && connection_established) {
        // Just disconnected
        connection_established = false;
        onWiFiDisconnected();
    }
}

bool ProfessionalWiFi::isConnected() {
    return (WiFi.status() == WL_CONNECTED);
}

String ProfessionalWiFi::getIP() {
    if (isConnected()) {
        return WiFi.localIP().toString();
    }
    return "0.0.0.0";
}

int ProfessionalWiFi::getRSSI() {
    if (isConnected()) {
        return WiFi.RSSI();
    }
    return -100;  // Very weak signal indicator
}

String ProfessionalWiFi::getSSID() {
    if (isConnected()) {
        return WiFi.SSID();
    }
    return "";
}

void ProfessionalWiFi::resetSettings() {
    Serial.println("[WiFiPro] Resetting WiFi settings");
    wm.resetSettings();
    connection_established = false;
}

void ProfessionalWiFi::startConfigPortal() {
    Serial.println("[WiFiPro] Starting configuration portal manually");
    portal_active = true;
    
    if (!wm.startConfigPortal("MAC-SYS-Config")) {
        Serial.println("[WiFiPro] Failed to connect through config portal");
        portal_active = false;
    } else {
        Serial.println("[WiFiPro] Connected through config portal");
        portal_active = false;
        connection_established = true;
        connection_established_time = millis();
        onWiFiConnected();
    }
}

void ProfessionalWiFi::stopConfigPortal() {
    if (portal_active) {
        Serial.println("[WiFiPro] Stopping configuration portal");
        wm.stopConfigPortal();
        portal_active = false;
    }
}

int ProfessionalWiFi::scanNetworks() {
    Serial.println("[WiFiPro] Scanning for networks...");
    return WiFi.scanNetworks();
}

String ProfessionalWiFi::getScannedSSID(int index) {
    return WiFi.SSID(index);
}

int ProfessionalWiFi::getScannedRSSI(int index) {
    return WiFi.RSSI(index);
}

int ProfessionalWiFi::getScannedEncryption(int index) {
    return (int)WiFi.encryptionType(index);
}

String ProfessionalWiFi::getEncryptionTypeString(int type) {
    switch(type) {
        case WIFI_AUTH_OPEN: return "Open";
        case WIFI_AUTH_WEP: return "WEP";
        case WIFI_AUTH_WPA_PSK: return "WPA";
        case WIFI_AUTH_WPA2_PSK: return "WPA2";
        case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/WPA2";
        case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-Enterprise";
        case WIFI_AUTH_WPA3_PSK: return "WPA3";
        default: return "Unknown";
    }
}

bool ProfessionalWiFi::connectToNetwork(const char* ssid, const char* password) {
    Serial.print("[WiFiPro] Connecting to network: ");
    Serial.println(ssid);
    
    connection_start_time = millis();
    
    WiFi.begin(ssid, password);
    
    // Wait for connection with timeout
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    Serial.println();
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("[WiFiPro] Connection successful");
        connection_established = true;
        connection_established_time = millis();
        onWiFiConnected();
        return true;
    } else {
        Serial.println("[WiFiPro] Connection failed");
        return false;
    }
}

bool ProfessionalWiFi::connectWithStaticIP(const char* ssid, const char* password, 
                                          IPAddress ip, IPAddress gateway, 
                                          IPAddress subnet, IPAddress dns) {
    Serial.println("[WiFiPro] Configuring static IP");
    
    if (!WiFi.config(ip, gateway, subnet, dns)) {
        Serial.println("[WiFiPro] Static IP configuration failed");
        return false;
    }
    
    return connectToNetwork(ssid, password);
}

void ProfessionalWiFi::disconnect() {
    Serial.println("[WiFiPro] Disconnecting from WiFi");
    WiFi.disconnect(true);
    connection_established = false;
    onWiFiDisconnected();
}

void ProfessionalWiFi::setDeviceInfo(const char* hostname, const char* ap_name) {
    Serial.print("[WiFiPro] Setting device hostname: ");
    Serial.println(hostname);
    WiFi.setHostname(hostname);
}

void ProfessionalWiFi::enablemDNS(const char* hostname) {
    if (isConnected()) {
        Serial.print("[WiFiPro] Starting mDNS service: ");
        Serial.println(hostname);
        
        if (MDNS.begin(hostname)) {
            MDNS.addService("http", "tcp", 80);
            MDNS.addServiceTxt("http", "tcp", "manufacturer", "MAC-SYS-Industries");
            MDNS.addServiceTxt("http", "tcp", "model", "Industrial-HVAC-Controller");
            MDNS.addServiceTxt("http", "tcp", "version", FIRMWARE_VERSION);
            MDNS.addServiceTxt("http", "tcp", "type", "Industrial-Controller");
            Serial.println("[WiFiPro] mDNS service started successfully");
        } else {
            Serial.println("[WiFiPro] mDNS service failed to start");
        }
    }
}

bool ProfessionalWiFi::pingHost(IPAddress host) {
    if (!isConnected()) {
        return false;
    }
    
    // Simple ping implementation
    // Note: ESP32 ping functionality may require additional libraries
    return true;  // Placeholder - implement proper ping
}

unsigned long ProfessionalWiFi::getConnectionTime() {
    if (connection_start_time > 0 && connection_established_time > connection_start_time) {
        return connection_established_time - connection_start_time;
    }
    return 0;
}

unsigned long ProfessionalWiFi::getUptime() {
    if (connection_established && connection_established_time > 0) {
        return millis() - connection_established_time;
    }
    return 0;
}

String ProfessionalWiFi::getConnectionQuality() {
    if (!isConnected()) {
        return "Disconnected";
    }
    
    int rssi = getRSSI();
    if (rssi >= -50) return "Excellent";
    else if (rssi >= -60) return "Good";
    else if (rssi >= -70) return "Fair";
    else if (rssi >= -80) return "Weak";
    else return "Very Weak";
}

bool ProfessionalWiFi::hasStoredCredentials() {
    return wm.getWiFiIsSaved();
}

// Callback setters
void ProfessionalWiFi::setOnConnectedCallback(std::function<void()> callback) {
    on_connected_callback = callback;
}

void ProfessionalWiFi::setOnDisconnectedCallback(std::function<void()> callback) {
    on_disconnected_callback = callback;
}

void ProfessionalWiFi::setOnConfigModeCallback(std::function<void(WiFiManager*)> callback) {
    on_config_mode_callback = callback;
}

// Internal callback handlers
void ProfessionalWiFi::onWiFiConnected() {
    Serial.println("[WiFiPro] WiFi connection established");
    
    // Update display
    setDisplayScreen(SCREEN_WIFI_STATUS);
    
    // Start mDNS
    enablemDNS("macsys-controller");
    
    // Call user callback if set
    if (on_connected_callback) {
        on_connected_callback();
    }
}

void ProfessionalWiFi::onWiFiDisconnected() {
    Serial.println("[WiFiPro] WiFi connection lost");
    
    // Update display
    setDisplayScreen(SCREEN_WIFI_SETUP);
    
    // Call user callback if set
    if (on_disconnected_callback) {
        on_disconnected_callback();
    }
}
