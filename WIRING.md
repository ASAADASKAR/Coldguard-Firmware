# ColdGuard Hardware — Electrical Connections

## Components
- ESP32 AZDelivery DevKit V4
- DS18B20 waterproof temperature sensor (1M cable)
- 4.7kΩ resistor (pull-up)
- Breadboard MB-102
- Dupont cables (M-F)

---

## Wiring Overview

| From | To | Color | Note |
|---|---|---|---|
| ESP32 3.3V | Breadboard Row A | Red | Power |
| ESP32 GND | Breadboard Row B | Black | Ground |
| ESP32 GPIO4 | Breadboard Row C | Yellow | Data |
| DS18B20 VCC | Breadboard Row A | Red | Same row as 3.3V |
| DS18B20 GND | Breadboard Row B | Black | Same row as GND |
| DS18B20 DQ | Breadboard Row C | Yellow | Same row as GPIO4 |
| Resistor Leg 1 | Breadboard Row A | — | Same row as 3.3V |
| Resistor Leg 2 | Breadboard Row C | — | Same row as GPIO4 |

---

## Breadboard Layout
Row A (3.3V):  [ESP32 3.3V] ── [DS18B20 VCC] ── [Resistor Leg 1]
Row B (GND):   [ESP32 GND]  ── [DS18B20 GND]
Row C (GPIO4): [ESP32 GPIO4] ── [DS18B20 DQ]  ── [Resistor Leg 2]

---

## Why Pull-up Resistor?

DS18B20 uses OneWire protocol — the data line (DQ)
needs to be pulled HIGH by default.

Without 4.7kΩ resistor:
- Sensor reads -127°C (error)
- Sensor not found

With 4.7kΩ resistor between 3.3V and DQ:
- Correct temperature reading ✅

---

## GPIO Pin — Where is GPIO4?

On ESP32 DevKit V4, GPIO4 is on the RIGHT side of the board.
Count from the top — check the pin label on the board.

---

## Common Errors

| Symptom | Cause | Fix |
|---|---|---|
| `-127°C` | No resistor or wrong GPIO | Check resistor + GPIO4 |
| `85°C` | Power issue | Check 3.3V connection |
| `Sensor not found` | Wrong wiring | Check all connections |
| Sensor very hot | VCC/GND reversed | Swap red/black cables! |

---

## Tested
- Date: 2026-05-20
- Result: ✅ 21.9°C reading confirmed
- Hardware ticket: KAN-28
- Firmware ticket: KAN-26