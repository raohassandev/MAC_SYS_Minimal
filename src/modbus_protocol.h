#ifndef MODBUS_PROTOCOL_H
#define MODBUS_PROTOCOL_H

#include <Arduino.h>
#include <WiFiClient.h>

// Modbus TCP constants
#define MODBUS_TCP_PORT 502
#define MODBUS_TCP_HEADER_SIZE 6
#define MODBUS_TCP_MAX_FRAME_SIZE 260
#define MODBUS_TCP_PROTOCOL_ID 0x0000
#define MODBUS_TCP_UNIT_ID 0x01

// Function codes
#define MODBUS_FC_READ_COILS 0x01
#define MODBUS_FC_READ_DISCRETE_INPUTS 0x02
#define MODBUS_FC_READ_HOLDING_REGISTERS 0x03
#define MODBUS_FC_READ_INPUT_REGISTERS 0x04
#define MODBUS_FC_WRITE_SINGLE_COIL 0x05
#define MODBUS_FC_WRITE_SINGLE_REGISTER 0x06
#define MODBUS_FC_WRITE_MULTIPLE_COILS 0x0F
#define MODBUS_FC_WRITE_MULTIPLE_REGISTERS 0x10

// Exception codes
#define MODBUS_EXCEPTION_ILLEGAL_FUNCTION 0x01
#define MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS 0x02
#define MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE 0x03
#define MODBUS_EXCEPTION_SLAVE_DEVICE_FAILURE 0x04
#define MODBUS_EXCEPTION_ACKNOWLEDGE 0x05
#define MODBUS_EXCEPTION_SLAVE_DEVICE_BUSY 0x06

// Register limits (from MODBUS_REGISTER_MAP.md)
#define MAX_COILS 320
#define MAX_DISCRETE_INPUTS 340
#define MAX_INPUT_REGISTERS 400
#define MAX_HOLDING_REGISTERS 1050

// Maximum quantities per function
#define MAX_READ_COILS 0x7D0          // 2000
#define MAX_READ_DISCRETE 0x7D0       // 2000
#define MAX_READ_REGISTERS 0x7D       // 125
#define MAX_WRITE_COILS 0x7B0         // 1968
#define MAX_WRITE_REGISTERS 0x7B      // 123

struct ModbusRequest {
    uint16_t transaction_id;
    uint16_t protocol_id;
    uint16_t length;
    uint8_t unit_id;
    uint8_t function_code;
    uint16_t address;
    uint16_t quantity;
    uint8_t byte_count;
    uint8_t data[256];
    bool valid;
};

struct ModbusResponse {
    uint16_t transaction_id;
    uint16_t protocol_id;
    uint16_t length;
    uint8_t unit_id;
    uint8_t function_code;
    uint8_t byte_count;
    uint8_t data[256];
    uint8_t exception_code;
    bool is_exception;
};

class ModbusProtocol {
private:
    uint8_t buffer[MODBUS_TCP_MAX_FRAME_SIZE];
    
public:
    ModbusProtocol();
    
    // Frame parsing and building
    bool parseRequest(uint8_t* frame, size_t length, ModbusRequest& request);
    size_t buildResponse(const ModbusResponse& response, uint8_t* frame);
    size_t buildExceptionResponse(uint16_t transaction_id, uint8_t function_code, uint8_t exception_code, uint8_t* frame);
    
    // Validation functions
    uint8_t validateRequest(const ModbusRequest& request);
    bool isValidFunctionCode(uint8_t function_code);
    bool isValidAddress(uint8_t function_code, uint16_t address, uint16_t quantity);
    bool isValidQuantity(uint8_t function_code, uint16_t quantity);
    
    // Utility functions
    uint16_t swapBytes(uint16_t value);
    void packBits(const bool* bits, uint8_t* bytes, uint16_t bit_count);
    void unpackBits(const uint8_t* bytes, bool* bits, uint16_t bit_count);
};

// Modbus statistics
struct ModbusStatistics {
    uint32_t total_requests;
    uint32_t successful_requests;
    uint32_t exception_responses;
    uint32_t illegal_function;
    uint32_t illegal_address;
    uint32_t illegal_value;
    uint32_t device_failure;
    unsigned long average_response_time_us;
    unsigned long max_response_time_us;
    uint16_t current_clients;
    uint32_t total_connections;
    uint32_t dropped_connections;
};

#endif // MODBUS_PROTOCOL_H