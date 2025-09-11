#include <WebServer.h>
#include <ArduinoJson.h>
#include "simple_temp_control.h"
#include "relay_control.h"
#include "config.h"
#include "temperature_html.h"
#include "wifi_professional.h"

extern WebServer* device_server;
extern float currentTemperature;

void setupTemperatureAPI() {
    if (!device_server) return;
    
    // GET /api/temperature/status - Get current status
    device_server->on("/api/temperature/status", HTTP_GET, []() {
        device_server->send(200, "application/json", simple_temp.getJsonStatus());
    });
    
    // GET /api/temperature/config - Get configuration
    device_server->on("/api/temperature/config", HTTP_GET, []() {
        device_server->send(200, "application/json", simple_temp.getJsonConfig());
    });
    
    // POST /api/temperature/config - Update configuration
    device_server->on("/api/temperature/config", HTTP_POST, []() {
        if (device_server->hasArg("plain")) {
            String json = device_server->arg("plain");
            if (simple_temp.updateFromJson(json)) {
                device_server->send(200, "application/json", "{\"status\":\"success\",\"message\":\"Configuration updated\"}");
            } else {
                device_server->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid configuration\"}");
            }
        } else {
            device_server->send(400, "application/json", "{\"status\":\"error\",\"message\":\"No data provided\"}");
        }
    });
    
    // GET /api/temperature/location - Get location name
    device_server->on("/api/temperature/location", HTTP_GET, []() {
        StaticJsonDocument<128> doc;
        doc["location"] = simple_temp.getConfig().location_name;
        String response;
        serializeJson(doc, response);
        device_server->send(200, "application/json", response);
    });
    
    // POST /api/temperature/location - Set location name
    device_server->on("/api/temperature/location", HTTP_POST, []() {
        if (device_server->hasArg("plain")) {
            StaticJsonDocument<128> doc;
            DeserializationError error = deserializeJson(doc, device_server->arg("plain"));
            if (!error && doc.containsKey("location")) {
                simple_temp.setLocationName(doc["location"]);
                device_server->send(200, "application/json", "{\"status\":\"success\"}");
            } else {
                device_server->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid location\"}");
            }
        } else {
            device_server->send(400, "application/json", "{\"status\":\"error\",\"message\":\"No data provided\"}");
        }
    });
    
    // GET /api/temperature/setpoint - Get setpoint
    device_server->on("/api/temperature/setpoint", HTTP_GET, []() {
        StaticJsonDocument<128> doc;
        doc["setpoint"] = simple_temp.getConfig().setpoint;
        doc["delta"] = simple_temp.getConfig().delta_temp;
        String response;
        serializeJson(doc, response);
        device_server->send(200, "application/json", response);
    });
    
    // POST /api/temperature/setpoint - Set setpoint
    device_server->on("/api/temperature/setpoint", HTTP_POST, []() {
        if (device_server->hasArg("plain")) {
            StaticJsonDocument<128> doc;
            DeserializationError error = deserializeJson(doc, device_server->arg("plain"));
            if (!error) {
                if (doc.containsKey("setpoint")) {
                    simple_temp.setSetpoint(doc["setpoint"]);
                }
                if (doc.containsKey("delta")) {
                    simple_temp.setDelta(doc["delta"]);
                }
                device_server->send(200, "application/json", "{\"status\":\"success\"}");
            } else {
                device_server->send(400, "application/json", "{\"status\":\"error\"}");
            }
        } else {
            device_server->send(400, "application/json", "{\"status\":\"error\",\"message\":\"No data provided\"}");
        }
    });
    
    // GET /api/temperature/compensation - Get compensation
    device_server->on("/api/temperature/compensation", HTTP_GET, []() {
        StaticJsonDocument<128> doc;
        doc["compensation"] = simple_temp.getConfig().delivery_compensation;
        String response;
        serializeJson(doc, response);
        device_server->send(200, "application/json", response);
    });
    
    // POST /api/temperature/compensation - Set compensation
    device_server->on("/api/temperature/compensation", HTTP_POST, []() {
        if (device_server->hasArg("plain")) {
            StaticJsonDocument<128> doc;
            DeserializationError error = deserializeJson(doc, device_server->arg("plain"));
            if (!error && doc.containsKey("compensation")) {
                simple_temp.setCompensation(doc["compensation"]);
                device_server->send(200, "application/json", "{\"status\":\"success\"}");
            } else {
                device_server->send(400, "application/json", "{\"status\":\"error\"}");
            }
        } else {
            device_server->send(400, "application/json", "{\"status\":\"error\",\"message\":\"No data provided\"}");
        }
    });
    
    // GET /api/temperature/mode - Get mode
    device_server->on("/api/temperature/mode", HTTP_GET, []() {
        StaticJsonDocument<256> doc;
        doc["mode"] = simple_temp.getConfig().mode;
        doc["mode_name"] = "";
        switch(simple_temp.getConfig().mode) {
            case TEMP_MODE_OFF: doc["mode_name"] = "OFF"; break;
            case TEMP_MODE_HEATING: doc["mode_name"] = "HEATING"; break;
            case TEMP_MODE_COOLING: doc["mode_name"] = "COOLING"; break;
            case TEMP_MODE_AUTO: doc["mode_name"] = "AUTO"; break;
            case TEMP_MODE_FAN_ONLY: doc["mode_name"] = "FAN_ONLY"; break;
            case TEMP_MODE_MANUAL: doc["mode_name"] = "MANUAL"; break;
        }
        doc["enabled"] = simple_temp.getConfig().enabled;
        String response;
        serializeJson(doc, response);
        device_server->send(200, "application/json", response);
    });
    
    // POST /api/temperature/mode - Set mode
    device_server->on("/api/temperature/mode", HTTP_POST, []() {
        if (device_server->hasArg("plain")) {
            StaticJsonDocument<128> doc;
            DeserializationError error = deserializeJson(doc, device_server->arg("plain"));
            if (!error) {
                if (doc.containsKey("mode")) {
                    simple_temp.setMode((TempControlMode)doc["mode"].as<int>());
                }
                if (doc.containsKey("enabled")) {
                    simple_temp.setEnabled(doc["enabled"]);
                }
                device_server->send(200, "application/json", "{\"status\":\"success\"}");
            } else {
                device_server->send(400, "application/json", "{\"status\":\"error\"}");
            }
        } else {
            device_server->send(400, "application/json", "{\"status\":\"error\",\"message\":\"No data provided\"}");
        }
    });
    
    // POST /api/temperature/enable - Enable system
    device_server->on("/api/temperature/enable", HTTP_POST, []() {
        simple_temp.setEnabled(true);
        device_server->send(200, "application/json", "{\"status\":\"success\",\"enabled\":true}");
    });
    
    // POST /api/temperature/disable - Disable system
    device_server->on("/api/temperature/disable", HTTP_POST, []() {
        simple_temp.setEnabled(false);
        device_server->send(200, "application/json", "{\"status\":\"success\",\"enabled\":false}");
    });
    
    // POST /api/temperature/emergency/stop - Emergency stop
    device_server->on("/api/temperature/emergency/stop", HTTP_POST, []() {
        simple_temp.emergencyStop();
        device_server->send(200, "application/json", "{\"status\":\"success\",\"message\":\"Emergency stop activated\"}");
    });
    
    // POST /api/temperature/emergency/clear - Clear emergency
    device_server->on("/api/temperature/emergency/clear", HTTP_POST, []() {
        simple_temp.clearEmergency();
        device_server->send(200, "application/json", "{\"status\":\"success\",\"message\":\"Emergency stop cleared\"}");
    });
    
    // GET /api/temperature/limits - Get emergency limits
    device_server->on("/api/temperature/limits", HTTP_GET, []() {
        StaticJsonDocument<128> doc;
        doc["emergency_high"] = simple_temp.getConfig().emergency_high;
        doc["emergency_low"] = simple_temp.getConfig().emergency_low;
        String response;
        serializeJson(doc, response);
        device_server->send(200, "application/json", response);
    });
    
    // POST /api/temperature/limits - Set emergency limits
    device_server->on("/api/temperature/limits", HTTP_POST, []() {
        if (device_server->hasArg("plain")) {
            StaticJsonDocument<128> doc;
            DeserializationError error = deserializeJson(doc, device_server->arg("plain"));
            if (!error && doc.containsKey("emergency_high") && doc.containsKey("emergency_low")) {
                simple_temp.setEmergencyLimits(doc["emergency_low"], doc["emergency_high"]);
                device_server->send(200, "application/json", "{\"status\":\"success\"}");
            } else {
                device_server->send(400, "application/json", "{\"status\":\"error\"}");
            }
        } else {
            device_server->send(400, "application/json", "{\"status\":\"error\",\"message\":\"No data provided\"}");
        }
    });
    
    // GET /api/temperature/timing - Get timing protection
    device_server->on("/api/temperature/timing", HTTP_GET, []() {
        StaticJsonDocument<128> doc;
        doc["min_on_time"] = simple_temp.getConfig().min_on_time;
        doc["min_off_time"] = simple_temp.getConfig().min_off_time;
        String response;
        serializeJson(doc, response);
        device_server->send(200, "application/json", response);
    });
    
    // POST /api/temperature/timing - Set timing protection
    device_server->on("/api/temperature/timing", HTTP_POST, []() {
        if (device_server->hasArg("plain")) {
            StaticJsonDocument<128> doc;
            DeserializationError error = deserializeJson(doc, device_server->arg("plain"));
            if (!error && doc.containsKey("min_on_time") && doc.containsKey("min_off_time")) {
                simple_temp.setTimingProtection(doc["min_on_time"], doc["min_off_time"]);
                device_server->send(200, "application/json", "{\"status\":\"success\"}");
            } else {
                device_server->send(400, "application/json", "{\"status\":\"error\"}");
            }
        } else {
            device_server->send(400, "application/json", "{\"status\":\"error\",\"message\":\"No data provided\"}");
        }
    });
    
    // GET /api/temperature/relays - Get relay assignments
    device_server->on("/api/temperature/relays", HTTP_GET, []() {
        StaticJsonDocument<256> doc;
        doc["compressor"] = simple_temp.getConfig().compressor_relay;
        doc["heater"] = simple_temp.getConfig().heater_relay;
        doc["fan"] = simple_temp.getConfig().fan_relay;
        doc["aux"] = simple_temp.getConfig().aux_relay;
        String response;
        serializeJson(doc, response);
        device_server->send(200, "application/json", response);
    });
    
    // POST /api/temperature/relays - Set relay assignments
    device_server->on("/api/temperature/relays", HTTP_POST, []() {
        if (device_server->hasArg("plain")) {
            StaticJsonDocument<256> doc;
            DeserializationError error = deserializeJson(doc, device_server->arg("plain"));
            if (!error && doc.containsKey("compressor") && doc.containsKey("heater") && 
                doc.containsKey("fan") && doc.containsKey("aux")) {
                simple_temp.setRelayAssignments(
                    doc["compressor"], 
                    doc["heater"],
                    doc["fan"],
                    doc["aux"]
                );
                device_server->send(200, "application/json", "{\"status\":\"success\"}");
            } else {
                device_server->send(400, "application/json", "{\"status\":\"error\"}");
            }
        } else {
            device_server->send(400, "application/json", "{\"status\":\"error\",\"message\":\"No data provided\"}");
        }
    });
    
    // GET /api/temperature/statistics - Get statistics
    device_server->on("/api/temperature/statistics", HTTP_GET, []() {
        StaticJsonDocument<256> doc;
        doc["runtime_hours"] = simple_temp.getConfig().total_runtime / 3600000.0;
        doc["compressor_cycles"] = simple_temp.getConfig().compressor_cycles;
        doc["min_temp"] = simple_temp.getConfig().min_temp_recorded;
        doc["max_temp"] = simple_temp.getConfig().max_temp_recorded;
        String response;
        serializeJson(doc, response);
        device_server->send(200, "application/json", response);
    });
    
    // POST /api/temperature/reset - Reset to defaults
    device_server->on("/api/temperature/reset", HTTP_POST, []() {
        simple_temp.resetToDefaults();
        simple_temp.saveConfig();
        device_server->send(200, "application/json", "{\"status\":\"success\",\"message\":\"Reset to defaults\"}");
    });
    
    // GET /api/relay/status - Get all relay states
    device_server->on("/api/relay/status", HTTP_GET, []() {
        StaticJsonDocument<256> doc;
        JsonArray relays = doc.createNestedArray("relays");
        for (int i = 0; i < NUM_RELAYS; i++) {
            JsonObject relay = relays.createNestedObject();
            relay["id"] = i;
            relay["state"] = relay_controller.getRelayState(i);
        }
        String response;
        serializeJson(doc, response);
        device_server->send(200, "application/json", response);
    });
    
    // POST /api/relay/set - Set relay state
    device_server->on("/api/relay/set", HTTP_POST, []() {
        if (device_server->hasArg("plain")) {
            StaticJsonDocument<128> doc;
            DeserializationError error = deserializeJson(doc, device_server->arg("plain"));
            if (!error && doc.containsKey("relay") && doc.containsKey("state")) {
                uint8_t relay = doc["relay"];
                bool state = doc["state"];
                if (relay < NUM_RELAYS) {
                    relay_controller.setRelay(relay, state);
                    device_server->send(200, "application/json", "{\"status\":\"success\"}");
                } else {
                    device_server->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid relay number\"}");
                }
            } else {
                device_server->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid request\"}");
            }
        } else {
            device_server->send(400, "application/json", "{\"status\":\"error\",\"message\":\"No data provided\"}");
        }
    });
    
    // GET /api/inputs/status - Get digital input states
    device_server->on("/api/inputs/status", HTTP_GET, []() {
        relay_controller.updateInputs();
        StaticJsonDocument<256> doc;
        JsonArray inputs = doc.createNestedArray("inputs");
        for (int i = 0; i < NUM_DIGITAL_INPUTS; i++) {
            JsonObject input = inputs.createNestedObject();
            input["id"] = i;
            input["state"] = relay_controller.getInputState(i);
        }
        String response;
        serializeJson(doc, response);
        device_server->send(200, "application/json", response);
    });
    
    // GET /api/system/info - Get system information
    device_server->on("/api/system/info", HTTP_GET, []() {
        StaticJsonDocument<512> doc;
        doc["firmware_version"] = FIRMWARE_VERSION;
        doc["system_name"] = SYSTEM_NAME;
        doc["manufacturer"] = MANUFACTURER;
        doc["uptime"] = millis() / 1000;
        doc["free_heap"] = ESP.getFreeHeap();
        doc["chip_id"] = ESP.getEfuseMac();
        String response;
        serializeJson(doc, response);
        device_server->send(200, "application/json", response);
    });
    
    // GET /api/health - Health check endpoint
    device_server->on("/api/health", HTTP_GET, []() {
        StaticJsonDocument<256> doc;
        doc["status"] = "healthy";
        doc["timestamp"] = millis();
        doc["temperature_sensor"] = simple_temp.isSensorValid();
        doc["relay_controller"] = relay_controller.isRelayControllerConnected();
        String response;
        serializeJson(doc, response);
        device_server->send(200, "application/json", response);
    });
    
    // Serve the temperature control page
    device_server->on("/temperature", HTTP_GET, []() {
        device_server->send_P(200, "text/html", temperature_html);
    });
}

// Professional Network Management API
// Integrates with existing webserver_api.h structure
void setupNetworkAPI() {
    if (!device_server) return;
    
    // GET /api/wifi/status - Current WiFi status
    device_server->on("/api/wifi/status", HTTP_GET, []() {
        DynamicJsonDocument doc(512);
        doc["connected"] = wifiPro.isConnected();
        doc["ssid"] = wifiPro.getSSID();
        doc["ip_address"] = wifiPro.getIP();
        doc["rssi"] = wifiPro.getRSSI();
        doc["signal_quality"] = wifiPro.getConnectionQuality();
        doc["uptime_seconds"] = wifiPro.getUptime() / 1000;
        doc["connection_time_ms"] = wifiPro.getConnectionTime();
        doc["mac_address"] = WiFi.macAddress();
        doc["hostname"] = WiFi.getHostname();
        
        if (wifiPro.isConnected()) {
            doc["gateway"] = WiFi.gatewayIP().toString();
            doc["dns"] = WiFi.dnsIP().toString();
            doc["subnet"] = WiFi.subnetMask().toString();
        }
        
        String response;
        serializeJson(doc, response);
        device_server->send(200, "application/json", response);
    });
    
    // GET /api/wifi/scan - Scan available networks
    device_server->on("/api/wifi/scan", HTTP_GET, []() {
        DynamicJsonDocument doc(2048);
        JsonArray networks = doc.createNestedArray("networks");
        
        int n = wifiPro.scanNetworks();
        doc["scan_count"] = n;
        
        for (int i = 0; i < n && i < 20; i++) {  // Limit to 20 networks
            JsonObject network = networks.createNestedObject();
            network["ssid"] = wifiPro.getScannedSSID(i);
            network["rssi"] = wifiPro.getScannedRSSI(i);
            network["encryption"] = wifiPro.getScannedEncryption(i);
            network["channel"] = WiFi.channel(i);
            
            // Add signal quality indicator
            int rssi = wifiPro.getScannedRSSI(i);
            if (rssi >= -50) network["quality"] = "excellent";
            else if (rssi >= -60) network["quality"] = "good";
            else if (rssi >= -70) network["quality"] = "fair";
            else if (rssi >= -80) network["quality"] = "weak";
            else network["quality"] = "very_weak";
        }
        
        String response;
        serializeJson(doc, response);
        device_server->send(200, "application/json", response);
    });
    
    // POST /api/wifi/connect - Connect to network
    device_server->on("/api/wifi/connect", HTTP_POST, []() {
        if (device_server->hasArg("plain")) {
            DynamicJsonDocument doc(256);
            DeserializationError error = deserializeJson(doc, device_server->arg("plain"));
            
            if (!error && doc.containsKey("ssid")) {
                String ssid = doc["ssid"];
                String password = doc["password"] | "";
                bool use_static = doc["use_static"] | false;
                
                bool success = false;
                
                if (use_static && doc.containsKey("static_ip") && 
                    doc.containsKey("gateway") && doc.containsKey("subnet")) {
                    // Static IP connection
                    IPAddress static_ip, gateway, subnet, dns;
                    static_ip.fromString(doc["static_ip"].as<String>());
                    gateway.fromString(doc["gateway"].as<String>());
                    subnet.fromString(doc["subnet"].as<String>());
                    dns.fromString(doc["dns"] | "8.8.8.8");
                    
                    success = wifiPro.connectWithStaticIP(ssid.c_str(), password.c_str(),
                                                         static_ip, gateway, subnet, dns);
                } else {
                    // DHCP connection
                    success = wifiPro.connectToNetwork(ssid.c_str(), password.c_str());
                }
                
                if (success) {
                    device_server->send(200, "application/json", 
                        "{\"status\":\"success\",\"message\":\"Connected successfully\"}");
                } else {
                    device_server->send(400, "application/json", 
                        "{\"status\":\"error\",\"message\":\"Connection failed\"}");
                }
            } else {
                device_server->send(400, "application/json", 
                    "{\"status\":\"error\",\"message\":\"Invalid request format\"}");
            }
        } else {
            device_server->send(400, "application/json", 
                "{\"status\":\"error\",\"message\":\"No data provided\"}");
        }
    });
    
    // POST /api/wifi/disconnect - Disconnect from current network
    device_server->on("/api/wifi/disconnect", HTTP_POST, []() {
        wifiPro.disconnect();
        device_server->send(200, "application/json", 
            "{\"status\":\"success\",\"message\":\"Disconnected from WiFi\"}");
    });
    
    // POST /api/wifi/reset - Reset WiFi settings
    device_server->on("/api/wifi/reset", HTTP_POST, []() {
        wifiPro.resetSettings();
        device_server->send(200, "application/json", 
            "{\"status\":\"success\",\"message\":\"WiFi settings reset\"}");
    });
    
    // POST /api/wifi/config-portal - Start configuration portal
    device_server->on("/api/wifi/config-portal", HTTP_POST, []() {
        wifiPro.startConfigPortal();
        device_server->send(200, "application/json", 
            "{\"status\":\"success\",\"message\":\"Configuration portal started\"}");
    });
    
    // GET /api/wifi/diagnostics - Network diagnostics
    device_server->on("/api/wifi/diagnostics", HTTP_GET, []() {
        DynamicJsonDocument doc(512);
        
        if (wifiPro.isConnected()) {
            doc["connected"] = true;
            doc["ping_gateway"] = wifiPro.pingHost(WiFi.gatewayIP());
            doc["ping_dns"] = wifiPro.pingHost(WiFi.dnsIP());
            doc["gateway_ip"] = WiFi.gatewayIP().toString();
            doc["dns_ip"] = WiFi.dnsIP().toString();
            doc["local_ip"] = WiFi.localIP().toString();
            doc["subnet_mask"] = WiFi.subnetMask().toString();
            doc["bssid"] = WiFi.BSSIDstr();
            doc["channel"] = WiFi.channel();
        } else {
            doc["connected"] = false;
            doc["message"] = "Not connected to WiFi";
        }
        
        String response;
        serializeJson(doc, response);
        device_server->send(200, "application/json", response);
    });
    
    // GET /api/wifi/info - Device network information
    device_server->on("/api/wifi/info", HTTP_GET, []() {
        DynamicJsonDocument doc(256);
        doc["hostname"] = WiFi.getHostname();
        doc["mac_address"] = WiFi.macAddress();
        doc["has_stored_credentials"] = wifiPro.hasStoredCredentials();
        doc["firmware_version"] = FIRMWARE_VERSION;
        doc["device_name"] = SYSTEM_NAME;
        doc["manufacturer"] = MANUFACTURER;
        
        String response;
        serializeJson(doc, response);
        device_server->send(200, "application/json", response);
    });
    
    DEBUG_PRINTLN("Network API endpoints configured");
}

