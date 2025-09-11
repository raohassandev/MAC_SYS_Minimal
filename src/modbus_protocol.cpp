#include "modbus_protocol.h"
#include "config.h"

ModbusProtocol::ModbusProtocol() {
    memset(buffer, 0, sizeof(buffer));
}

bool ModbusProtocol::parseRequest(uint8_t* frame, size_t length, ModbusRequest& request) {
    if (!frame || length < MODBUS_TCP_HEADER_SIZE + 2) {
        request.valid = false;
        return false;
    }
    
    // Parse TCP header
    request.transaction_id = (frame[0] << 8) | frame[1];
    request.protocol_id = (frame[2] << 8) | frame[3];
    request.length = (frame[4] << 8) | frame[5];
    
    // Validate protocol ID
    if (request.protocol_id != MODBUS_TCP_PROTOCOL_ID) {
        request.valid = false;
        return false;
    }
    
    // Validate length
    if (request.length < 2 || request.length > length - MODBUS_TCP_HEADER_SIZE) {
        request.valid = false;
        return false;
    }
    
    // Parse PDU
    request.unit_id = frame[6];
    request.function_code = frame[7];
    
    // Parse function-specific data
    switch (request.function_code) {
        case MODBUS_FC_READ_COILS:
        case MODBUS_FC_READ_DISCRETE_INPUTS:
        case MODBUS_FC_READ_HOLDING_REGISTERS:
        case MODBUS_FC_READ_INPUT_REGISTERS:
            if (length < MODBUS_TCP_HEADER_SIZE + 5) {
                request.valid = false;
                return false;
            }
            request.address = (frame[8] << 8) | frame[9];
            request.quantity = (frame[10] << 8) | frame[11];
            break;
            
        case MODBUS_FC_WRITE_SINGLE_COIL:
        case MODBUS_FC_WRITE_SINGLE_REGISTER:
            if (length < MODBUS_TCP_HEADER_SIZE + 5) {
                request.valid = false;
                return false;
            }
            request.address = (frame[8] << 8) | frame[9];
            request.quantity = 1;
            request.data[0] = frame[10];
            request.data[1] = frame[11];
            break;
            
        case MODBUS_FC_WRITE_MULTIPLE_COILS:
        case MODBUS_FC_WRITE_MULTIPLE_REGISTERS:
            if (length < MODBUS_TCP_HEADER_SIZE + 6) {
                request.valid = false;
                return false;
            }
            request.address = (frame[8] << 8) | frame[9];
            request.quantity = (frame[10] << 8) | frame[11];
            request.byte_count = frame[12];
            
            // Validate byte count
            uint16_t expected_bytes;
            if (request.function_code == MODBUS_FC_WRITE_MULTIPLE_COILS) {
                expected_bytes = (request.quantity + 7) / 8;
            } else {
                expected_bytes = request.quantity * 2;
            }
            
            if (request.byte_count != expected_bytes || 
                length < MODBUS_TCP_HEADER_SIZE + 6 + request.byte_count) {
                request.valid = false;
                return false;
            }
            
            // Copy data
            memcpy(request.data, &frame[13], request.byte_count);
            break;
            
        default:
            request.valid = false;
            return false;
    }
    
    request.valid = true;
    return true;
}

size_t ModbusProtocol::buildResponse(const ModbusResponse& response, uint8_t* frame) {
    if (!frame) return 0;
    
    size_t frame_length = 0;
    
    if (response.is_exception) {
        return buildExceptionResponse(response.transaction_id, 
                                    response.function_code, 
                                    response.exception_code, 
                                    frame);
    }
    
    // TCP header
    frame[0] = (response.transaction_id >> 8) & 0xFF;
    frame[1] = response.transaction_id & 0xFF;
    frame[2] = (MODBUS_TCP_PROTOCOL_ID >> 8) & 0xFF;
    frame[3] = MODBUS_TCP_PROTOCOL_ID & 0xFF;
    
    // PDU
    frame[6] = MODBUS_TCP_UNIT_ID;
    frame[7] = response.function_code;
    
    switch (response.function_code) {
        case MODBUS_FC_READ_COILS:
        case MODBUS_FC_READ_DISCRETE_INPUTS:
            frame[8] = response.byte_count;
            // Coils and discrete inputs are already bit-packed bytes
            memcpy(&frame[9], response.data, response.byte_count);
            frame_length = 9 + response.byte_count;
            break;
            
        case MODBUS_FC_READ_HOLDING_REGISTERS:
        case MODBUS_FC_READ_INPUT_REGISTERS:
            frame[8] = response.byte_count;
            // Convert uint16_t register values to big-endian bytes
            {
                uint16_t* reg_data = (uint16_t*)response.data;
                uint16_t num_registers = response.byte_count / 2;
                for (uint16_t i = 0; i < num_registers; i++) {
                    uint16_t value = reg_data[i];
                    frame[9 + i * 2] = (value >> 8) & 0xFF;     // High byte first
                    frame[9 + i * 2 + 1] = value & 0xFF;       // Low byte second
                }
            }
            frame_length = 9 + response.byte_count;
            break;
            
        case MODBUS_FC_WRITE_SINGLE_COIL:
        case MODBUS_FC_WRITE_SINGLE_REGISTER:
        case MODBUS_FC_WRITE_MULTIPLE_COILS:
        case MODBUS_FC_WRITE_MULTIPLE_REGISTERS:
            // Echo back address and quantity
            memcpy(&frame[8], response.data, 4);
            frame_length = 12;
            break;
            
        default:
            return 0;
    }
    
    // Set length field
    uint16_t pdu_length = frame_length - MODBUS_TCP_HEADER_SIZE;
    frame[4] = (pdu_length >> 8) & 0xFF;
    frame[5] = pdu_length & 0xFF;
    
    return frame_length;
}

size_t ModbusProtocol::buildExceptionResponse(uint16_t transaction_id, uint8_t function_code, uint8_t exception_code, uint8_t* frame) {
    if (!frame) return 0;
    
    // TCP header
    frame[0] = (transaction_id >> 8) & 0xFF;
    frame[1] = transaction_id & 0xFF;
    frame[2] = (MODBUS_TCP_PROTOCOL_ID >> 8) & 0xFF;
    frame[3] = MODBUS_TCP_PROTOCOL_ID & 0xFF;
    frame[4] = 0x00; // Length high byte
    frame[5] = 0x03; // Length low byte (3 bytes for exception response)
    
    // PDU
    frame[6] = MODBUS_TCP_UNIT_ID;
    frame[7] = function_code | 0x80; // Set exception bit
    frame[8] = exception_code;
    
    return 9;
}

uint8_t ModbusProtocol::validateRequest(const ModbusRequest& request) {
    if (!request.valid) {
        return MODBUS_EXCEPTION_ILLEGAL_FUNCTION;
    }
    
    // Validate function code
    if (!isValidFunctionCode(request.function_code)) {
        return MODBUS_EXCEPTION_ILLEGAL_FUNCTION;
    }
    
    // Validate quantity
    if (!isValidQuantity(request.function_code, request.quantity)) {
        return MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
    }
    
    // Validate address
    if (!isValidAddress(request.function_code, request.address, request.quantity)) {
        return MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
    }
    
    return 0; // No error
}

bool ModbusProtocol::isValidFunctionCode(uint8_t function_code) {
    switch (function_code) {
        case MODBUS_FC_READ_COILS:
        case MODBUS_FC_READ_DISCRETE_INPUTS:
        case MODBUS_FC_READ_HOLDING_REGISTERS:
        case MODBUS_FC_READ_INPUT_REGISTERS:
        case MODBUS_FC_WRITE_SINGLE_COIL:
        case MODBUS_FC_WRITE_SINGLE_REGISTER:
        case MODBUS_FC_WRITE_MULTIPLE_COILS:
        case MODBUS_FC_WRITE_MULTIPLE_REGISTERS:
            return true;
        default:
            return false;
    }
}

bool ModbusProtocol::isValidAddress(uint8_t function_code, uint16_t address, uint16_t quantity) {
    uint16_t max_address;
    
    switch (function_code) {
        case MODBUS_FC_READ_COILS:
        case MODBUS_FC_WRITE_SINGLE_COIL:
        case MODBUS_FC_WRITE_MULTIPLE_COILS:
            max_address = MAX_COILS;
            break;
            
        case MODBUS_FC_READ_DISCRETE_INPUTS:
            max_address = MAX_DISCRETE_INPUTS;
            break;
            
        case MODBUS_FC_READ_INPUT_REGISTERS:
            max_address = MAX_INPUT_REGISTERS;
            break;
            
        case MODBUS_FC_READ_HOLDING_REGISTERS:
        case MODBUS_FC_WRITE_SINGLE_REGISTER:
        case MODBUS_FC_WRITE_MULTIPLE_REGISTERS:
            max_address = MAX_HOLDING_REGISTERS;
            break;
            
        default:
            return false;
    }
    
    // Check address range
    if (address >= max_address) {
        return false;
    }
    
    // Check address + quantity doesn't exceed range
    if (address + quantity > max_address) {
        return false;
    }
    
    return true;
}

bool ModbusProtocol::isValidQuantity(uint8_t function_code, uint16_t quantity) {
    if (quantity == 0) {
        return false;
    }
    
    switch (function_code) {
        case MODBUS_FC_READ_COILS:
            return quantity <= MAX_READ_COILS;
            
        case MODBUS_FC_READ_DISCRETE_INPUTS:
            return quantity <= MAX_READ_DISCRETE;
            
        case MODBUS_FC_READ_HOLDING_REGISTERS:
        case MODBUS_FC_READ_INPUT_REGISTERS:
            return quantity <= MAX_READ_REGISTERS;
            
        case MODBUS_FC_WRITE_SINGLE_COIL:
        case MODBUS_FC_WRITE_SINGLE_REGISTER:
            return quantity == 1;
            
        case MODBUS_FC_WRITE_MULTIPLE_COILS:
            return quantity <= MAX_WRITE_COILS;
            
        case MODBUS_FC_WRITE_MULTIPLE_REGISTERS:
            return quantity <= MAX_WRITE_REGISTERS;
            
        default:
            return false;
    }
}

uint16_t ModbusProtocol::swapBytes(uint16_t value) {
    return (value << 8) | (value >> 8);
}

void ModbusProtocol::packBits(const bool* bits, uint8_t* bytes, uint16_t bit_count) {
    uint16_t byte_count = (bit_count + 7) / 8;
    memset(bytes, 0, byte_count);
    
    for (uint16_t i = 0; i < bit_count; i++) {
        if (bits[i]) {
            bytes[i / 8] |= (1 << (i % 8));
        }
    }
}

void ModbusProtocol::unpackBits(const uint8_t* bytes, bool* bits, uint16_t bit_count) {
    for (uint16_t i = 0; i < bit_count; i++) {
        bits[i] = (bytes[i / 8] & (1 << (i % 8))) != 0;
    }
}