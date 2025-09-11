# 📘 MAC-SYS Arduino Modbus TCP User Manual

## 🔗 Quick Connection Guide
```
IP Address: 192.168.1.15 (or your device IP)
Port: 502 (Standard Modbus TCP)
Protocol: Modbus TCP/IP
Max Clients: 5 concurrent connections
```

---

## 📋 Table of Contents

1. [Connection Setup](#connection-setup)
2. [Register Map Overview](#register-map-overview)  
3. [Function Code Support](#function-code-support)
4. [Data Types and Scaling](#data-types-and-scaling)
5. [Common Operations](#common-operations)
6. [SCADA Integration Examples](#scada-integration-examples)
7. [Error Handling](#error-handling)
8. [Troubleshooting](#troubleshooting)

---

## 🔌 Connection Setup

### Network Configuration
- **Device IP**: Configure via web interface at `/network`
- **Modbus Port**: 502 (standard, not configurable)
- **Connection Type**: TCP/IP over Ethernet or WiFi
- **Max Concurrent Clients**: 5

### Client Configuration Examples

**ModbusPoll (Windows)**:
```
Connection: TCP/IP
IP Address: 192.168.1.15
Port: 502
Modbus ID: 1
Function: 03 (Read Holding Registers)
Address: 40001
Quantity: 10
```

**Python pymodbus**:
```python
from pymodbus.client.sync import ModbusTcpClient

client = ModbusTcpClient('192.168.1.15', port=502)
result = client.read_holding_registers(0, 10, unit=1)  # Read 10 registers starting at 40001
client.close()
```

**AdvancedHMI (VB.NET)**:
```
IPAddress: 192.168.1.15
TcpipPort: 502
ModbusAddress: 40001
```

---

## 📊 Register Map Overview

The MAC-SYS Arduino implements **2110 total registers** across all Modbus register types:

| Register Type | Address Range | Count | Purpose |
|---------------|---------------|-------|---------|
| **Coils** | 00001-00320 | 320 | Digital Outputs & System Controls |
| **Discrete Inputs** | 10001-10340 | 340 | Digital Inputs & Status Flags |
| **Input Registers** | 30001-30400 | 400 | Sensor Readings & System Data |
| **Holding Registers** | 40001-41050 | 1050 | Configuration & Control Parameters |

### Quick Reference - Most Used Registers

**Temperature Sensors (Input Registers)**:
```
30001: DS18B20 #1 Temperature (°C × 10)
30002: DS18B20 #2 Temperature (°C × 10)  
30003: DS18B20 #3 Temperature (°C × 10)
30011: AM2302 Temperature (°C × 10)
30012: AM2302 Humidity (% × 10)
30021: LM35 Temperature (°C × 10)
```

**System Control (Coils)**:
```
00001: Compressor ON/OFF
00002: Fan ON/OFF
00003: Heater ON/OFF
00011: System Enable/Disable
00012: Manual/Auto Mode
```

**Configuration (Holding Registers)**:
```
40001: Temperature Setpoint (°C × 10)
40002: Control Delta/Hysteresis (°C × 10)
40003: Control Mode (0=Off, 1=Cool, 2=Heat, 3=Auto)
40011: Active Sensor Selection (0=DS18B20_1, 1=DS18B20_2, 2=DS18B20_3, 3=AM2302, 4=LM35)
```

---

## ⚙️ Function Code Support

| Function Code | Name | Support | Description |
|---------------|------|---------|-------------|
| **01** | Read Coils | ✅ Full | Read digital outputs/controls |
| **02** | Read Discrete Inputs | ✅ Full | Read digital inputs/status |
| **03** | Read Holding Registers | ✅ Full | Read configuration values |
| **04** | Read Input Registers | ✅ Full | Read sensor data |
| **05** | Write Single Coil | ✅ Full | Control single digital output |
| **06** | Write Single Register | ✅ Full | Set single configuration value |
| **15** | Write Multiple Coils | ✅ Full | Control multiple digital outputs |
| **16** | Write Multiple Registers | ✅ Full | Set multiple configuration values |

### Function Code Examples

**Read Temperature (Function Code 04)**:
```
Request: [Unit=1, FC=04, Address=30001, Quantity=1]
Response: [Temperature × 10] → 235 = 23.5°C
```

**Control Compressor (Function Code 05)**:
```
Request: [Unit=1, FC=05, Address=00001, Value=ON]
Response: [Confirmation]
```

**Set Temperature Setpoint (Function Code 06)**:
```
Request: [Unit=1, FC=06, Address=40001, Value=250] → 25.0°C setpoint
Response: [Confirmation]
```

---

## 📏 Data Types and Scaling

### Temperature Values (×10 Scaling)
- **Format**: Signed 16-bit integer
- **Scale**: Multiply by 10 (e.g., 235 = 23.5°C)
- **Range**: -327.0°C to +327.0°C
- **Invalid**: 32767 (0x7FFF) indicates sensor error

```python
# Python example - Read temperature
result = client.read_input_registers(0, 1, unit=1)  # Address 30001
if result.registers[0] != 32767:
    temperature = result.registers[0] / 10.0
    print(f"Temperature: {temperature}°C")
else:
    print("Sensor error")
```

### Humidity Values (×10 Scaling)
- **Format**: Unsigned 16-bit integer  
- **Scale**: Multiply by 10 (e.g., 655 = 65.5% RH)
- **Range**: 0.0% to 100.0% RH

### Boolean/Digital Values
- **Coils**: 0 = OFF/False, 1 = ON/True
- **Discrete Inputs**: 0 = Inactive, 1 = Active

### Enumerated Values
**Control Mode (40003)**:
```
0 = OFF
1 = COOLING  
2 = HEATING
3 = AUTO
```

**Sensor Selection (40011)**:
```
0 = DS18B20 #1
1 = DS18B20 #2  
2 = DS18B20 #3
3 = AM2302
4 = LM35
```

---

## 🎯 Common Operations

### 1. Read All Temperature Sensors
```python
# Read DS18B20 sensors (30001-30003)
temps = client.read_input_registers(0, 3, unit=1)
for i, temp in enumerate(temps.registers):
    if temp != 32767:
        print(f"DS18B20 #{i+1}: {temp/10.0}°C")
    else:
        print(f"DS18B20 #{i+1}: Error")

# Read AM2302 (30011-30012)  
am2302 = client.read_input_registers(10, 2, unit=1)
temp, humidity = am2302.registers
if temp != 32767:
    print(f"AM2302 Temp: {temp/10.0}°C, Humidity: {humidity/10.0}%")
```

### 2. Basic Temperature Control
```python
# Read current temperature and setpoint
current_temp = client.read_input_registers(0, 1, unit=1).registers[0] / 10.0
setpoint = client.read_holding_registers(0, 1, unit=1).registers[0] / 10.0

print(f"Current: {current_temp}°C, Setpoint: {setpoint}°C")

# Adjust setpoint
new_setpoint = 24.5  # 24.5°C
client.write_register(0, int(new_setpoint * 10), unit=1)  # Address 40001

# Enable system
client.write_coil(10, True, unit=1)  # Address 00011 (System Enable)
```

### 3. System Status Monitoring
```python
# Read system status (Discrete Inputs 10001-10020)
status = client.read_discrete_inputs(0, 20, unit=1)

print("System Status:")
print(f"System Running: {status.bits[0]}")
print(f"Compressor On: {status.bits[1]}")  
print(f"Fan On: {status.bits[2]}")
print(f"Heater On: {status.bits[3]}")
print(f"Temperature OK: {status.bits[10]}")
print(f"Network Connected: {status.bits[11]}")
```

### 4. Schedule Control
```python
# Enable/disable schedule (Coil 00021)
client.write_coil(20, True, unit=1)  # Enable schedule

# Set schedule parameters (Holding Registers 40101-40120)
schedule_data = [
    int(7.0 * 10),    # Start hour (7:00 AM)
    int(0.0 * 10),    # Start minute  
    int(22.0 * 10),   # Stop hour (10:00 PM)
    int(0.0 * 10),    # Stop minute
    int(22.0 * 10),   # Active temp (22.0°C)
    int(18.0 * 10),   # Inactive temp (18.0°C)
    0x7F              # Days of week (Mon-Sun)
]
client.write_registers(100, schedule_data, unit=1)  # Address 40101+
```

---

## 🏭 SCADA Integration Examples

### Wonderware InTouch
```
Topic: MODBUS
Item: 400001,L1,4    ; Read temperature setpoint
Item: 100001,L1,1    ; Read compressor status  
Item: 000001,L1,5    ; Write compressor control
```

### Schneider Electric Vijeo Citect
```
Variable Tags:
TempSetpoint = 40001    ; INT, Holding Register
CompressorOn = 10002    ; BOOL, Discrete Input
CompressorCtrl = 1      ; BOOL, Coil
```

### Siemens WinCC
```
Connection: TCP/IP Client
Variable: Temperature
Address: DB1.DBW0 -> 30001 (Input Register)
Data Type: Word -> Scale /10 for display
```

### FactoryTalk View
```
HMI Tag: Temperature_SP
Controller Tag: [MODBUS]40001
Data Type: INT
Min/Max: 100/400 (10.0°C to 40.0°C)
```

---

## ⚠️ Error Handling

### Modbus Exception Codes
| Code | Name | Description | Common Causes |
|------|------|-------------|---------------|
| **01** | Illegal Function | Unsupported function code | Using unsupported FC |
| **02** | Illegal Data Address | Invalid register address | Address out of range |
| **03** | Illegal Data Value | Invalid data value | Value out of valid range |
| **04** | Slave Device Failure | Device error | System malfunction |
| **06** | Slave Device Busy | Device processing | Try again later |

### Client-Side Error Handling
```python
def safe_modbus_read(client, register, count=1):
    try:
        result = client.read_holding_registers(register, count, unit=1)
        if result.isError():
            print(f"Modbus Error: {result}")
            return None
        return result.registers
    except Exception as e:
        print(f"Communication Error: {e}")
        return None

# Usage
temperature = safe_modbus_read(client, 0, 1)  # Read 40001
if temperature is not None:
    temp_celsius = temperature[0] / 10.0
    print(f"Temperature: {temp_celsius}°C")
```

### System Error Indicators
**Error Status (Input Registers 30351-30360)**:
```
30351: Last Error Code
30352: Error Count  
30353: I2C Error Count
30354: Network Error Count
30355: Sensor Error Count
```

**Error Codes**:
```
0: No Error
1: Temperature Sensor Error
2: I2C Communication Error  
3: Network Error
4: EEPROM Error
5: RTC Error
10: Emergency Stop Active
```

---

## 🛠️ Troubleshooting

### Connection Issues

**Problem**: Cannot connect to device
```
✅ Check network connectivity (ping 192.168.1.15)
✅ Verify device is powered and booted
✅ Confirm Modbus TCP server is running (check web interface)
✅ Check firewall settings on client computer
✅ Try different Modbus client software
```

**Problem**: Connection established but no response
```
✅ Verify Unit ID is set to 1
✅ Check function code support (01,02,03,04,05,06,15,16)
✅ Confirm register addresses are within valid ranges
✅ Monitor device serial debug for error messages
```

### Data Issues

**Problem**: Temperature reads as 32767 (Invalid)
```
✅ Check sensor physical connections
✅ Verify sensor power supply
✅ Test sensor using web interface /sensors
✅ Check sensor selection (40011) is correct
✅ Monitor sensor availability status (10031-10035)
```

**Problem**: Control commands not working
```
✅ Verify system is enabled (Coil 00011)
✅ Check emergency stop status (10021)
✅ Confirm manual mode is active (Coil 00012)  
✅ Monitor relay status via discrete inputs
✅ Check hardware connections to relays
```

### Performance Issues

**Problem**: Slow response times
```
✅ Reduce polling frequency on client
✅ Use bulk reads instead of single register reads
✅ Check network latency and bandwidth
✅ Monitor concurrent client connections (<5)
✅ Verify ESP32 is not overloaded
```

**Problem**: Connection drops frequently
```
✅ Check network stability
✅ Verify power supply is stable  
✅ Monitor device temperature (Input Register 30201)
✅ Check for electromagnetic interference
✅ Review client timeout settings
```

### Diagnostic Commands

**Network Test from Device**:
- Access web interface at `http://192.168.1.15/diagnostics`
- Check "Network Status" and "System Health"

**Serial Debug Output**:
```
Modbus TCP: Client connected from 192.168.1.100
Modbus TCP: FC=03, Address=40001, Quantity=1  
Modbus TCP: Response sent, 7 bytes
```

**Register Monitoring**:
```python
# Monitor system health registers
health = client.read_input_registers(350, 10, unit=1)  # 30351-30360
print(f"Last Error: {health.registers[0]}")
print(f"Error Count: {health.registers[1]}")
print(f"Uptime: {health.registers[5]} minutes")
```

---

## 📋 Quick Reference Card

### Essential Addresses
```
🌡️  TEMPERATURES
     30001-30003: DS18B20 #1-3 (°C × 10)
     30011: AM2302 Temp (°C × 10)  
     30012: AM2302 Humidity (% × 10)
     30021: LM35 Temp (°C × 10)

🎛️  CONTROLS  
     00001: Compressor (Coil)
     00002: Fan (Coil)
     00003: Heater (Coil)
     00011: System Enable (Coil)
     00012: Manual Mode (Coil)

⚙️  CONFIGURATION
     40001: Temperature Setpoint (°C × 10)
     40002: Control Delta (°C × 10)
     40003: Control Mode (0-3)
     40011: Active Sensor (0-4)

📊  STATUS
     10001: System Running
     10002: Compressor Status  
     10003: Fan Status
     10011: Temperature OK
     10021: Emergency Stop
```

### Function Code Quick Guide
```
FC 01: Read Coils           → Digital Controls
FC 02: Read Discrete Input  → Digital Status  
FC 03: Read Holding Reg     → Configuration
FC 04: Read Input Reg       → Sensor Data
FC 05: Write Single Coil    → Control Action
FC 06: Write Single Reg     → Set Parameter
```

### Scaling Reference
```
Temperatures: Value ÷ 10 = °C
Humidity: Value ÷ 10 = %RH
Pressures: Value ÷ 100 = kPa  
Times: Value = Minutes
Error Code: 32767 = Invalid/Error
```

---

## 📞 Support Information

**Device Information**:
- Model: MAC-SYS Arduino HVAC Controller
- Firmware: Check via web interface `/system`
- Protocol: Modbus TCP (IEEE 802.3, RFC 1006)

**Default Network Settings**:
- DHCP: Enabled (check router for assigned IP)
- Static IP: Configure via web interface `/network`
- mDNS: macsys.local (if enabled)

**Web Interface**: `http://[device-ip]/`
**Documentation**: See `/help` on device web interface

---

**📋 Document Version**: 1.0  
**📅 Last Updated**: Based on MODBUS_REGISTER_MAP.md  
**🔧 Compatible Firmware**: 2.0+

---

*This manual covers complete Modbus TCP integration for the MAC-SYS Arduino HVAC Controller. For additional support, consult the device web interface or system documentation.*