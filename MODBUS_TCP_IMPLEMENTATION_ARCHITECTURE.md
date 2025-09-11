# 🏗️ Modbus TCP Implementation Architecture

## 📋 Executive Summary

Complete implementation plan for professional Modbus TCP server integration into the MAC-SYS Arduino HVAC Controller. This architecture builds on the existing web server infrastructure while adding industrial-grade Modbus TCP capabilities for SCADA/BMS integration.

---

## 🎯 Implementation Goals

- **Professional Standards**: IEEE 802.3, Modbus TCP specification compliance
- **Industrial Integration**: Full SCADA/BMS compatibility with major platforms  
- **Complete Access**: Read/write access to all 2110 system registers
- **High Performance**: 5 concurrent clients, sub-100ms response times
- **Robust Operation**: Error handling, connection management, data validation

---

## 🔧 Current System Analysis

### Existing Architecture Strengths
✅ **Solid Foundation**: Well-structured web server using ESP32 WebServer  
✅ **Modular Design**: Separate files for different functionalities  
✅ **Comprehensive APIs**: 20+ RESTful endpoints already implemented  
✅ **Data Management**: SimpleTempController, relay_controller classes  
✅ **Network Stack**: WiFi Professional with diagnostics and auto-reconnect  

### Integration Points
- **Web Server**: Existing `device_server` (WebServer*) for HTTP on port 80
- **Data Sources**: All sensor/control data accessible via existing classes
- **Configuration**: EEPROM-based settings with validation
- **Error Handling**: Comprehensive error tracking and reporting system

---

## 🏛️ Implementation Architecture

### 1. Core Modbus TCP Server

**File Structure:**
```
src/modbus_tcp/
├── modbus_tcp_server.h         # Main server class
├── modbus_tcp_server.cpp       # TCP server implementation  
├── modbus_protocol.h           # Protocol definitions
├── modbus_protocol.cpp         # Frame processing
├── modbus_register_manager.h   # Register mapping
├── modbus_register_manager.cpp # Register access
└── modbus_client_manager.h     # Client connection management
```

### 2. Class Architecture

```cpp
// Main Modbus TCP Server Class
class ModbusTCPServer {
private:
    WiFiServer* tcp_server;
    ModbusClientManager client_manager;
    ModbusRegisterManager register_manager;
    ModbusProtocol protocol_handler;
    
public:
    bool begin(uint16_t port = 502);
    void handleClients();
    void processRequest(WiFiClient& client, uint8_t* request, size_t length);
    void sendResponse(WiFiClient& client, uint8_t* response, size_t length);
    
    // Statistics and monitoring
    ModbusStatistics getStatistics();
    String getDiagnostics();
};

// Client Connection Management
class ModbusClientManager {
private:
    struct ClientConnection {
        WiFiClient client;
        unsigned long last_activity;
        uint32_t client_ip;
        uint16_t transaction_id;
        bool active;
    };
    ClientConnection clients[MAX_MODBUS_CLIENTS];
    
public:
    bool addClient(WiFiClient& client);
    void removeClient(uint8_t slot);
    void handleTimeouts();
    uint8_t getActiveClientCount();
};

// Register Access Manager  
class ModbusRegisterManager {
private:
    uint16_t coils[MAX_COILS / 16];           // Bit-packed coils
    uint16_t discrete_inputs[MAX_DISCRETE / 16]; // Bit-packed inputs
    uint16_t input_registers[MAX_INPUT_REGS];    // Input registers
    uint16_t holding_registers[MAX_HOLDING_REGS]; // Holding registers
    
public:
    // Register access methods
    bool readCoils(uint16_t address, uint16_t quantity, uint8_t* data);
    bool readDiscreteInputs(uint16_t address, uint16_t quantity, uint8_t* data);
    bool readHoldingRegisters(uint16_t address, uint16_t quantity, uint16_t* data);
    bool readInputRegisters(uint16_t address, uint16_t quantity, uint16_t* data);
    
    bool writeCoil(uint16_t address, bool value);
    bool writeHoldingRegister(uint16_t address, uint16_t value);
    bool writeMultipleCoils(uint16_t address, uint16_t quantity, uint8_t* data);
    bool writeMultipleRegisters(uint16_t address, uint16_t quantity, uint16_t* data);
    
    // System integration
    void updateFromSystem();  // Read current system state
    void applyToSystem();     // Apply changes to system
};
```

### 3. Integration with Existing System

**Data Flow Integration:**
```cpp
// In main loop - alongside existing web server handling
void loop() {
    // Existing code
    handleDeviceWebServer();
    simple_temp.process();
    relay_controller.process();
    
    // NEW: Modbus TCP handling
    modbus_server.handleClients();
    
    // Existing system updates
    updateSystemStatus();
    esp_task_wdt_reset();
}

// Register Manager Integration
void ModbusRegisterManager::updateFromSystem() {
    // Temperature sensors (Input Registers 30001-30400)
    input_registers[0] = (uint16_t)(ds18b20_temp_1 * 10);  // 30001
    input_registers[1] = (uint16_t)(ds18b20_temp_2 * 10);  // 30002
    input_registers[10] = (uint16_t)(am2302_temp * 10);    // 30011
    input_registers[11] = (uint16_t)(am2302_humidity * 10); // 30012
    
    // System status (Discrete Inputs 10001-10340)
    setBit(discrete_inputs, 0, simple_temp.isCompressorOn());  // 10001
    setBit(discrete_inputs, 1, simple_temp.isFanOn());        // 10002
    setBit(discrete_inputs, 10, simple_temp.isSensorValid()); // 10011
    
    // Configuration (Holding Registers 40001-41050)
    holding_registers[0] = (uint16_t)(simple_temp.getConfig().setpoint * 10); // 40001
    holding_registers[1] = (uint16_t)(simple_temp.getConfig().delta_temp * 10); // 40002
    holding_registers[10] = (uint16_t)(getCurrentTempSensorType()); // 40011
}

void ModbusRegisterManager::applyToSystem() {
    // Apply setpoint changes
    float new_setpoint = holding_registers[0] / 10.0f;  // 40001
    if (new_setpoint != simple_temp.getConfig().setpoint) {
        simple_temp.setSetpoint(new_setpoint);
    }
    
    // Apply control mode changes  
    TempControlMode new_mode = (TempControlMode)holding_registers[2]; // 40003
    if (new_mode != simple_temp.getConfig().mode) {
        simple_temp.setMode(new_mode);
    }
    
    // Apply relay controls from coils
    if (getBit(coils, 0) != simple_temp.isCompressorOn()) {  // 00001
        simple_temp.setCompressorState(getBit(coils, 0));
    }
}
```

---

## 📊 Detailed Implementation Plan

### Phase 1: Core Infrastructure (Week 1-2)

**1.1 Modbus Protocol Handler**
```cpp
// modbus_protocol.h
class ModbusProtocol {
public:
    struct ModbusRequest {
        uint16_t transaction_id;
        uint16_t protocol_id;
        uint16_t length;
        uint8_t unit_id;
        uint8_t function_code;
        uint16_t address;
        uint16_t quantity;
        uint8_t* data;
    };
    
    struct ModbusResponse {
        uint16_t transaction_id;
        uint16_t protocol_id;
        uint16_t length;
        uint8_t unit_id;
        uint8_t function_code;
        uint8_t byte_count;
        uint8_t* data;
        uint8_t exception_code;
    };
    
    bool parseRequest(uint8_t* buffer, size_t length, ModbusRequest& request);
    size_t buildResponse(ModbusResponse& response, uint8_t* buffer);
    uint8_t validateRequest(ModbusRequest& request);
};
```

**1.2 TCP Server Foundation**
- Initialize WiFiServer on port 502
- Client connection management (max 5 clients)
- Request/response handling with proper framing
- Timeout management (connection idle timeout: 60s)

**1.3 Register Access Framework**
- Memory allocation for all register types
- Bit manipulation utilities for coils/discrete inputs  
- Address validation and range checking
- Error code generation (01-06 exception codes)

### Phase 2: Function Code Implementation (Week 2-3)

**2.1 Read Functions (01, 02, 03, 04)**
```cpp
// Function Code 01: Read Coils
uint8_t ModbusRegisterManager::handleReadCoils(uint16_t address, uint16_t quantity, uint8_t* response) {
    if (address >= MAX_COILS || address + quantity > MAX_COILS) {
        return 0x02; // Illegal Data Address
    }
    
    if (quantity == 0 || quantity > 0x7D0) {
        return 0x03; // Illegal Data Value  
    }
    
    uint8_t byte_count = (quantity + 7) / 8;
    response[0] = byte_count;
    
    // Pack bits into response bytes
    for (uint16_t i = 0; i < quantity; i++) {
        bool bit_value = getBit(coils, address + i);
        if (bit_value) {
            response[1 + (i / 8)] |= (1 << (i % 8));
        }
    }
    
    return 0x00; // No error
}
```

**2.2 Write Functions (05, 06, 15, 16)**  
```cpp
// Function Code 05: Write Single Coil
uint8_t ModbusRegisterManager::handleWriteSingleCoil(uint16_t address, uint16_t value) {
    if (address >= MAX_COILS) {
        return 0x02; // Illegal Data Address
    }
    
    if (value != 0x0000 && value != 0xFF00) {
        return 0x03; // Illegal Data Value
    }
    
    bool state = (value == 0xFF00);
    setBit(coils, address, state);
    
    // Apply to system immediately for critical controls
    if (address == 0) { // Compressor control
        simple_temp.setCompressorState(state);
    }
    
    return 0x00; // No error  
}
```

**2.3 Error Handling**
- Exception response generation
- Invalid address/quantity detection
- Function code validation
- Data value validation

### Phase 3: System Integration (Week 3-4)

**3.1 Real-time Data Updates**
```cpp
// Background task for register updates (1Hz for most data, 10Hz for critical)
void updateModbusRegisters() {
    static unsigned long last_update = 0;
    static unsigned long last_fast_update = 0;
    
    if (millis() - last_fast_update > 100) { // 10Hz critical updates
        // Temperature readings (Input Registers)
        register_manager.updateTemperatureRegisters();
        
        // System status (Discrete Inputs)  
        register_manager.updateStatusFlags();
        
        last_fast_update = millis();
    }
    
    if (millis() - last_update > 1000) { // 1Hz general updates
        // Configuration registers
        register_manager.updateConfigurationRegisters();
        
        // Statistics and diagnostics
        register_manager.updateDiagnosticRegisters();
        
        last_update = millis();
    }
}
```

**3.2 Configuration Persistence**
```cpp
// Auto-save critical configuration changes
void ModbusRegisterManager::writeHoldingRegister(uint16_t address, uint16_t value) {
    holding_registers[address] = value;
    
    // Auto-save critical configuration
    switch (address) {
        case 0:   // Temperature setpoint (40001)
        case 1:   // Delta temperature (40002)  
        case 2:   // Control mode (40003)
        case 10:  // Active sensor (40011)
            saveConfiguration();
            break;
    }
}
```

### Phase 4: Advanced Features (Week 4-5)

**4.1 Performance Optimization**
- Connection pooling and reuse
- Response caching for read-only registers
- Bulk register updates to minimize system calls
- Memory optimization for ESP32 constraints

**4.2 Security & Access Control**
```cpp
// IP whitelist for Modbus access
class ModbusAccessControl {
    IPAddress whitelist[MAX_WHITELIST_IPS];
    bool whitelist_enabled;
    
public:
    bool isClientAllowed(IPAddress client_ip);
    void addToWhitelist(IPAddress ip);
    void setWhitelistEnabled(bool enabled);
};
```

**4.3 Diagnostics & Monitoring**
```cpp
struct ModbusStatistics {
    uint32_t total_requests;
    uint32_t successful_requests;
    uint32_t exception_responses;
    uint32_t timeout_disconnects;
    uint32_t connection_count;
    uint16_t current_clients;
    unsigned long average_response_time;
    unsigned long max_response_time;
};
```

---

## 🔌 Integration Points

### Web Interface Integration
**New API Endpoints:**
```cpp
// GET /api/modbus/status - Modbus server status
device_server->on("/api/modbus/status", HTTP_GET, []() {
    StaticJsonDocument<256> doc;
    doc["enabled"] = modbus_server.isRunning();
    doc["port"] = 502;
    doc["clients"] = modbus_server.getActiveClientCount();
    doc["requests"] = modbus_server.getStatistics().total_requests;
    // ... more status info
});

// POST /api/modbus/enable - Enable/disable Modbus server
device_server->on("/api/modbus/enable", HTTP_POST, []() {
    // Enable/disable logic with validation
});
```

### Configuration Management
```cpp
// Add to existing EEPROM structure
struct ModbusConfig {
    bool enabled;
    uint16_t port;
    bool whitelist_enabled;
    IPAddress whitelist_ips[MAX_WHITELIST_IPS];
    uint8_t timeout_seconds;
    bool auto_save_config;
};

// Integration with existing saveConfiguration()
void saveConfiguration() {
    // Existing config save code...
    
    // Save Modbus configuration
    EEPROM.put(MODBUS_CONFIG_OFFSET, modbus_config);
    EEPROM.commit();
}
```

---

## 📈 Performance Specifications

### Response Time Targets
- **Read Operations**: < 50ms for up to 100 registers
- **Write Operations**: < 100ms including system updates
- **Connection Setup**: < 200ms for new client connections  
- **Error Responses**: < 10ms for exception responses

### Memory Usage
- **RAM**: ~15KB for all register storage + client buffers
- **Flash**: ~25KB for Modbus TCP implementation
- **EEPROM**: ~200 bytes for configuration storage

### Network Performance
- **Concurrent Clients**: 5 maximum (SCADA + 4 monitoring clients)
- **Request Rate**: 50+ requests/second sustained
- **Connection Timeout**: 60 seconds idle timeout
- **Keepalive**: TCP keepalive every 30 seconds

---

## 🛡️ Security & Reliability

### Security Features
1. **IP Whitelist**: Configurable allowed client IP addresses
2. **Rate Limiting**: Max 10 requests/second per client 
3. **Input Validation**: All register addresses and values validated
4. **Memory Protection**: Buffer overflow prevention
5. **Connection Limits**: Hard limit on concurrent connections

### Reliability Features  
1. **Error Recovery**: Automatic client disconnection on errors
2. **Watchdog Protection**: Integration with existing ESP32 WDT
3. **Memory Management**: Automatic cleanup of disconnected clients
4. **Data Consistency**: Atomic register updates where critical
5. **Graceful Degradation**: Web interface remains functional if Modbus fails

### Diagnostics
```cpp
// Built-in diagnostic registers (Input Registers 30351-30400)
30351: Modbus Server Status (0=Disabled, 1=Running, 2=Error)
30352: Active Client Count
30353: Total Request Count (Low Word)
30354: Total Request Count (High Word)  
30355: Exception Count
30356: Average Response Time (ms)
30357: Maximum Response Time (ms)
30358: Last Error Code
30359: Server Uptime (minutes, Low Word)
30360: Server Uptime (minutes, High Word)
```

---

## 🧪 Testing Strategy

### Unit Testing
- Protocol parsing/generation functions
- Register access validation  
- Error handling edge cases
- Memory management

### Integration Testing  
- Multi-client connection handling
- Concurrent read/write operations
- System integration (register updates)
- Error recovery scenarios

### Performance Testing
- Load testing with 5 concurrent clients
- Response time measurement under load
- Memory usage monitoring
- Long-duration stability testing

### Compatibility Testing
**SCADA Platforms:**
- ModbusPoll/Modscan32 (Windows)
- QModMaster (Cross-platform)
- pymodbus (Python)
- Wonderware InTouch
- Schneider Electric Vijeo Citect
- Siemens WinCC

---

## 📋 Implementation Checklist

### Core Infrastructure ✅
- [ ] ModbusTCPServer class foundation
- [ ] WiFiServer integration on port 502
- [ ] Client connection management
- [ ] Basic request/response handling

### Protocol Implementation
- [ ] Function Code 01: Read Coils
- [ ] Function Code 02: Read Discrete Inputs  
- [ ] Function Code 03: Read Holding Registers
- [ ] Function Code 04: Read Input Registers
- [ ] Function Code 05: Write Single Coil
- [ ] Function Code 06: Write Single Register
- [ ] Function Code 15: Write Multiple Coils
- [ ] Function Code 16: Write Multiple Registers

### Register Integration  
- [ ] Temperature sensor register mapping
- [ ] System control coil mapping
- [ ] Configuration holding register mapping
- [ ] Status discrete input mapping
- [ ] Real-time data synchronization

### System Integration
- [ ] Web interface API endpoints
- [ ] EEPROM configuration storage
- [ ] Error handling integration
- [ ] Performance optimization

### Testing & Validation
- [ ] Unit test suite
- [ ] SCADA compatibility testing  
- [ ] Performance benchmarking
- [ ] Long-term stability testing

---

## 🚀 Deployment Plan

### Development Environment
- **IDE**: PlatformIO with ESP32 support
- **Testing Tools**: ModbusPoll, QModMaster, pymodbus
- **Network Tools**: Wireshark for protocol analysis
- **Hardware**: ESP32-WROOM-32 with Ethernet capability

### Library Dependencies
```cpp
// No additional libraries required - built on ESP32 WiFi stack
#include <WiFi.h>        // Built-in ESP32 WiFi
#include <WiFiClient.h>  // TCP client handling  
#include <WiFiServer.h>  // TCP server
#include <ArduinoJson.h> // Existing dependency for web APIs
```

### Memory Allocation
```cpp
// Compile-time memory allocation
#define MAX_COILS 320
#define MAX_DISCRETE_INPUTS 340  
#define MAX_INPUT_REGISTERS 400
#define MAX_HOLDING_REGISTERS 1050
#define MAX_MODBUS_CLIENTS 5
#define MODBUS_BUFFER_SIZE 256
```

---

## 📞 Maintenance & Support

### Monitoring Capabilities
- Real-time client connection monitoring via web interface
- Request/response statistics and timing
- Error logging with timestamps
- Performance metrics dashboard

### Remote Diagnostics
- Modbus register access for all system parameters
- Network connectivity testing through existing APIs
- System health monitoring via dedicated registers
- Configuration backup/restore via Modbus

### Update Strategy
- Firmware updates preserve Modbus configuration
- Backward compatibility for register mappings  
- Graceful handling of version changes
- Migration utilities for configuration updates

---

**📋 Document Version**: 1.0  
**📅 Created**: Based on existing system analysis and MODBUS_REGISTER_MAP.md  
**🎯 Target**: Complete professional Modbus TCP implementation  
**⚡ Status**: Ready for implementation  

---

*This architecture provides a complete roadmap for implementing industrial-grade Modbus TCP server capabilities while preserving all existing system functionality and maintaining the high-quality codebase standards.*