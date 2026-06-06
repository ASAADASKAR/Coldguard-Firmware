// ColdGuard/src/config.example.h
// ⚠️ Copy this file to config.h and fill in your values!
// config.h is NOT committed to Git — keep your credentials safe!

#ifndef CONFIG_H
#define CONFIG_H

// WiFi credentials
#define WIFI_SSID       "YOUR_WIFI_SSID"
#define WIFI_PASSWORD   "YOUR_WIFI_PASSWORD"

// API Server — local development
#define API_URL         "http://YOUR_SERVER_IP:8000/api/temperature/"

// Log API Endpoint
#define LOG_URL         "http://YOUR_SERVER_IP:8000/api/logs/"

// Device identification
#define DEVICE_KEY      "YOUR_DEVICE_KEY"

#endif