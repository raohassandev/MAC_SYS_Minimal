# 🏭 MAC-SYS HVAC Controller - Complete Modbus TCP Register Map

**Version**: 1.0  
**Date**: 2024  
**Protocol**: Modbus TCP (Port 502)  
**Device**: ESP32-based Industrial HVAC Controller  

---

## 📋 **OVERVIEW**

This document defines the complete Modbus TCP register mapping for the MAC-SYS HVAC Controller, providing full read/write access to all system features including sensors, controls, configuration, and diagnostics.

**Supported Function Codes:**
- **FC01**: Read Coils (Digital Outputs)
- **FC02**: Read Discrete Inputs (Digital Inputs) 
- **FC03**: Read Holding Registers (Configuration/Control)
- **FC04**: Read Input Registers (Sensor Data/Status)
- **FC05**: Write Single Coil
- **FC06**: Write Single Holding Register
- **FC15**: Write Multiple Coils
- **FC16**: Write Multiple Holding Registers

---

## 🔧 **COILS (00001-09999) - Digital Outputs [Read/Write]**

### **Relay Control (00001-00008)**
| Address | Name | Description | Type | Notes |
|---------|------|-------------|------|-------|
| 00001 | Relay_1_Control | Relay 1 On/Off | BOOL | Physical relay output |
| 00002 | Relay_2_Control | Relay 2 On/Off | BOOL | Physical relay output |
| 00003 | Relay_3_Control | Relay 3 On/Off | BOOL | Physical relay output |
| 00004 | Relay_4_Control | Relay 4 On/Off | BOOL | Physical relay output |
| 00005 | Relay_5_Control | Relay 5 On/Off | BOOL | Physical relay output |
| 00006 | Relay_6_Control | Relay 6 On/Off | BOOL | Physical relay output |
| 00007 | All_Relays_On | Turn all relays ON | BOOL | Bulk control |
| 00008 | All_Relays_Off | Turn all relays OFF | BOOL | Bulk control |

### **HVAC System Control (00101-00120)**
| Address | Name | Description | Type | Notes |
|---------|------|-------------|------|-------|
| 00101 | HVAC_System_Enable | Enable/Disable HVAC | BOOL | Master system control |
| 00102 | Heating_Enable | Enable heating mode | BOOL | Zone heating control |
| 00103 | Cooling_Enable | Enable cooling mode | BOOL | Zone cooling control |
| 00104 | Fan_Enable | Enable fan operation | BOOL | Circulation fan |
| 00105 | Auto_Mode_Enable | Enable automatic mode | BOOL | Auto temp control |
| 00106 | Schedule_Enable | Enable scheduling | BOOL | Schedule override |
| 00107 | Emergency_Stop | Emergency system stop | BOOL | Safety shutdown |
| 00108 | Compressor_Enable | Enable compressor | BOOL | Compressor control |
| 00109 | Aux_Heat_Enable | Enable auxiliary heat | BOOL | Backup heating |
| 00110 | Economy_Mode | Enable economy mode | BOOL | Energy saving mode |

### **Zone Control (00201-00240) - 4 Zones × 10 controls each**
| Address | Name | Description | Type | Notes |
|---------|------|-------------|------|-------|
| 00201 | Zone1_Enable | Zone 1 Enable/Disable | BOOL | Individual zone control |
| 00202 | Zone1_Heating | Zone 1 Heating Active | BOOL | Zone heating status |
| 00203 | Zone1_Cooling | Zone 1 Cooling Active | BOOL | Zone cooling status |
| 00204 | Zone1_Fan | Zone 1 Fan Active | BOOL | Zone fan control |
| 00205 | Zone1_Override | Zone 1 Manual Override | BOOL | Override schedule |
| 00206 | Zone1_Occupied | Zone 1 Occupancy | BOOL | Occupancy simulation |
| 00207 | Zone1_Vacation | Zone 1 Vacation Mode | BOOL | Extended setback |
| 00208 | Zone1_Boost | Zone 1 Boost Mode | BOOL | Temporary override |
| 00209 | Zone1_Fault_Reset | Zone 1 Fault Reset | BOOL | Clear zone faults |
| 00210 | Zone1_Test_Mode | Zone 1 Test Mode | BOOL | Testing/commissioning |
| 00211-00220 | Zone2_xxx | Zone 2 Controls | BOOL | Same as Zone 1 |
| 00221-00230 | Zone3_xxx | Zone 3 Controls | BOOL | Same as Zone 1 |
| 00231-00240 | Zone4_xxx | Zone 4 Controls | BOOL | Same as Zone 1 |

### **Alarm Management (00301-00320)**
| Address | Name | Description | Type | Notes |
|---------|------|-------------|------|-------|
| 00301 | High_Temp_Alarm_Ack | Ack high temperature alarm | BOOL | Reset alarm |
| 00302 | Low_Temp_Alarm_Ack | Ack low temperature alarm | BOOL | Reset alarm |
| 00303 | Sensor_Fault_Ack | Ack sensor fault alarm | BOOL | Reset sensor alarm |
| 00304 | Communication_Fault_Ack | Ack comm fault alarm | BOOL | Reset comm alarm |
| 00305 | Power_Fault_Ack | Ack power fault alarm | BOOL | Reset power alarm |
| 00306 | System_Fault_Ack | Ack system fault alarm | BOOL | Reset system alarm |
| 00307 | Maintenance_Ack | Ack maintenance alarm | BOOL | Reset maint alarm |
| 00308 | Filter_Change_Ack | Ack filter change alarm | BOOL | Reset filter alarm |
| 00309 | Alarm_Silence | Silence audible alarms | BOOL | Temporary silence |
| 00310 | All_Alarms_Ack | Acknowledge all alarms | BOOL | Global reset |

---

## 🔍 **DISCRETE INPUTS (10001-19999) - Digital Inputs [Read Only]**

### **Physical Digital Inputs (10001-10008)**
| Address | Name | Description | Type | Notes |
|---------|------|-------------|------|-------|
| 10001 | Digital_Input_1 | Physical input 1 state | BOOL | Hardware input |
| 10002 | Digital_Input_2 | Physical input 2 state | BOOL | Hardware input |
| 10003 | Digital_Input_3 | Physical input 3 state | BOOL | Hardware input |
| 10004 | Digital_Input_4 | Physical input 4 state | BOOL | Hardware input |
| 10005 | Digital_Input_5 | Physical input 5 state | BOOL | Hardware input |
| 10006 | Digital_Input_6 | Physical input 6 state | BOOL | Hardware input |
| 10007 | All_Inputs_State | OR of all inputs | BOOL | Any input active |
| 10008 | Input_Change_Flag | Input state changed | BOOL | Change detection |

### **System Status (10101-10130)**
| Address | Name | Description | Type | Notes |
|---------|------|-------------|------|-------|
| 10101 | System_Running | System operational | BOOL | Overall status |
| 10102 | WiFi_Connected | WiFi connection active | BOOL | Network status |
| 10103 | Modbus_Server_Active | Modbus server running | BOOL | Communication |
| 10104 | Web_Server_Active | Web server running | BOOL | Web interface |
| 10105 | Sensors_Healthy | All sensors healthy | BOOL | Sensor status |
| 10106 | I2C_Communication_OK | I2C bus operational | BOOL | Hardware comm |
| 10107 | EEPROM_OK | Configuration storage OK | BOOL | Data integrity |
| 10108 | RTC_Valid | Real-time clock valid | BOOL | Time accuracy |
| 10109 | Schedule_Active | Schedule currently active | BOOL | Scheduling |
| 10110 | Manual_Override_Active | Manual override in effect | BOOL | Override status |

### **Alarm Status (10201-10230)**
| Address | Name | Description | Type | Notes |
|---------|------|-------------|------|-------|
| 10201 | High_Temperature_Alarm | High temp alarm active | BOOL | Safety alarm |
| 10202 | Low_Temperature_Alarm | Low temp alarm active | BOOL | Safety alarm |
| 10203 | Sensor_Fault_Alarm | Sensor failure alarm | BOOL | Hardware alarm |
| 10204 | Communication_Fault | Comm failure alarm | BOOL | Network alarm |
| 10205 | Power_Supply_Fault | Power supply alarm | BOOL | Power alarm |
| 10206 | System_Fault | General system fault | BOOL | System alarm |
| 10207 | Maintenance_Required | Maintenance due alarm | BOOL | Service alarm |
| 10208 | Filter_Change_Required | Filter change needed | BOOL | Filter alarm |
| 10209 | Any_Alarm_Active | Any alarm condition | BOOL | Global alarm |
| 10210 | Alarm_Horn_Active | Audible alarm active | BOOL | Sound status |

### **Zone Status (10301-10340) - 4 Zones × 10 status each**
| Address | Name | Description | Type | Notes |
|---------|------|-------------|------|-------|
| 10301 | Zone1_Active | Zone 1 active | BOOL | Zone operation |
| 10302 | Zone1_Heating_Active | Zone 1 heating on | BOOL | Heating status |
| 10303 | Zone1_Cooling_Active | Zone 1 cooling on | BOOL | Cooling status |
| 10304 | Zone1_At_Setpoint | Zone 1 at target temp | BOOL | Control status |
| 10305 | Zone1_Call_For_Heat | Zone 1 needs heating | BOOL | Demand status |
| 10306 | Zone1_Call_For_Cool | Zone 1 needs cooling | BOOL | Demand status |
| 10307 | Zone1_Occupied | Zone 1 occupied status | BOOL | Occupancy |
| 10308 | Zone1_Fault | Zone 1 fault condition | BOOL | Zone fault |
| 10309 | Zone1_Override_Active | Zone 1 manual override | BOOL | Override |
| 10310 | Zone1_Test_Active | Zone 1 in test mode | BOOL | Test status |
| 10311-10320 | Zone2_xxx | Zone 2 Status | BOOL | Same as Zone 1 |
| 10321-10330 | Zone3_xxx | Zone 3 Status | BOOL | Same as Zone 1 |
| 10331-10340 | Zone4_xxx | Zone 4 Status | BOOL | Same as Zone 1 |

---

## 📊 **INPUT REGISTERS (30001-39999) - Sensor Data [Read Only]**

### **Temperature Sensors (30001-30050)**
| Address | Name | Description | Type | Scale | Units | Range |
|---------|------|-------------|------|-------|-------|--------|
| 30001 | Current_Temperature | Primary temperature | INT16 | 0.1°C | °C | -400 to 1250 |
| 30002 | DS18B20_Temperature | DS18B20 sensor temp | INT16 | 0.1°C | °C | -400 to 1250 |
| 30003 | AM2302_Temperature | AM2302 sensor temp | INT16 | 0.1°C | °C | -400 to 800 |
| 30004 | LM35_Temperature | LM35 sensor temp | INT16 | 0.1°C | °C | -100 to 1000 |
| 30005 | Temperature_Average | Average of all sensors | INT16 | 0.1°C | °C | -400 to 1250 |
| 30006 | Temperature_Min | Minimum sensor reading | INT16 | 0.1°C | °C | -400 to 1250 |
| 30007 | Temperature_Max | Maximum sensor reading | INT16 | 0.1°C | °C | -400 to 1250 |
| 30008 | Primary_Sensor_ID | Active primary sensor | UINT16 | 1 | ID | 1=DS18B20, 2=AM2302, 3=LM35 |
| 30009 | Sensor_Count_Available | Number of working sensors | UINT16 | 1 | Count | 0 to 3 |
| 30010 | Temperature_Trend | Temperature trend | INT16 | 0.1°C/min | °C/min | -100 to 100 |

### **Humidity Sensors (30051-30070)**
| Address | Name | Description | Type | Scale | Units | Range |
|---------|------|-------------|------|-------|-------|--------|
| 30051 | Current_Humidity | Primary humidity | UINT16 | 0.1% | %RH | 0 to 1000 |
| 30052 | AM2302_Humidity | AM2302 humidity reading | UINT16 | 0.1% | %RH | 0 to 1000 |
| 30053 | Humidity_Average | Average humidity | UINT16 | 0.1% | %RH | 0 to 1000 |
| 30054 | Humidity_Min | Minimum humidity | UINT16 | 0.1% | %RH | 0 to 1000 |
| 30055 | Humidity_Max | Maximum humidity | UINT16 | 0.1% | %RH | 0 to 1000 |
| 30056 | Dew_Point | Calculated dew point | INT16 | 0.1°C | °C | -400 to 800 |
| 30057 | Heat_Index | Calculated heat index | INT16 | 0.1°C | °C | -400 to 1200 |
| 30058 | Absolute_Humidity | Absolute humidity | UINT16 | 0.1 g/m³ | g/m³ | 0 to 500 |

### **System Monitoring (30101-30150)**
| Address | Name | Description | Type | Scale | Units | Range |
|---------|------|-------------|------|-------|-------|--------|
| 30101 | System_Uptime_Hours | System runtime | UINT32 | 1 hr | Hours | 0 to 4294967295 |
| 30102 | System_Uptime_Hours_H | Uptime high word | UINT16 | - | - | High 16 bits |
| 30103 | System_Uptime_Hours_L | Uptime low word | UINT16 | - | - | Low 16 bits |
| 30104 | Free_Heap_Memory | Available memory | UINT32 | 1 byte | Bytes | 0 to 327680 |
| 30105 | Free_Heap_Memory_H | Free memory high | UINT16 | - | - | High 16 bits |
| 30106 | Free_Heap_Memory_L | Free memory low | UINT16 | - | - | Low 16 bits |
| 30107 | WiFi_RSSI | WiFi signal strength | INT16 | 1 dBm | dBm | -100 to 0 |
| 30108 | CPU_Usage | CPU utilization | UINT16 | 0.1% | % | 0 to 1000 |
| 30109 | Loop_Execution_Time | Main loop time | UINT16 | 1 ms | ms | 0 to 65535 |
| 30110 | I2C_Error_Count | I2C communication errors | UINT16 | 1 | Count | 0 to 65535 |
| 30111 | Network_Packets_Sent | Total packets sent | UINT32 | 1 | Count | 0 to 4294967295 |
| 30112 | Network_Packets_Sent_H | Packets sent high | UINT16 | - | - | High 16 bits |
| 30113 | Network_Packets_Sent_L | Packets sent low | UINT16 | - | - | Low 16 bits |
| 30114 | Active_Modbus_Clients | Connected Modbus clients | UINT16 | 1 | Count | 0 to 10 |
| 30115 | ESP32_Core_Temperature | Internal chip temp | INT16 | 0.1°C | °C | 200 to 1000 |

### **HVAC Performance (30201-30250)**
| Address | Name | Description | Type | Scale | Units | Range |
|---------|------|-------------|------|-------|-------|--------|
| 30201 | Control_Output | Current control output | INT16 | 0.1% | % | -1000 to 1000 |
| 30202 | Heating_Output | Heating control output | UINT16 | 0.1% | % | 0 to 1000 |
| 30203 | Cooling_Output | Cooling control output | UINT16 | 0.1% | % | 0 to 1000 |
| 30204 | Fan_Speed | Fan speed output | UINT16 | 0.1% | % | 0 to 1000 |
| 30205 | Compressor_Runtime | Compressor runtime today | UINT32 | 1 min | Minutes | 0 to 1440 |
| 30206 | Compressor_Runtime_H | Runtime high word | UINT16 | - | - | High 16 bits |
| 30207 | Compressor_Runtime_L | Runtime low word | UINT16 | - | - | Low 16 bits |
| 30208 | Heating_Runtime | Heating runtime today | UINT32 | 1 min | Minutes | 0 to 1440 |
| 30209 | Heating_Runtime_H | Heating runtime high | UINT16 | - | - | High 16 bits |
| 30210 | Heating_Runtime_L | Heating runtime low | UINT16 | - | - | Low 16 bits |
| 30211 | System_Efficiency | Calculated efficiency | UINT16 | 0.1% | % | 0 to 1000 |
| 30212 | Energy_Consumption | Energy used today | UINT32 | 0.1 kWh | kWh | 0 to 999999 |
| 30213 | Energy_Consumption_H | Energy high word | UINT16 | - | - | High 16 bits |
| 30214 | Energy_Consumption_L | Energy low word | UINT16 | - | - | Low 16 bits |

### **Zone Temperatures (30301-30400) - 4 Zones × 25 values each**
| Address | Name | Description | Type | Scale | Units | Range |
|---------|------|-------------|------|-------|-------|--------|
| 30301 | Zone1_Temperature | Zone 1 current temp | INT16 | 0.1°C | °C | -400 to 1250 |
| 30302 | Zone1_Setpoint | Zone 1 active setpoint | INT16 | 0.1°C | °C | 100 to 350 |
| 30303 | Zone1_Heating_Setpoint | Zone 1 heating setpoint | INT16 | 0.1°C | °C | 100 to 350 |
| 30304 | Zone1_Cooling_Setpoint | Zone 1 cooling setpoint | INT16 | 0.1°C | °C | 100 to 350 |
| 30305 | Zone1_Temperature_Error | Zone 1 temp error | INT16 | 0.1°C | °C | -500 to 500 |
| 30306 | Zone1_Control_Output | Zone 1 control output | INT16 | 0.1% | % | -1000 to 1000 |
| 30307 | Zone1_Runtime_Today | Zone 1 runtime today | UINT32 | 1 min | Minutes | 0 to 1440 |
| 30308 | Zone1_Runtime_Today_H | Zone 1 runtime high | UINT16 | - | - | High 16 bits |
| 30309 | Zone1_Runtime_Today_L | Zone 1 runtime low | UINT16 | - | - | Low 16 bits |
| 30310 | Zone1_Energy_Today | Zone 1 energy today | UINT32 | 0.1 kWh | kWh | 0 to 99999 |
| 30311 | Zone1_Energy_Today_H | Zone 1 energy high | UINT16 | - | - | High 16 bits |
| 30312 | Zone1_Energy_Today_L | Zone 1 energy low | UINT16 | - | - | Low 16 bits |
| 30326-30350 | Zone2_xxx | Zone 2 Values | - | - | - | Same as Zone 1 |
| 30351-30375 | Zone3_xxx | Zone 3 Values | - | - | - | Same as Zone 1 |
| 30376-30400 | Zone4_xxx | Zone 4 Values | - | - | - | Same as Zone 1 |

---

## ⚙️ **HOLDING REGISTERS (40001-49999) - Configuration/Control [Read/Write]**

### **System Configuration (40001-40050)**
| Address | Name | Description | Type | Scale | Units | Range | Default |
|---------|------|-------------|------|-------|-------|--------|---------|
| 40001 | System_Enable | Enable/disable system | UINT16 | 1 | Bool | 0=Off, 1=On | 1 |
| 40002 | Operation_Mode | System operation mode | UINT16 | 1 | Enum | 0=Off,1=Heat,2=Cool,3=Auto,4=Fan | 3 |
| 40003 | Primary_Sensor_Select | Primary temperature sensor | UINT16 | 1 | Enum | 1=DS18B20,2=AM2302,3=LM35 | 1 |
| 40004 | Temperature_Units | Temperature unit selection | UINT16 | 1 | Enum | 0=Celsius,1=Fahrenheit | 0 |
| 40005 | Control_Algorithm | Control algorithm type | UINT16 | 1 | Enum | 0=OnOff,1=PID,2=Fuzzy | 0 |
| 40006 | System_Type | HVAC system type | UINT16 | 1 | Enum | 0=HeatPump,1=FurnaceAC,2=Boiler | 0 |
| 40007 | Number_of_Zones | Active zone count | UINT16 | 1 | Count | 1 to 4 | 1 |
| 40008 | Master_Reset | Reset all settings | UINT16 | 1 | Command | 1=Reset | 0 |
| 40009 | Configuration_Lock | Lock configuration changes | UINT16 | 1 | Bool | 0=Unlock,1=Lock | 0 |
| 40010 | Data_Logging_Enable | Enable data logging | UINT16 | 1 | Bool | 0=Disable,1=Enable | 1 |

### **Temperature Control (40051-40100)**
| Address | Name | Description | Type | Scale | Units | Range | Default |
|---------|------|-------------|------|-------|-------|--------|---------|
| 40051 | Master_Setpoint | Global temperature setpoint | INT16 | 0.1°C | °C | 100 to 350 | 220 |
| 40052 | Heating_Setpoint | Heating setpoint | INT16 | 0.1°C | °C | 100 to 350 | 200 |
| 40053 | Cooling_Setpoint | Cooling setpoint | INT16 | 0.1°C | °C | 100 to 350 | 240 |
| 40054 | Deadband | Temperature deadband | UINT16 | 0.1°C | °C | 5 to 100 | 20 |
| 40055 | Temperature_Offset | Global temperature offset | INT16 | 0.1°C | °C | -100 to 100 | 0 |
| 40056 | Sensor_Filter_Time | Sensor filtering time | UINT16 | 1 s | Seconds | 1 to 300 | 30 |
| 40057 | Control_Cycle_Time | Control cycle time | UINT16 | 1 s | Seconds | 10 to 3600 | 60 |
| 40058 | Minimum_On_Time | Minimum compressor on | UINT16 | 1 s | Seconds | 60 to 1800 | 180 |
| 40059 | Minimum_Off_Time | Minimum compressor off | UINT16 | 1 s | Seconds | 60 to 1800 | 300 |
| 40060 | Maximum_Run_Time | Maximum continuous run | UINT16 | 1 min | Minutes | 10 to 480 | 240 |

### **Safety Limits (40101-40130)**
| Address | Name | Description | Type | Scale | Units | Range | Default |
|---------|------|-------------|------|-------|-------|--------|---------|
| 40101 | High_Temperature_Alarm | High temp alarm limit | INT16 | 0.1°C | °C | 200 to 500 | 350 |
| 40102 | Low_Temperature_Alarm | Low temp alarm limit | INT16 | 0.1°C | °C | -100 to 200 | 50 |
| 40103 | High_Temperature_Cutout | High temp cutout | INT16 | 0.1°C | °C | 200 to 600 | 400 |
| 40104 | Low_Temperature_Cutout | Low temp cutout | INT16 | 0.1°C | °C | -200 to 200 | 0 |
| 40105 | Sensor_Fault_Timeout | Sensor fault timeout | UINT16 | 1 s | Seconds | 30 to 3600 | 300 |
| 40106 | Communication_Timeout | Comm timeout | UINT16 | 1 s | Seconds | 60 to 7200 | 600 |
| 40107 | Emergency_Mode_Enable | Enable emergency operation | UINT16 | 1 | Bool | 0=Disable,1=Enable | 1 |
| 40108 | Safety_Interlock_Bypass | Bypass safety interlocks | UINT16 | 1 | Bool | 0=Active,1=Bypass | 0 |

### **PID Control Parameters (40201-40230)**
| Address | Name | Description | Type | Scale | Units | Range | Default |
|---------|------|-------------|------|-------|-------|--------|---------|
| 40201 | PID_Kp | Proportional gain | UINT16 | 0.01 | - | 0 to 1000 | 100 |
| 40202 | PID_Ki | Integral gain | UINT16 | 0.001 | - | 0 to 1000 | 50 |
| 40203 | PID_Kd | Derivative gain | UINT16 | 0.01 | - | 0 to 1000 | 10 |
| 40204 | PID_Output_Min | PID output minimum | INT16 | 0.1% | % | -1000 to 0 | -1000 |
| 40205 | PID_Output_Max | PID output maximum | INT16 | 0.1% | % | 0 to 1000 | 1000 |
| 40206 | PID_Integral_Limit | Integral windup limit | INT16 | 0.1% | % | 0 to 1000 | 500 |
| 40207 | PID_Derivative_Filter | Derivative filter time | UINT16 | 0.1 s | Seconds | 1 to 100 | 10 |
| 40208 | PID_Auto_Tune | Auto-tune PID parameters | UINT16 | 1 | Command | 1=Start | 0 |

### **Schedule Configuration (40301-40500) - Daily Schedule**
| Address | Name | Description | Type | Scale | Units | Range | Default |
|---------|------|-------------|------|-------|-------|--------|---------|
| 40301 | Schedule_Enable_Global | Enable scheduling | UINT16 | 1 | Bool | 0=Disable,1=Enable | 0 |
| 40302 | Schedule_Mode | Schedule mode | UINT16 | 1 | Enum | 0=Daily,1=Weekly,2=Holiday | 0 |
| 40303 | Holiday_Mode_Active | Holiday mode status | UINT16 | 1 | Bool | 0=Normal,1=Holiday | 0 |
| 40304 | Weekend_Schedule_Diff | Weekend schedule different | UINT16 | 1 | Bool | 0=Same,1=Different | 0 |
| 40305 | Vacation_Mode | Vacation mode | UINT16 | 1 | Bool | 0=Normal,1=Vacation | 0 |

#### **Daily Schedule Events (40311-40400) - 8 Events × 11 parameters**
| Address | Name | Description | Type | Scale | Units | Range | Default |
|---------|------|-------------|------|-------|-------|--------|---------|
| 40311 | Event1_Enable | Event 1 enable | UINT16 | 1 | Bool | 0=Disable,1=Enable | 0 |
| 40312 | Event1_Hour | Event 1 hour | UINT16 | 1 | Hour | 0 to 23 | 6 |
| 40313 | Event1_Minute | Event 1 minute | UINT16 | 1 | Minute | 0 to 59 | 0 |
| 40314 | Event1_Heating_Setpoint | Event 1 heat setpoint | INT16 | 0.1°C | °C | 100 to 350 | 200 |
| 40315 | Event1_Cooling_Setpoint | Event 1 cool setpoint | INT16 | 0.1°C | °C | 100 to 350 | 240 |
| 40316 | Event1_Mode | Event 1 system mode | UINT16 | 1 | Enum | 0=Off,1=Heat,2=Cool,3=Auto | 3 |
| 40317 | Event1_Days_Mask | Event 1 active days | UINT16 | 1 | Bitmap | Bit0=Sun,Bit6=Sat | 127 |
| 40318 | Event1_Zone_Mask | Event 1 active zones | UINT16 | 1 | Bitmap | Bit0=Zone1,Bit3=Zone4 | 15 |
| 40319 | Event1_Fan_Mode | Event 1 fan mode | UINT16 | 1 | Enum | 0=Auto,1=On,2=Circulate | 0 |
| 40320 | Event1_Priority | Event 1 priority | UINT16 | 1 | Priority | 1=Low,10=High | 5 |
| 40321 | Event1_Duration | Event 1 duration | UINT16 | 1 min | Minutes | 0=Permanent,1-1440 | 0 |
| 40322-40332 | Event2_xxx | Event 2 parameters | - | - | - | Same as Event 1 | - |
| 40333-40343 | Event3_xxx | Event 3 parameters | - | - | - | Same as Event 1 | - |
| 40344-40354 | Event4_xxx | Event 4 parameters | - | - | - | Same as Event 1 | - |
| 40355-40365 | Event5_xxx | Event 5 parameters | - | - | - | Same as Event 1 | - |
| 40366-40376 | Event6_xxx | Event 6 parameters | - | - | - | Same as Event 1 | - |
| 40377-40387 | Event7_xxx | Event 7 parameters | - | - | - | Same as Event 1 | - |
| 40388-40398 | Event8_xxx | Event 8 parameters | - | - | - | Same as Event 1 | - |

### **Zone Configuration (40501-40700) - 4 Zones × 50 parameters each**
| Address | Name | Description | Type | Scale | Units | Range | Default |
|---------|------|-------------|------|-------|-------|--------|---------|
| 40501 | Zone1_Enable | Zone 1 enable | UINT16 | 1 | Bool | 0=Disable,1=Enable | 1 |
| 40502 | Zone1_Name_1 | Zone 1 name part 1 | UINT16 | ASCII | - | ASCII chars | 0x5A31 ("Z1") |
| 40503 | Zone1_Name_2 | Zone 1 name part 2 | UINT16 | ASCII | - | ASCII chars | 0x6E65 ("ne") |
| 40504 | Zone1_Heating_Setpoint | Zone 1 heating setpoint | INT16 | 0.1°C | °C | 100 to 350 | 200 |
| 40505 | Zone1_Cooling_Setpoint | Zone 1 cooling setpoint | INT16 | 0.1°C | °C | 100 to 350 | 240 |
| 40506 | Zone1_Deadband | Zone 1 temperature deadband | UINT16 | 0.1°C | °C | 5 to 100 | 20 |
| 40507 | Zone1_Temperature_Offset | Zone 1 temp offset | INT16 | 0.1°C | °C | -100 to 100 | 0 |
| 40508 | Zone1_Sensor_Address | Zone 1 sensor assignment | UINT16 | 1 | Enum | 1=DS18B20,2=AM2302,3=LM35 | 1 |
| 40509 | Zone1_Control_Mode | Zone 1 control mode | UINT16 | 1 | Enum | 0=Off,1=Heat,2=Cool,3=Auto | 3 |
| 40510 | Zone1_Priority | Zone 1 priority | UINT16 | 1 | Priority | 1=Low,10=High | 5 |
| 40511 | Zone1_Heating_Relay | Zone 1 heating relay | UINT16 | 1 | Relay | 1 to 6 | 1 |
| 40512 | Zone1_Cooling_Relay | Zone 1 cooling relay | UINT16 | 1 | Relay | 1 to 6 | 2 |
| 40513 | Zone1_Fan_Relay | Zone 1 fan relay | UINT16 | 1 | Relay | 1 to 6 | 3 |
| 40514 | Zone1_Aux_Relay | Zone 1 auxiliary relay | UINT16 | 1 | Relay | 1 to 6 | 4 |
| 40515 | Zone1_Min_On_Time | Zone 1 minimum on time | UINT16 | 1 s | Seconds | 60 to 1800 | 180 |
| 40516 | Zone1_Min_Off_Time | Zone 1 minimum off time | UINT16 | 1 s | Seconds | 60 to 1800 | 300 |
| 40517 | Zone1_Max_Run_Time | Zone 1 maximum run time | UINT16 | 1 min | Minutes | 10 to 480 | 240 |
| 40518 | Zone1_Occupied_Schedule | Zone 1 occupied schedule | UINT16 | 1 | Bool | 0=Unoccupied,1=Occupied | 1 |
| 40519 | Zone1_Vacation_Setback | Zone 1 vacation setback | INT16 | 0.1°C | °C | -100 to 100 | 30 |
| 40520 | Zone1_Night_Setback | Zone 1 night setback | INT16 | 0.1°C | °C | -100 to 100 | 20 |
| 40551-40600 | Zone2_xxx | Zone 2 Configuration | - | - | - | Same as Zone 1 | - |
| 40601-40650 | Zone3_xxx | Zone 3 Configuration | - | - | - | Same as Zone 1 | - |
| 40651-40700 | Zone4_xxx | Zone 4 Configuration | - | - | - | Same as Zone 1 | - |

### **Network Configuration (40801-40850)**
| Address | Name | Description | Type | Scale | Units | Range | Default |
|---------|------|-------------|------|-------|-------|--------|---------|
| 40801 | Modbus_TCP_Enable | Enable Modbus TCP server | UINT16 | 1 | Bool | 0=Disable,1=Enable | 1 |
| 40802 | Modbus_TCP_Port | Modbus TCP port | UINT16 | 1 | Port | 1 to 65535 | 502 |
| 40803 | Modbus_Unit_ID | Modbus unit identifier | UINT16 | 1 | ID | 1 to 247 | 1 |
| 40804 | Modbus_Max_Clients | Maximum concurrent clients | UINT16 | 1 | Count | 1 to 10 | 8 |
| 40805 | Modbus_Timeout | Connection timeout | UINT16 | 1 s | Seconds | 10 to 300 | 30 |
| 40806 | Web_Server_Enable | Enable web server | UINT16 | 1 | Bool | 0=Disable,1=Enable | 1 |
| 40807 | Web_Server_Port | Web server port | UINT16 | 1 | Port | 80, 8080 | 80 |
| 40808 | WiFi_AP_Mode | WiFi access point mode | UINT16 | 1 | Bool | 0=Station,1=AP | 0 |
| 40809 | DHCP_Enable | Enable DHCP client | UINT16 | 1 | Bool | 0=Static,1=DHCP | 1 |
| 40810 | Network_Restart | Restart network | UINT16 | 1 | Command | 1=Restart | 0 |

### **Data Logging Configuration (40901-40950)**
| Address | Name | Description | Type | Scale | Units | Range | Default |
|---------|------|-------------|------|-------|-------|--------|---------|
| 40901 | Data_Log_Enable | Enable data logging | UINT16 | 1 | Bool | 0=Disable,1=Enable | 1 |
| 40902 | Log_Interval | Data logging interval | UINT16 | 1 s | Seconds | 30 to 3600 | 300 |
| 40903 | Log_Temperature | Log temperature data | UINT16 | 1 | Bool | 0=No,1=Yes | 1 |
| 40904 | Log_Humidity | Log humidity data | UINT16 | 1 | Bool | 0=No,1=Yes | 1 |
| 40905 | Log_System_Status | Log system status | UINT16 | 1 | Bool | 0=No,1=Yes | 1 |
| 40906 | Log_Energy_Data | Log energy consumption | UINT16 | 1 | Bool | 0=No,1=Yes | 1 |
| 40907 | Log_Retention_Days | Data retention period | UINT16 | 1 day | Days | 1 to 365 | 30 |
| 40908 | Log_Clear_All | Clear all logged data | UINT16 | 1 | Command | 1=Clear | 0 |

### **Diagnostics and Maintenance (41001-41050)**
| Address | Name | Description | Type | Scale | Units | Range | Default |
|---------|------|-------------|------|-------|-------|--------|---------|
| 41001 | System_Self_Test | Run system self-test | UINT16 | 1 | Command | 1=Run | 0 |
| 41002 | Calibrate_Sensors | Calibrate all sensors | UINT16 | 1 | Command | 1=Calibrate | 0 |
| 41003 | Factory_Reset | Factory reset system | UINT16 | 1 | Command | 12345=Reset | 0 |
| 41004 | Backup_Configuration | Backup config to memory | UINT16 | 1 | Command | 1=Backup | 0 |
| 41005 | Restore_Configuration | Restore config from backup | UINT16 | 1 | Command | 1=Restore | 0 |
| 41006 | Maintenance_Hours | Maintenance interval | UINT16 | 1 hr | Hours | 100 to 8760 | 2160 |
| 41007 | Filter_Hours | Filter change interval | UINT16 | 1 hr | Hours | 100 to 4380 | 720 |
| 41008 | Last_Maintenance_Reset | Reset maintenance counter | UINT16 | 1 | Command | 1=Reset | 0 |
| 41009 | Compressor_Cycles | Compressor cycle count | UINT32 | 1 | Count | 0 to 4294967295 | 0 |
| 41010 | Compressor_Cycles_H | Compressor cycles high | UINT16 | - | - | High 16 bits | 0 |
| 41011 | Compressor_Cycles_L | Compressor cycles low | UINT16 | - | - | Low 16 bits | 0 |

---

## 🚨 **ERROR CODES AND EXCEPTION RESPONSES**

### **Modbus Exception Codes**
- **0x01**: Illegal Function - Unsupported function code
- **0x02**: Illegal Data Address - Register address out of range  
- **0x03**: Illegal Data Value - Invalid data value for register
- **0x04**: Slave Device Failure - Internal device error
- **0x05**: Acknowledge - Long-running command accepted
- **0x06**: Slave Device Busy - Device temporarily busy
- **0x08**: Memory Parity Error - Configuration data corruption
- **0x0A**: Gateway Path Unavailable - Internal communication error
- **0x0B**: Gateway Target Failed - Sensor communication failure

### **System Error Codes (Register 30006)**
| Code | Description | Action Required |
|------|-------------|-----------------|
| 0x0000 | No Error | Normal operation |
| 0x0001 | Sensor Communication Error | Check sensor connections |
| 0x0002 | I2C Bus Error | Check I2C wiring |
| 0x0003 | WiFi Connection Lost | Check network |
| 0x0004 | Configuration Corruption | Factory reset required |
| 0x0005 | Memory Allocation Error | System restart required |
| 0x0006 | Watchdog Timer Reset | Check system stability |
| 0x0007 | Temperature Sensor Fault | Replace sensor |
| 0x0008 | Relay Control Error | Check relay hardware |
| 0x0009 | Real-Time Clock Error | Set system time |
| 0x000A | Flash Memory Error | Hardware failure |

---

## 📋 **IMPLEMENTATION NOTES**

### **Data Scaling and Precision**
- **Temperature**: 0.1°C resolution (e.g., 235 = 23.5°C)
- **Humidity**: 0.1% RH resolution (e.g., 654 = 65.4%)
- **Percentage**: 0.1% resolution (e.g., 1000 = 100.0%)
- **Time**: Various scales as specified in each register
- **32-bit Values**: Split into high/low 16-bit registers

### **Register Access Permissions**
- **Input Registers**: Read-only, updated automatically
- **Holding Registers**: Read/write, persistent in EEPROM
- **Coils**: Read/write, immediate relay control
- **Discrete Inputs**: Read-only, hardware input states

### **Concurrent Client Support**
- **Maximum Clients**: 8 concurrent TCP connections
- **Connection Timeout**: 30 seconds default (configurable)
- **Response Time**: <100ms typical, <500ms maximum
- **TCP Port**: 502 (standard Modbus TCP port)

### **Configuration Persistence**
- All Holding Register values are automatically saved to EEPROM
- Configuration takes effect immediately upon write
- Factory reset restores all default values
- Backup/restore functionality available via commands

### **Error Handling**
- Invalid register addresses return Exception 0x02
- Out-of-range values return Exception 0x03
- Sensor communication failures update status registers
- System faults trigger appropriate alarm discrete inputs

---

## 📚 **REGISTER SUMMARY**

| Register Type | Address Range | Count | Description |
|---------------|---------------|-------|-------------|
| **Coils** | 00001-00320 | 320 | Digital outputs and system controls |
| **Discrete Inputs** | 10001-10340 | 340 | Digital inputs and status flags |
| **Input Registers** | 30001-30400 | 400 | Sensor readings and system data |
| **Holding Registers** | 40001-41050 | 1050 | Configuration and control parameters |
| **Total Registers** | - | **2110** | Complete system access |

This comprehensive register map provides complete Modbus TCP access to all MAC-SYS HVAC Controller features, enabling full integration with SCADA systems, building management systems, and industrial automation platforms.