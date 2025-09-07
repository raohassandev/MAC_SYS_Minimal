#ifndef NETWORK_H
#define NETWORK_H

#include "config.h"

// Network Status
enum NetworkStatus {
    NET_DISCONNECTED = 0,
    NET_CONNECTING = 1,
    NET_CONNECTED_STA = 2,
    NET_AP_MODE = 3,
    NET_ERROR = 4
};

// Network Statistics structure
struct NetworkStatistics {
    uint16_t successful_connections;
    uint16_t failed_connections;
    uint8_t reconnect_attempts;
    unsigned long total_uptime;
    unsigned long current_session_time;
    unsigned long average_connection_time;
    unsigned long last_disconnect_time;
};

// Network initialization and management
bool initializeNetworkStack();
NetworkStatus getNetworkStatus();

// Event callbacks
void onWiFiConnected();
void onWiFiDisconnected();
void onWiFiGotIP();

// WiFi Station (STA) mode functions
bool connectToWiFi(const char* ssid, const char* password, bool use_static = false);
bool connectToWiFiWithConfig(const NetworkConfig& network_config);
bool disconnectWiFi();
bool isWiFiConnected();
int getCurrentRSSI();
String getCurrentSSID();
String getNetworkStatusString(NetworkStatus status);

// Network configuration functions
bool saveNetworkConfig(const NetworkConfig& config);
NetworkConfig loadNetworkConfig();
void printNetworkConfig(const NetworkConfig& config);

// mDNS functions
bool startMDNS(const char* hostname);
void stopMDNS();
bool isMDNSActive();

// Network monitoring and diagnostics
void performNetworkDiagnostics();
bool pingGateway();
bool checkInternetConnectivity();
String getNetworkInfo();
void resetNetworkStatistics();
NetworkStatistics getNetworkStatistics();
void handleNetworkEvents();
void enableAutoReconnect(bool enable);
bool isAutoReconnectEnabled();

// Auto reconnect functions for compatibility
void setAutoReconnectEnabled(bool enable);  // Alias for enableAutoReconnect
void handleAutoReconnect();                 // Alias for handleNetworkEvents

// Network event handlers (internal)
void onWiFiConnected();
void onWiFiDisconnected();
void onWiFiGotIP();

#endif // NETWORK_H