# PocketDoctor Mark 4 — EEPROM Memory Specification

This document details the binary memory architecture, data structures, serialization logic, and layout map used by **PocketDoctor Mark 4** to persist system configurations and multi-patient profiles inside the ESP32 EEPROM (512 Bytes).

---

## 1. EEPROM Memory Allocation Map

PocketDoctor reserves **512 Bytes** of EEPROM memory space (`#define EEPROM_SIZE 512`).

```
Byte Address Range     Size (Bytes)      Data Segment Description
─────────────────────────────────────────────────────────────────────────────
0x000 - 0x001           2 Bytes           Magic Key Identifier (0xABCD)
0x002 - 0x022           33 Bytes          Stored Wi-Fi SSID String (null-terminated)
0x023 - 0x043           33 Bytes          Stored Wi-Fi Password String (null-terminated)
0x044 - 0x063           32 Bytes          [Reserved / Unused System Config]
0x064 - 0x1FF          412 Bytes          Profile Registry Storage Segment
  └─ 0x064 (Addr 100)    1 Byte           Profile Count (uint8_t: 0 to 3)
  └─ 0x065+              Variable         Serialized Profile Records 0, 1, 2
```

---

## 2. Data Structures & Layout

### 2.1 Wi-Fi Configuration Structure (`Config`)
Stored starting at Byte `0` (`#define CONFIG_START 0`).

```cpp
struct Config {
  uint16_t magic;      // Magic verification bytes: 0xABCD
  char ssid[33];       // Null-terminated Wi-Fi Network Name (max 32 chars)
  char password[33];   // Null-terminated Wi-Fi Password (max 32 chars)
};
```

- **Validation Check**: On boot, `loadWiFiConfig()` checks if `cfg.magic == 0xABCD`.
- If valid, network attempts connection using stored credentials.
- If invalid or uninitialized, falls back to default firmware credentials (`NKJIO2.4`).

---

### 2.2 Patient Profile Structure (`Profile`)

```cpp
struct Profile {
  String name;                 // Patient Name
  int age;                     // Patient Age in Years (1–120)
  int height;                  // Height in centimeters (50–250 cm)
  int weight;                  // Weight in kilograms (20–300 kg)
  String intoxStatus;          // "None", "Alcohol", "Tobacco", "Both"
  bool hasMedicalHistory;      // Medical History Flag (true/false)
  String treatmentPreference;  // "Allopathy", "Homeopathy", "Ayurvedic", "All types"
};
```

---

## 3. Byte Serialization & Deserialization Protocol

Profiles are stored starting at Byte `100` (`#define PROFILES_START 100`). Up to **3 profiles** can be registered.

### 3.1 Serialization (`saveProfilesToEEPROM()`)

```
Byte Offset      Data Element               Encoding Format
─────────────────────────────────────────────────────────────────────────────
Address 100      Profile Count              uint8_t (0, 1, 2, or 3)

[ For each saved profile index i ]:
+0               Name String Length (L1)    uint8_t
+1 to +L1        Name Characters            ASCII bytes (L1 bytes)
+(1 + L1)        Age                        uint8_t byte
+(2 + L1)        Height                     uint8_t byte
+(3 + L1)        Weight                     uint8_t byte
+(4 + L1)        Intox String Length (L2)   uint8_t
+(5+L1)..(L1+L2) Intox Characters           ASCII bytes (L2 bytes)
+Next Byte       Medical History Flag       uint8_t (1 = Yes, 0 = No)
+Next Byte       Treatment Length (L3)      uint8_t
+Next Bytes      Treatment Characters       ASCII bytes (L3 bytes)
```

- Reads/writes via `EEPROM.read()` and `EEPROM.write()`.
- After writing bytes, `EEPROM.commit()` commits changes to non-volatile flash.

### 3.2 Deserialization (`loadProfilesFromEEPROM()`)
- Address 100 is evaluated. If `profileCount > 3` or `profileCount < 0`, memory is flagged uninitialized (`profileCount = 0`).
- Iterates through `profileCount`, reading string length markers, extracting ASCII sequences into dynamic strings, and restoring age, height, weight, medical history, and treatment choices.
