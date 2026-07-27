# PocketDoctor Mark 4 — Hardware & Pinout Specification

This document details the hardware schematics, pin mapping, analog threshold logic, display configuration, and electrical considerations for building **PocketDoctor Mark 4**.

---

## 1. Complete Pin Mapping Table

| Peripherals | Module Pin | ESP32 Pin | Signal Type | Electrical Specification | Description |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **OLED Display** | VCC | 3V3 / 5V | Power | 3.3V - 5V DC Supply | Main Display Power |
| **OLED Display** | GND | GND | Power | Ground Reference | Common Ground |
| **OLED Display** | SDA | **GPIO 21** | Digital (I2C) | 3.3V Logic | Serial Data Line |
| **OLED Display** | SCL | **GPIO 22** | Digital (I2C) | 3.3V Logic | Serial Clock Line |
| **Joystick** | VCC | 3V3 | Power | 3.3V DC Supply | Potentiometer Power |
| **Joystick** | GND | GND | Power | Ground Reference | Common Ground |
| **Joystick** | VRx | **GPIO 34** | Analog (ADC) | ADC1_CH6 (0 - 3.3V) | X-Axis Motion Control |
| **Joystick** | VRy | **GPIO 35** | Analog (ADC) | ADC1_CH7 (0 - 3.3V) | Y-Axis Value Adjust |
| **Joystick** | SW | **GPIO 32** | Digital Input | `INPUT_PULLUP` (Active LOW) | Integrated Pushbutton Switch |

---

## 2. Component Specifications

### 2.1 SSD1306 OLED Display
- **Screen Resolution**: 128 x 64 monochrome pixels.
- **Protocol**: I2C (Inter-Integrated Circuit).
- **I2C Bus Address**: `0x3C` (initialized via `Adafruit_SSD1306 display(128, 64, &Wire, -1);`).
- **Operating Voltage**: 3.3V DC.
- **Refresh Rate**: ~30 FPS over I2C standard fast mode (400 kHz).

### 2.2 Dual-Axis Analog Joystick (KY-023 / PS2 Joystick Module)
- **Potentiometer Resistance**: 10 kΩ per axis.
- **X-Axis Pin (`VRx`)**: Connected to ESP32 ADC pin **GPIO 34**.
- **Y-Axis Pin (`VRy`)**: Connected to ESP32 ADC pin **GPIO 35**.
- **Switch Pin (`SW`)**: Connected to ESP32 pin **GPIO 32** configured with `pinMode(SW, INPUT_PULLUP)`.

---

## 3. Joystick Threshold & Debounce Logic

ESP32 12-bit ADC converts analog signals from `0` to `4095` (center position ≈ `2048`).

```cpp
#define VRx 34
#define VRy 35
#define SW 32

int joyThreshold = 1000;
unsigned long lastMove = 0;
int debounceDelay = 200;
```

### Movement Condition Logic:
- **Up / Left Trigger**: `yValue < (2048 - joyThreshold)` (ADC Value < ~1048).
- **Down / Right Trigger**: `yValue > (2048 + joyThreshold)` (ADC Value > ~3048).
- **Debounce Window**: Minimum `200 ms` elapsed between movement triggers (`now - lastMove > debounceDelay`).
- **Button Click Action**: Evaluates `digitalRead(SW) == LOW` with debounce check.
- **Button Hold Action** (Used for Name Input Completion): Detects continuous press state (`swState == LOW`) exceeding `2000 ms`.

---

## 4. Wiring Diagram (Schematic View)

```
        ESP32 Dev Module
     ┌──────────────────────┐
     │                      │
     │               GPIO 21├───────[ SDA ] ───┐
     │               GPIO 22├───────[ SCL ] ───┼──> SSD1306 OLED (128x64)
     │                  3V3 ├───────[ VCC ] ───┼──> (Address: 0x3C)
     │                  GND ├───────[ GND ] ───┘
     │                      │
     │               GPIO 34├───────[ VRx ] ───┐
     │               GPIO 35├───────[ VRy ] ───┼──> 2-Axis Joystick Module
     │               GPIO 32├───────[ SW  ] ───┤    (KY-023)
     │                  3V3 ├───────[ VCC ] ───┤
     │                  GND ├───────[ GND ] ───┘
     │                      │
     └──────────────────────┘
```
