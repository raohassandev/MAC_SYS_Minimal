#!/bin/bash

# MAC-SYS HVAC Controller Automated Test Script
# Tests all major functionality via curl commands

IP="192.168.1.106"
BASE_URL="http://$IP"

echo "========================================"
echo "MAC-SYS HVAC Controller Automated Tests"
echo "Target: $BASE_URL"
echo "========================================"
echo ""

# Color codes for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test counter
TESTS_PASSED=0
TESTS_FAILED=0

# Function to test endpoint
test_endpoint() {
    local description="$1"
    local method="$2"
    local endpoint="$3"
    local data="$4"
    local expected_code="$5"
    
    echo -n "Testing: $description ... "
    
    if [ "$method" == "GET" ]; then
        response=$(curl -s -w "\n%{http_code}" "$BASE_URL$endpoint" 2>/dev/null)
    else
        response=$(curl -s -w "\n%{http_code}" -X "$method" -d "$data" "$BASE_URL$endpoint" 2>/dev/null)
    fi
    
    http_code=$(echo "$response" | tail -1)
    body=$(echo "$response" | head -n -1)
    
    if [ "$http_code" == "$expected_code" ]; then
        echo -e "${GREEN}✓ PASSED${NC} (HTTP $http_code)"
        ((TESTS_PASSED++))
        return 0
    else
        echo -e "${RED}✗ FAILED${NC} (Expected $expected_code, Got $http_code)"
        ((TESTS_FAILED++))
        return 1
    fi
}

# 1. Test Main Page
echo "=== 1. CONNECTIVITY TESTS ==="
test_endpoint "Main status page" "GET" "/" "200"
test_endpoint "API status endpoint" "GET" "/api/status" "200"
echo ""

# 2. Test Relay Control
echo "=== 2. RELAY CONTROL TESTS ==="

# Test individual relay control
for i in 0 1 2 3 4 5; do
    # Turn relay ON
    test_endpoint "Turn Relay $((i+1)) ON" "POST" "/relay" "relay=$i&action=set&state=1" "200"
    sleep 1
    
    # Turn relay OFF
    test_endpoint "Turn Relay $((i+1)) OFF" "POST" "/relay" "relay=$i&action=set&state=0" "200"
    sleep 1
done

# Test bulk relay control
test_endpoint "Turn all relays ON" "POST" "/relay" "action=all&state=1" "200"
sleep 2
test_endpoint "Turn all relays OFF" "POST" "/relay" "action=all&state=0" "200"
echo ""

# 3. Test Temperature Control
echo "=== 3. TEMPERATURE CONTROL TESTS ==="

# Test different temperature setpoints
test_endpoint "Set temperature to 20°C" "POST" "/tempcontrol" "zone=0&setpoint=20&delta=1&mode=1" "200"
sleep 2
test_endpoint "Set temperature to 25°C" "POST" "/tempcontrol" "zone=0&setpoint=25&delta=1.5&mode=1" "200"
sleep 2
test_endpoint "Set temperature to 22°C" "POST" "/tempcontrol" "zone=0&setpoint=22&delta=1&mode=1" "200"

# Test different modes
test_endpoint "Set mode to OFF" "POST" "/tempcontrol" "zone=0&setpoint=22&delta=1&mode=0" "200"
sleep 1
test_endpoint "Set mode to HEATING" "POST" "/tempcontrol" "zone=0&setpoint=22&delta=1&mode=1" "200"
sleep 1
test_endpoint "Set mode to COOLING" "POST" "/tempcontrol" "zone=0&setpoint=22&delta=1&mode=2" "200"
sleep 1
test_endpoint "Set mode to AUTO" "POST" "/tempcontrol" "zone=0&setpoint=22&delta=1&mode=3" "200"
echo ""

# 4. Test Network Configuration
echo "=== 4. NETWORK CONFIGURATION TESTS ==="
test_endpoint "WiFi config page" "GET" "/wifi-config" "200"
echo ""

# 5. API Status Check
echo "=== 5. JSON API VALIDATION ==="
echo "Fetching current status via API..."
status=$(curl -s "$BASE_URL/api/status")
echo "API Response:"
echo "$status" | python3 -m json.tool 2>/dev/null || echo "$status"
echo ""

# 6. Performance Test
echo "=== 6. PERFORMANCE TESTS ==="
echo "Testing response time (10 requests)..."
total_time=0
for i in {1..10}; do
    start=$(date +%s%N)
    curl -s "$BASE_URL/api/status" > /dev/null
    end=$(date +%s%N)
    elapsed=$((($end - $start) / 1000000))
    total_time=$((total_time + elapsed))
    echo -n "Request $i: ${elapsed}ms "
done
echo ""
avg_time=$((total_time / 10))
echo "Average response time: ${avg_time}ms"
echo ""

# 7. Relay Toggle Stress Test
echo "=== 7. RELAY STRESS TEST ==="
echo "Rapid relay toggling (10 cycles)..."
for cycle in {1..10}; do
    echo -n "Cycle $cycle: "
    curl -s -X POST -d "relay=0&action=toggle" "$BASE_URL/relay" > /dev/null && echo -n "✓ "
    sleep 0.5
done
echo ""
echo ""

# 8. Temperature Control Logic Test
echo "=== 8. TEMPERATURE LOGIC TEST ==="
echo "Setting temperature below current (should activate heating)..."
current_temp=$(curl -s "$BASE_URL/api/status" | grep -o '"temperature":[0-9.]*' | cut -d: -f2)
echo "Current temperature: ${current_temp}°C"

# Set setpoint below current temperature
low_setpoint=$(echo "$current_temp - 5" | bc 2>/dev/null || echo "15")
test_endpoint "Set low setpoint ($low_setpoint°C)" "POST" "/tempcontrol" "zone=0&setpoint=$low_setpoint&delta=1&mode=1" "200"
sleep 3

# Check if heating activated
echo "Checking system status..."
curl -s "$BASE_URL/" | grep -q "ACTIVE" && echo -e "${GREEN}✓ Heating activated${NC}" || echo -e "${YELLOW}⚠ Heating may not have activated${NC}"
echo ""

# Summary
echo "========================================"
echo "TEST SUMMARY"
echo "========================================"
echo -e "Tests Passed: ${GREEN}$TESTS_PASSED${NC}"
echo -e "Tests Failed: ${RED}$TESTS_FAILED${NC}"

if [ $TESTS_FAILED -eq 0 ]; then
    echo -e "\n${GREEN}✅ ALL TESTS PASSED!${NC}"
    exit 0
else
    echo -e "\n${RED}⚠️ SOME TESTS FAILED${NC}"
    exit 1
fi