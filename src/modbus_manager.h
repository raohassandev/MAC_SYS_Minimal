#ifndef MODBUS_MANAGER_H
#define MODBUS_MANAGER_H

#include <Arduino.h>
#include "modbus_tcp_server.h"
#include "modbus_register_manager.h"

// Global Modbus TCP server and register manager
extern ModbusTCPServer modbus_server;
extern ModbusRegisterManager modbus_registers;

// Modbus management functions
void initializeModbus();
void handleModbus();
bool isModbusEnabled();
void enableModbus(bool enable);

// Configuration functions
void configureModbusPort(uint16_t port = MODBUS_TCP_PORT);
String getModbusStatus();
String getModbusClientList();

#endif // MODBUS_MANAGER_H