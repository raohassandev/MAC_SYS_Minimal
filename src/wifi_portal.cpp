// Simple, Reliable WiFi Portal - Real-world approach
#include "wifi_manager.h"
#include "utils.h"
#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>
#include <esp_task_wdt.h>

extern WebServer* config_server;
extern bool attemptWiFiConnection(const char* ssid, const char* password);

// WiFi credentials structure
struct WiFiCredentials {
    char ssid[64];
    char password[64];
    uint16_t checksum;
};

String generate_config_page() {
    String html = "<!DOCTYPE html><html><head>";
    html += "<title>MAC-SYS WiFi Setup</title>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<style>";
    html += "body{font-family:Arial;margin:20px;background:#f5f5f5;}";
    html += ".container{max-width:400px;margin:auto;background:white;padding:30px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1);}";
    html += ".header{text-align:center;margin-bottom:30px;}";
    html += ".device-info{background:#e3f2fd;padding:15px;border-radius:5px;margin-bottom:20px;font-size:14px;}";
    html += "label{display:block;margin:15px 0 5px 0;font-weight:bold;}";
    html += "input,select{width:100%;padding:12px;border:1px solid #ddd;border-radius:5px;font-size:16px;}";
    html += ".btn{width:100%;padding:15px;margin:10px 0;border:none;border-radius:5px;font-size:16px;cursor:pointer;}";
    html += ".btn-primary{background:#2196F3;color:white;}";
    html += ".btn-secondary{background:#757575;color:white;}";
    html += ".btn-success{background:#4CAF50;color:white;}";
    html += ".btn:hover{opacity:0.9;}";
    html += ".status{margin:20px 0;padding:15px;border-radius:5px;text-align:center;}";
    html += ".hidden{display:none;}";
    html += "</style></head><body>";
    
    html += "<div class='container'>";
    html += "<div class='header'>";
    html += "<h2>MAC-SYS WiFi Setup</h2>";
    html += "</div>";
    
    html += "<div class='device-info'>";
    html += "<strong>Device Information</strong><br>";
    html += "IP Address: " + WiFi.softAPIP().toString() + "<br>";
    html += "MAC Address: " + WiFi.softAPmacAddress() + "";
    html += "</div>";
    
    // Main WiFi form
    html += "<form id='wifiForm'>";
    html += "<label for='ssid'>WiFi Network Name (SSID):</label>";
    html += "<input type='text' id='ssid' name='ssid' placeholder='Enter your WiFi network name' required>";
    html += "<label for='password'>WiFi Password:</label>";
    html += "<div style='position:relative;'>";
    html += "<input type='password' id='password' name='password' placeholder='Enter WiFi password' style='padding-right:60px;width:calc(100% - 70px);'>";
    html += "<button type='button' onclick='togglePassword()' style='position:absolute;right:5px;top:50%;transform:translateY(-50%);background:#007bff;color:white;border:none;cursor:pointer;padding:5px 10px;border-radius:3px;font-size:12px;' id='toggleBtn'>Show</button>";
    html += "</div>";
    html += "<button type='submit' class='btn btn-primary'>Connect to WiFi</button>";
    html += "</form>";
    
    // Optional scan button
    html += "<button onclick='scanNetworks()' id='scanBtn' class='btn btn-secondary'>Scan for Networks (Optional)</button>";
    
    // Reset button
    html += "<button onclick='resetConfig()' class='btn btn-secondary'>Forget Saved WiFi</button>";
    
    // Status area
    html += "<div id='status' class='status hidden'></div>";
    
    // Scan results area
    html += "<div id='scanResults' class='hidden'>";
    html += "<h3>Available Networks:</h3>";
    html += "<div id='networks'></div>";
    html += "</div>";
    
    // JavaScript
    html += "<script>";
    html += "function showStatus(msg,type){ ";
    html += "var s=document.getElementById('status'); ";
    html += "s.className='status '+(type||''); ";
    html += "s.innerHTML=msg;s.classList.remove('hidden');}";
    
    html += "function hideStatus(){ ";
    html += "document.getElementById('status').classList.add('hidden');}";
    
    html += "document.getElementById('wifiForm').onsubmit=function(e){ ";
    html += "e.preventDefault(); ";
    html += "var ssid=document.getElementById('ssid').value; ";
    html += "var pass=document.getElementById('password').value; ";
    html += "if(!ssid){showStatus('Please enter WiFi network name','error');return;} ";
    html += "connectWiFi(ssid,pass);};";
    
    html += "function connectWiFi(ssid,pass){ ";
    html += "showStatus('Connecting to '+ssid+'...','info'); ";
    html += "var data='ssid='+encodeURIComponent(ssid)+'&password='+encodeURIComponent(pass); ";
    html += "fetch('/connect',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:data}) ";
    html += ".then(r=>r.text()).then(result=>{ ";
    html += "if(result.includes('SUCCESS')){ ";
    html += "showStatus('Connected successfully! You can now close this page.','success'); ";
    html += "}else{showStatus('Connection failed: '+result,'error');} ";
    html += "}).catch(e=>showStatus('Error: '+e,'error'));}";
    
    html += "function scanNetworks(){ ";
    html += "var btn=document.getElementById('scanBtn'); ";
    html += "btn.disabled=true;btn.innerHTML='Scanning...'; ";
    html += "showStatus('Scanning for networks... (WiFi may disconnect briefly)','info'); ";
    html += "fetch('/scan').then(r=>r.json()).then(networks=>{ ";
    html += "var div=document.getElementById('networks'); ";
    html += "if(networks.length>0){ ";
    html += "div.innerHTML=''; ";
    html += "networks.forEach(n=>{ ";
    html += "var btn=document.createElement('button'); ";
    html += "btn.className='btn btn-secondary'; ";
    html += "btn.style.marginBottom='5px'; ";
    html += "btn.innerHTML=n.ssid+' ('+n.rssi+' dBm) '+(n.secure?'[Secured]':'[Open]'); ";
    html += "btn.onclick=function(){document.getElementById('ssid').value=n.ssid;hideStatus();}; ";
    html += "div.appendChild(btn);}); ";
    html += "document.getElementById('scanResults').classList.remove('hidden'); ";
    html += "showStatus('Found '+networks.length+' networks. Click on one to select it.','success'); ";
    html += "}else{showStatus('No networks found. Please enter manually.','error');} ";
    html += "}).catch(e=>showStatus('Scan failed: '+e,'error')) ";
    html += ".finally(()=>{btn.disabled=false;btn.innerHTML='Scan for Networks (Optional)';});}";
    
    html += "function resetConfig(){ ";
    html += "if(confirm('Forget saved WiFi settings?')){ ";
    html += "fetch('/reset').then(()=>{showStatus('WiFi settings cleared!','success');});}}";
    
    html += "function togglePassword(){ ";
    html += "var pwd=document.getElementById('password'); ";
    html += "var btn=document.getElementById('toggleBtn'); ";
    html += "if(pwd.type==='password'){ ";
    html += "pwd.type='text';btn.innerHTML='Hide'; ";
    html += "}else{pwd.type='password';btn.innerHTML='Show';}} ";
    
    html += "</script></div></body></html>";
    
    return html;
}

void handle_config_root() {
    if (!config_server) return;
    String html = generate_config_page();
    config_server->send(200, "text/html", html);
}

void handle_connect() {
    if (!config_server) return;
    
    String ssid = config_server->arg("ssid");
    String password = config_server->arg("password");
    
    DEBUG_PRINTF("Connect request - SSID: %s\n", ssid.c_str());
    DEBUG_PRINTF("Password length received: %d\n", password.length());
    
    // Debug: Show each character code
    DEBUG_PRINT("Password chars (ASCII): ");
    for(int i = 0; i < password.length(); i++) {
        DEBUG_PRINTF("%d ", (int)password.charAt(i));
    }
    DEBUG_PRINTLN("");
    
    if (ssid.length() == 0) {
        config_server->send(400, "text/plain", "ERROR: No SSID provided");
        return;
    }
    
    // Save credentials first
    WiFiCredentials creds;
    memset(&creds, 0, sizeof(creds));
    strncpy(creds.ssid, ssid.c_str(), sizeof(creds.ssid) - 1);
    strncpy(creds.password, password.c_str(), sizeof(creds.password) - 1);
    
    // Calculate checksum
    uint16_t checksum = 0;
    const uint8_t* bytes = (const uint8_t*)&creds;
    for (size_t i = 0; i < sizeof(creds) - sizeof(creds.checksum); i++) {
        checksum += bytes[i];
    }
    creds.checksum = checksum;
    
    EEPROM.put(100, creds); // WIFI_CREDENTIALS_ADDR = 100
    EEPROM.commit();
    
    // Send immediate response before attempting connection
    config_server->send(200, "text/plain", "Credentials saved. Attempting connection... Check status in 30 seconds.");
    
    DEBUG_PRINTLN("Starting connection attempt in background...");
    
    // Delay to let the HTTP response complete, then attempt connection
    delay(1000);
    
    DEBUG_PRINTF("Attempting connection to: %s\n", ssid.c_str());
    DEBUG_PRINTF("Password: %s\n", password.c_str());
    
    // Attempt connection
    if (attemptWiFiConnection(ssid.c_str(), password.c_str())) {
        DEBUG_PRINTLN("✅ WiFi connection successful!");
        // Restart to switch from AP mode to STA mode
        delay(1000);
        ESP.restart();
    } else {
        DEBUG_PRINTLN("❌ WiFi connection failed, staying in AP mode");
    }
}

void handle_scan() {
    if (!config_server) return;
    
    DEBUG_PRINTLN("Starting WiFi scan in AP+STA mode...");
    esp_task_wdt_reset();
    
    // Use AP+STA mode to maintain connection while scanning
    WiFi.mode(WIFI_AP_STA);
    delay(100);
    
    // Start async scan
    int n = WiFi.scanNetworks(false, false); // sync scan, no hidden networks
    esp_task_wdt_reset();
    
    DEBUG_PRINTF("WiFi scan found %d networks\n", n);
    
    String json = "[";
    if (n > 0) {
        for (int i = 0; i < n && i < 15; i++) {
            if (i > 0) json += ",";
            String ssid = WiFi.SSID(i);
            int rssi = WiFi.RSSI(i);
            bool secure = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
            
            json += "{\"ssid\":\"" + ssid + "\",\"rssi\":" + String(rssi) + ",\"secure\":" + String(secure ? "true" : "false") + "}";
        }
    }
    json += "]";
    
    config_server->send(200, "application/json", json);
}