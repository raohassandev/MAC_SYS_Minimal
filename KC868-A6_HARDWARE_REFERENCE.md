# KC868-A6 Hardware Reference

## Official Documentation Links
- Hardware Design Details: https://www.kincony.com/kc868-a6-hardware-design-details.html
- Product Page: https://www.kincony.com/esp32-6-channel-relay-module-kc868-a6.html  
- ESPHome Configuration: https://devices.esphome.io/devices/KinCony-KC868-A6

## Core Hardware Specifications

### ESP32 Module
- **MCU**: ESP32-WROOM-32
- **Flash**: 4MB
- **Power**: DC-DC converter XL1509-5, 12V/5V/3.3V regulators
- **Programming**: USB-C with CH340C

### I2C Configuration
- **SDA Pin**: GPIO4
- **SCL Pin**: GPIO15 (requires ignore_strapping_warning: true)
- **Frequency**: 100kHz recommended

### PCF8574 I/O Expanders
- **Input Expander**: Address 0x22 (6 digital optocoupled inputs)
- **Output Expander**: Address 0x24 (6 relay controls)
- **Mode**: Inverted logic (active LOW)

### Relay System
- **Count**: 6 relays
- **Ratings**: 10A, 220V AC
- **Contacts**: NO/COM/NC available
- **Control**: PCF8574 outputs 0-5 (via 74HCT14 + ULN2003A)

### Analog Inputs
- **Count**: 4 channels (0-5V)
- **Amplifier**: LM224 op-amp
- **Resolution**: 12-bit ADC

### Temperature Sensors
- **Digital**: DS18B20 support via GPIO32
- **Analog**: LM35 via analog inputs
- **Environmental**: DHT22/AM2302 via GPIO33

### Communication Interfaces
- **RS485**: TX=GPIO27, RX=GPIO14  
- **RS232**: TX=GPIO17, RX=GPIO16 (DB9 connector)
- **Serial**: GPIO12/13
- **SPI**: CLK=GPIO18, MOSI=GPIO23, MISO=GPIO19, CS=GPIO5

### Display & RTC
- **Display**: SSD1306 OLED (I2C address 0x3C)
- **RTC**: DS1307 with CR1220 battery backup

### Wireless Extensions
- **nRF24L01**: SPI-based connector
- **LoRa**: SPI-based connector (mutually exclusive with nRF24)

## Hardware Notes
1. PCF8574 addresses are fixed in hardware
2. I2C pull-up resistors are on-board
3. Relays have LED indicators
4. Digital inputs are optocoupled for isolation
5. GPIO15 SCL pin requires strapping warning ignore
6. Temperature sensors connect to dedicated sensor points

## Firmware Compatibility
- Arduino IDE compatible
- ESPHome supported
- Custom firmware development supported
- MAC-SYS thermal control system compatible