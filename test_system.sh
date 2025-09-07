#!/bin/bash

# ESP32 System Test Script
ESP32_IP="192.168.1.106"
MAX_RETRIES=10
RETRY_DELAY=3

echo "🔧 MAC-SYS Arduino ESP32 Sensor Test"
echo "======================================"

# Function to check network connectivity
check_connectivity() {
    echo "📡 Checking ESP32 connectivity..."
    if ping -c 1 -W 1000 $ESP32_IP > /dev/null 2>&1; then
        echo "✅ ESP32 is reachable at $ESP32_IP"
        return 0
    else
        echo "❌ ESP32 not reachable at $ESP32_IP"
        return 1
    fi
}

# Function to test API endpoint
test_api() {
    local endpoint=$1
    local description=$2
    
    echo "🔍 Testing $description..."
    response=$(curl -s -m 5 http://$ESP32_IP$endpoint 2>/dev/null)
    
    if [ $? -eq 0 ] && [ -n "$response" ]; then
        echo "✅ $description: SUCCESS"
        echo "   Response: $response"
        return 0
    else
        echo "❌ $description: FAILED"
        return 1
    fi
}

# Main test loop
for attempt in $(seq 1 $MAX_RETRIES); do
    echo ""
    echo "🔄 Attempt $attempt/$MAX_RETRIES"
    
    # Check basic connectivity
    if check_connectivity; then
        
        # Test web server root
        if test_api "/" "Web Server Root"; then
            # Test sensors API
            test_api "/api/sensors" "Sensor Status API"
            
            # Test system status API
            test_api "/api/status" "System Status API"
            
            echo ""
            echo "✅ All tests completed successfully!"
            exit 0
        fi
    fi
    
    if [ $attempt -lt $MAX_RETRIES ]; then
        echo "⏳ Waiting ${RETRY_DELAY}s before retry..."
        sleep $RETRY_DELAY
    fi
done

echo ""
echo "❌ System test failed after $MAX_RETRIES attempts"
echo "💡 Troubleshooting steps:"
echo "   1. Check ESP32 power and USB connection"
echo "   2. Verify WiFi credentials and network connectivity"  
echo "   3. Check serial monitor for error messages"
echo "   4. Ensure IP address $ESP32_IP is correct"
exit 1
