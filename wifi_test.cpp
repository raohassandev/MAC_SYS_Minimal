#include <WiFi.h>

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("=== WiFi Scan Test ===");
  
  // Set WiFi to station mode
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  
  Serial.println("Starting WiFi scan...");
  
  // Scan for networks
  int n = WiFi.scanNetworks();
  
  Serial.printf("Scan complete. Found %d networks:\n", n);
  
  if (n == 0) {
    Serial.println("No networks found!");
  } else {
    for (int i = 0; i < n; ++i) {
      Serial.printf("%d: %s (%d dBm) %s\n", 
        i + 1, 
        WiFi.SSID(i).c_str(), 
        WiFi.RSSI(i),
        (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "Open" : "Encrypted"
      );
    }
  }
  
  Serial.println("\n=== Test AP Mode ===");
  
  WiFi.mode(WIFI_AP);
  bool apStarted = WiFi.softAP("TEST-AP", "test123");
  Serial.printf("AP started: %s\n", apStarted ? "SUCCESS" : "FAILED");
  Serial.printf("AP IP: %s\n", WiFi.softAPIP().toString().c_str());
  
  Serial.println("\n=== Test AP+STA Mode ===");
  
  WiFi.mode(WIFI_AP_STA);
  delay(100);
  
  Serial.println("Scanning in AP+STA mode...");
  WiFi.scanDelete();
  int n2 = WiFi.scanNetworks();
  Serial.printf("AP+STA scan found %d networks\n", n2);
}

void loop() {
  delay(10000);
  Serial.println("Still running...");
}