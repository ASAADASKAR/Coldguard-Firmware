/**
 * =====================================================
 * ColdGuard Firmware v2.0
 * =====================================================
 * Project:     ColdGuard — IoT Temperature Monitoring
 * Hardware:    ESP32 DevKit V4 + DS18B20 Sensor
 * Author:      Asaad Askar
 * Date:        May 2026
 * Repo:        github.com/asaadaskar/Coldguard-Firmware
 * =====================================================
 */

#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"

// ─────────────────────────────────────────────────────
// HARDWARE CONFIGURATION
// ─────────────────────────────────────────────────────
#define ONE_WIRE_BUS     4
#define TEMP_MAX         8.0
#define TEMP_MIN         1.0
#define MEASURE_INTERVAL 5000
#define MAX_RETRIES      3

// ─────────────────────────────────────────────────────
// LOG LEVELS
// ─────────────────────────────────────────────────────
#define LOG_INFO  "INFO"
#define LOG_WARN  "WARN"
#define LOG_ERROR "ERROR"
#define LOG_HTTP  "HTTP"
#define LOG_WIFI  "WIFI"
#define LOG_TEMP  "TEMP"

// ─────────────────────────────────────────────────────
// FORWARD DECLARATIONS
// ─────────────────────────────────────────────────────
void log(const char* level, const char* message);
void logMessage(const char* level, const char* format, ...);

// ─────────────────────────────────────────────────────
// OBJECTS
// ─────────────────────────────────────────────────────
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
int failedRequests = 0;

// ─────────────────────────────────────────────────────
// FUNCTION: log(level, message)
// ─────────────────────────────────────────────────────
void log(const char* level, const char* message) {
    unsigned long ms  = millis();
    unsigned long sec = ms / 1000;
    unsigned long min = sec / 60;
    Serial.printf("[%02lu:%02lu] [%s] %s\n",
        min, sec % 60, level, message);
}

// ─────────────────────────────────────────────────────
// FUNCTION: logMessage(level, format, ...)
// ─────────────────────────────────────────────────────
void logMessage(const char* level, const char* format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    log(level, buffer);
}

// ─────────────────────────────────────────────────────
// FUNCTION: sendLog(level, message)
// ─────────────────────────────────────────────────────
// Sends error/warning logs to Django backend.
// Only sends if WiFi is connected.
// ─────────────────────────────────────────────────────
void sendLog(const char* level, const char* message) {
    if (WiFi.status() != WL_CONNECTED) return;

    HTTPClient http;
    http.begin(LOG_URL);
    http.setTimeout(5000);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-Device-Key", DEVICE_KEY);

    JsonDocument doc;
    doc["level"]   = level;
    doc["message"] = message;

    String payload;
    serializeJson(doc, payload);

    int code = http.POST(payload);
    http.end();

    if (code == 201) {
        logMessage(LOG_INFO, "Log sent: [%s] %s", level, message);
    }
}

// ─────────────────────────────────────────────────────
// FUNCTION: connectWiFi()
// ─────────────────────────────────────────────────────
bool connectWiFi() {
    logMessage(LOG_WIFI, "Connecting to %s", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        logMessage(LOG_WIFI, "Connected! IP: %s",
            WiFi.localIP().toString().c_str());
        return true;
    }

    log(LOG_WIFI, "FAILED — no connection!");
    sendLog("ERROR", "WiFi connection failed");
    return false;
}

// ─────────────────────────────────────────────────────
// FUNCTION: sendToAPI(temp, status)
// ─────────────────────────────────────────────────────
bool sendToAPI(float temp, String status) {

    if (WiFi.status() != WL_CONNECTED) {
        log(LOG_WIFI, "WiFi lost — reconnecting...");
        if (!connectWiFi()) return false;
    }

    HTTPClient http;
    http.begin(API_URL);
    http.setTimeout(5000);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-Device-Key", DEVICE_KEY);

    JsonDocument doc;
    doc["temperature"] = temp;
    doc["status"]      = status;
    doc["device"]      = DEVICE_KEY;

    String payload;
    serializeJson(doc, payload);

    logMessage(LOG_HTTP, "POST → %s", payload.c_str());

    int code = http.POST(payload);
    http.end();

    if (code == 200 || code == 201) {
        logMessage(LOG_HTTP, "Success! Code: %d", code);
        failedRequests = 0;
        return true;
    }

    logMessage(LOG_HTTP, "Failed! Code: %d", code);
    failedRequests++;
    return false;
}

// ─────────────────────────────────────────────────────
// FUNCTION: getStatus(temp)
// ─────────────────────────────────────────────────────
String getStatus(float temp) {
    if (temp > TEMP_MAX) return "ALARM_HIGH";
    if (temp < TEMP_MIN) return "ALARM_LOW";
    return "OK";
}

// ─────────────────────────────────────────────────────
// SETUP
// ─────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);

    log(LOG_INFO, "=====================================");
    log(LOG_INFO, "  ColdGuard v2.0 started!");
    log(LOG_INFO, "=====================================");

    sensors.begin();
    log(LOG_INFO, "DS18B20 sensor initialized");

    connectWiFi();
}

// ─────────────────────────────────────────────────────
// LOOP
// ─────────────────────────────────────────────────────
void loop() {

    // STEP 1: Read temperature
    sensors.requestTemperatures();
    float tempC = sensors.getTempCByIndex(0);

    // STEP 2: Check sensor error
    if (tempC == DEVICE_DISCONNECTED_C) {
        log(LOG_ERROR, "Sensor not found! Check wiring.");
        sendLog("ERROR", "Sensor not found — check wiring");
        delay(MEASURE_INTERVAL);
        return;
    }

    // STEP 3: Log temperature
    logMessage(LOG_TEMP, "%.1f°C", tempC);

    // STEP 4: Determine status
    String status = getStatus(tempC);

    if (status == "ALARM_HIGH") {
        log(LOG_WARN, "TOO WARM! Cold chain at risk!");
    } else if (status == "ALARM_LOW") {
        log(LOG_WARN, "TOO COLD! Freezing risk!");
    } else {
        log(LOG_INFO, "Temperature in normal range.");
    }

    // STEP 5: Send to API with retry
    for (int i = 0; i < MAX_RETRIES; i++) {
        if (sendToAPI(tempC, status)) break;
        logMessage(LOG_HTTP, "Retry %d/%d", i + 1, MAX_RETRIES);
        delay(2000);
    }

    // STEP 6: Heartbeat check
    if (failedRequests >= 3) {
        log(LOG_WARN, "Server unreachable! Owner will be alarmed soon.");
        sendLog("WARN", "Server unreachable after 3 retries");
    }

    // STEP 7: Wait
    delay(MEASURE_INTERVAL);
}