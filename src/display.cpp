#include "display.h"
#include "network.h"
#include "temperature.h"
#include "wifi_manager.h"
#include "rtc_manager.h"
#include <WiFi.h>

// Global display object
static Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Display state variables
static DisplayScreen current_screen = SCREEN_BOOT;
static unsigned long last_display_update = 0;
static unsigned long display_cycle_time = 0;
static bool display_initialized = false;

bool initializeDisplay() {
    DEBUG_PRINTLN("Initializing SSD1306 OLED display...");
    
    // Initialize display with I2C address
    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        DEBUG_PRINTLN("ERROR: SSD1306 allocation failed");
        return false;
    }
    
    // Clear the buffer
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.display();
    
    display_initialized = true;
    DEBUG_PRINTLN("SSD1306 display initialized successfully");
    
    // Initialize timing
    display_cycle_time = millis();
    
    // Show boot screen
    showBootScreen();
    
    return true;
}

void updateDisplay() {
    if (!display_initialized) return;
    
    unsigned long current_time = millis();
    
    // Update display every 2 seconds or when screen changes
    if (current_time - last_display_update >= 2000) {
        switch (current_screen) {
            case SCREEN_BOOT:
                showBootScreen();
                break;
            case SCREEN_TEMPERATURE:
                showTemperatureScreen();
                break;
            case SCREEN_SYSTEM_INFO:
                showSystemInfo();
                break;
            case SCREEN_WIFI_STATUS:
                showWiFiStatus();
                break;
            case SCREEN_NETWORK_INFO:
                showNetworkInfo();
                break;
            case SCREEN_WIFI_SETUP:
                if (isConfigurationModeActive()) {
                    showWiFiSetup("MAC-SYS-CONFIG", WiFi.softAPIP().toString().c_str());
                } else {
                    showWiFiStatus();
                }
                break;
            case SCREEN_ERROR:
                // Error screen is shown manually
                break;
        }
        last_display_update = current_time;
    }
    
    // Auto-cycle screens every 5 seconds (except error screens)
    if (current_screen != SCREEN_ERROR) {
        if (current_time - display_cycle_time >= 5000) {
            cycleDisplayScreen();
            display_cycle_time = current_time;
        }
    }
}

void showBootScreen() {
    if (!display_initialized) return;
    
    display.clearDisplay();
    display.setTextSize(2);
    drawCenteredText("MAC-SYS", 10);
    
    display.setTextSize(1);
    drawCenteredText("Industrial HVAC", 35);
    drawCenteredText("Controller", 45);
    drawCenteredText("v" FIRMWARE_VERSION, 55);
    
    display.display();
}

void showTemperatureScreen() {
    if (!display_initialized) return;
    
    display.clearDisplay();
    
    // Title and time
    display.setTextSize(1);
    drawCenteredText("Temperature Control", 0);
    
    // Show current time in top right
    String current_time = rtc_manager.getFormattedTime();
    display.setCursor(SCREEN_WIDTH - 35, 0);
    display.print(current_time);
    
    display.drawLine(0, 10, SCREEN_WIDTH, 10, SSD1306_WHITE);
    
    // Use last measured temperature from system status to avoid blocking sensor reads here
    float currentTemp = g_system_status.current_temperature;
    float setpoint = g_system_config.ac_setpoint;
    float compensation = g_system_config.delivery_compensation;
    float compensatedTemp = applyDeliveryCompensation(currentTemp, compensation);
    
    // Current Temperature (large display)
    display.setTextSize(3);
    display.setCursor(0, 15);
    display.printf("%.1f", compensatedTemp);
    display.setTextSize(1);
    display.print("C");
    
    // Setpoint
    display.setCursor(0, 42);
    display.setTextSize(1);
    display.printf("Set: %.1f°C", setpoint);
    
    // Operation mode
    display.setCursor(0, 52);
    const char* mode = g_system_config.operation_mode == 0 ? "DIRECT" : "SCHEDULE";
    display.printf("Mode: %s", mode);
    
    // Compressor status
    display.setCursor(80, 42);
    bool compressorOn = g_system_status.compressor_running;
    display.printf("AC: %s", compressorOn ? "ON" : "OFF");
    
    // Sensor type
    display.setCursor(80, 52);
    display.printf("LM35");
    
    // Temperature trend indicator
    static float lastTemp = compensatedTemp;
    if (compensatedTemp > lastTemp + 0.1) {
        display.setCursor(110, 20);
        display.print("^");
    } else if (compensatedTemp < lastTemp - 0.1) {
        display.setCursor(110, 20);
        display.print("v");
    }
    lastTemp = compensatedTemp;
    
    display.display();
}

void showWiFiStatus() {
    if (!display_initialized) return;
    
    display.clearDisplay();
    display.setTextSize(1);
    
    // Title and time
    drawCenteredText("WiFi Manager", 0);
    
    // Show current time in top right
    String current_time = rtc_manager.getFormattedTime();
    display.setCursor(SCREEN_WIDTH - 35, 0);
    display.print(current_time);
    
    display.drawLine(0, 10, SCREEN_WIDTH, 10, SSD1306_WHITE);
    
    // Check if in configuration mode
    if (isConfigurationModeActive()) {
        display.setCursor(0, 15);
        display.println("Config Mode Active");
        
        display.setCursor(0, 25);
        display.print("AP: MAC-SYS-CONFIG");
        
        display.setCursor(0, 35);
        display.print("Device IP: ");
        display.println(WiFi.softAPIP().toString());
        
        display.setCursor(0, 45);
        display.print("Password: admin123");
        
        display.setCursor(0, 55);
        display.println("Browse to configure");
    }
    // Check WiFi connection status  
    else if (WiFi.status() == WL_CONNECTED) {
        display.setCursor(0, 15);
        display.print("SSID: ");
        display.println(WiFi.SSID());
        
        display.setCursor(0, 25);
        display.print("IP: ");
        display.println(WiFi.localIP().toString());
        
        display.setCursor(0, 35);
        display.print("RSSI: ");
        display.print(WiFi.RSSI());
        display.println(" dBm");
        
        // Draw WiFi signal strength
        drawWiFiSignal(WiFi.RSSI());
        
        drawCenteredText("✓ Connected", 55);
    } else {
        drawCenteredText("Not Connected", 30);
        drawCenteredText("Check Configuration", 45);
    }
    
    display.display();
}

void showSystemInfo() {
    if (!display_initialized) return;
    
    display.clearDisplay();
    display.setTextSize(1);
    
    // Title and time
    drawCenteredText("System Info", 0);
    
    // Show current time in top right
    String current_time = rtc_manager.getFormattedTime();
    display.setCursor(SCREEN_WIDTH - 35, 0);
    display.print(current_time);
    
    display.drawLine(0, 10, SCREEN_WIDTH, 10, SSD1306_WHITE);
    
    // System information
    display.setCursor(0, 15);
    display.print("Uptime: ");
    display.print(g_system_status.uptime / 3600);
    display.println("h");
    
    display.setCursor(0, 25);
    display.print("Memory: ");
    display.print(g_system_status.free_memory / 1024);
    display.println(" KB");
    
    display.setCursor(0, 35);
    display.print("Time: ");
    display.println(rtc_manager.getFormattedDateTime());
    
    display.setCursor(0, 45);
    display.print("State: ");
    const char* state_names[] = {"INIT", "CONN", "READY", "RUN", "ERROR"};
    if (g_system_status.state < 5) {
        display.println(state_names[g_system_status.state]);
    } else {
        display.println(g_system_status.state);
    }
    
    display.setCursor(0, 55);
    display.print("MAC: ");
    String mac = WiFi.macAddress();
    display.println(mac.substring(9)); // Show last 6 chars
    
    display.display();
}

void showWiFiSetup(const char* ssid, const char* ip) {
    if (!display_initialized) return;
    
    display.clearDisplay();
    display.setTextSize(1);
    
    // Title with captive portal indicator
    drawCenteredText("WiFi Portal Active", 0);
    display.drawLine(0, 10, SCREEN_WIDTH, 10, SSD1306_WHITE);
    
    // Access Point info
    display.setCursor(0, 15);
    display.print("AP: ");
    display.println(ssid);
    
    display.setCursor(0, 25);
    display.print("IP: ");
    display.println(ip);
    
    // Captive portal instructions
    display.setCursor(0, 35);
    display.println("1. Join WiFi network");
    display.setCursor(0, 45);
    display.println("2. Browser auto-opens");
    display.setCursor(0, 55);
    display.println("3. Select your WiFi");
    
    display.display();
}

void showNetworkInfo() {
    if (!display_initialized) return;
    
    display.clearDisplay();
    display.setTextSize(1);
    
    // Title and time
    drawCenteredText("Network", 0);
    
    // Show current time in top right
    String current_time = rtc_manager.getFormattedTime();
    display.setCursor(SCREEN_WIDTH - 35, 0);
    display.print(current_time);
    
    display.drawLine(0, 10, SCREEN_WIDTH, 10, SSD1306_WHITE);
    
    // Network status
    NetworkStatus net_status = getNetworkStatus();
    display.setCursor(0, 15);
    display.print("Status: ");
    switch (net_status) {
        case NET_CONNECTED_STA:
            display.println("STA Mode");
            break;
        case NET_AP_MODE:
            display.println("AP Mode");
            break;
        case NET_CONNECTING:
            display.println("Connecting");
            break;
        case NET_DISCONNECTED:
            display.println("Disconnected");
            break;
        case NET_ERROR:
            display.println("Error");
            break;
    }
    
    if (getNetworkStatus() == NET_AP_MODE) {
        display.setCursor(0, 25);
        display.print("AP IP: ");
        display.println("192.168.4.1"); // AP mode placeholder
        
        display.setCursor(0, 35);
        display.print("Clients: ");
        display.println("0"); // No clients in compatibility mode
    }
    
    display.setCursor(0, 45);
    display.print("Date: ");
    display.println(rtc_manager.getFormattedDate());
    
    display.display();
}

void showError(const char* message) {
    if (!display_initialized) return;
    
    display.clearDisplay();
    display.setTextSize(1);
    
    // Error title
    display.setTextSize(2);
    drawCenteredText("ERROR", 10);
    
    display.setTextSize(1);
    display.drawLine(0, 30, SCREEN_WIDTH, 30, SSD1306_WHITE);
    
    // Error message
    display.setCursor(0, 35);
    display.println(message);
    
    display.display();
}

void showMessage(const char* line1, const char* line2, const char* line3, const char* line4) {
    if (!display_initialized) return;
    
    display.clearDisplay();
    display.setTextSize(1);
    
    if (line1) {
        display.setCursor(0, 0);
        display.println(line1);
    }
    if (line2) {
        display.setCursor(0, 15);
        display.println(line2);
    }
    if (line3) {
        display.setCursor(0, 30);
        display.println(line3);
    }
    if (line4) {
        display.setCursor(0, 45);
        display.println(line4);
    }
    
    display.display();
}

void clearDisplay() {
    if (!display_initialized) return;
    display.clearDisplay();
    display.display();
}

void setDisplayScreen(DisplayScreen screen) {
    current_screen = screen;
    display_cycle_time = millis(); // Reset cycle timer
}

DisplayScreen getCurrentScreen() {
    return current_screen;
}

void cycleDisplayScreen() {
    DEBUG_PRINT("Cycling from screen: ");
    DEBUG_PRINTLN(current_screen);
    
    // Check if we're in configuration mode
    bool in_config_mode = isConfigurationModeActive();
    
    // Define available screens based on mode
    DisplayScreen available_screens[6];
    int screen_count = 0;
    
    // Always include basic screens
    available_screens[screen_count++] = SCREEN_TEMPERATURE;
    available_screens[screen_count++] = SCREEN_SYSTEM_INFO;
    available_screens[screen_count++] = SCREEN_WIFI_STATUS;
    available_screens[screen_count++] = SCREEN_NETWORK_INFO;
    
    // Include WiFi setup only in config mode
    if (in_config_mode) {
        available_screens[screen_count++] = SCREEN_WIFI_SETUP;
    }
    
    // Find current screen index
    int current_index = 0;
    for (int i = 0; i < screen_count; i++) {
        if (available_screens[i] == current_screen) {
            current_index = i;
            break;
        }
    }
    
    // Move to next screen
    current_index = (current_index + 1) % screen_count;
    current_screen = available_screens[current_index];
    
    DEBUG_PRINT("Cycling to screen: ");
    DEBUG_PRINTLN(current_screen);
}

// Utility functions
void drawCenteredText(const char* text, int y) {
    if (!display_initialized) return;
    
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    int x = (SCREEN_WIDTH - w) / 2;
    display.setCursor(x, y);
    display.print(text);
}

void drawProgressBar(int progress, const char* label) {
    if (!display_initialized) return;
    
    // Draw label
    if (label) {
        display.setCursor(0, 35);
        display.print(label);
    }
    
    // Draw progress bar frame
    display.drawRect(0, 45, SCREEN_WIDTH, 10, SSD1306_WHITE);
    
    // Draw progress fill
    int fill_width = (progress * (SCREEN_WIDTH - 2)) / 100;
    if (fill_width > 0) {
        display.fillRect(1, 46, fill_width, 8, SSD1306_WHITE);
    }
    
    // Draw percentage
    display.setCursor(55, 57);
    display.print(progress);
    display.print("%");
}

void drawWiFiSignal(int rssi) {
    if (!display_initialized) return;
    
    // Draw WiFi signal bars (simple representation)
    int signal_x = 110;
    int signal_y = 45;
    
    // Determine signal strength (4 bars max)
    int bars = 0;
    if (rssi >= -50) bars = 4;
    else if (rssi >= -60) bars = 3;
    else if (rssi >= -70) bars = 2;
    else if (rssi >= -80) bars = 1;
    
    // Draw signal bars
    for (int i = 0; i < 4; i++) {
        int bar_height = 3 + (i * 2);
        if (i < bars) {
            display.fillRect(signal_x + (i * 3), signal_y + (12 - bar_height), 2, bar_height, SSD1306_WHITE);
        } else {
            display.drawRect(signal_x + (i * 3), signal_y + (12 - bar_height), 2, bar_height, SSD1306_WHITE);
        }
    }
}
