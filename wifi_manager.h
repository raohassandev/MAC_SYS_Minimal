#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "config.h"
#include "network.h"

// WiFi Manager functions
bool initializeWiFiManager();
void startConfigurationMode();
void stopConfigurationMode();
void handleWiFiManager();
bool isConfigurationModeActive();
void autoStartConfigMode();

// Configuration portal functions
void setupConfigPortalRoutes();
void handleConfigRoot();
void handleWiFiScan();
void handleConfigSave();
void handleConfigStatus();
void handleConfigReset();

// HTML generation functions
String generateConfigPage();
String generateSaveSuccessPage();

#endif // WIFI_MANAGER_H