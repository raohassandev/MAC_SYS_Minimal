// WiFi Configuration Portal Handlers
#include "wifi_manager.h"
#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>

extern WebServer* config_server;

// External WiFi credentials structure and address
struct WiFiCredentials {
    char ssid[64];
    char password[64];
    uint16_t checksum;
};
#define WIFI_CREDENTIALS_ADDR 100

// External function declarations
extern bool saveWiFiCredentials(const char* ssid, const char* password);
extern bool attemptWiFiConnection(const char* ssid, const char* password);
extern void stopConfigurationMode();

void handleConfigRoot() {
    if (!config_server) return;
    
    String html = generateConfigPage();
    config_server->send(200, "text/html", html);
}

void handleWiFiScan() {
    if (!config_server) return;
    
    DEBUG_PRINTLN("Scanning for WiFi networks...");
    int networks = WiFi.scanNetworks();
    
    String json = "[";
    for (int i = 0; i < networks; i++) {
        if (i > 0) json += ",";
        json += "{";
        json += "\"ssid\":\"" + WiFi.SSID(i) + "\",";
        json += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
        json += "\"secure\":" + String(WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
        json += "}";
    }
    json += "]";
    
    config_server->send(200, "application/json", json);
}

void handleConfigSave() {
    if (!config_server) return;
    
    String ssid = config_server->arg("ssid");
    String password = config_server->arg("password");
    
    if (ssid.length() > 0) {
        DEBUG_PRINTF("Saving WiFi config: %s\n", ssid.c_str());
        
        if (saveWiFiCredentials(ssid.c_str(), password.c_str())) {
            // Test connection
            if (attemptWiFiConnection(ssid.c_str(), password.c_str())) {
                String html = generateSaveSuccessPage();
                config_server->send(200, "text/html", html);
                
                // Stop config mode after successful connection
                delay(2000);
                stopConfigurationMode();
                return;
            }
        }
    }
    
    config_server->send(400, "text/html", "<h1>Configuration Failed</h1><p>Please try again.</p>");
}

void handleConfigStatus() {
    if (!config_server) return;
    
    String json = "{";
    json += "\"connected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",";
    json += "\"ssid\":\"" + WiFi.SSID() + "\",";
    json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
    json += "\"rssi\":" + String(WiFi.RSSI());
    json += "}";
    
    config_server->send(200, "application/json", json);
}

void handleConfigReset() {
    if (!config_server) return;
    
    DEBUG_PRINTLN("WiFi config reset requested");
    
    // Clear saved credentials
    WiFiCredentials empty_creds;
    memset(&empty_creds, 0, sizeof(empty_creds));
    EEPROM.put(WIFI_CREDENTIALS_ADDR, empty_creds);
    EEPROM.commit();
    
    config_server->send(200, "text/html", "<h1>Reset Complete</h1><p>WiFi configuration cleared. Device will restart.</p>");
    
    delay(2000);
    ESP.restart();
}

String generateConfigPage() {
    String html = "<!DOCTYPE html><html><head><title>MAC-SYS WiFi Setup</title>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<style>body{font-family:Arial;margin:20px;background:#f0f0f0;}";
    html += ".container{max-width:400px;margin:auto;background:white;padding:20px;border-radius:8px;}";
    html += ".info{background:#e7f3ff;padding:10px;margin:10px 0;border-radius:4px;font-size:14px;}";
    html += "input,select{width:100%;padding:10px;margin:8px 0;border:1px solid #ddd;border-radius:4px;}";
    html += "button{width:100%;padding:12px;background:#007bff;color:white;border:none;border-radius:4px;cursor:pointer;}";
    html += "button:hover{background:#0056b3;}</style></head><body>";
    html += "<div class='container'><h2>MAC-SYS WiFi Configuration</h2>";
    html += "<div class='info'><strong>Device Info:</strong><br>";
    html += "AP IP: " + WiFi.softAPIP().toString() + "<br>";
    html += "MAC: " + WiFi.softAPmacAddress() + "</div>";
    html += "<form method='post' action='/save'>";
    html += "<label>WiFi Network:</label>";
    html += "<select id='ssid' name='ssid'><option value=''>Select network...</option></select>";
    html += "<input type='text' name='ssid' placeholder='Or enter SSID manually'>";
    html += "<label>Password:</label>";
    html += "<input type='password' name='password' placeholder='WiFi Password'>";
    html += "<button type='submit'>Connect</button></form>";
    html += "<button onclick='scan()'>Scan Networks</button>";
    html += "<button onclick='reset()'>Reset Config</button>";
    html += "<div id='status'></div></div>";
    html += "<script>function scan(){fetch('/scan').then(r=>r.json()).then(nets=>{";
    html += "const s=document.getElementById('ssid');s.innerHTML='<option value=\"\">Select...</option>';";
    html += "nets.forEach(n=>s.innerHTML+='<option value=\"'+n.ssid+'\">'+n.ssid+' ('+n.rssi+' dBm)</option>');});}";
    html += "function reset(){if(confirm('Reset WiFi?'))fetch('/reset').then(()=>location.reload());}";
    html += "scan();setInterval(()=>{fetch('/status').then(r=>r.json()).then(s=>{";
    html += "document.getElementById('status').innerHTML=s.connected?";
    html += "'<p style=\"color:green\">Connected: '+s.ssid+' ('+s.ip+')</p>':";
    html += "'<p style=\"color:red\">Not connected</p>';});},5000);</script></body></html>";
    
    return html;
}

String generateSaveSuccessPage() {
    String html = "<!DOCTYPE html><html><head><title>MAC-SYS - Connected</title>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<style>body{font-family:Arial;margin:20px;background:#f0f0f0;text-align:center;}";
    html += ".container{max-width:400px;margin:auto;background:white;padding:30px;border-radius:8px;}";
    html += "h1{color:#28a745;}</style></head><body>";
    html += "<div class='container'><h1>✓ WiFi Connected Successfully</h1>";
    html += "<p>Your MAC-SYS device is now connected to WiFi.</p>";
    html += "<p>Configuration mode will close automatically.</p>";
    html += "<p>You can now access the device through your network.</p>";
    html += "</div></body></html>";
    return html;
}