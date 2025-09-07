#ifndef HARDWARE_H
#define HARDWARE_H

#include <Arduino.h>
#include <RTClib.h>

// External variables from main.cpp
extern bool i2c_initialized;

// Core Hardware Initialization
bool initializeHardware();
bool initializeI2C();

// PCF8574 I/O Expander Functions
bool initializePCF8574();
bool setRelayState(uint8_t relay_num, bool state);
bool getRelayState(uint8_t relay_num);
uint8_t getAllRelayStates();
bool setAllRelayStates(uint8_t states);
bool readInputStates();
bool getInputState(uint8_t input_num);
uint8_t getAllInputStates();

// HVAC Control Functions
bool setCompressorState(bool state);
bool setFanState(bool state);
bool setHeatingState(bool state);
bool emergencyStop();

// RTC Functions
bool initializeRTC();
bool setRTCTime(uint16_t year, uint8_t month, uint8_t day, 
                uint8_t hour, uint8_t minute, uint8_t second);
DateTime getCurrentTime();
bool isRTCAvailable();
uint16_t getCurrentMinutes();
uint8_t getCurrentDayOfWeek();

// System Health Functions
bool performHardwareSelfTest();
void resetHardwareErrors();
void updateHardwareStatus();

#endif // HARDWARE_H