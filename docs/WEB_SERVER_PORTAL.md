# PocketDoctor Mark 4 — Web Server & Remote Management Portal

This document describes the embedded HTTP web server (Port 80), routing table, PROGMEM HTML template engine, remote Wi-Fi configuration management, profile web CRUD operations, and live diagnostic report viewing in **PocketDoctor Mark 4**.

---

## 1. Web Server Architecture

PocketDoctor runs an embedded `WebServer` instance on Port 80 (`WebServer server(80)`).
- **Storage**: HTML pages are stored in flash memory using `PROGMEM` blocks (`DASHBOARD_HTML`, `WIFI_HTML`, `PROFILES_HTML`, `PROFILE_EDIT_HTML`).
- **Template Processing**: The `processTemplate()` function dynamically replaces `%IP%`, `%UPTIME%`, `%SSID%`, `%PROFILES%`, and form selections before dispatching HTTP responses.

---

## 2. HTTP Route Map

| URI Path | HTTP Method | Handler Function | Description |
| :--- | :--- | :--- | :--- |
| `/` | `GET` | `handleDashboard()` | Primary dashboard showing system status, local IP, and uptime. |
| `/portal` | `GET` | `handleDashboard()` | Dashboard route alias. |
| `/wifi` | `GET` | `handleWifi()` | Wi-Fi network configuration page. |
| `/wifi/update` | `POST` | `handleWifiUpdate()` | Receives new SSID/Password, saves to EEPROM, and reboots ESP32. |
| `/profiles` | `GET` | `handleProfiles()` | Patient profile registry list with edit/delete actions. |
| `/profile/edit` | `GET` | `handleProfileEdit()` | Form for editing an existing profile or initializing a new profile. |
| `/profile/update`| `POST` | `handleProfileUpdate()`| Validates submitted profile fields, updates memory, and writes to EEPROM. |
| `/profile/delete`| `GET` | `handleProfileDelete()`| Removes profile at specified `index`, shifts array, and updates EEPROM. |
| `/report` | `GET` | `handleReport()` | Renders the full responsive HTML medical report for the latest diagnostic session. |

---

## 3. Web Features & Workflows

### 3.1 Wi-Fi Credential Management (`/wifi` & `/wifi/update`)
1. User navigates to `/wifi`. Form presents pre-filled SSID value.
2. On POST submit (`/wifi/update`), trailing whitespace is trimmed.
3. If SSID is valid, `saveWiFiConfig(ssid, pass)` commits changes to EEPROM (starting at byte `0` with magic key `0xABCD`).
4. Server returns a reboot notification page and executes `ESP.restart()` after 500 ms.

### 3.2 Patient Profile Web CRUD (`/profiles`)
- **Read (`/profiles`)**: Iterates `savedProfiles[3]` array and constructs HTML card list.
- **Create / Edit (`/profile/edit`)**: Renders HTML form with options pre-selected according to profile state (`intoxStatus`, `hasMedicalHistory`, `treatmentPreference`).
- **Update (`/profile/update`)**: Validates input constraints:
  - Name cannot be empty.
  - Age must be between `1` and `120`.
  - Enforces maximum limit of 3 profiles.
  - Calls `saveProfilesToEEPROM()` to persist changes.
- **Delete (`/profile/delete?index=N`)**: Shifts array left by 1 element, decrements `profileCount`, and updates EEPROM.

### 3.3 Medical Report Viewer (`/report`)
- If a diagnostic session has completed (`reportReady == true`), `/report` serves `htmlReport`.
- If no diagnosis has been performed yet since boot, returns an informative prompt page directing the user to start a session on the physical device.
