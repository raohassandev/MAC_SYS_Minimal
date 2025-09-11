#include "modbus_manager.h"
#include "config.h"

// Global Modbus TCP server and register manager instances
ModbusTCPServer modbus_server;
ModbusRegisterManager modbus_registers;

void initializeModbus() {
    DEBUG_PRINTLN("Initializing Modbus TCP server...");
    
    // Initialize register manager
    modbus_registers.begin();
    
    // Set register manager in server
    modbus_server.setRegisterManager(&modbus_registers);
    
    // Start Modbus TCP server
    if (modbus_server.begin(MODBUS_TCP_PORT)) {
        DEBUG_PRINTF("Modbus TCP server started on port %d\n", MODBUS_TCP_PORT);
    } else {
        DEBUG_PRINTLN("ERROR: Failed to start Modbus TCP server");
    }
}

void handleModbus() {
    // Update registers from system state
    modbus_registers.updateFromSystem();
    
    // Handle Modbus TCP clients
    modbus_server.handle();
    
    // Apply any register changes to system
    modbus_registers.applyToSystem();
}

bool isModbusEnabled() {
    return modbus_server.isEnabled();
}

void enableModbus(bool enable) {
    if (enable) {
        modbus_server.enable();
        if (!modbus_server.isRunning()) {
            initializeModbus();
        }
    } else {
        modbus_server.disable();
    }
}

void configureModbusPort(uint16_t port) {
    if (modbus_server.isRunning()) {
        modbus_server.stop();
    }
    modbus_server.setPort(port);
    if (modbus_server.isEnabled()) {
        modbus_server.begin(port);
    }
}

String getModbusStatus() {
    return modbus_server.getDiagnostics();
}

String getModbusClientList() {
    return modbus_server.getClientList();
}