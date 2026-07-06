/*
 * ESP8266 Environment Validation Sketch
 *
 * Tests core ESP8266 functionality:
 * - System information (chip ID, flash size, CPU speed)
 * - GPIO output (built-in LED blink)
 * - WiFi scan
 * - Memory info
 * - Serial communication
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>

// Pin definitions for common ESP8266 boards
// NodeMCU: Built-in LED on D0 (GPIO16), active LOW
// WeMos D1 Mini: Built-in LED on D4 (GPIO2), active LOW
// ESP-01: Built-in LED on GPIO2, active LOW
#if defined(LED_BUILTIN)
  #define TEST_LED LED_BUILTIN
#else
  #define TEST_LED 2  // GPIO2 common across most boards
#endif

// Forward declarations
void printSystemInfo();
void testBlink();
void testWiFiScan();
void printMemoryInfo();

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(5000);

  // Wait for serial connection (5s timeout for debug)
  unsigned long start = millis();
  while (!Serial && (millis() - start < 5000)) {
    delay(100);
  }

  Serial.println();
  Serial.println("=========================================");
  Serial.println("  ESP8266 Environment Validation Sketch");
  Serial.println("=========================================");
  Serial.println();

  // Init LED
  pinMode(TEST_LED, OUTPUT);
  digitalWrite(TEST_LED, HIGH);  // off (active LOW)

  // Run tests
  printSystemInfo();
  testBlink();
  printMemoryInfo();
  testWiFiScan();

  Serial.println();
  Serial.println("=== All tests completed! ===");
  Serial.println("Entering main loop (LED heartbeat)...");
}

void loop() {
  // Heartbeat blink
  digitalWrite(TEST_LED, LOW);   // ON
  delay(200);
  digitalWrite(TEST_LED, HIGH);  // OFF
  delay(1800);

  // Keep printing something periodically so we know it's alive
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 10000) {
    lastPrint = millis();
    Serial.print("[");
    Serial.print(millis() / 1000);
    Serial.println("s] System running...");
  }
}

// ====================================================================
// Test 1: System Information
// ====================================================================
void printSystemInfo() {
  Serial.println("[TEST] System Information");
  Serial.println("------------------------");

  // Chip info
  Serial.print("  Chip ID          : 0x");
  Serial.print(ESP.getChipId(), HEX);
  Serial.print(" (");
  Serial.print(ESP.getChipId());
  Serial.println(")");

  Serial.print("  CPU Frequency    : ");
  Serial.print(ESP.getCpuFreqMHz());
  Serial.println(" MHz");

  Serial.print("  SDK Version      : ");
  Serial.println(ESP.getSdkVersion());

  Serial.print("  Core Version     : ");
  Serial.println(ESP.getCoreVersion());

  Serial.print("  Boot Version     : ");
  Serial.println(ESP.getBootVersion());

  Serial.print("  Boot Mode        : ");
  Serial.println(ESP.getBootMode());

  // Flash info
  Serial.print("  Flash Chip ID    : 0x");
  Serial.println(ESP.getFlashChipId(), HEX);

  Serial.print("  Flash Size       : ");
  Serial.print(ESP.getFlashChipRealSize() / 1024);
  Serial.println(" KB");

  Serial.print("  Flash Speed      : ");
  Serial.print(ESP.getFlashChipSpeed() / 1000000);
  Serial.println(" MHz");

  Serial.print("  Flash Mode       : ");
  Serial.println(ESP.getFlashChipMode());

  // Unique MAC
  uint8_t mac[6];
  WiFi.macAddress(mac);
  Serial.print("  MAC Address      : ");
  for (int i = 0; i < 6; i++) {
    if (i > 0) Serial.print(":");
    Serial.print(mac[i], HEX);
  }
  Serial.println();

  // Reset reason
  Serial.print("  Reset Reason     : ");
  Serial.println(ESP.getResetReason());

  // Free sketch space
  Serial.print("  Sketch Size      : ");
  Serial.print(ESP.getSketchSize());
  Serial.print(" bytes (used: ");
  Serial.print((ESP.getSketchSize() * 100) / ESP.getFlashChipRealSize());
  Serial.println("%)");

  Serial.println();
}

// ====================================================================
// Test 2: GPIO Blink Test
// ====================================================================
void testBlink() {
  Serial.println("[TEST] GPIO Blink (LED on pin " + String(TEST_LED) + ")");
  Serial.println("  Blinking 5 times...");

  for (int i = 0; i < 5; i++) {
    digitalWrite(TEST_LED, LOW);   // ON
    delay(150);
    digitalWrite(TEST_LED, HIGH);  // OFF
    delay(150);
  }

  Serial.println("  [PASS] GPIO output works");
  Serial.println();
}

// ====================================================================
// Test 3: Memory Information
// ====================================================================
void printMemoryInfo() {
  Serial.println("[TEST] Memory Information");
  Serial.println("-----------------------");

  Serial.print("  Free Heap         : ");
  Serial.print(ESP.getFreeHeap());
  Serial.println(" bytes");

  Serial.print("  Max Free Block    : ");
  Serial.print(ESP.getMaxFreeBlockSize());
  Serial.println(" bytes");

  Serial.print("  Heap Fragmentation: ");
  Serial.print(ESP.getHeapFragmentation());
  Serial.println("%");

  Serial.print("  Free Stack        : ");
  Serial.print(ESP.getFreeContStack());
  Serial.println(" bytes");

  // Note: ESP8266 has no PSRAM (unlike ESP32)

  Serial.println();
}

// ====================================================================
// Test 4: WiFi Scan
// ====================================================================
void testWiFiScan() {
  Serial.println("[TEST] WiFi Networks Scan");
  Serial.println("  Scanning...");

  // Set WiFi to station mode and disconnect
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  int networks = WiFi.scanNetworks();
  Serial.print("  Found ");
  Serial.print(networks);
  Serial.println(" networks");

  if (networks > 0) {
    Serial.println();
    Serial.println("  Networks:");
    Serial.println("  #  SSID                              RSSI   CH  Enc");
    Serial.println("  -- --------------------------------- ------ ---  ---");

    for (int i = 0; i < networks && i < 10; i++) {
      char buf[40];
      String ssid = WiFi.SSID(i);
      ssid.toCharArray(buf, 39);

      Serial.print("  ");
      if (i < 9) Serial.print(" ");
      Serial.print(i + 1);
      Serial.print(" ");
      Serial.print(buf);
      for (int s = ssid.length(); s < 33; s++) Serial.print(" ");

      Serial.print(" ");
      Serial.print(WiFi.RSSI(i));
      Serial.print(" dBm");

      Serial.print("  ");
      Serial.print(WiFi.channel(i));
      if (WiFi.channel(i) < 10) Serial.print(" ");

      Serial.print("  ");
      Serial.println(WiFi.encryptionType(i) == AUTH_OPEN ? "OPEN" : "YES");
    }
    Serial.println("  (showing max 10 networks)");
  }

  Serial.println("  [PASS] WiFi radio functional");
  Serial.println();
}
