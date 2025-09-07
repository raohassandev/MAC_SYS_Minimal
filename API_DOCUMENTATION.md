# MAC-SYS RESTful API Documentation

## Overview
The MAC-SYS Industrial Controller provides a comprehensive RESTful API for monitoring and controlling all system functions. The API uses JSON format for data exchange and supports proper HTTP status codes.

**Base URL:** `http://[ESP32_IP_ADDRESS]/api`

## API Endpoints

### 📋 System Information

#### GET /api
Get API information and available endpoints
```bash
curl http://192.168.1.100/api
```
**Response:**
```json
{
  "api_version": "1.0",
  "system": "MAC-SYS-MINIMAL",
  "firmware": "1.0.0",
  "manufacturer": "MAC-SYS",
  "endpoints": {
    "system": "/api/system",
    "relays": "/api/relays",
    "sensors": "/api/sensors",
    "temperature": "/api/temperature",
    "schedule": "/api/schedule",
    "network": "/api/network",
    "diagnostics": "/api/diagnostics"
  }
}
```

#### GET /api/system
Get complete system status and information
```bash
curl http://192.168.1.100/api/system
```
**Response:**
```json
{
  "status": "RUNNING",
  "uptime": 3600,
  "free_memory": 234567,
  "firmware_version": "1.0.0",
  "system_name": "MAC-SYS-MINIMAL",
  "manufacturer": "MAC-SYS",
  "chip_model": "ESP32",
  "mac_address": "38:18:2B:F0:FF:7C",
  "flash_size": 4194304,
  "cpu_frequency": 240,
  "timestamp": "1234567890"
}
```

#### POST /api/system/restart
Restart the system
```bash
curl -X POST http://192.168.1.100/api/system/restart
```
**Response:**
```json
{
  "success": true,
  "message": "System restart initiated"
}
```

---

### 🔌 Relay Control

#### GET /api/relays
Get status of all relays
```bash
curl http://192.168.1.100/api/relays
```
**Response:**
```json
{
  "relays": [
    {
      "id": 0,
      "name": "Relay 1",
      "state": true,
      "manual_override": false
    },
    {
      "id": 1,
      "name": "Relay 2", 
      "state": false,
      "manual_override": false
    }
  ]
}
```

#### GET /api/relays/info?id=[RELAY_ID]
Get status of specific relay
```bash
curl http://192.168.1.100/api/relays/info?id=0
```
**Response:**
```json
{
  "success": true,
  "id": 0,
  "name": "Relay 1",
  "state": true,
  "manual_override": false
}
```

#### POST /api/relays/control
Control individual relay
```bash
curl -X POST http://192.168.1.100/api/relays/control \
  -d "relay=0&state=true"
```
**Response:**
```json
{
  "success": true,
  "relay": 0,
  "state": true,
  "message": "Relay 1 set to ON"
}
```

#### POST /api/relays/all
Control all relays simultaneously
```bash
curl -X POST http://192.168.1.100/api/relays/all \
  -d "state=false"
```
**Response:**
```json
{
  "success": true,
  "state": false,
  "message": "All relays set to OFF"
}
```

---

### 🌡️ Temperature & Sensors

#### GET /api/temperature
Get current temperature data
```bash
curl http://192.168.1.100/api/temperature
```
**Response:**
```json
{
  "current_temperature": 24.5,
  "sensor_type": "AM2302",
  "sensor_available": true,
  "last_read_time": 1234567890,
  "sensors_initialized": true,
  "available_sensors": {
    "ds18b20": false,
    "am2302": true,
    "lm35": false
  }
}
```

#### GET /api/sensors/data
Get detailed sensor readings from all available sensors
```bash
curl http://192.168.1.100/api/sensors/data
```
**Response:**
```json
{
  "timestamp": 1234567890,
  "temperature": {
    "current": 24.50,
    "sensor_type": "AM2302",
    "available": true
  },
  "sensors": {
    "ds18b20": {
      "available": false
    },
    "am2302": {
      "available": true,
      "temperature": 24.50,
      "humidity": 65.2
    },
    "lm35": {
      "available": false
    }
  }
}
```

#### GET /api/temperature/zones
Get temperature zone configurations
```bash
curl http://192.168.1.100/api/temperature/zones
```
**Response:**
```json
{
  "zones": [
    {
      "id": 0,
      "name": "Zone 1",
      "enabled": true,
      "setpoint": 22.0,
      "mode": "AUTO",
      "current_state": false,
      "current_temp": 24.5
    }
  ]
}
```

#### POST /api/temperature/control
Control temperature zone settings
```bash
curl -X POST http://192.168.1.100/api/temperature/control \
  -d "zone=0&setpoint=23.5&mode=COOLING&enabled=true"
```
**Response:**
```json
{
  "success": true,
  "zone": 0,
  "message": "Zone 1 configuration updated"
}
```

---

### 📊 Digital Inputs

#### GET /api/inputs
Get status of all digital inputs
```bash
curl http://192.168.1.100/api/inputs
```
**Response:**
```json
{
  "inputs": [
    {
      "id": 0,
      "name": "Input 1",
      "state": false
    },
    {
      "id": 1,
      "name": "Input 2",
      "state": true
    }
  ]
}
```

---

### 🌐 Network Information

#### GET /api/network
Get network connection details
```bash
curl http://192.168.1.100/api/network
```
**Response:**
```json
{
  "wifi_connected": true,
  "ip_address": "192.168.1.100",
  "mac_address": "38:18:2B:F0:FF:7C",
  "ssid": "MyNetwork",
  "rssi": -45,
  "gateway": "192.168.1.1",
  "dns": "192.168.1.1",
  "subnet": "255.255.255.0"
}
```

---

### 🔧 Diagnostics

#### GET /api/diagnostics  
Get detailed system diagnostics
```bash
curl http://192.168.1.100/api/diagnostics
```
**Response:**
```json
{
  "uptime": 3600,
  "free_memory": 234567,
  "total_memory": 327680,
  "memory_usage": 28.4,
  "cpu_frequency": 240,
  "flash_size": 4194304,
  "flash_speed": 40000000,
  "chip_revision": 3,
  "sdk_version": "v4.4.2",
  "wifi_rssi": -45,
  "temperature_sensors_ok": true,
  "relay_count": 6
}
```

---

## Automatic Updates Implementation

### JavaScript Client Example
```html
<!DOCTYPE html>
<html>
<head>
    <title>MAC-SYS API Client</title>
</head>
<body>
    <h1>MAC-SYS Live Dashboard</h1>
    <div id="temperature">Loading...</div>
    <div id="relays">Loading...</div>
    <div id="system-info">Loading...</div>

    <script>
    const API_BASE = 'http://192.168.1.100/api';

    // Update functions with different intervals
    function updateTemperature() {
        fetch(`${API_BASE}/sensors/data`)
            .then(response => response.json())
            .then(data => {
                document.getElementById('temperature').innerHTML = 
                    `Temperature: ${data.temperature.current}°C (${data.temperature.sensor_type})`;
            })
            .catch(err => console.log('Temperature update failed:', err));
    }

    function updateRelays() {
        fetch(`${API_BASE}/relays`)
            .then(response => response.json())
            .then(data => {
                const relayHtml = data.relays.map(relay => 
                    `Relay ${relay.id + 1}: ${relay.state ? 'ON' : 'OFF'}`
                ).join('<br>');
                document.getElementById('relays').innerHTML = relayHtml;
            })
            .catch(err => console.log('Relay update failed:', err));
    }

    function updateSystemInfo() {
        fetch(`${API_BASE}/system`)
            .then(response => response.json())
            .then(data => {
                document.getElementById('system-info').innerHTML = 
                    `Uptime: ${data.uptime}s | Memory: ${Math.round(data.free_memory/1024)}KB`;
            })
            .catch(err => console.log('System info update failed:', err));
    }

    // Set up automatic updates with appropriate intervals
    setInterval(updateTemperature, 2000);  // Every 2 seconds
    setInterval(updateRelays, 2000);       // Every 2 seconds  
    setInterval(updateSystemInfo, 30000);  // Every 30 seconds

    // Initial load
    updateTemperature();
    updateRelays();
    updateSystemInfo();
    </script>
</body>
</html>
```

### Python Client Example
```python
import requests
import time
import json

class MACSYSClient:
    def __init__(self, base_url):
        self.base_url = base_url
        
    def get_temperature(self):
        try:
            response = requests.get(f"{self.base_url}/sensors/data")
            return response.json()
        except requests.RequestException as e:
            print(f"Error getting temperature: {e}")
            return None
            
    def control_relay(self, relay_id, state):
        try:
            data = {'relay': relay_id, 'state': 'true' if state else 'false'}
            response = requests.post(f"{self.base_url}/relays/control", data=data)
            return response.json()
        except requests.RequestException as e:
            print(f"Error controlling relay: {e}")
            return None

    def get_system_status(self):
        try:
            response = requests.get(f"{self.base_url}/system")
            return response.json()
        except requests.RequestException as e:
            print(f"Error getting system status: {e}")
            return None

# Usage example with automatic updates
client = MACSYSClient("http://192.168.1.100/api")

def monitor_system():
    while True:
        # Get temperature data every 2 seconds
        temp_data = client.get_temperature()
        if temp_data:
            print(f"Temperature: {temp_data['temperature']['current']}°C")
            
        # Get system status every 30 seconds
        if int(time.time()) % 30 == 0:
            status = client.get_system_status()
            if status:
                print(f"Uptime: {status['uptime']}s, Memory: {status['free_memory']}B")
        
        time.sleep(2)

# monitor_system()  # Uncomment to run
```

---

## Update Intervals Recommendation

| Data Type | Recommended Interval | Reason |
|-----------|---------------------|---------|
| **Temperature/Sensors** | 2-5 seconds | Critical for HVAC control |
| **Relay States** | 2-3 seconds | Real-time control feedback |
| **Digital Inputs** | 5 seconds | Status monitoring |
| **System Status** | 30 seconds | Slow-changing information |
| **Network Info** | 60 seconds | Rarely changes |
| **Diagnostics** | 60 seconds | System health monitoring |

---

## Error Handling

### HTTP Status Codes
- `200` - Success
- `400` - Bad Request (invalid parameters)
- `404` - Not Found (invalid endpoint)
- `500` - Internal Server Error

### Error Response Format
```json
{
  "success": false,
  "error": "Error description",
  "details": "Additional error information"
}
```

---

## Rate Limiting
The API can handle approximately **50+ requests per minute** per endpoint. For optimal performance:
- Use appropriate update intervals
- Implement error handling with retry logic
- Avoid simultaneous requests to the same endpoint

---

## Integration Examples

### SCADA/HMI Integration
```javascript
// OPC-UA style data acquisition
setInterval(() => {
    fetch('/api/sensors/data')
        .then(response => response.json())
        .then(data => {
            // Update SCADA tags
            updateTag('PLC.Temperature.Current', data.temperature.current);
            updateTag('PLC.Humidity.Current', data.sensors.am2302.humidity);
        });
}, 1000);
```

### Home Automation Integration
```bash
# Home Assistant REST API example
curl -X POST http://192.168.1.100/api/relays/control \
  -H "Content-Type: application/x-www-form-urlencoded" \
  -d "relay=0&state=true"
```

This comprehensive API enables full integration with industrial control systems, building management systems, and IoT platforms.