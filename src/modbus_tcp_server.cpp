#include "modbus_tcp_server.h"
#include "modbus_register_manager.h"
#include "config.h"

ModbusTCPServer::ModbusTCPServer() :
    tcp_server(nullptr),
    register_manager(nullptr),
    server_enabled(false),
    server_port(MODBUS_TCP_PORT),
    server_start_time(0)
{
    // Initialize client connections
    for (uint8_t i = 0; i < MAX_MODBUS_CLIENTS; i++) {
        clients[i].active = false;
        clients[i].last_activity = 0;
        clients[i].client_ip = 0;
        clients[i].last_transaction_id = 0;
        clients[i].request_count = 0;
        clients[i].connect_time = 0;
    }
    
    // Initialize statistics
    memset(&stats, 0, sizeof(ModbusStatistics));
}

ModbusTCPServer::~ModbusTCPServer() {
    stop();
}

bool ModbusTCPServer::begin(uint16_t port) {
    if (tcp_server) {
        stop();
    }
    
    server_port = port;
    tcp_server = new WiFiServer(server_port);
    
    if (!tcp_server) {
        DEBUG_PRINTLN("ERROR: Failed to create Modbus TCP server");
        return false;
    }
    
    tcp_server->begin();
    server_enabled = true;
    server_start_time = millis();
    
    DEBUG_PRINTF("Modbus TCP server started on port %d\n", server_port);
    return true;
}

void ModbusTCPServer::stop() {
    if (tcp_server) {
        disconnectAllClients();
        tcp_server->stop();
        delete tcp_server;
        tcp_server = nullptr;
    }
    
    server_enabled = false;
    DEBUG_PRINTLN("Modbus TCP server stopped");
}

void ModbusTCPServer::handle() {
    if (!server_enabled || !tcp_server || !register_manager) {
        return;
    }
    
    // First, clean up any disconnected clients to free slots immediately
    cleanupDisconnectedClients();
    
    // Handle client timeouts
    handleClientTimeouts();
    
    // Handle new client connections
    WiFiClient newClient = tcp_server->available();
    if (newClient) {
        int8_t slot = findFreeClientSlot();
        if (slot >= 0) {
            clients[slot].client = newClient;
            clients[slot].active = true;
            clients[slot].last_activity = millis();
            clients[slot].client_ip = newClient.remoteIP();
            clients[slot].request_count = 0;
            clients[slot].connect_time = millis();
            
            stats.total_connections++;
            stats.current_clients++;
            
            DEBUG_PRINTF("Modbus client connected: %s (slot %d)\n", 
                        newClient.remoteIP().toString().c_str(), slot);
        } else {
            // No free slots - reject connection
            DEBUG_PRINTF("Modbus client rejected - no free slots: %s\n", 
                        newClient.remoteIP().toString().c_str());
            newClient.stop();
            stats.dropped_connections++;
        }
    }
    
    // Handle existing client requests
    for (uint8_t i = 0; i < MAX_MODBUS_CLIENTS; i++) {
        if (clients[i].active && clients[i].client.connected()) {
            if (clients[i].client.available()) {
                processClientRequest(i);
            }
        }
    }
}

int8_t ModbusTCPServer::findFreeClientSlot() {
    for (uint8_t i = 0; i < MAX_MODBUS_CLIENTS; i++) {
        if (!clients[i].active) {
            return i;
        }
    }
    return -1;
}

int8_t ModbusTCPServer::findClientSlot(WiFiClient& client) {
    for (uint8_t i = 0; i < MAX_MODBUS_CLIENTS; i++) {
        if (clients[i].active && clients[i].client == client) {
            return i;
        }
    }
    return -1;
}

void ModbusTCPServer::removeClient(uint8_t slot) {
    if (slot >= MAX_MODBUS_CLIENTS || !clients[slot].active) {
        return;
    }
    
    DEBUG_PRINTF("Modbus client disconnected: %s (slot %d, %lu requests)\n", 
                clients[slot].client.remoteIP().toString().c_str(), 
                slot, clients[slot].request_count);
    
    // Force stop the connection and flush any pending data
    if (clients[slot].client) {
        clients[slot].client.flush();
        clients[slot].client.stop();
    }
    
    // Clear all client state immediately
    clients[slot].active = false;
    clients[slot].last_activity = 0;
    clients[slot].client_ip = 0;
    clients[slot].request_count = 0;
    clients[slot].connect_time = 0;
    clients[slot].last_transaction_id = 0;
    
    if (stats.current_clients > 0) {
        stats.current_clients--;
    }
}

void ModbusTCPServer::handleClientTimeouts() {
    unsigned long current_time = millis();
    
    for (uint8_t i = 0; i < MAX_MODBUS_CLIENTS; i++) {
        if (clients[i].active) {
            bool should_disconnect = false;
            
            // Check if client is still connected
            if (!clients[i].client.connected()) {
                should_disconnect = true;
            }
            // Check timeout
            else if (current_time - clients[i].last_activity > MODBUS_CLIENT_TIMEOUT) {
                should_disconnect = true;
                DEBUG_PRINTF("Modbus client timeout: %s\n", 
                           clients[i].client.remoteIP().toString().c_str());
            }
            
            if (should_disconnect) {
                removeClient(i);
            }
        }
    }
}

void ModbusTCPServer::cleanupDisconnectedClients() {
    for (uint8_t i = 0; i < MAX_MODBUS_CLIENTS; i++) {
        if (clients[i].active) {
            // Check if client connection is still valid
            if (!clients[i].client || !clients[i].client.connected()) {
                DEBUG_PRINTF("Modbus client disconnected - cleaning up slot %d\n", i);
                removeClient(i);
            }
        }
    }
}

void ModbusTCPServer::processClientRequest(uint8_t client_slot) {
    if (client_slot >= MAX_MODBUS_CLIENTS || !clients[client_slot].active) {
        return;
    }
    
    WiFiClient& client = clients[client_slot].client;
    unsigned long start_time = micros();
    
    // Read request data
    size_t bytes_available = client.available();
    if (bytes_available > MODBUS_BUFFER_SIZE) {
        bytes_available = MODBUS_BUFFER_SIZE;
    }
    
    size_t bytes_read = client.read(request_buffer, bytes_available);
    if (bytes_read < MODBUS_TCP_HEADER_SIZE + 2) {
        // Incomplete request
        return;
    }
    
    clients[client_slot].last_activity = millis();
    clients[client_slot].request_count++;
    stats.total_requests++;
    
    // Parse request
    ModbusRequest request;
    bool parsed = protocol.parseRequest(request_buffer, bytes_read, request);
    
    if (!parsed) {
        // Invalid request - send exception
        size_t response_size = protocol.buildExceptionResponse(0, 0, 
                                    MODBUS_EXCEPTION_ILLEGAL_FUNCTION, response_buffer);
        client.write(response_buffer, response_size);
        updateStatistics(false, micros() - start_time, MODBUS_EXCEPTION_ILLEGAL_FUNCTION);
        return;
    }
    
    // Validate request
    uint8_t exception_code = protocol.validateRequest(request);
    if (exception_code != 0) {
        size_t response_size = protocol.buildExceptionResponse(request.transaction_id, 
                                    request.function_code, exception_code, response_buffer);
        client.write(response_buffer, response_size);
        updateStatistics(false, micros() - start_time, exception_code);
        return;
    }
    
    // Process request
    ModbusResponse response;
    response.transaction_id = request.transaction_id;
    response.protocol_id = MODBUS_TCP_PROTOCOL_ID;
    response.unit_id = MODBUS_TCP_UNIT_ID;
    response.function_code = request.function_code;
    response.is_exception = false;
    response.exception_code = 0;
    
    // Handle based on function code
    switch (request.function_code) {
        case MODBUS_FC_READ_COILS:
            handleReadCoils(request, response);
            break;
        case MODBUS_FC_READ_DISCRETE_INPUTS:
            handleReadDiscreteInputs(request, response);
            break;
        case MODBUS_FC_READ_HOLDING_REGISTERS:
            handleReadHoldingRegisters(request, response);
            break;
        case MODBUS_FC_READ_INPUT_REGISTERS:
            handleReadInputRegisters(request, response);
            break;
        case MODBUS_FC_WRITE_SINGLE_COIL:
            handleWriteSingleCoil(request, response);
            break;
        case MODBUS_FC_WRITE_SINGLE_REGISTER:
            handleWriteSingleRegister(request, response);
            break;
        case MODBUS_FC_WRITE_MULTIPLE_COILS:
            handleWriteMultipleCoils(request, response);
            break;
        case MODBUS_FC_WRITE_MULTIPLE_REGISTERS:
            handleWriteMultipleRegisters(request, response);
            break;
        default:
            response.is_exception = true;
            response.exception_code = MODBUS_EXCEPTION_ILLEGAL_FUNCTION;
            break;
    }
    
    // Build and send response
    size_t response_size = protocol.buildResponse(response, response_buffer);
    if (response_size > 0) {
        client.write(response_buffer, response_size);
    }
    
    unsigned long response_time = micros() - start_time;
    updateStatistics(!response.is_exception, response_time, response.exception_code);
    
    clients[client_slot].last_transaction_id = request.transaction_id;
}

void ModbusTCPServer::handleReadCoils(const ModbusRequest& request, ModbusResponse& response) {
    if (!register_manager->readCoils(request.address, request.quantity, response.data)) {
        response.is_exception = true;
        response.exception_code = MODBUS_EXCEPTION_SLAVE_DEVICE_FAILURE;
        return;
    }
    
    response.byte_count = (request.quantity + 7) / 8;
    response.length = 3 + response.byte_count;
}

void ModbusTCPServer::handleReadDiscreteInputs(const ModbusRequest& request, ModbusResponse& response) {
    if (!register_manager->readDiscreteInputs(request.address, request.quantity, response.data)) {
        response.is_exception = true;
        response.exception_code = MODBUS_EXCEPTION_SLAVE_DEVICE_FAILURE;
        return;
    }
    
    response.byte_count = (request.quantity + 7) / 8;
    response.length = 3 + response.byte_count;
}

void ModbusTCPServer::handleReadHoldingRegisters(const ModbusRequest& request, ModbusResponse& response) {
    uint16_t* reg_data = (uint16_t*)response.data;
    if (!register_manager->readHoldingRegisters(request.address, request.quantity, reg_data)) {
        response.is_exception = true;
        response.exception_code = MODBUS_EXCEPTION_SLAVE_DEVICE_FAILURE;
        return;
    }
    
    response.byte_count = request.quantity * 2;
    response.length = 3 + response.byte_count;
}

void ModbusTCPServer::handleReadInputRegisters(const ModbusRequest& request, ModbusResponse& response) {
    uint16_t* reg_data = (uint16_t*)response.data;
    if (!register_manager->readInputRegisters(request.address, request.quantity, reg_data)) {
        response.is_exception = true;
        response.exception_code = MODBUS_EXCEPTION_SLAVE_DEVICE_FAILURE;
        return;
    }
    
    response.byte_count = request.quantity * 2;
    response.length = 3 + response.byte_count;
}

void ModbusTCPServer::handleWriteSingleCoil(const ModbusRequest& request, ModbusResponse& response) {
    uint16_t value = (request.data[0] << 8) | request.data[1];
    bool state = (value == 0xFF00);
    
    if (!register_manager->writeCoil(request.address, state)) {
        response.is_exception = true;
        response.exception_code = MODBUS_EXCEPTION_SLAVE_DEVICE_FAILURE;
        return;
    }
    
    // Echo back address and value
    response.data[0] = (request.address >> 8) & 0xFF;
    response.data[1] = request.address & 0xFF;
    response.data[2] = request.data[0];
    response.data[3] = request.data[1];
    response.length = 6;
}

void ModbusTCPServer::handleWriteSingleRegister(const ModbusRequest& request, ModbusResponse& response) {
    uint16_t value = (request.data[0] << 8) | request.data[1];
    
    if (!register_manager->writeHoldingRegister(request.address, value)) {
        response.is_exception = true;
        response.exception_code = MODBUS_EXCEPTION_SLAVE_DEVICE_FAILURE;
        return;
    }
    
    // Echo back address and value
    response.data[0] = (request.address >> 8) & 0xFF;
    response.data[1] = request.address & 0xFF;
    response.data[2] = request.data[0];
    response.data[3] = request.data[1];
    response.length = 6;
}

void ModbusTCPServer::handleWriteMultipleCoils(const ModbusRequest& request, ModbusResponse& response) {
    if (!register_manager->writeMultipleCoils(request.address, request.quantity, request.data)) {
        response.is_exception = true;
        response.exception_code = MODBUS_EXCEPTION_SLAVE_DEVICE_FAILURE;
        return;
    }
    
    // Echo back address and quantity
    response.data[0] = (request.address >> 8) & 0xFF;
    response.data[1] = request.address & 0xFF;
    response.data[2] = (request.quantity >> 8) & 0xFF;
    response.data[3] = request.quantity & 0xFF;
    response.length = 6;
}

void ModbusTCPServer::handleWriteMultipleRegisters(const ModbusRequest& request, ModbusResponse& response) {
    uint16_t* reg_data = (uint16_t*)request.data;
    
    if (!register_manager->writeMultipleRegisters(request.address, request.quantity, reg_data)) {
        response.is_exception = true;
        response.exception_code = MODBUS_EXCEPTION_SLAVE_DEVICE_FAILURE;
        return;
    }
    
    // Echo back address and quantity
    response.data[0] = (request.address >> 8) & 0xFF;
    response.data[1] = request.address & 0xFF;
    response.data[2] = (request.quantity >> 8) & 0xFF;
    response.data[3] = request.quantity & 0xFF;
    response.length = 6;
}

void ModbusTCPServer::updateStatistics(bool success, unsigned long response_time_us, uint8_t exception_code) {
    if (success) {
        stats.successful_requests++;
    } else {
        stats.exception_responses++;
        
        switch (exception_code) {
            case MODBUS_EXCEPTION_ILLEGAL_FUNCTION:
                stats.illegal_function++;
                break;
            case MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS:
                stats.illegal_address++;
                break;
            case MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE:
                stats.illegal_value++;
                break;
            case MODBUS_EXCEPTION_SLAVE_DEVICE_FAILURE:
                stats.device_failure++;
                break;
        }
    }
    
    // Update response time statistics
    if (response_time_us > stats.max_response_time_us) {
        stats.max_response_time_us = response_time_us;
    }
    
    // Calculate running average response time
    if (stats.total_requests > 0) {
        stats.average_response_time_us = (stats.average_response_time_us * (stats.total_requests - 1) + response_time_us) / stats.total_requests;
    }
}

uint8_t ModbusTCPServer::getActiveClientCount() const {
    return stats.current_clients;
}

void ModbusTCPServer::disconnectAllClients() {
    for (uint8_t i = 0; i < MAX_MODBUS_CLIENTS; i++) {
        if (clients[i].active) {
            removeClient(i);
        }
    }
}

void ModbusTCPServer::disconnectClient(uint32_t client_ip) {
    for (uint8_t i = 0; i < MAX_MODBUS_CLIENTS; i++) {
        if (clients[i].active && clients[i].client_ip == client_ip) {
            removeClient(i);
            break;
        }
    }
}

void ModbusTCPServer::resetStatistics() {
    memset(&stats, 0, sizeof(ModbusStatistics));
    stats.current_clients = getActiveClientCount();
}

String ModbusTCPServer::getDiagnostics() const {
    String diag = "Modbus TCP Server Diagnostics:\n";
    diag += "Server Status: " + String(server_enabled ? "Running" : "Stopped") + "\n";
    diag += "Port: " + String(server_port) + "\n";
    diag += "Uptime: " + String(getUptime() / 1000) + " seconds\n";
    diag += "Active Clients: " + String(stats.current_clients) + "/" + String(MAX_MODBUS_CLIENTS) + "\n";
    diag += "Total Requests: " + String(stats.total_requests) + "\n";
    diag += "Successful: " + String(stats.successful_requests) + "\n";
    diag += "Exceptions: " + String(stats.exception_responses) + "\n";
    diag += "Avg Response: " + String(stats.average_response_time_us) + " μs\n";
    diag += "Max Response: " + String(stats.max_response_time_us) + " μs\n";
    diag += "Total Connections: " + String(stats.total_connections) + "\n";
    diag += "Dropped Connections: " + String(stats.dropped_connections) + "\n";
    return diag;
}

String ModbusTCPServer::getClientList() const {
    String list = "Connected Clients:\n";
    for (uint8_t i = 0; i < MAX_MODBUS_CLIENTS; i++) {
        if (clients[i].active) {
            list += "Slot " + String(i) + ": " + clients[i].client.remoteIP().toString();
            list += " (Requests: " + String(clients[i].request_count) + ")\n";
        }
    }
    return list;
}

bool ModbusTCPServer::isClientConnected(uint32_t client_ip) const {
    for (uint8_t i = 0; i < MAX_MODBUS_CLIENTS; i++) {
        if (clients[i].active && clients[i].client_ip == client_ip) {
            return true;
        }
    }
    return false;
}

unsigned long ModbusTCPServer::getUptime() const {
    if (server_start_time == 0) return 0;
    return millis() - server_start_time;
}