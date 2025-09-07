#!/bin/bash
# Comprehensive MAC-SYS WiFi Portal Testing Script

IP="192.168.4.1"
BASE_URL="http://$IP"

echo "=== MAC-SYS WiFi Portal Testing Suite ==="
echo "Device IP: $IP"
echo "Time: $(date)"
echo

# Test 1: Basic connectivity
echo "🔌 Test 1: Basic Device Connectivity"
if curl -s --connect-timeout 5 "$BASE_URL" > /dev/null; then
    echo "✅ Device is reachable at $IP"
else
    echo "❌ Device not reachable. Make sure you're connected to MACSYS-CONFIG WiFi"
    exit 1
fi
echo

# Test 2: Main portal page
echo "🏠 Test 2: Main Portal Page"
response=$(curl -s "$BASE_URL")
if [[ $response == *"MAC-SYS WiFi Setup"* ]]; then
    echo "✅ Main portal page loads correctly"
    if [[ $response == *"WiFi Network Name (SSID)"* ]]; then
        echo "✅ Manual entry form present"
    else
        echo "❌ Manual entry form missing"
    fi
    if [[ $response == *"Scan for Networks"* ]]; then
        echo "✅ Scan button present"
    else
        echo "❌ Scan button missing"
    fi
else
    echo "❌ Main portal page failed to load properly"
fi
echo

# Test 3: Mobile captive portal endpoints
echo "📱 Test 3: Mobile Captive Portal Detection"
endpoints=(
    "/generate_204"     # Android
    "/fwlink"          # Microsoft  
    "/hotspot-detect.html"  # iOS
    "/connecttest.txt" # Windows
    "/redirect"        # Generic
)

for endpoint in "${endpoints[@]}"; do
    response=$(curl -s -w "%{http_code}" "$BASE_URL$endpoint")
    http_code="${response: -3}"
    if [[ "$http_code" == "200" ]]; then
        echo "✅ Mobile endpoint $endpoint works (HTTP $http_code)"
    else
        echo "❌ Mobile endpoint $endpoint failed (HTTP $http_code)"
    fi
done
echo

# Test 4: WiFi scanning
echo "📡 Test 4: WiFi Network Scanning"
echo "Note: This will temporarily disconnect your WiFi for 2-3 seconds"
read -p "Press Enter to continue with scan test..."

scan_result=$(curl -s "$BASE_URL/scan" 2>/dev/null)
if [[ $scan_result == "["* ]]; then
    echo "✅ Scan endpoint responds with JSON"
    network_count=$(echo "$scan_result" | grep -o '"ssid"' | wc -l)
    echo "📊 Found $network_count networks"
    
    if [[ $network_count -gt 0 ]]; then
        echo "✅ WiFi scanning is working!"
        echo "Sample networks found:"
        echo "$scan_result" | grep -o '"ssid":"[^"]*"' | head -3
    else
        echo "⚠️  No networks found - may be location/interference issue"
    fi
else
    echo "❌ Scan failed or returned invalid response"
    echo "Response: $scan_result"
fi
echo

# Test 5: WiFi connection with test credentials
echo "🔗 Test 5: WiFi Connection Test"
echo "Testing with dummy credentials (should fail gracefully)..."

connect_result=$(curl -s -X POST \
    -H "Content-Type: application/x-www-form-urlencoded" \
    -d "ssid=TestNetwork&password=testpass123" \
    "$BASE_URL/connect")

if [[ $connect_result == *"Attempting connection"* ]] || [[ $connect_result == *"FAILED"* ]]; then
    echo "✅ Connection endpoint responds properly"
    echo "Response: $connect_result"
else
    echo "❌ Connection endpoint failed"
    echo "Response: $connect_result"
fi
echo

# Test 6: Connection with your actual network
echo "🏠 Test 6: Real Network Connection"
echo "Enter your actual WiFi credentials to test real connection:"
read -p "WiFi Network Name (SSID): " real_ssid
read -s -p "WiFi Password: " real_password
echo

if [[ -n "$real_ssid" ]]; then
    echo "Attempting connection to '$real_ssid'..."
    real_connect_result=$(curl -s -X POST \
        -H "Content-Type: application/x-www-form-urlencoded" \
        -d "ssid=$real_ssid&password=$real_password" \
        "$BASE_URL/connect")
    
    echo "Response: $real_connect_result"
    
    if [[ $real_connect_result == *"SUCCESS"* ]]; then
        echo "🎉 Successfully connected to $real_ssid!"
        echo "You should now see the connection on your OLED display"
    elif [[ $real_connect_result == *"FAILED"* ]]; then
        echo "❌ Connection failed - check password and network name"
    fi
else
    echo "⏭️  Skipping real network test"
fi
echo

# Test 7: Reset functionality
echo "🔄 Test 7: WiFi Reset"
read -p "Test WiFi reset? This will clear saved credentials [y/N]: " reset_test
if [[ $reset_test =~ ^[Yy] ]]; then
    reset_result=$(curl -s "$BASE_URL/reset")
    if [[ $reset_result == *"cleared"* ]]; then
        echo "✅ WiFi reset works"
        echo "Response: $reset_result"
    else
        echo "❌ WiFi reset failed"
        echo "Response: $reset_result"
    fi
else
    echo "⏭️  Skipping reset test"
fi
echo

# Test 8: Error handling
echo "🚨 Test 8: Error Handling"
# Test empty SSID
empty_result=$(curl -s -X POST \
    -H "Content-Type: application/x-www-form-urlencoded" \
    -d "ssid=&password=test" \
    "$BASE_URL/connect")

if [[ $empty_result == *"ERROR"* ]] || [[ $empty_result == *"No SSID"* ]]; then
    echo "✅ Empty SSID handling works"
else
    echo "❌ Empty SSID not handled properly"
    echo "Response: $empty_result"
fi

# Test invalid endpoint
invalid_result=$(curl -s -w "%{http_code}" "$BASE_URL/invalid")
http_code="${invalid_result: -3}"
if [[ "$http_code" == "200" ]]; then
    echo "✅ Invalid URLs redirect to main page (captive portal behavior)"
else
    echo "⚠️  Invalid URL returned HTTP $http_code"
fi
echo

echo "=== Test Summary ==="
echo "✅ Basic connectivity and portal functionality tested"
echo "✅ Mobile device compatibility endpoints verified" 
echo "✅ WiFi scanning and connection tested"
echo "✅ Error handling verified"
echo
echo "🎯 Next Steps:"
echo "1. Connect to MACSYS-CONFIG WiFi"
echo "2. Open browser to http://192.168.4.1"
echo "3. Enter your WiFi credentials manually"
echo "4. Check OLED display for connection status"
echo
echo "📋 Manual Tests Still Needed:"
echo "- Mobile browser captive portal auto-popup"
echo "- OLED display cycling with WiFi status"
echo "- Actual WiFi connection stability"
echo "- Multiple device connections"
echo

echo "Test completed at $(date)"