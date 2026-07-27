# 🚀 PocketDoctor Mark 4 — Release Notes & UI Update Changelog (v4.0.0-beta)

> **Release Version**: `v4.0.0-beta`  
> **Author & Lead Engineer**: Vaidik Khurana  
> **Target Framework**: ESP32 / Arduino C++ / Modern Web  
> **Date**: July 27, 2026

---

## 📌 Executive Summary

Release **v4.0.0-beta** delivers a comprehensive UI overhaul across the PocketDoctor Mark 4 web portal and clinical diagnostic report engine, alongside full Vaidik Khurana branding, automated test suite suite integration, and git history secret scrubbing.

---

## 🎨 Key Features & UI Improvements

### 1. Control Portal UI Overhaul (`DASHBOARD_HTML`, `WIFI_HTML`, `PROFILES_HTML`, `PROFILE_EDIT_HTML`)
- **Glassmorphism Aesthetic**: Implemented a frosted glass card design (`backdrop-filter: blur(16px)`), curated medical crimson/burgundy palette (`#5A0C16`, `#7A1422`, `#E63946`), and ambient drop shadows.
- **Typography & Branding**: Integrated Google Fonts `Plus Jakarta Sans` and `Outfit` with official branding: **"Designed & Engineered by Vaidik Khurana"**.
- **Interactive Navigation & Controls**: Added active status pulse indicator (`● Online`), card hover animations, and floating form input focus glowing outlines.

### 2. Prescription & Clinical Triage Report (`createComprehensiveHTMLReport()`)
- **Official Rx Medical Header**: Added an `Rx` watermark emblem, system model stamp (`MK4-LLAMA-3.3`), and patient biometrics grid (`Name`, `Age`, `Weight`, `Height`, `Modality`).
- **Clinical Diagnosis & AI Confidence**: Added a confidence meter bar with exact accuracy percentages, and dynamic priority status callouts (`URGENT TRIAGE` vs `STANDARD CARE`).
- **Rx Prescribed Medication Protocol**: Structured table defining dosing schedules, dietary timing, missed dose rules, and side effect surveillance rules.
- **Dietary, Hydration & Activity Plan**: Integrated physical activity limits, sleep hygiene rules, and daily hydration targets (2.0–2.5 Litres).
- **Official AI Attestation**: Includes formal AI triage attestation signed by **Vaidik Khurana**.

---

## 🧪 Automated Test Suite & Toolkit (`tests/`)

- **Template Placeholder Auditing**: Added `tests/test_ui_templates.py` to verify that all C++ template placeholders (`%IP%`, `%UPTIME%`, `%SSID%`, `%PASS%`, `%PROFILES%`, `%HEADER%`, `%INDEX%`, `%NAME%`, `%AGE%`, `%HEIGHT%`, `%WEIGHT%`, `%SEL_*%`) are strictly preserved.
- **EEPROM Boundary Validation**: Added `tests/test_profile_validation.py` verifying name non-emptiness, age bounds (1–120), height bounds (50–250 cm), weight bounds (20–300 kg), and 3-profile max capacity limits.
- **Changelog Toolkit (`tests/toolkit/`)**: Added `changelog_manager.py` Python CLI module to generate, search, and render Markdown test changelogs.

---

## 🔒 Security & Repository Hygiene

- **Secret Sanitization**: Removed hardcoded Groq API keys from `src/main.ino` and git commit history.
- **Local Credentials File**: Created `.gitignore` protected `credentials.txt` to store local environment secrets safely.
