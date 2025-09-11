# 🚀 MAC-SYS Development Roadmap (dev branch)

## 📋 Current Status
- ✅ **Core System**: Professional web interface with real-time sensor monitoring
- ✅ **REST API**: 15+ endpoints for complete system control
- ✅ **Sensor Management**: Temperature sensor selection with validation
- ✅ **Network Configuration**: Professional WiFi management
- ✅ **JavaScript Issues**: All console errors resolved, optimized for ESP32

---

## 🎯 **PRIORITY FEATURES FOR DEV BRANCH**

### **HIGH PRIORITY (Must Have)**

#### 1. **Modbus TCP Server Implementation** 🔥
*Target: 1-2 weeks*

- [ ] **TCP Server Core**
  - Multi-client TCP server on port 502
  - Concurrent connection handling (up to 5 clients) 
  - Proper Modbus TCP frame processing
  - Function code support: 01, 02, 03, 04, 05, 06, 15, 16

- [ ] **Register Mapping**
  - **Input Registers (30001-39999)**: Sensor readings
    - 30001-30010: Temperature sensors
    - 30011-30020: Humidity sensors  
    - 30021-30030: System status
  - **Holding Registers (40001-49999)**: Configuration/Setpoints
    - 40001-40010: Temperature setpoints
    - 40011-40020: Schedule parameters
    - 40021-40030: System configuration
  - **Coils (00001-09999)**: Digital controls
    - 00001-00010: Relay controls
    - 00011-00020: System enable/disable

#### 2. **Enhanced Scheduling System** 🕐
*Target: 1-2 weeks*

- [ ] **Advanced Schedule Features**
  - Holiday calendar integration
  - Exception handling for special dates
  - Conditional scheduling based on sensor readings
  - Schedule templates for different usage patterns

- [ ] **Astronomical Clock**
  - Sunrise/sunset calculations based on GPS coordinates
  - Daylight saving time automatic adjustment
  - Solar angle calculations for optimal scheduling

#### 3. **User Management & Role-Based Access** 👥
*Target: 1 week*

- [ ] **Role-Based Access Control**
  - Admin: Full access to all functions
  - Operator: Control operations, limited configuration
  - Viewer: Read-only access to data and status
  - Guest: Basic dashboard viewing only

- [ ] **Enhanced Authentication**
  - Session management with timeouts
  - Multiple user accounts with different roles
  - Password complexity requirements
  - User activity logging

---

### **MEDIUM PRIORITY (Should Have)**

#### 4. **Advanced System Monitoring** 📊
*Target: 1 week*

- [ ] **Enhanced Health Monitoring**
  - Uptime monitoring and reboot detection
  - Network statistics (connections, requests/min)
  - Memory usage tracking (HEAP, SPIFFS)
  - Temperature monitoring of ESP32 itself

- [ ] **Improved Diagnostics**
  - System log viewing with filtering
  - Configuration backup and restore
  - Network connectivity status
  - Sensor health status dashboard

#### 5. **Data Analytics & Reporting** 📈
*Target: 1-2 weeks*

- [ ] **Historical Data Tracking**
  - Temperature trends over time
  - Usage pattern analysis
  - Energy consumption estimates
  - System performance metrics

- [ ] **Report Generation**
  - Daily/weekly/monthly reports
  - CSV data export functionality
  - Graphical trend displays
  - Performance summaries

---

### **LOW PRIORITY (Nice to Have)**

#### 6. **Enhanced Web Interface** 🎨
- [ ] **Dashboard Improvements**
  - Interactive charts and graphs
  - Customizable widget layouts
  - Mobile-responsive design optimization
  - Theme customization options

#### 7. **Advanced Configuration** ⚙️
- [ ] **System Templates**
  - Pre-configured settings for different building types
  - Quick setup wizards
  - Configuration validation and recommendations

---

## ❌ **EXCLUDED FEATURES (Not Required)**

The following features from PHASES.md are **NOT** being implemented:

1. ❌ **RS485 Hardware Setup** - No MAX485 transceiver needed
2. ❌ **Modbus RTU Master** - No sensor communication via RTU
3. ❌ **Device Scanning** - No auto-discovery of Modbus sensors  
4. ❌ **HTTPS Security** - HTTP sufficient for current needs
5. ❌ **Advanced System Diagnostics** - Basic monitoring only

---

## 📅 **DEVELOPMENT TIMELINE**

| Priority | Feature | Duration | Status |
|----------|---------|----------|---------|
| HIGH | Modbus TCP Server | 1-2 weeks | ⏳ Planning |
| HIGH | Enhanced Scheduling | 1-2 weeks | ⏳ Planning |
| HIGH | User Management | 1 week | ⏳ Planning |
| MEDIUM | System Monitoring | 1 week | ⏳ Planning |
| MEDIUM | Data Analytics | 1-2 weeks | ⏳ Planning |
| LOW | UI Enhancements | 1 week | ⏳ Planning |

**Total Estimated Time: 6-9 weeks**

---

## 🎯 **SUCCESS CRITERIA**

### **Modbus TCP Server**
- [ ] Handle 3+ concurrent TCP clients successfully
- [ ] Respond to all standard Modbus function codes
- [ ] Serve real sensor data via Input Registers
- [ ] Allow configuration changes via Holding Registers

### **Enhanced Scheduling** 
- [ ] Support complex scheduling with exceptions
- [ ] Automatic sunrise/sunset scheduling
- [ ] Holiday calendar affects schedules correctly
- [ ] Conditional scheduling based on temperature

### **User Management**
- [ ] 4 user roles work with proper access restrictions
- [ ] Session management prevents unauthorized access
- [ ] User activity is logged and viewable
- [ ] Password policies are enforced

### **System Monitoring**
- [ ] Real-time system health dashboard
- [ ] Historical data trends display correctly
- [ ] System logs are searchable and exportable
- [ ] Performance metrics are accurate

---

## 🚀 **NEXT STEPS**

1. **Start with Modbus TCP Server** - This is the highest priority missing piece
2. **Enhanced Scheduling** - Build on existing schedule framework  
3. **User Management** - Add role-based security layer
4. **System Monitoring** - Expand current basic monitoring
5. **Data Analytics** - Add historical tracking and reporting

This roadmap focuses on practical, implementable features that will make the MAC-SYS controller a complete professional HVAC management system without unnecessary complexity.