#!/bin/bash

# MAC-SYS Temperature Control API Test Script
# Usage: ./test_api.sh [IP_ADDRESS]

IP="${1:-192.168.1.100}"
BASE_URL="http://$IP/api"

echo "========================================="
echo "MAC-SYS Temperature Control API Testing"
echo "Testing device at: $IP"
echo "========================================="
echo ""

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Function to test API endpoint
test_endpoint() {
    local method=$1
    local endpoint=$2
    local data=$3
    local description=$4
    
    echo -n "Testing $description... "
    
    if [ "$method" == "GET" ]; then
        response=$(curl -s -w "\n%{http_code}" -X GET "$BASE_URL$endpoint")
    else
        response=$(curl -s -w "\n%{http_code}" -X $method \
            -H "Content-Type: application/json" \
            --data-raw "$data" \
            "$BASE_URL$endpoint")
    fi
    
    http_code=$(echo "$response" | tail -n 1)
    body=$(echo "$response" | head -n -1)
    
    if [ "$http_code" == "200" ]; then
        echo -e "${GREEN}✓ OK${NC} (HTTP $http_code)"
        if [ ! -z "$body" ]; then
            echo "  Response: $body" | head -c 100
            echo ""
        fi
    else
        echo -e "${RED}✗ FAILED${NC} (HTTP $http_code)"
        echo "  Response: $body"
    fi
    echo ""
}

echo "=== SYSTEM ENDPOINTS ==="
test_endpoint "GET" "/health" "" "Health check"
test_endpoint "GET" "/system/info" "" "System information"
echo ""

echo "=== TEMPERATURE STATUS ENDPOINTS ==="
test_endpoint "GET" "/temperature/status" "" "Temperature status"
test_endpoint "GET" "/temperature/config" "" "Temperature configuration"
test_endpoint "GET" "/temperature/location" "" "Location name"
test_endpoint "GET" "/temperature/statistics" "" "Temperature statistics"
echo ""

echo "=== TEMPERATURE CONFIGURATION ENDPOINTS ==="
test_endpoint "GET" "/temperature/setpoint" "" "Get setpoint"
test_endpoint "POST" "/temperature/setpoint" '{"setpoint":25.0,"delta":1.5}' "Set setpoint"
test_endpoint "GET" "/temperature/compensation" "" "Get compensation"
test_endpoint "POST" "/temperature/compensation" '{"compensation":1.0}' "Set compensation"
test_endpoint "GET" "/temperature/mode" "" "Get mode"
test_endpoint "POST" "/temperature/mode" '{"mode":2,"enabled":true}' "Set mode to COOLING"
echo ""

echo "=== LOCATION CONFIGURATION ==="
test_endpoint "POST" "/temperature/location" '{"location":"Server Room"}' "Set location to Server Room"
test_endpoint "GET" "/temperature/location" "" "Verify location name"
echo ""

echo "=== CONTROL ENDPOINTS ==="
test_endpoint "POST" "/temperature/enable" "" "Enable system"
test_endpoint "POST" "/temperature/disable" "" "Disable system"
echo ""

echo "=== SAFETY ENDPOINTS ==="
test_endpoint "GET" "/temperature/limits" "" "Get emergency limits"
test_endpoint "POST" "/temperature/limits" '{"emergency_low":-5,"emergency_high":45}' "Set emergency limits"
test_endpoint "GET" "/temperature/timing" "" "Get timing protection"
test_endpoint "POST" "/temperature/timing" '{"min_on_time":180000,"min_off_time":180000}' "Set timing protection"
echo ""

echo "=== RELAY ENDPOINTS ==="
test_endpoint "GET" "/temperature/relays" "" "Get relay assignments"
test_endpoint "POST" "/temperature/relays" '{"compressor":0,"heater":1,"fan":2,"aux":3}' "Set relay assignments"
test_endpoint "GET" "/relay/status" "" "Get relay status"
test_endpoint "POST" "/relay/set" '{"relay":2,"state":true}' "Set relay 2 ON"
test_endpoint "POST" "/relay/set" '{"relay":2,"state":false}' "Set relay 2 OFF"
echo ""

echo "=== INPUT ENDPOINTS ==="
test_endpoint "GET" "/inputs/status" "" "Get digital input status"
echo ""

echo "=== EMERGENCY ENDPOINTS ==="
test_endpoint "POST" "/temperature/emergency/stop" "" "Activate emergency stop"
test_endpoint "GET" "/temperature/status" "" "Check emergency status"
test_endpoint "POST" "/temperature/emergency/clear" "" "Clear emergency stop"
echo ""

echo "=== COMPREHENSIVE CONFIG TEST ==="
config_json='{
    "location": "Main Office",
    "setpoint": 24.0,
    "delta": 1.0,
    "compensation": 0.5,
    "mode": 3,
    "enabled": true,
    "limits": {
        "emergency_high": 50.0,
        "emergency_low": -10.0
    },
    "timing": {
        "min_on_time": 180000,
        "min_off_time": 180000
    },
    "relays": {
        "compressor": 0,
        "heater": 1,
        "fan": 2,
        "aux": 3
    }
}'
test_endpoint "POST" "/temperature/config" "$config_json" "Full configuration update"
test_endpoint "GET" "/temperature/config" "" "Verify configuration"
echo ""

echo "=== RESET TEST ==="
test_endpoint "POST" "/temperature/reset" "" "Reset to defaults"
test_endpoint "GET" "/temperature/config" "" "Verify reset"
echo ""

echo "========================================="
echo "API Testing Complete!"
echo "========================================="