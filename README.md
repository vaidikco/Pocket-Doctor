# PocketDoctor Mark 4

[![Platform](https://img.shields.io/badge/Platform-ESP32-blue.svg)](https://www.espressif.com/en/products/socs/esp32)
[![Core](https://img.shields.io/badge/Framework-Arduino%20C%2B%2B-00979D.svg)](https://www.arduino.cc/)
[![AI-Engine](https://img.shields.io/badge/AI%20Engine-Groq%20LLaMA%203.3%2070B-orange.svg)](https://groq.com/)
[![Gateway](https://img.shields.io/badge/Gateway-pocdoc.vaidik.co-purple.svg)](https://pocdoc.vaidik.co)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](file:///c:/Users/Vaidik.Laptop/Desktop/pocdocmk4/LICENSE)

PocketDoctor Mark 4 is an autonomous, handheld medical triage assistant powered by an ESP32 microcontroller and Groq's `llama-3.3-70b-versatile` cloud model. It integrates a physical OLED display and two-axis joystick interface, persistent EEPROM profile storage, a local HTTP web server portal, direct SSL SMTP emergency dispatching, and multi-turn LLM clinical reasoning for point-of-care medical assessments.

Project Documentation Gateway: [https://pocdoc.vaidik.co](https://pocdoc.vaidik.co)

---

## Table of Contents

1. [Problem Statement](#problem-statement)
2. [Key System Features](#key-system-features)
3. [System Architecture](#system-architecture)
4. [Hardware Pinout Summary](#hardware-pinout-summary)
5. [Technical Documentation Index](#technical-documentation-index)
6. [Firmware Codebase Walkthrough](#firmware-codebase-walkthrough)
7. [Software Dependencies and Setup](#software-dependencies-and-setup)
8. [Operating Guide](#operating-guide)
9. [Medical Disclaimer](#medical-disclaimer)
10. [License and Credits](#license-and-credits)

---

## Problem Statement

In modern healthcare systems—particularly within rural clinics, emergency triage centers, and resource-constrained environments—access to immediate clinical guidance is frequently restricted. Patients face long waiting times for basic diagnostic triage, causing delays in acute treatment or unnecessary overcrowding in emergency departments for minor complaints.

As documented on [https://pocdoc.vaidik.co](https://pocdoc.vaidik.co), traditional mobile health software relies heavily on smartphones, continuous high-bandwidth cellular connections, or complex software installations.

PocketDoctor Mark 4 resolves these operational limitations by providing a low-cost, self-contained handheld device that:
- Facilitates physical symptom entry via an OLED screen and analog joystick without requiring an external smartphone application.
- Retains patient demographics and medical history using persistent EEPROM storage.
- Conducts an interactive, multi-turn AI clinical dialogue with the Groq Cloud API over Wi-Fi.
- Synthesizes structured clinical assessments and hosts responsive HTML reports locally on the ESP32.
- Dispatches direct encrypted SMTPS emergency alerts over SSL (Port 465) in critical situations.

---

## Key System Features

- **Hardware User Interface**: 128x64 OLED display + two-axis analog joystick with pushbutton debouncing and text input navigation.
- **Persistent EEPROM Storage (512 Bytes)**: Magic-byte (`0xABCF`) verified storage for Wi-Fi credentials, SMTP emergency contacts, and patient profiles.
- **Groq LLaMA 3.3 70B AI Engine**: Multi-turn dialogue generating context-sensitive follow-up questions and structured diagnostic summaries.
- **Direct SSL SMTP Dispatch Engine**: Native `WiFiClientSecure` SMTPS integration for sending HTML emergency emails directly from the ESP32.
- **Embedded Web Management Portal**: Built-in Port 80 WebServer serving responsive HTML control dashboards directly from ESP32 flash memory (`PROGMEM`).

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
  |  | Wi-Fi + Profiles     | | PROGMEM HTML Templates |  |
  |  +----------------------+ +-----+------------------+  |
  +---------+-----------------------+---------------------+
            | Wi-Fi Connection (STA / SoftAP)
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

For complete hardware documentation, refer to [HARDWARE_PINOUT.md](file:///c:/Users/Vaidik.Laptop/Desktop/pocdocmk4/docs/HARDWARE_PINOUT.md).

---

## Technical Documentation Index

Comprehensive technical documentation is organized within the [`docs/`](file:///c:/Users/Vaidik.Laptop/Desktop/pocdocmk4/docs) directory:

- **[Master Engineering Documentation (PDF)](file:///c:/Users/Vaidik.Laptop/Desktop/pocdocmk4/docs/master_documentation.pdf)** — Complete engineering handbook and manual ([Markdown](file:///c:/Users/Vaidik.Laptop/Desktop/pocdocmk4/docs/MASTER_DOCUMENTATION.md)).
- **[Visual Flow and System Diagrams (PDF)](file:///c:/Users/Vaidik.Laptop/Desktop/pocdocmk4/docs/visual_flow.pdf)** — Architecture flowcharts and sequence diagrams ([Markdown](file:///c:/Users/Vaidik.Laptop/Desktop/pocdocmk4/docs/ARCHITECTURE.md)).
- **[Official Release Notes](file:///c:/Users/Vaidik.Laptop/Desktop/pocdocmk4/RELEASE_NOTES.md)** — Firmware release map and version specifications.
- **[System Architecture Guide](file:///c:/Users/Vaidik.Laptop/Desktop/pocdocmk4/docs/ARCHITECTURE.md)** — State machine lifecycle and memory management.
- **[Hardware and Pinout Guide](file:///c:/Users/Vaidik.Laptop/Desktop/pocdocmk4/docs/HARDWARE_PINOUT.md)** — Wiring schematics and ADC threshold values.
- **[EEPROM Memory Specification](file:///c:/Users/Vaidik.Laptop/Desktop/pocdocmk4/docs/EEPROM_MAP.md)** — Memory layout and binary profile serialization.
- **[Groq AI Integration](file:///c:/Users/Vaidik.Laptop/Desktop/pocdocmk4/docs/GROQ_AI_INTEGRATION.md)** — Multi-turn JSON prompt structure and regex parsing.
- **[Web Server Portal Guide](file:///c:/Users/Vaidik.Laptop/Desktop/pocdocmk4/docs/WEB_SERVER_PORTAL.md)** — Port 80 HTTP routes and management workflows.

---

## Firmware Codebase Walkthrough

The firmware codebase is structured into modular software blocks:
1. **Network and Display Driver Initialization**: Configures the SSD1306 OLED display on I2C address `0x3C` and initializes analog joystick input pins.
2. **EEPROM Serialization Module**: Implements binary serialization routines (`loadWiFiConfig()`, `saveWiFiConfig()`, `saveProfilesToEEPROM()`, `loadProfilesFromEEPROM()`).
3. **On-Device UI Form Engine**: Manages non-blocking joystick navigation, letter scrolling for name entry, and numeric adjusters for physical metrics.
4. **Groq Cloud API Client**: Constructs dynamic JSON request bodies, executes HTTP POST requests over TLS 1.2/1.3, and parses JSON responses using `ArduinoJson`.
5. **Direct SSL SMTP Engine**: Executes base64 authentication and MIME HTML message transmission over `WiFiClientSecure` Port 465.
6. **Embedded WebServer Module**: Serves PROGMEM HTML dashboards for remote Wi-Fi configuration, patient profile updates, and clinical report viewing.
7. **State Machine Execution Loop**: Non-blocking `loop()` servicing background web clients while processing UI state transitions.

---

## Software Dependencies and Setup

- **Board Manager**: ESP32 Arduino Core (`esp32` by Espressif Systems v2.0+)
- **Required Libraries**: `Adafruit_SSD1306`, `Adafruit_GFX_Library`, `ArduinoJson`, `WiFi`, `WiFiClientSecure`, `HTTPClient`, `WebServer`, `EEPROM`.
- **Flashing Instructions**:
  1. Open `tests/beta/release/4.8.2.ino` or `src/main.ino` in Arduino IDE or VS Code.
  2. Select Board **ESP32 Dev Module** and Partition Scheme **Default 4MB with spiffs**.
  3. Compile and flash the binary to the ESP32 board.

---

## Operating Guide

### 1. Physical Device Workflow
1. **Power On**: Select an existing patient profile or create a new profile.
2. **Select Symptoms and Duration**: Choose active symptoms from 35 predefined options and specify duration.
3. **Answer AI Triage Questions**: Respond to the follow-up questions on screen using the joystick (**YES** / **NO**).
4. **View Summary and Report**: Review OLED metrics and access the complete clinical report via the local IP address.

### 2. Web Portal Workflow
1. Connect a client device (laptop or smartphone) to the same local Wi-Fi network.
2. Navigate to `http://<ESP32_IP_ADDRESS>` to access device controls, edit patient records, or view HTML reports.

---

## Medical Disclaimer

PocketDoctor Mark 4 is an experimental AI-powered clinical diagnostic prototype. Diagnostic outputs are generated by Large Language Model inference (`llama-3.3-70b-versatile`) and self-reported patient data. It does not replace professional medical advice, clinical diagnosis, or emergency medical treatment.

---

## License and Credits

Licensed under the **MIT License**. See the [LICENSE](file:///c:/Users/Vaidik.Laptop/Desktop/pocdocmk4/LICENSE) file for complete details.

Copyright (c) 2026 **Vaidik Khurana**  
Project Documentation Gateway: [https://pocdoc.vaidik.co](https://pocdoc.vaidik.co)
