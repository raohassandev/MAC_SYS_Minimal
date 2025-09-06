#include "display.h"
#include "network.h"
#include "temperature.h"

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
                // WiFi setup screen is shown manually
                break;
            case SCREEN_ERROR:
                // Error screen is shown manually
                break;
        }
        last_display_update = current_time;
    }
    
    // Auto-cycle screens every 10 seconds (except setup/error screens)
    if (current_screen != SCREEN_WIFI_SETUP && current_screen != SCREEN_ERROR) {
        if (current_time - display_cycle_time >= 10000) {
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
    
    // Title
    display.setTextSize(1);
    drawCenteredText("Temperature Control", 0);
    display.drawLine(0, 10, SCREEN_WIDTH, 10, SSD1306_WHITE);
    
    // Get current temperature
    float currentTemp = readTemperature();
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
    
    // Title
    drawCenteredText("WiFi Status", 0);
    display.drawLine(0, 10, SCREEN_WIDTH, 10, SSD1306_WHITE);
    
    // WiFi connection status
    if (isWiFiConnected()) {
        display.setCursor(0, 15);
        display.print("SSID: ");
        display.println(getCurrentSSID());
        
        display.setCursor(0, 25);
        display.print("IP: ");
        display.println("N/A"); // WiFi compatibility mode
        
        display.setCursor(0, 35);
        display.print("RSSI: ");
        display.print(getCurrentRSSI());
        display.println(" dBm");
        
        // Draw WiFi signal strength
        drawWiFiSignal(getCurrentRSSI());
        
        drawCenteredText("Connected", 55);
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
    
    // Title
    drawCenteredText("System Info", 0);
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
    display.print("Temp: ");
    display.print(g_system_status.current_temperature, 1);
    display.println(" C");
    
    display.setCursor(0, 45);
    display.print("State: ");
    display.println(g_system_status.state);
    
    display.display();
}

void showWiFiSetup(const char* ssid, const char* ip) {
    if (!display_initialized) return;
    
    display.clearDisplay();
    display.setTextSize(1);
    
    // Title
    drawCenteredText("WiFi Setup", 0);
    display.drawLine(0, 10, SCREEN_WIDTH, 10, SSD1306_WHITE);
    
    // Setup instructions
    display.setCursor(0, 15);
    display.println("1. Connect to:");
    display.setCursor(0, 25);
    display.println(ssid);
    
    display.setCursor(0, 35);
    display.println("2. Open browser:");
    display.setCursor(0, 45);
    display.println(ip);
    
    display.setCursor(0, 55);
    display.println("3. Configure WiFi");
    
    display.display();
}

void showNetworkInfo() {
    if (!display_initialized) return;
    
    display.clearDisplay();
    display.setTextSize(1);
    
    // Title
    drawCenteredText("Network", 0);
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
    display.print("Hostname: ");
    display.println(g_system_config.network.hostname);
    
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
    // Skip WiFi setup and error screens in auto-cycle
    do {
        current_screen = (DisplayScreen)((current_screen + 1) % 7);
    } while (current_screen == SCREEN_WIFI_SETUP || current_screen == SCREEN_ERROR);
    
    // After boot screen, always go to temperature screen first
    if (current_screen == SCREEN_BOOT) {
        current_screen = SCREEN_TEMPERATURE;
    }
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