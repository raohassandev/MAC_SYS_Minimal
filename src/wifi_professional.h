#ifndef WIFI_PROFESSIONAL_H
#define WIFI_PROFESSIONAL_H

#include <Arduino.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include "config.h"

// Professional WiFi Management Class
// Integrates tzapu/WiFiManager with existing MAC-SYS architecture
class ProfessionalWiFi {
public:
    // Core WiFi management functions
    bool begin();                           // Initialize with auto-connect
    void process();                         // Non-blocking processing in main loop
    bool isConnected();                     // Current connection status
    String getIP();                         // Current IP address
    int getRSSI();                          // Signal strength in dBm
    String getSSID();                       // Current network name
    void resetSettings();                   // Clear saved credentials
    void startConfigPortal();               // Manual portal start
    void stopConfigPortal();                // Stop portal mode
    
    // Network scanning and management
    int scanNetworks();                     // Scan for available networks
    String getScannedSSID(int index);       // Get scanned network SSID
    int getScannedRSSI(int index);          // Get scanned network signal
    int getScannedEncryption(int index);    // Get encryption type
    
    // Connection management
    bool connectToNetwork(const char* ssid, const char* password);
    bool connectWithStaticIP(const char* ssid, const char* password, 
                           IPAddress ip, IPAddress gateway, IPAddress subnet, IPAddress dns);
    void disconnect();
    
    // Device identity and branding
    void setDeviceInfo(const char* hostname, const char* ap_name);
    void enablemDNS(const char* hostname);
    
    // Status and diagnostics
    bool pingHost(IPAddress host);
    unsigned long getConnectionTime();      // Time to last connection
    unsigned long getUptime();             // Connection uptime
    String getConnectionQuality();         // Connection quality assessment
    
    // Configuration management
    void saveNetworkConfig();
    void loadNetworkConfig();
    bool hasStoredCredentials();
    
    // Integration with existing system
    void setOnConnectedCallback(std::function<void()> callback);
    void setOnDisconnectedCallback(std::function<void()> callback);
    void setOnConfigModeCallback(std::function<void(WiFiManager*)> callback);
    
public:
    // Callback functions for integration (need to be accessible by static callbacks)
    std::function<void()> on_connected_callback;
    std::function<void()> on_disconnected_callback;
    std::function<void(WiFiManager*)> on_config_mode_callback;
    
    // Internal helper functions (need to be accessible by static callbacks)
    void onWiFiConnected();
    void onWiFiDisconnected();

private:
    WiFiManager wm;
    bool portal_active;
    bool connection_established;
    unsigned long last_connection_check;
    unsigned long connection_start_time;
    unsigned long connection_established_time;
    
    // Internal helper functions
    void onConfigModeStarted(WiFiManager* wm);
    String getEncryptionTypeString(int type);
    void updateConnectionStatus();
};

// Global instance for easy access
extern ProfessionalWiFi wifiPro;

// Network statistics for monitoring
struct NetworkStats {
    unsigned long total_connections;
    unsigned long failed_connections;
    unsigned long total_uptime;
    unsigned long last_disconnect_duration;
    float average_signal_strength;
    unsigned long bytes_sent;
    unsigned long bytes_received;
};

#endif // WIFI_PROFESSIONAL_H