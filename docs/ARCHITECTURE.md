# PocketDoctor Mark 4 — System Architecture

This document provides a detailed breakdown of the internal software architecture, state machine execution logic, memory allocation model, and data structures implemented in **PocketDoctor Mark 4** (`src/main.ino`).

---

## 1. System Execution Lifecycle

The ESP32 application operates on a single-core main loop architecture that combines synchronous UI rendering, non-blocking input handling, asynchronous web client processing, and HTTP REST API communication with Groq Cloud.

```
                  ┌──────────────────────────────┐
                  │         Power On / Reset     │
                  └──────────────┬───────────────┘
                                 │
                                 ▼
                  ┌──────────────────────────────┐
                  │    Hardware & Peripherals    │
                  │  - Serial @ 115200 Baud      │
                  │  - OLED Display (0x3C I2C)   │
                  │  - Joystick Pin Modes        │
                  └──────────────┬───────────────┘
                                 │
                                 ▼
                  ┌──────────────────────────────┐
                  │   EEPROM Configuration Load   │
                  │  - Read Magic (0xABCD)       │
                  │  - Read Wi-Fi SSID / Pass    │
                  │  - Read 3 Patient Profiles   │
                  └──────────────┬───────────────┘
                                 │
                                 ▼
                  ┌──────────────────────────────┐
                  │      Wi-Fi Connection        │
                  │  - Attempt connection        │
                  │  - Fallback to Defaults      │
                  └──────────────┬───────────────┘
                                 │
                                 ▼
                  ┌──────────────────────────────┐
                  │    Start Web Server (P80)    │
                  │  - Register Handlers         │
                  │  - Listen for Web Clients    │
                  └──────────────┬───────────────┘
                                 │
                                 ▼
                  ┌──────────────────────────────┐
                  │     UI State Machine Loop    │
                  │  - Handle Web Server Requests│
                  │  - Read Joystick & Button    │
                  │  - Update Screen & State     │
                  └──────────────────────────────┘
```

---

## 2. Core State Machine & Flow Logic

The UI state machine is managed via global boolean control flags:

| State Flag | Active Phase | Primary Handler Function | Description |
| :--- | :--- | :--- | :--- |
| `inProfileMenu` | Profile Selection | `handleProfileMenu()` | Allows choosing an existing EEPROM profile or initiating profile creation. |
| `inProfileCreation` | Dynamic Form Entry | `handleProfileCreation()` | Multi-step interactive profile builder (Name, Age, Height, Weight, Intox, History, Treatment). |
| *[Default Root]* | Main Diagnostic Screen | `loop()` root branch | Displays readiness menu; button press enters symptom selection. |
| `inSymptomSelection` | Symptom Checklist | `loop()` symptom branch | Interactive list of 35 symptoms with toggleable `[x]` checkmarks. |
| `inDurationSelection` | Duration Menu | `loop()` duration branch | Selection of symptom duration (1-3 days, 4-7 days, 1-2 weeks, >2 weeks). |
| `inGeminiQuestions` | Groq AI Interactive Q&A | `handleGeminiQuestions()` | Executes 10-turn dynamic follow-up conversation via Groq API. |

---

## 3. Detailed Phase Breakdown

### Phase 1: Profile Selection & Management
- On boot, saved profiles are read from EEPROM byte address `100`.
- The user can select an existing profile or click **Create New**.
- **On-Device Profile Builder**:
  - `profileFieldIndex = 0`: Character selection using alphabet lookup (`A-Z `) with joystick Y-axis scrolling and short press append. 2-second button hold commits the name string.
  - `profileFieldIndex = 1..3`: Numeric wheel selectors for Age (1–120), Height (50–250 cm), and Weight (20–300 kg).
  - `profileFieldIndex = 4..6`: Categorical pickers for Intoxication status, Medical History flag, and Preferred Treatment modality.
  - `profileFieldIndex = 7`: Commits structure to array and writes to EEPROM.

### Phase 2: Symptom & Duration Selection
- 35 standard symptoms listed in `const char* symptoms[]`.
- Multi-select interface updates `selectedSymptoms[35]` boolean array.
- Prevents progression until at least one symptom is selected.
- Duration selection captures symptom timeline into `selectedDurationIndex`.

### Phase 3: Groq LLM Diagnostic Engine
1. **Context Initialization**: `buildProfileContext()` serializes active patient metrics (Name, Age, Height, Weight, Intoxication, Medical History, Treatment Preference) into a structured prompt header.
2. **Initial API Request**: Constructs initial payload containing patient context + selected symptoms + duration, instructing Groq (`llama-3.3-70b-versatile`) to generate single concise Yes/No questions.
3. **Interactive 10-Question Loop**: User answers **YES** or **NO** using the physical joystick. Each response appends to `conversationHistory`.
4. **Final Diagnosis Extraction**: After 10 questions, PocketDoctor requests final summary output in exact tagged format:
   ```text
   Diagnosed Disease: [disease name]
   Accuracy: [percentage]%
   Medicines: [2-3 medicine names]
   Precautions: [one short sentence]
   Critical: [Yes/No]
   ```
5. **Short Display & Full HTML Generation**: Displays key metrics on OLED and synthesizes comprehensive CSS-styled HTML report stored in `htmlReport` for web portal access.

---

## 4. Memory Allocation & Optimization Strategy

- **EEPROM Storage Limit**: 512 bytes reserved for binary config + profile records.
- **PROGMEM HTML Storage**: Static HTML/CSS web templates (`DASHBOARD_HTML`, `WIFI_HTML`, `PROFILES_HTML`, `PROFILE_EDIT_HTML`) are declared with `PROGMEM` and fetched via `FPSTR()` macro to conserve ESP32 SRAM.
- **Dynamic Data Structures**:
  - `DynamicJsonDocument doc(4096)` allocated on heap during Groq response parsing.
  - String buffers are reset between diagnostic sessions to mitigate memory fragmentation.
