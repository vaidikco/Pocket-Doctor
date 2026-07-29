# PocketDoctor Mark 4 — Official Release Notes

**System Version:** v4.8.2 and v4.8.2.3  
**Target Hardware:** ESP32 (OLED SSD1306 128x64, Analog Joystick VRx/VRy/SW)  
**Date:** July 29, 2026  
**Repository Location:** `c:\Users\Vaidik.Laptop\Desktop\pocdocmk4\`

---

## Source Code Files and Directory Structure

| Release Version | File Name and Path | Description |
| :--- | :--- | :--- |
| **v4.8.2 (Main)** | [`tests/beta/release/4.8.2.ino`](file:///c:/Users/Vaidik.Laptop/Desktop/pocdocmk4/tests/beta/release/4.8.2.ino) | **Official Main Release.** Features Groq LLaMA 3.3 70B clinical Q&A, Direct SSL SMTP Emergency SOS dispatch, strict Wi-Fi fallback validation, and RFC-compliant email delivery. |
| **v4.8.2.3 (SoftAP)** | [`tests/beta/release/4.8.2.3.ino`](file:///c:/Users/Vaidik.Laptop/Desktop/pocdocmk4/tests/beta/release/4.8.2.3.ino) | **SoftAP Variant.** Includes automatic Access Point fallback (`PocketDoctor-AP` at `192.168.4.1`) for zero-configuration Wi-Fi setup when unconfigured. |
| **v4.8.1** | [`tests/beta/release/4.8.1.ino`](file:///c:/Users/Vaidik.Laptop/Desktop/pocdocmk4/tests/beta/release/4.8.1.ino) | Previous release with basic SOS trigger and EEPROM structure `0xABCF`. |
| **v4.8.0** | [`tests/beta/release/4.8.ino`](file:///c:/Users/Vaidik.Laptop/Desktop/pocdocmk4/tests/beta/release/4.8.ino) | Early v4.8 release with 10-question Groq diagnostic triage. |
| **v4.7.0** | [`tests/beta/release/4.7.ino`](file:///c:/Users/Vaidik.Laptop/Desktop/pocdocmk4/tests/beta/release/4.7.ino) | Base legacy release (EEPROM magic `0xABCD`). |
| **Core Source** | [`src/main.ino`](file:///c:/Users/Vaidik.Laptop/Desktop/pocdocmk4/src/main.ino) | Active production source file. |

---

## Key Improvements and Bug Fixes in v4.8.2

### 1. Wi-Fi Connectivity and EEPROM Fallback Fix
* **Problem Resolved:** Previously, if EEPROM contained magic header `0xABCF` with blank SSID strings (or after flashing from older versions), `connectWiFi()` locked onto the empty EEPROM SSID and ignored hardcoded fallback credentials (`default_ssid`), resulting in `Loc Sync: OFFLINE`.
* **Implementation:** Updated `connectWiFi()` to evaluate `if (wifiConfigValid && strlen(wifiConfig.ssid) > 0)` and added explicit `WiFi.mode(WIFI_STA)` and `WiFi.disconnect(true)` radio initialization.

### 2. Direct SSL SMTP Emergency Email Delivery Fix
* **RFC 5321 Line-Length Compliance:** Formatted HTML template line output in `createSOSEmailHTML()` by inserting explicit CRLF (`\r\n`) breaks. Eliminates single-line payloads exceeding 1,000 octets that caused Gmail to silently reject messages.
* **RFC 5322 MIME Header Compliance:** Added compliant headers (`From:`, `To:`, `Reply-To:`, `Message-ID:`). Removed static `Date:` headers so Gmail dynamically attaches official server GMT timestamps.
* **TCP Buffer Sanitization:** Refactored `readSmtpResponse()` to parse responses line-by-line using `readStringUntil('\n')`, eliminating race conditions from multiline server responses.
* **Strict Command Error Validation:** Enforced explicit validation checks on `MAIL FROM`, `RCPT TO`, and `DATA` SMTP command steps.

### 3. Privacy and Credential Sanitation
* All hardcoded credentials (`default_ssid`, `default_password`, `default_sos_email`, `default_smtp_user`, `default_smtp_pass`, `groq_api_key`) in [4.8.2.ino](file:///c:/Users/Vaidik.Laptop/Desktop/pocdocmk4/tests/beta/release/4.8.2.ino) have been scrubbed to blank strings (`""`).
* Removed temporary development files (`4.8.2DELL.ino`).

---

## Special Note on Gmail SOS Alert Delivery

When sending SOS alerts using Gmail SMTP (`smtp.gmail.com:465`):
* **Self-Sent Email Behavior:** If the Sender (`default_smtp_user`) and Recipient (`default_sos_email`) are set to the **same Gmail address**, Gmail automatically archives the message into **"Sent"** / **"All Mail"** and skips the primary Inbox.
* **Recommendation:** To receive SOS alerts directly in an Inbox, set the recipient email address to a **different address** than the sender.

---

## Configuration and Web Portal Access

1. **Station Mode (Default):** Connects to configured Wi-Fi router. Access control dashboard via `http://<ESP32_IP>/`.
2. **Web Portal Routes:**
   - `/` — System Control Portal and Status Telemetry
   - `/wifi` & `/wifi/update` — Wi-Fi Network Credentials Configuration
   - `/sos` & `/sos/update` — Emergency Recipient and SMTP Server Settings
   - `/profiles` — Patient Vitals and Medical History Management
   - `/report` — HTML Clinical Triage Report
