#include "utils.h"

uint16_t calculateChecksum(const void* data, size_t len) {
    uint16_t sum = 0;
    const uint8_t* bytes = (const uint8_t*)data;
    for (size_t i = 0; i < len; i++) {
        sum += bytes[i];
    }
    return sum;
}