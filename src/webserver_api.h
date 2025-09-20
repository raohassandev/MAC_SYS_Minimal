#include <WebServer.h>
#include <ArduinoJson.h>
#include "simple_temp_control.h"
#include "relay_control.h"
#include "schedule_manager.h"
#include "config.h"
#include "temperature_html.h"
#include "wifi_professional.h"
#include "modbus_manager.h"

extern WebServer* device_server;
extern float currentTemperature;
// Forward declaration from main.cpp
void saveConfiguration();

void setupTemperatureAPI() {
    if (!device_server) return;
    
    // GET /api/temperature/status - Get current status (system-config based)
    device_server->on("/api/temperature/status", HTTP_GET, []() {
        StaticJsonDocument<640> doc;
        float raw = g_system_status.current_temperature;
        float comp = raw + g_system_config.delivery_compensation;
        const float manual_setpoint = g_system_config.ac_setpoint;
        const float schedule_setpoint = simple_temp.getConfig().setpoint;
        const bool schedule_global = schedule_manager.isScheduleActive();
        WeeklySchedule& schedule = schedule_manager.getZoneSchedule(0);
        const bool schedule_ready = schedule.enabled && schedule.active_events > 0;
        const bool using_schedule = (g_system_config.operation_mode == 1) && schedule_global && schedule_ready;
        const float active_setpoint = using_schedule ? schedule_setpoint : manual_setpoint;
        doc["location"] = "Main Unit";
        doc["enabled"] = g_system_config.ac_control_enabled;
        doc["mode"] = g_system_config.ac_control_enabled ? 2 : 0; // 2=COOLING, 0=OFF
        doc["current_temp"] = raw;
        doc["compensated_temp"] = comp;
        doc["setpoint"] = active_setpoint;
        doc["active_setpoint"] = active_setpoint;
        doc["manual_setpoint"] = manual_setpoint;
        doc["schedule_setpoint"] = schedule_setpoint;
        doc["delta"] = g_system_config.delta_temperature;
        doc["compensation"] = g_system_config.delivery_compensation;
        doc["sensor_valid"] = (raw > -50 && raw < 100);
        doc["emergency_stop"] = (g_system_status.state == STATE_ERROR);
        doc["operation_mode"] = g_system_config.operation_mode;
        doc["setpoint_source"] = using_schedule ? "schedule" : "direct";
        doc["schedule_active"] = schedule_global;
        doc["schedule_ready"] = schedule_ready;
        JsonObject state = doc.createNestedObject("state");
        state["compressor"] = g_system_status.compressor_running;
        state["heater"] = relay_controller.getRelayState(2);
        state["fan"] = relay_controller.getRelayState(1);
        doc["relay0"] = relay_controller.getRelayState(simple_temp.getConfig().compressor_relay);
        JsonObject stats = doc.createNestedObject("stats");
        stats["runtime_hours"] = 0.0;
        stats["cycles"] = 0;
        stats["min_temp"] = raw;
        stats["max_temp"] = raw;
        String response; serializeJson(doc, response);
        device_server->send(200, "application/json", response);
    });
    
    // GET /api/temperature/config - Get configuration (system-config based)
    device_server->on("/api/temperature/config", HTTP_GET, []() {
        StaticJsonDocument<512> doc;
        doc["location"] = "Main Unit";
        doc["setpoint"] = g_system_config.ac_setpoint;
        doc["delta"] = g_system_config.delta_temperature;
        doc["compensation"] = g_system_config.delivery_compensation;
        doc["mode"] = g_system_config.ac_control_enabled ? 2 : 0;
        doc["enabled"] = g_system_config.ac_control_enabled;
        JsonObject limits = doc.createNestedObject("limits");
        limits["emergency_high"] = 60.0;
        limits["emergency_low"] = -20.0;
        JsonObject timing = doc.createNestedObject("timing");
        unsigned long ms = (unsigned long)g_system_config.hvac.min_cycle_time * 1000UL;
        timing["min_on_time"] = ms;
        timing["min_off_time"] = ms;
        JsonObject relays = doc.createNestedObject("relays");
        relays["compressor"] = 0; relays["heater"] = 2; relays["fan"] = 1; relays["aux"] = 3;
        String response; serializeJson(doc, response);
        device_server->send(200, "application/json", response);
    });
    
    // POST /api/temperature/config - Update setpoint/delta/compensation
    device_server->on("/api/temperature/config", HTTP_POST, []() {
        if (!device_server->hasArg("plain")) {
            device_server->send(400, "application/json", "{\"status\":\"error\",\"message\":\"No data provided\"}");
            return;
        }
        StaticJsonDocument<256> doc;
        if (deserializeJson(doc, device_server->arg("plain"))) {
            device_server->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
            return;
        }
        if (doc.containsKey("setpoint")) {
            g_system_config.ac_setpoint = CONSTRAIN_TEMP(doc["setpoint"].as<float>());
            simple_temp.setSetpoint(g_system_config.ac_setpoint);
        }
        if (doc.containsKey("delta")) {
            g_system_config.delta_temperature = doc["delta"].as<float>();
            simple_temp.setDelta(g_system_config.delta_temperature);
        }
        if (doc.containsKey("compensation")) {
            g_system_config.delivery_compensation = doc["compensation"].as<float>();
            simple_temp.setCompensation(g_system_config.delivery_compensation);
        }
        simple_temp.saveConfig();
        saveConfiguration();
        device_server->send(200, "application/json", "{\"status\":\"success\"}");
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
        doc["setpoint"] = g_system_config.ac_setpoint;
        doc["delta"] = g_system_config.delta_temperature;
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
                    float setpoint = CONSTRAIN_TEMP(doc["setpoint"].as<float>());
                    g_system_config.ac_setpoint = setpoint;
                    simple_temp.setSetpoint(setpoint);
                }
                if (doc.containsKey("delta")) {
                    float delta = doc["delta"].as<float>();
                    g_system_config.delta_temperature = delta;
                    simple_temp.setDelta(delta);
                }
                // simple_temp setters persist automatically
                saveConfiguration();
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
        doc["compensation"] = g_system_config.delivery_compensation;
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
                g_system_config.delivery_compensation = doc["compensation"].as<float>();
                simple_temp.setCompensation(g_system_config.delivery_compensation);
                simple_temp.saveConfig();
                saveConfiguration();
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
        doc["mode"] = g_system_config.ac_control_enabled ? 2 : 0;
        doc["mode_name"] = "";
        switch(simple_temp.getConfig().mode) {
            case TEMP_MODE_OFF: doc["mode_name"] = "OFF"; break;
            case TEMP_MODE_HEATING: doc["mode_name"] = "HEATING"; break;
            case TEMP_MODE_COOLING: doc["mode_name"] = "COOLING"; break;
            case TEMP_MODE_AUTO: doc["mode_name"] = "AUTO"; break;
            case TEMP_MODE_FAN_ONLY: doc["mode_name"] = "FAN_ONLY"; break;
            case TEMP_MODE_MANUAL: doc["mode_name"] = "MANUAL"; break;
        }
        doc["enabled"] = g_system_config.ac_control_enabled;
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
                if (doc.containsKey("enabled")) {
                    bool enabled = doc["enabled"].as<bool>();
                    g_system_config.ac_control_enabled = enabled;
                    simple_temp.setEnabled(enabled);
                }
                if (doc.containsKey("mode") && doc["mode"].as<int>() == 0) {
                    g_system_config.ac_control_enabled = false;
                    simple_temp.setEnabled(false);
                }
                saveConfiguration();
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
        g_system_config.ac_control_enabled = true;
        simple_temp.setEnabled(true);
        saveConfiguration();
        device_server->send(200, "application/json", "{\"status\":\"success\",\"enabled\":true}");
    });
    
    // POST /api/temperature/disable - Disable system
    device_server->on("/api/temperature/disable", HTTP_POST, []() {
        g_system_config.ac_control_enabled = false;
        simple_temp.setEnabled(false);
        saveConfiguration();
        device_server->send(200, "application/json", "{\"status\":\"success\",\"enabled\":false}");
    });
    
    // POST /api/temperature/emergency/stop - Emergency stop
    device_server->on("/api/temperature/emergency/stop", HTTP_POST, []() {
        emergencyStop();
        device_server->send(200, "application/json", "{\"status\":\"success\",\"message\":\"Emergency stop activated\"}");
    });
    
    // POST /api/temperature/emergency/clear - Clear emergency
    device_server->on("/api/temperature/emergency/clear", HTTP_POST, []() {
        g_system_status.state = STATE_RUNNING;
        device_server->send(200, "application/json", "{\"status\":\"success\",\"message\":\"Emergency stop cleared\"}");
    });
    
    // GET /api/temperature/limits - Get emergency limits
    device_server->on("/api/temperature/limits", HTTP_GET, []() {
        StaticJsonDocument<128> doc;
        doc["emergency_high"] = 60.0;
        doc["emergency_low"] = -20.0;
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
        unsigned long ms = (unsigned long)g_system_config.hvac.min_cycle_time * 1000UL;
        doc["min_on_time"] = ms;
        doc["min_off_time"] = ms;
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
                unsigned long min_on_ms = doc["min_on_time"].as<unsigned long>();
                unsigned long min_off_ms = doc["min_off_time"].as<unsigned long>();
                unsigned long min_ms2 = (min_on_ms < min_off_ms) ? min_on_ms : min_off_ms;
                g_system_config.hvac.min_cycle_time = (uint16_t)constrain(min_ms2 / 1000UL, 0UL, 65535UL);
                saveConfiguration();
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

// Modbus TCP API endpoints
void setupModbusAPI() {
    if (!device_server) return;
    
    // GET /api/modbus/status - Get Modbus server status
    device_server->on("/api/modbus/status", HTTP_GET, []() {
        DynamicJsonDocument doc(1024);
        
        doc["enabled"] = isModbusEnabled();
        doc["running"] = modbus_server.isRunning();
        doc["port"] = modbus_server.getPort();
        doc["uptime_ms"] = modbus_server.getUptime();
        doc["active_clients"] = modbus_server.getActiveClientCount();
        doc["total_connections"] = modbus_server.getTotalConnections();
        
        const ModbusStatistics& stats = modbus_server.getStatistics();
        JsonObject statistics = doc.createNestedObject("statistics");
        statistics["total_requests"] = stats.total_requests;
        statistics["successful_requests"] = stats.successful_requests;
        statistics["exception_responses"] = stats.exception_responses;
        statistics["avg_response_time_us"] = stats.average_response_time_us;
        statistics["max_response_time_us"] = stats.max_response_time_us;
        statistics["illegal_function"] = stats.illegal_function;
        statistics["illegal_address"] = stats.illegal_address;
        statistics["illegal_value"] = stats.illegal_value;
        statistics["device_failure"] = stats.device_failure;
        
        String response;
        serializeJson(doc, response);
        device_server->send(200, "application/json", response);
    });
    
    // GET /api/modbus/clients - Get connected client list
    device_server->on("/api/modbus/clients", HTTP_GET, []() {
        device_server->send(200, "text/plain", modbus_server.getClientList());
    });
    
    // POST /api/modbus/enable - Enable Modbus server
    device_server->on("/api/modbus/enable", HTTP_POST, []() {
        enableModbus(true);
        device_server->send(200, "application/json", 
            "{\"status\":\"success\",\"message\":\"Modbus TCP server enabled\"}");
    });
    
    // POST /api/modbus/disable - Disable Modbus server
    device_server->on("/api/modbus/disable", HTTP_POST, []() {
        enableModbus(false);
        device_server->send(200, "application/json", 
            "{\"status\":\"success\",\"message\":\"Modbus TCP server disabled\"}");
    });
    
    // POST /api/modbus/disconnect - Disconnect all clients
    device_server->on("/api/modbus/disconnect", HTTP_POST, []() {
        modbus_server.disconnectAllClients();
        device_server->send(200, "application/json", 
            "{\"status\":\"success\",\"message\":\"All Modbus clients disconnected\"}");
    });
    
    // GET /api/modbus/registers/dump - Dump register values for debugging
    device_server->on("/api/modbus/registers/dump", HTTP_GET, []() {
        String register_type = device_server->arg("type");
        String start_str = device_server->arg("start");
        String count_str = device_server->arg("count");
        
        uint8_t type = register_type.toInt();
        uint16_t start = start_str.toInt();
        uint16_t count = count_str.isEmpty() ? 10 : count_str.toInt();
        
        if (type < 1 || type > 4) {
            device_server->send(400, "application/json", 
                "{\"status\":\"error\",\"message\":\"Invalid register type (1-4)\"}");
            return;
        }
        
        String dump = modbus_registers.getRegisterDump(type, start, count);
        device_server->send(200, "text/plain", dump);
    });
    
    // POST /api/modbus/registers/clear - Clear registers (for testing)
    device_server->on("/api/modbus/registers/clear", HTTP_POST, []() {
        String register_type = device_server->arg("type");
        uint8_t type = register_type.toInt();
        
        if (type < 1 || type > 4) {
            device_server->send(400, "application/json", 
                "{\"status\":\"error\",\"message\":\"Invalid register type (1-4)\"}");
            return;
        }
        
        modbus_registers.clearRegisters(type);
        device_server->send(200, "application/json", 
            "{\"status\":\"success\",\"message\":\"Registers cleared\"}");
    });
    
    // GET /api/modbus/diagnostics - Get detailed diagnostics
    device_server->on("/api/modbus/diagnostics", HTTP_GET, []() {
        device_server->send(200, "text/plain", modbus_server.getDiagnostics());
    });
    
    // GET /modbus/manual - Download Modbus TCP User Manual
    device_server->on("/modbus/manual", HTTP_GET, []() {
        // Set headers for file download
        device_server->sendHeader("Content-Disposition", "attachment; filename=\"MAC-SYS_Modbus_TCP_Manual.txt\"");
        device_server->sendHeader("Content-Type", "text/plain");
        
        // Send the comprehensive manual content
        String manual_content = "MAC-SYS Arduino COMPLETE Modbus TCP Manual\n";
        manual_content += "=============================================\n\n";
        manual_content += "QUICK CONNECTION GUIDE\n";
        manual_content += "======================\n";
        manual_content += "IP Address: " + WiFi.localIP().toString() + " (your device IP)\n";
        manual_content += "Port: 502 (Standard Modbus TCP)\n";
        manual_content += "Protocol: Modbus TCP/IP\n";
        manual_content += "Unit ID: 1\n";
        manual_content += "Max Clients: 5 concurrent connections\n\n";
        
        manual_content += "FUNCTION CODE SUPPORT\n";
        manual_content += "=====================\n";
        manual_content += "Function Code 01: Read Coils ✓\n";
        manual_content += "Function Code 02: Read Discrete Inputs ✓\n";
        manual_content += "Function Code 03: Read Holding Registers ✓\n";
        manual_content += "Function Code 04: Read Input Registers ✓\n";
        manual_content += "Function Code 05: Write Single Coil ✓\n";
        manual_content += "Function Code 06: Write Single Register ✓\n";
        manual_content += "Function Code 15: Write Multiple Coils ✓\n";
        manual_content += "Function Code 16: Write Multiple Registers ✓\n\n";
        
        manual_content += "COMPLETE REGISTER MAPPING\n";
        manual_content += "==========================\n";
        manual_content += "Total Registers: 2110 (320 Coils + 340 Discrete + 400 Input + 1050 Holding)\n\n";
        
        // COILS Section
        manual_content += "COILS (00001-00320) - Digital Outputs [Read/Write]\n";
        manual_content += "---------------------------------------------------\n";
        manual_content += "Relay Control (00001-00006):\n";
        manual_content += "00001: Relay 1 Control (Compressor)\n";
        manual_content += "00002: Relay 2 Control (Fan)\n";
        manual_content += "00003: Relay 3 Control (Heater)\n";
        manual_content += "00004: Relay 4 Control (Aux 1)\n";
        manual_content += "00005: Relay 5 Control (Aux 2)\n";
        manual_content += "00006: Relay 6 Control (Aux 3)\n\n";
        
        manual_content += "System Control (00011-00020):\n";
        manual_content += "00011: System Enable/Disable\n";
        manual_content += "00012: Manual/Auto Mode (0=Auto, 1=Manual)\n";
        manual_content += "00013: Emergency Stop Reset\n";
        manual_content += "00014: Maintenance Mode\n";
        manual_content += "00015: Test Mode Enable\n";
        manual_content += "00016: Alarm Silence\n";
        manual_content += "00017: Force Cooling\n";
        manual_content += "00018: Force Heating\n";
        manual_content += "00019: Economy Mode\n";
        manual_content += "00020: Service Mode\n\n";
        
        manual_content += "Schedule Control (00021-00030):\n";
        manual_content += "00021: Schedule Enable/Disable\n";
        manual_content += "00022: Schedule Override\n";
        manual_content += "00023: Vacation Mode\n";
        manual_content += "00024: Boost Mode\n";
        manual_content += "00025: Night Setback Enable\n";
        manual_content += "00026: Holiday Mode\n";
        manual_content += "00027: Adaptive Control\n";
        manual_content += "00028: Remote Control Enable\n";
        manual_content += "00029: Auto Fan Control\n";
        manual_content += "00030: Energy Save Mode\n\n";
        
        // DISCRETE INPUTS Section  
        manual_content += "DISCRETE INPUTS (10001-10340) - Digital Status [Read Only]\n";
        manual_content += "------------------------------------------------------------\n";
        manual_content += "System Status (10001-10020):\n";
        manual_content += "10001: System Running Status\n";
        manual_content += "10002: Compressor Running Status\n";
        manual_content += "10003: Fan Running Status\n";
        manual_content += "10004: Heater Running Status\n";
        manual_content += "10005: Emergency Stop Active\n";
        manual_content += "10006: Maintenance Mode Active\n";
        manual_content += "10007: Auto Mode Active\n";
        manual_content += "10008: Schedule Active\n";
        manual_content += "10009: Alarm Condition Present\n";
        manual_content += "10010: Communication OK\n";
        manual_content += "10011: Temperature OK (within limits)\n";
        manual_content += "10012: High Temperature Alarm\n";
        manual_content += "10013: Low Temperature Alarm\n";
        manual_content += "10014: Temperature Sensor Fault\n";
        manual_content += "10015: Power Supply OK\n";
        manual_content += "10016: WiFi Connected\n";
        manual_content += "10017: Network Connected\n";
        manual_content += "10018: Time Synchronized\n";
        manual_content += "10019: Configuration Valid\n";
        manual_content += "10020: System Healthy\n\n";
        
        manual_content += "Sensor Availability (10031-10040):\n";
        manual_content += "10031: DS18B20 #1 Available\n";
        manual_content += "10032: DS18B20 #2 Available\n";
        manual_content += "10033: DS18B20 #3 Available\n";
        manual_content += "10034: AM2302 Available\n";
        manual_content += "10035: LM35 Available\n";
        manual_content += "10036: I2C Sensors Available\n";
        manual_content += "10037: Analog Sensors Available\n";
        manual_content += "10038: External Sensors Available\n";
        manual_content += "10039: Backup Sensor Available\n";
        manual_content += "10040: Sensor Calibration OK\n\n";
        
        // INPUT REGISTERS Section
        manual_content += "INPUT REGISTERS (30001-30400) - Sensor Data [Read Only]\n";
        manual_content += "---------------------------------------------------------\n";
        manual_content += "Temperature Sensors (30001-30030, °C × 10):\n";
        manual_content += "30001: DS18B20 #1 Temperature\n";
        manual_content += "30002: DS18B20 #2 Temperature\n";
        manual_content += "30003: DS18B20 #3 Temperature\n";
        manual_content += "30004-30010: Reserved for additional DS18B20\n";
        manual_content += "30011: AM2302 Temperature\n";
        manual_content += "30012: AM2302 Humidity (% × 10)\n";
        manual_content += "30013-30020: Reserved for AM2302 expansion\n";
        manual_content += "30021: LM35 Temperature\n";
        manual_content += "30022-30030: Reserved for analog sensors\n\n";
        
        manual_content += "System Measurements (30101-30130):\n";
        manual_content += "30101: Supply Voltage (V × 100)\n";
        manual_content += "30102: Current Consumption (mA)\n";
        manual_content += "30103: Power Consumption (W × 10)\n";
        manual_content += "30104: ESP32 Core Temperature (°C × 10)\n";
        manual_content += "30105: Ambient Temperature (°C × 10)\n";
        manual_content += "30106: CPU Usage (%)\n";
        manual_content += "30107: Memory Usage (%)\n";
        manual_content += "30108: WiFi Signal Strength (dBm + 128)\n";
        manual_content += "30109: Network Latency (ms)\n";
        manual_content += "30110: Flash Usage (%)\n\n";
        
        manual_content += "Runtime Statistics (30201-30250):\n";
        manual_content += "30201: System Uptime (minutes)\n";
        manual_content += "30202: Compressor Runtime (minutes)\n";
        manual_content += "30203: Fan Runtime (minutes)\n";
        manual_content += "30204: Heater Runtime (minutes)\n";
        manual_content += "30205: Total Cycles Count\n";
        manual_content += "30206: Heating Cycles\n";
        manual_content += "30207: Cooling Cycles\n";
        manual_content += "30208: Manual Overrides\n";
        manual_content += "30209: Alarm Events\n";
        manual_content += "30210: Communication Errors\n\n";
        
        manual_content += "Error Diagnostics (30351-30370):\n";
        manual_content += "30351: Last Error Code\n";
        manual_content += "30352: Total Error Count\n";
        manual_content += "30353: Communication Errors\n";
        manual_content += "30354: Sensor Errors\n";
        manual_content += "30355: System Errors\n";
        manual_content += "30356: Configuration Errors\n";
        manual_content += "30357: Hardware Errors\n";
        manual_content += "30358: Software Errors\n";
        manual_content += "30359: Network Errors\n";
        manual_content += "30360: Power Errors\n\n";
        
        // HOLDING REGISTERS Section
        manual_content += "HOLDING REGISTERS (40001-41050) - Configuration [Read/Write]\n";
        manual_content += "--------------------------------------------------------------\n";
        manual_content += "Temperature Control (40001-40020):\n";
        manual_content += "40001: Temperature Setpoint (°C × 10)\n";
        manual_content += "40002: Control Delta/Hysteresis (°C × 10)\n";
        manual_content += "40003: Control Mode (0=Off, 1=Cool, 2=Heat, 3=Auto)\n";
        manual_content += "40004: Min Temperature Limit (°C × 10)\n";
        manual_content += "40005: Max Temperature Limit (°C × 10)\n";
        manual_content += "40006: Heating Setpoint Offset (°C × 10)\n";
        manual_content += "40007: Cooling Setpoint Offset (°C × 10)\n";
        manual_content += "40008: Deadband Temperature (°C × 10)\n";
        manual_content += "40009: Control Loop Period (seconds)\n";
        manual_content += "40010: Temperature Filter Constant\n\n";
        
        manual_content += "Sensor Configuration (40011-40030):\n";
        manual_content += "40011: Active Sensor (0=DS18B20_1, 1=DS18B20_2, 2=DS18B20_3, 3=AM2302, 4=LM35)\n";
        manual_content += "40012: Sensor Read Interval (seconds)\n";
        manual_content += "40013: Sensor Filter Time Constant\n";
        manual_content += "40014: Temperature Offset Compensation (°C × 10)\n";
        manual_content += "40015: Humidity Offset Compensation (% × 10)\n";
        manual_content += "40016: Sensor Timeout (seconds)\n";
        manual_content += "40017: Sensor Retry Count\n";
        manual_content += "40018: Calibration Factor (× 1000)\n";
        manual_content += "40019: Backup Sensor Enable\n";
        manual_content += "40020: Sensor Auto-Switch Enable\n\n";
        
        manual_content += "Safety & Limits (40021-40040):\n";
        manual_content += "40021: Emergency High Temperature (°C × 10)\n";
        manual_content += "40022: Emergency Low Temperature (°C × 10)\n";
        manual_content += "40023: Maximum Runtime (minutes)\n";
        manual_content += "40024: Minimum On Time (seconds)\n";
        manual_content += "40025: Minimum Off Time (seconds)\n";
        manual_content += "40026: Compressor Protection Delay (minutes)\n";
        manual_content += "40027: Maximum Start Attempts\n";
        manual_content += "40028: Alarm Delay Time (seconds)\n";
        manual_content += "40029: Emergency Shutdown Enable\n";
        manual_content += "40030: Safety Interlock Enable\n\n";
        
        manual_content += "Schedule Configuration - Entry 1 (40101-40107):\n";
        manual_content += "40101: Start Hour (0-23)\n";
        manual_content += "40102: Start Minute (0-59)\n";
        manual_content += "40103: Stop Hour (0-23)\n";
        manual_content += "40104: Stop Minute (0-59)\n";
        manual_content += "40105: Active Temperature (°C × 10)\n";
        manual_content += "40106: Inactive Temperature (°C × 10)\n";
        manual_content += "40107: Days of Week Mask (bit field: Mon=1, Tue=2, Wed=4, etc.)\n\n";
        
        manual_content += "Network Configuration (40201-40220):\n";
        manual_content += "40201: DHCP Enable (0=Static, 1=DHCP)\n";
        manual_content += "40202: IP Address Byte 1 (if static)\n";
        manual_content += "40203: IP Address Byte 2\n";
        manual_content += "40204: IP Address Byte 3\n";
        manual_content += "40205: IP Address Byte 4\n";
        manual_content += "40206: Subnet Mask (/24, /16, etc.)\n";
        manual_content += "40207: Gateway Byte 1\n";
        manual_content += "40208: Gateway Byte 2\n";
        manual_content += "40209: Gateway Byte 3\n";
        manual_content += "40210: Gateway Byte 4\n\n";
        
        manual_content += "DATA SCALING & FORMATS\n";
        manual_content += "======================\n";
        manual_content += "- Temperature Values: Raw ÷ 10 = °C (235 = 23.5°C)\n";
        manual_content += "- Humidity Values: Raw ÷ 10 = %RH (655 = 65.5%)\n";
        manual_content += "- Voltage Values: Raw ÷ 100 = V (1200 = 12.00V)\n";
        manual_content += "- Power Values: Raw ÷ 10 = W (125 = 12.5W)\n";
        manual_content += "- Invalid/Error: 32767 (0x7FFF)\n";
        manual_content += "- Boolean: 0=False/Off, 1=True/On\n\n";
        
        manual_content += "EXAMPLE MODBUS OPERATIONS\n";
        manual_content += "==========================\n";
        manual_content += "Read current temp: FC04, Addr=30001, Qty=1\n";
        manual_content += "Set temperature: FC06, Addr=40001, Value=245 (24.5°C)\n";
        manual_content += "Turn on compressor: FC05, Addr=00001, Value=1\n";
        manual_content += "Read all temps: FC04, Addr=30001, Qty=21\n";
        manual_content += "Read system status: FC02, Addr=10001, Qty=20\n";
        manual_content += "Control multiple relays: FC15, Addr=00001, Qty=6\n\n";
        
        manual_content += "ERROR CODES\n";
        manual_content += "===========\n";
        manual_content += "0: No Error\n";
        manual_content += "1: Temperature Sensor Error\n";
        manual_content += "2: Communication Error\n";
        manual_content += "3: Configuration Error\n";
        manual_content += "4: Safety Limit Exceeded\n";
        manual_content += "5: Hardware Fault\n";
        manual_content += "6: Network Error\n";
        manual_content += "7: Power Supply Error\n";
        manual_content += "8: Calibration Error\n";
        manual_content += "9: System Overload\n";
        manual_content += "10: Emergency Stop Active\n\n";
        
        manual_content += "TROUBLESHOOTING\n";
        manual_content += "===============\n";
        manual_content += "Connection Issues:\n";
        manual_content += "✓ Check IP: " + WiFi.localIP().toString() + "\n";
        manual_content += "✓ Verify port 502 not blocked\n";
        manual_content += "✓ Use Unit ID = 1\n";
        manual_content += "✓ Max 5 concurrent clients\n";
        manual_content += "✓ Check function code support\n";
        manual_content += "✓ Verify address ranges\n\n";
        
        manual_content += "Data Issues:\n";
        manual_content += "✓ Apply scaling (temps ÷ 10)\n";
        manual_content += "✓ Check for error value 32767\n";
        manual_content += "✓ Verify sensor connections\n";
        manual_content += "✓ Check emergency stop status\n";
        manual_content += "✓ Validate configuration\n\n";
        
        manual_content += "WEB INTERFACE URLS\n";
        manual_content += "==================\n";
        manual_content += "Dashboard: http://" + WiFi.localIP().toString() + "/\n";
        manual_content += "System: http://" + WiFi.localIP().toString() + "/system\n";
        manual_content += "Relays: http://" + WiFi.localIP().toString() + "/relays\n";
        manual_content += "Temperature: http://" + WiFi.localIP().toString() + "/temperature\n";
        manual_content += "Schedule: http://" + WiFi.localIP().toString() + "/schedule\n";
        manual_content += "Sensors: http://" + WiFi.localIP().toString() + "/sensors\n\n";
        
        manual_content += "API ENDPOINTS\n";
        manual_content += "=============\n";
        manual_content += "System API: /api/system\n";
        manual_content += "Temperature API: /api/temperature/status\n";
        manual_content += "Modbus Status: /api/modbus/status\n";
        manual_content += "Relay Control: /api/relays/control\n";
        manual_content += "Configuration: /api/config\n\n";
        
        manual_content += "DEVICE INFORMATION\n";
        manual_content += "==================\n";
        manual_content += "Generated: " + String(millis()/1000) + " seconds uptime\n";
        manual_content += "Model: MAC-SYS Arduino HVAC Controller\n";
        manual_content += "Protocol: Modbus TCP/IP (RFC 1006)\n";
        manual_content += "Manual Version: 3.0 (Complete Register Map)\n";
        
        device_server->send(200, "text/plain", manual_content);
    });
    
    DEBUG_PRINTLN("Modbus API endpoints configured");
}
