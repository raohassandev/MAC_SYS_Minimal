#ifndef MODBUS_TCP_SERVER_H
#define MODBUS_TCP_SERVER_H

#include <Arduino.h>
#include <WiFiServer.h>
#include <WiFiClient.h>
#include "modbus_protocol.h"

#define MAX_MODBUS_CLIENTS 5
#define MODBUS_CLIENT_TIMEOUT 60000 // 60 seconds
#define MODBUS_BUFFER_SIZE 260

struct ClientConnection {
    WiFiClient client;
    unsigned long last_activity;
    uint32_t client_ip;
    uint16_t last_transaction_id;
    bool active;
    uint32_t request_count;
    unsigned long connect_time;
};

class ModbusRegisterManager; // Forward declaration

class ModbusTCPServer {
private:
    WiFiServer* tcp_server;
    ClientConnection clients[MAX_MODBUS_CLIENTS];
    ModbusProtocol protocol;
    ModbusRegisterManager* register_manager;
    ModbusStatistics stats;
    
    bool server_enabled;
    uint16_t server_port;
    unsigned long server_start_time;
    uint8_t request_buffer[MODBUS_BUFFER_SIZE];
    uint8_t response_buffer[MODBUS_BUFFER_SIZE];
    
    // Client management
    int8_t findFreeClientSlot();
    int8_t findClientSlot(WiFiClient& client);
    void removeClient(uint8_t slot);
    void handleClientTimeouts();
    void cleanupDisconnectedClients();
    
    // Request processing
    void processClientRequest(uint8_t client_slot);
    void handleReadCoils(const ModbusRequest& request, ModbusResponse& response);
    void handleReadDiscreteInputs(const ModbusRequest& request, ModbusResponse& response);
    void handleReadHoldingRegisters(const ModbusRequest& request, ModbusResponse& response);
    void handleReadInputRegisters(const ModbusRequest& request, ModbusResponse& response);
    void handleWriteSingleCoil(const ModbusRequest& request, ModbusResponse& response);
    void handleWriteSingleRegister(const ModbusRequest& request, ModbusResponse& response);
    void handleWriteMultipleCoils(const ModbusRequest& request, ModbusResponse& response);
    void handleWriteMultipleRegisters(const ModbusRequest& request, ModbusResponse& response);
    
    // Statistics
    void updateStatistics(bool success, unsigned long response_time_us, uint8_t exception_code = 0);
    
public:
    ModbusTCPServer();
    ~ModbusTCPServer();
    
    // Server lifecycle
    bool begin(uint16_t port = MODBUS_TCP_PORT);
    void stop();
    void handle();
    bool isRunning() const { return server_enabled && tcp_server != nullptr; }
    
    // Configuration
    void setRegisterManager(ModbusRegisterManager* manager) { register_manager = manager; }
    void setPort(uint16_t port) { server_port = port; }
    uint16_t getPort() const { return server_port; }
    
    // Client management
    uint8_t getActiveClientCount() const;
    void disconnectAllClients();
    void disconnectClient(uint32_t client_ip);
    
    // Statistics and diagnostics
    const ModbusStatistics& getStatistics() const { return stats; }
    void resetStatistics();
    String getDiagnostics() const;
    String getClientList() const;
    
    // Status
    bool isClientConnected(uint32_t client_ip) const;
    unsigned long getUptime() const;
    uint32_t getTotalConnections() const { return stats.total_connections; }
    
    // Enable/disable
    void enable() { server_enabled = true; }
    void disable() { server_enabled = false; }
    bool isEnabled() const { return server_enabled; }
};

#endif // MODBUS_TCP_SERVER_H