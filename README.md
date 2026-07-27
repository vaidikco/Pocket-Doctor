# PocketDoctor Mark 4

[![Platform](https://img.shields.io/badge/Platform-ESP32-blue.svg)](https://www.espressif.com/en/products/socs/esp32)
[![Core](https://img.shields.io/badge/Framework-Arduino%20C%2B%2B-00979D.svg)](https://www.arduino.cc/)
[![AI-Engine](https://img.shields.io/badge/AI%20Engine-Groq%20LLaMA%203.3%2070B-orange.svg)](https://groq.com/)
[![Gateway](https://img.shields.io/badge/Gateway-pocdoc.vaidik.co-purple.svg)](https://pocdoc.vaidik.co)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](file:///c:/Users/Vaidik.Laptop/Desktop/pocdocmk4/LICENSE)

**PocketDoctor Mark 4** is an autonomous, handheld medical triage assistant powered by an ESP32 microcontroller and Groq's `llama-3.3-70b-versatile` cloud model. It combines a physical OLED display and 2-axis joystick interface, persistent EEPROM profile storage, a local HTTP web server portal, and multi-turn LLM clinical reasoning for immediate point-of-care medical assessments.

Introduction to the Project.: [https://pocdoc.vaidik.co](https://pocdoc.vaidik.co)

---

## Table of Contents

1. [Problem Statement](#problem-statement)
2. [Key System Features](#key-system-features)
3. [System Architecture](#system-architecture)
4. [Hardware Pinout Summary](#hardware-pinout-summary)
5. [Technical Documentation Index](#technical-documentation-index)
6. [Firmware Codebase Walkthrough](#firmware-codebase-walkthrough)
7. [Software Dependencies & Setup](#software-dependencies--setup)
8. [Operating Guide](#operating-guide)
9. [Medical Disclaimer](#medical-disclaimer)
10. [License & Credits](#license--credits)

---

## Problem Statement

In modern healthcare systems—especially in rural areas, emergency triage centers, and resource-constrained environments—access to immediate clinical guidance is severely restricted. Patients face long waiting times for basic diagnostic triage, causing delays in acute treatment or unnecessary overcrowding in emergency rooms for minor complaints.

As detailed on [https://pocdoc.vaidik.co](https://pocdoc.vaidik.co), traditional health software relies heavily on smartphone apps, constant high-bandwidth internet, or complex installations. 

**PocketDoctor Mark 4** solves this by providing a low-cost, self-contained handheld device that:
- Allows physical symptom entry via OLED + Joystick without needing a smartphone app.
- Remembers patient demographics and medical history using 512-byte persistent EEPROM storage.
- Runs a dynamic 10-turn AI dialogue with Groq Cloud API over Wi-Fi.
- Synthesizes structured clinical assessments and hosts responsive HTML reports locally on the ESP32.

---

## Key System Features

- **Hardware User Interface**: 128x64 OLED display + 2-axis analog joystick with pushbutton debouncing and text input engine.
- **Persistent EEPROM Storage (512 Bytes)**: Magic-byte (`0xABCD`) verified storage for Wi-Fi credentials and up to 3 individual patient profiles.
- **Groq LLaMA 3.3 70B AI Engine**: Multi-turn dialogue generating 10 context-sensitive Yes/No follow-up questions and structured diagnostic summaries.
- **Embedded Web Management Portal**: Built-in Port 80 WebServer serving responsive HTML dashboards directly from ESP32 flash memory (`PROGMEM`).

---

## System Architecture

```
  +----------------------------------------------------------+
  |                  ESP32 Microcontroller                   |
  |                                                          |
  |  +-----------------+       +--------------------------+  |
  |  | 128x64 OLED     |       | 2-Axis Joystick + Button |  |
  |  | (SSD1306 I2C)   |       | (GPIO 34, 35, 32)        |  |
  |  +--------^--------+       +------------+-------------+  |
  |           |                             |                |
  |  +--------+-----------------------------v----------+  |
  |  |             Core System & UI State Engine       |  |
  |  +------+-----------------------+------------------+  |
  |         |                       |                     |
  |  +------v---------------+ +-----v------------------+  |
  |  | EEPROM (512 Bytes)   | | Web Server (Port 80)   |  |
  |  | Wi-Fi + 3 Profiles   | | PROGMEM HTML Templates |  |
  |  +----------------------+ +-----+------------------+  |
  +---------+-----------------------+---------------------+
            | Wi-Fi Connection (STA)
            v
  +----------------------------------------------------------+
  |             Groq Cloud API Infrastructure                |
  |             (llama-3.3-70b-versatile Model)              |
  +----------------------------------------------------------+
```

---

## Hardware Pinout Summary

| Component | Pin / Function | ESP32 GPIO | Description |
| :--- | :--- | :--- | :--- |
| **SSD1306 OLED** | SDA | **GPIO 21** | I2C Data Line |
| **SSD1306 OLED** | SCL | **GPIO 22** | I2C Clock Line |
| **Joystick** | VRx | **GPIO 34** | Analog X-Axis Navigation |
| **Joystick** | VRy | **GPIO 35** | Analog Y-Axis Value Adjust |
| **Joystick** | SW | **GPIO 32** | Digital Push Button (`INPUT_PULLUP`) |

For full wiring schematics, see [HARDWARE_PINOUT.md](file:///c:/Users/Vaidik.Laptop/Desktop/pocdocmk4/docs/HARDWARE_PINOUT.md).

---

## Technical Documentation Index

Comprehensive documentation is organized inside the [`docs/`](file:///c:/Users/Vaidik.Laptop/Desktop/pocdocmk4/docs) directory:

- **[Master Engineering Documentation (PDF)](file:///c:/Users/Vaidik.Laptop/Desktop/pocdocmk4/docs/master_documentation.pdf)** — Complete engineering handbook and manual ([Markdown](file:///c:/Users/Vaidik.Laptop/Desktop/pocdocmk4/docs/MASTER_DOCUMENTATION.md)).
- **[Visual Flow & System Diagrams (PDF)](file:///c:/Users/Vaidik.Laptop/Desktop/pocdocmk4/docs/visual_flow.pdf)** — High-resolution architecture flowcharts and sequence diagrams ([Markdown](file:///c:/Users/Vaidik.Laptop/Desktop/pocdocmk4/docs/ARCHITECTURE.md)).
- **[System Architecture Guide](file:///c:/Users/Vaidik.Laptop/Desktop/pocdocmk4/docs/ARCHITECTURE.md)** — State machine lifecycle and memory management.
- **[Hardware & Pinout Guide](file:///c:/Users/Vaidik.Laptop/Desktop/pocdocmk4/docs/HARDWARE_PINOUT.md)** — Wiring schematics and ADC threshold math.
- **[EEPROM Memory Specification](file:///c:/Users/Vaidik.Laptop/Desktop/pocdocmk4/docs/EEPROM_MAP.md)** — Binary memory map and profile serialization.
- **[Groq AI Integration](file:///c:/Users/Vaidik.Laptop/Desktop/pocdocmk4/docs/GROQ_AI_INTEGRATION.md)** — Multi-turn JSON prompt engineering and regex extraction.
- **[Web Server Portal Guide](file:///c:/Users/Vaidik.Laptop/Desktop/pocdocmk4/docs/WEB_SERVER_PORTAL.md)** — Port 80 routes and Web CRUD actions.

---

## Firmware Codebase Walkthrough

`src/main.ino` is structured into six modular software blocks:
1. **Network & Display Driver Initialization**: Configures OLED on I2C address `0x3C` and setup joystick pins.
2. **EEPROM Serialization Module**: Implements `loadWiFiConfig()`, `saveWiFiConfig()`, `saveProfilesToEEPROM()`, and `loadProfilesFromEEPROM()` starting at byte 100.
3. **On-Device UI Form Engine**: Manages joystick navigation, letter scrolling for name entry, and numeric adjusters for body metrics.
4. **Groq Cloud API Client**: Constructs dynamic JSON payloads, executes HTTP POST requests over HTTPS, and parses multi-turn dialogue choices using `ArduinoJson`.
5. **Embedded WebServer Module**: Hosts PROGMEM HTML dashboards for remote Wi-Fi updates, web profile management, and clinical HTML report viewing.
6. **State Machine Execution Loop**: Non-blocking `loop()` evaluating system state flags while servicing background web clients.

---

## Software Dependencies & Setup

- **Board Manager**: ESP32 Arduino Core (`esp32` by Espressif Systems v2.0+)
- **Required Libraries**: `Adafruit_SSD1306`, `Adafruit_GFX_Library`, `ArduinoJson`, `WiFi`, `HTTPClient`, `WebServer`, `EEPROM`.
- **Flashing Instructions**:
  1. Open `src/main.ino` in Arduino IDE or VS Code.
  2. Set Board to `ESP32 Dev Module` and Partition Scheme to `Default 4MB with spiffs`.
  3. Compile and flash to the ESP32 board.

---

## Operating Guide

### 1. Physical Device Workflow
1. **Power On**: Select an existing saved profile or select **Create New**.
2. **Select Symptoms & Duration**: Pick active symptoms from 35 standard options and select duration.
3. **Answer AI Questions**: Answer the 10 follow-up questions on screen using the joystick (**YES** / **NO**).
4. **View Summary & Report**: View short OLED metrics and open the full report using the device IP address.

### 2. Web Portal Workflow
1. Connect smartphone or laptop to the same Wi-Fi network.
2. Navigate to `http://<ESP32_IP_ADDRESS>` to access settings, edit profiles, or view clinical reports.

---

## Medical Disclaimer

PocketDoctor Mark 4 is an experimental AI-powered diagnostic prototype. Diagnostic outputs are generated by Large Language Model inference (`llama-3.3-70b-versatile`) and self-reported data. It does not replace professional medical advice, clinical diagnosis, or medical treatment.

---

## License & Credits

Licensed under the **MIT License**. See the [LICENSE](file:///c:/Users/Vaidik.Laptop/Desktop/pocdocmk4/LICENSE) file for details.

Copyright (c) 2026 **Vaidik Khurana**  
Project Gateway: [https://pocdoc.vaidik.co](https://pocdoc.vaidik.co)
