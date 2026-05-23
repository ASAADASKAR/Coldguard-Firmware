/**
 * =====================================================
 * ColdGuard Firmware v2.0
 * =====================================================
 * Project:     ColdGuard — IoT Temperature Monitoring
 * Hardware:    ESP32 DevKit V4 + DS18B20 Sensor
 * Author:      Asaad Askar
 * Date:        May 2026
 * Repo:        github.com/asaadaskar/Coldguard-Firmware
 *
 * What's new in v2.0 (compared to v1.0):
 *   - WiFi connection on startup
 *   - HTTP POST sends data to Django API
 *   - Retry logic: tries 3x if request fails
 *   - Heartbeat counter: warns if server unreachable
 *   - Device API Key: identifies which device sends data
 *
 * How it works (step by step):
 *   1. ESP32 starts → connects to WiFi
 *   2. Every 5 seconds: reads temperature from DS18B20
 *   3. Checks if temperature is in safe range
 *   4. Sends data to Django API via HTTP POST
 *   5. If sending fails → retries up to 3 times
 *   6. If 3+ failures → heartbeat warning
 *
 * Wiring:
 *   DS18B20 VCC  --> ESP32 3.3V
 *   DS18B20 GND  --> ESP32 GND
 *   DS18B20 DQ   --> ESP32 GPIO4
 *   4.7kOhm resistor between DQ and 3.3V (Pull-up)
 * =====================================================
 */

// ─────────────────────────────────────────────────────
// LIBRARIES
// ─────────────────────────────────────────────────────

#include <Arduino.h>          // Base ESP32 functions (setup, loop, Serial...)
#include <OneWire.h>          // 1-Wire protocol — needed for DS18B20
#include <DallasTemperature.h>// DS18B20 specific functions (read temp etc.)
#include <WiFi.h>             // ESP32 WiFi — connect to network
#include <HTTPClient.h>       // Send HTTP requests (GET, POST...)
#include <ArduinoJson.h>      // Build and parse JSON payloads

// ─────────────────────────────────────────────────────
// WIFI CONFIGURATION
// ─────────────────────────────────────────────────────

// WiFi network name (SSID)
// "Wokwi-GUEST" is the built-in simulator network
// On real hardware: replace with customer's WiFi name
const char* WIFI_SSID     = "YOUR_WIFI_SSID";

// WiFi password
// Empty "" because Wokwi-GUEST is an open network
// On real hardware: replace with customer's WiFi password
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// ─────────────────────────────────────────────────────
// API CONFIGURATION
// ─────────────────────────────────────────────────────

// URL of the Django API endpoint that receives temperature data
// Replace "your-server.com" with real Hetzner server URL in production
// Example: "http://coldguard.de/api/temperature/"
const char* API_URL      = "http://YOUR_SERVER_IP:8000/api/temperature/";

// Unique key that identifies this specific device
// Django uses this to know WHICH device is sending data
// Each customer gets a different key
// Example: "coldguard-restaurant-hamburg-01"
const char* DEVICE_KEY = "coldguard-device-003";

// ─────────────────────────────────────────────────────
// HARDWARE CONFIGURATION
// ─────────────────────────────────────────────────────

// GPIO pin where DS18B20 DATA wire is connected
// We chose GPIO4 — free pin, no conflicts
#define ONE_WIRE_BUS 4

// ─────────────────────────────────────────────────────
// TEMPERATURE THRESHOLDS
// ─────────────────────────────────────────────────────

// Upper limit: fridge should never go above 8°C
// If temp > 8.0 → send ALARM_HIGH → email to owner
#define TEMP_MAX 8.0

// Lower limit: fridge should never go below 1°C
// If temp < 1.0 → send ALARM_LOW → risk of freezing
#define TEMP_MIN 1.0

// ─────────────────────────────────────────────────────
// TIMING CONFIGURATION
// ─────────────────────────────────────────────────────

// How often to measure temperature (in milliseconds)
// 5000  = every 5 seconds  → for simulator (fast testing)
// 60000 = every 60 seconds → for production (real device)
#define MEASURE_INTERVAL 5000

// How many times to retry a failed HTTP request
// If all 3 retries fail → heartbeat warning is shown
#define MAX_RETRIES 3

// ─────────────────────────────────────────────────────
// OBJECTS
// ─────────────────────────────────────────────────────

// OneWire object: manages communication on the data wire
// Needs to know which GPIO pin the sensor is on
OneWire oneWire(ONE_WIRE_BUS);

// DallasTemperature object: handles DS18B20 specific commands
// Uses the OneWire object to talk to the sensor
DallasTemperature sensors(&oneWire);

// ─────────────────────────────────────────────────────
// GLOBAL STATE
// ─────────────────────────────────────────────────────

// Counts how many HTTP requests failed in a row
// Reset to 0 when a request succeeds
// Used to detect if server is unreachable (heartbeat)
int failedRequests = 0;

// ─────────────────────────────────────────────────────
// FUNCTION: connectWiFi()
// ─────────────────────────────────────────────────────
// Connects ESP32 to WiFi network
// Returns: true if connected, false if failed
// ─────────────────────────────────────────────────────
bool connectWiFi() {
  Serial.print("[WiFi] Connecting to ");
  Serial.println(WIFI_SSID);

  // Start WiFi connection with SSID and password
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  // Wait until connected — check every 500ms
  // Stop after 20 attempts (= 10 seconds max)
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print("."); // Show progress dots
    attempts++;
  }

  Serial.println(); // New line after dots

  // Check if we connected successfully
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("[WiFi] Connected! IP: ");
    Serial.println(WiFi.localIP()); // Show assigned IP address
    return true;
  }

  // If we reach here — connection failed
  Serial.println("[WiFi] FAILED — no connection!");
  return false;
}

// ─────────────────────────────────────────────────────
// FUNCTION: sendToAPI(temp, status)
// ─────────────────────────────────────────────────────
// Sends temperature data to Django API via HTTP POST
// Parameters:
//   temp   — temperature in Celsius (e.g. 4.5)
//   status — "OK", "ALARM_HIGH" or "ALARM_LOW"
// Returns: true if server responded with 200/201
// ─────────────────────────────────────────────────────
bool sendToAPI(float temp, String status) {

  // Check WiFi is still connected before sending
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[HTTP] WiFi lost — reconnecting...");
    if (!connectWiFi()) return false; // Give up if can't reconnect
  }

  // Create HTTP client object
  HTTPClient http;

  // Set the target URL (Django API endpoint)
  http.begin(API_URL);

  // Set timeout: give up if server doesn't respond in 5 seconds
  http.setTimeout(5000);

  // Set headers so Django knows what we're sending
  http.addHeader("Content-Type", "application/json"); // We're sending JSON
  http.addHeader("X-Device-Key", DEVICE_KEY);         // Our device identity

  // ── Build JSON payload ──
  // This is the data we send to Django
  // Django will receive: {"temperature": 22, "status": "ALARM_HIGH", "device": "coldguard-device-001"}
  JsonDocument doc;            // Create empty JSON document
  doc["temperature"] = temp;   // Add temperature value
  doc["status"]      = status; // Add status string
  doc["device"]      = DEVICE_KEY; // Add device identifier

  // Convert JSON document to String
  String payload;
  serializeJson(doc, payload);

  // Show what we're sending in Serial Monitor
  Serial.print("[HTTP] POST → ");
  Serial.println(payload);

  // ── Send HTTP POST request ──
  int code = http.POST(payload); // Returns HTTP response code
  http.end();                    // Close connection to free memory

  // Check if server accepted our data
  if (code == 200 || code == 201) {
    // 200 = OK, 201 = Created — both mean success
    Serial.print("[HTTP] Success! Code: ");
    Serial.println(code);
    failedRequests = 0; // Reset failure counter on success
    return true;
  }

  // If we reach here — request failed
  // code = -1 means no server found (DNS failed)
  Serial.print("[HTTP] Failed! Code: ");
  Serial.println(code);
  failedRequests++; // Increment failure counter
  return false;
}

// ─────────────────────────────────────────────────────
// FUNCTION: getStatus(temp)
// ─────────────────────────────────────────────────────
// Determines the status string based on temperature
// Returns: "OK", "ALARM_HIGH" or "ALARM_LOW"
// ─────────────────────────────────────────────────────
String getStatus(float temp) {
  if (temp > TEMP_MAX) return "ALARM_HIGH"; // Too warm
  if (temp < TEMP_MIN) return "ALARM_LOW";  // Too cold
  return "OK";                              // Normal range
}

// ─────────────────────────────────────────────────────
// FUNCTION: printAlarm(message)
// ─────────────────────────────────────────────────────
// Prints a formatted alarm message to Serial Monitor
// ─────────────────────────────────────────────────────
void printAlarm(String message) {
  Serial.println("=====================================");
  Serial.println("[ALARM] " + message);
  Serial.println("=====================================");
}

// ─────────────────────────────────────────────────────
// SETUP — runs once when ESP32 starts
// ─────────────────────────────────────────────────────
void setup() {

  // Start Serial Monitor at 115200 baud
  // This allows us to see debug output in the terminal
  Serial.begin(115200);

  // Print startup banner
  Serial.println("=====================================");
  Serial.println("  ColdGuard v2.0 started!");
  Serial.println("=====================================");

  // Initialize DS18B20 sensor
  // This searches for sensors on the OneWire bus
  sensors.begin();
  Serial.println("[Sensor] DS18B20 ready");

  // Connect to WiFi
  // If connection fails — device still measures but can't send data
  connectWiFi();
}

// ─────────────────────────────────────────────────────
// LOOP — runs forever, every MEASURE_INTERVAL ms
// ─────────────────────────────────────────────────────
void loop() {

  // ── STEP 1: Request temperature from sensor ──
  // Tell all sensors on the bus to measure temperature
  sensors.requestTemperatures();

  // Read the result from the first sensor (index 0)
  float tempC = sensors.getTempCByIndex(0);

  // ── STEP 2: Check for sensor error ──
  // DEVICE_DISCONNECTED_C = -127.0
  // This means sensor is not connected or wiring is wrong
  if (tempC == DEVICE_DISCONNECTED_C) {
    Serial.println("[ERROR] Sensor not found! Check wiring.");
    delay(MEASURE_INTERVAL);
    return; // Skip rest of loop and start again
  }

  // ── STEP 3: Print temperature to Serial Monitor ──
  Serial.println("-------------------------------------");
  Serial.print("[Temp] ");
  Serial.print(tempC, 1); // 1 decimal place, e.g. "22.0"
  Serial.println(" C");

  // ── STEP 4: Determine status and show alarm ──
  String status = getStatus(tempC);

  if (status == "ALARM_HIGH") {
    printAlarm("TOO WARM! Cold chain at risk!");
  } else if (status == "ALARM_LOW") {
    printAlarm("TOO COLD! Freezing risk!");
  } else {
    Serial.println("[OK]   Temperature in normal range.");
  }

  // ── STEP 5: Send data to Django API with retry ──
  // Try up to MAX_RETRIES times before giving up
  for (int i = 0; i < MAX_RETRIES; i++) {
    if (sendToAPI(tempC, status)) {
      break; // Success! Stop retrying
    }
    // Failed — show retry message and wait before next attempt
    Serial.print("[HTTP] Retry ");
    Serial.print(i + 1);
    Serial.print("/");
    Serial.println(MAX_RETRIES);
    delay(2000); // Wait 2 seconds before retry
  }

  // ── STEP 6: Heartbeat check ──
  // If 3 or more requests failed in a row
  // → server is unreachable (could be power outage)
  // → Django server will send email alarm to owner
  if (failedRequests >= 3) {
    Serial.println("[WARN] Server unreachable!");
    Serial.println("       Owner will be alarmed soon.");
  }

  // ── STEP 7: Wait before next measurement ──
  // 5000ms = 5 seconds in simulator
  // Change to 60000 for production
  delay(MEASURE_INTERVAL);
}