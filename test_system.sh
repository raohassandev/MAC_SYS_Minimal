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
    
    # Default to 200 if no expected code provided
    if [ -z "$expected_code" ]; then
        expected_code="200"
    fi
    
    echo -n "Testing: $description ... "
    
    if [ "$method" == "GET" ]; then
        response=$(curl -s -w "\n%{http_code}" "$BASE_URL$endpoint" 2>/dev/null)
    else
        response=$(curl -s -w "\n%{http_code}" -X "$method" -d "$data" "$BASE_URL$endpoint" 2>/dev/null)
    fi
    
    http_code=$(echo "$response" | tail -1)
    body=$(echo "$response" | sed '$d')
    
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

# 9. Schedule Management Tests
echo "=== 9. SCHEDULE MANAGEMENT TESTS ==="

# Test schedule page access
test_endpoint "Schedule configuration page" "GET" "/schedule" "200"

# Test global schedule controls
test_endpoint "Enable global schedule" "POST" "/api/schedule/global" "enabled=true" "200"
sleep 1
test_endpoint "Disable global schedule" "POST" "/api/schedule/global" "enabled=false" "200"
sleep 1
test_endpoint "Re-enable global schedule" "POST" "/api/schedule/global" "enabled=true" "200"
sleep 1

# Test holiday mode
test_endpoint "Enable holiday mode" "POST" "/api/schedule/holiday" "enabled=true" "200"
sleep 1
test_endpoint "Disable holiday mode" "POST" "/api/schedule/holiday" "enabled=false" "200"
sleep 1

# Test zone schedule controls
test_endpoint "Enable zone 0 schedule" "POST" "/api/schedule/zone" "zone=0&enabled=true" "200"
sleep 1
test_endpoint "Disable zone 0 schedule" "POST" "/api/schedule/zone" "zone=0&enabled=false" "200"
sleep 1
test_endpoint "Re-enable zone 0 schedule" "POST" "/api/schedule/zone" "zone=0&enabled=true" "200"
sleep 1

# Test schedule event management
echo "Testing schedule event creation..."
event_data="zone=0&desc=TestEvent2&time=16:00&temp=23.0&delta=1.0&mode=1&days=32"  # Friday only (binary: 100000 = 32)
test_endpoint "Add schedule event" "POST" "/api/schedule/event" "$event_data" "200"
sleep 2

# Test schedule event removal (the newly added event should be at index 5 since zone 0 already has 5 events)
test_endpoint "Remove schedule event" "DELETE" "/api/schedule/event" "zone=0&index=5" "200"
echo ""

# 10. RTC and Time Management Tests  
echo "=== 10. RTC AND TIME MANAGEMENT TESTS ==="
echo "Checking schedule page for time information..."
schedule_response=$(curl -s "$BASE_URL/schedule" 2>/dev/null)

if echo "$schedule_response" | grep -q "Current Time:"; then
    echo -e "${GREEN}✓ Schedule page accessible with time display${NC}"
else
    echo -e "${RED}✗ Schedule page time display issue${NC}"
fi

if echo "$schedule_response" | grep -q "RTC Status:"; then
    echo -e "${GREEN}✓ RTC status displayed${NC}"  
else
    echo -e "${RED}✗ RTC status not displayed${NC}"
fi

if echo "$schedule_response" | grep -q "NTP Status:"; then
    echo -e "${GREEN}✓ NTP status displayed${NC}"
else
    echo -e "${RED}✗ NTP status not displayed${NC}"
fi

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