#ifndef DISPLAY_H
#define DISPLAY_H

#include "config.h"

// Display screen types
enum DisplayScreen {
    SCREEN_BOOT = 0,
    SCREEN_TEMPERATURE = 1,
    SCREEN_SYSTEM_INFO = 2,
    SCREEN_WIFI_STATUS = 3,
    SCREEN_NETWORK_INFO = 4,
    SCREEN_WIFI_SETUP = 5,
    SCREEN_ERROR = 6
};

// Display functions
bool initializeDisplay();
void updateDisplay();
void showBootScreen();
void showTemperatureScreen();
void showWiFiStatus();
void showSystemInfo();
void showWiFiSetup(const char* ssid, const char* ip);
void showNetworkInfo();
void showError(const char* message);
void showMessage(const char* line1, const char* line2 = "", const char* line3 = "", const char* line4 = "");
void clearDisplay();
void setDisplayScreen(DisplayScreen screen);
DisplayScreen getCurrentScreen();
void cycleDisplayScreen();

// Display utility functions
void drawCenteredText(const char* text, int y);
void drawProgressBar(int progress, const char* label);
void drawWiFiSignal(int rssi);

#endif // DISPLAY_H