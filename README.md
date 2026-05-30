# ColdGuard Firmware

ESP32 firmware for ColdGuard — IoT temperature monitoring for restaurants, pharmacies and labs.

---

## Hardware

| Component | Details |
|---|---|
| Microcontroller | ESP32 AZDelivery DevKit V4 |
| Sensor | DS18B20 waterproof (1M cable) |
| Resistor | 4.7kΩ pull-up |
| Connection | Micro-USB (data cable required!) |

## Wiring

See [WIRING.md](WIRING.md) for full wiring diagram.

| DS18B20 Pin | Color | ESP32 Pin |
|---|---|---|
| VCC | Red | 3.3V |
| GND | Black | GND |
| DQ | Yellow | GPIO4 |

4.7kΩ resistor between 3.3V and GPIO4 (pull-up).

---

## Getting Started

### Prerequisites
- VSCode
- PlatformIO Extension
- Wokwi Extension (for simulator)

### Installation

**Step 1 — Clone the repository:**
```bash
git clone https://github.com/asaadaskar/Coldguard-Firmware.git
cd Coldguard-Firmware
```

**Step 2 — Create config.h:**
```bash
cd ColdGuard/src
cp config.example.h config.h
```

**Step 3 — Fill in your credentials in config.h:**
```cpp
#define WIFI_SSID     "your-wifi-name"
#define WIFI_PASSWORD "your-wifi-password"
#define API_URL       "http://YOUR_SERVER_IP:8000/api/temperature/"
#define DEVICE_KEY    "your-device-key"
```

---

## Running with Wokwi Simulator (no hardware needed)

**Step 1 — Set Wokwi credentials in config.h:**
```cpp
#define WIFI_SSID     "Wokwi-GUEST"
#define WIFI_PASSWORD ""
#define API_URL       "http://host.wokwi.internal:8000/api/temperature/"
#define DEVICE_KEY    "coldguard-device-003"
```

**Step 2 — Start Django Backend:**
```bash
cd Coldguard-Backend
python3 manage.py runserver
```

**Step 3 — Compile firmware:**
```bash
cd Coldguard-Firmware/ColdGuard
~/.platformio/penv/bin/pio run
```

**Step 4 — Start Wokwi Simulation:**
VSCode → Wokwi Extension → Start Simulation

**Step 5 — Watch Serial Monitor output:**
[00:01] [WIFI] Connecting to Wokwi-GUEST...
[00:03] [WIFI] Connected! IP: 10.13.37.2
[00:05] [TEMP] 22.5°C
[00:05] [WARN] TOO WARM! Cold chain at risk!
[00:05] [HTTP] POST → {...}
[00:05] [HTTP] Success! Code: 201

---

## Flashing to Real Hardware

**Step 1 — Set real credentials in config.h:**
```cpp
#define WIFI_SSID     "your-wifi-name"
#define WIFI_PASSWORD "your-wifi-password"
#define API_URL       "http://192.168.X.X:8000/api/temperature/"
#define DEVICE_KEY    "your-device-key"
```

**Step 2 — Connect ESP32 via Micro-USB data cable**

⚠️ Must be a data cable — charging-only cables will not work!

**Step 3 — Flash firmware:**
```bash
cd ColdGuard
~/.platformio/penv/bin/pio run --target upload
```

**Step 4 — Open Serial Monitor:**
```bash
~/.platformio/penv/bin/pio device monitor \
  --port /dev/cu.usbserial-XXX \
  --baud 115200
```

---

## Serial Monitor Log Format
[MM:SS] [LEVEL] message
[00:01] [INFO]  ColdGuard v2.0 started!
[00:02] [WIFI]  Connected! IP: 192.168.1.42
[00:05] [TEMP]  4.2°C
[00:05] [INFO]  Temperature in normal range.
[00:05] [HTTP]  Success! Code: 201
[01:30] [WARN]  TOO WARM! Cold chain at risk!
[01:30] [HTTP]  POST → {"temperature":9.5,...}

---

## Log Levels

| Level | Meaning |
|---|---|
| `INFO` | Normal operation |
| `WARN` | Temperature alarm |
| `ERROR` | Sensor or connection error |
| `WIFI` | WiFi status |
| `HTTP` | API communication |
| `TEMP` | Temperature reading |

---

## Temperature Thresholds

| Setting | Value |
|---|---|
| Max safe temperature | 8.0°C |
| Min safe temperature | 1.0°C |
| Measurement interval | 5s (testing) / 60s (production) |
| HTTP retries | 3 |

---

## Common Errors

| Error | Cause | Fix |
|---|---|---|
| `Sensor not found` | Wrong wiring | Check WIRING.md |
| `-127°C` | Missing resistor | Add 4.7kΩ pull-up |
| `WiFi FAILED` | Wrong credentials | Check config.h |
| `HTTP Failed: -1` | Server unreachable | Check API_URL |
| `HTTP Failed: 404` | Device not in DB | Create device in Django admin |

---

## Related Repositories

| Repo | Description |
|---|---|
| [Coldguard-Backend](https://github.com/asaadaskar/Coldguard-Backend) | Django REST API |
| [Coldguard-Frontend](https://github.com/asaadaskar/Coldguard-Frontend) | Dashboard |
