# PocketDoctor Mark 4 — Technical Reference Guide & UI Specification

This document serves as the authoritative reference guide for the **PocketDoctor Mark 4** web portal, clinical diagnostic report engine, design system tokens, C++ PROGMEM template substitution matrix, and automated test suite.

---

## 🎨 1. Modern Design System & Token Architecture

The UI system utilizes a glassmorphic aesthetic built around a curated medical crimson/burgundy palette with dynamic gradients, backdrop filters, custom typography, and micro-interactions.

### Color Tokens

| Variable | HEX / Value | Role / Usage |
| :--- | :--- | :--- |
| `--burg` | `#6B0F1A` | Primary Medical Burgundy Brand Color |
| `--burg-dark` | `#4A0A12` | Dark Heading & Contrast Typography Color |
| `--accent` | `#D4384B` | Vibrant Crimson Highlight / Hover Gradient |
| `--bg-start` | `#FDF6F7` | Soft Background Canvas Light Gradient Start |
| `--bg-end` | `#E8D8DA` | Soft Background Canvas Light Gradient End |
| `--card-bg` | `rgba(255, 255, 255, 0.88)` | Frosted Glass Card Background (`backdrop-filter: blur(16px)`) |
| `--card-border` | `rgba(232, 197, 201, 0.6)` | Subtle Translucent Border |
| `--shadow` | `0 14px 40px rgba(107, 15, 26, 0.10)` | Elevated Card Ambient Drop Shadow |
| `--text` | `#2D0A0E` | High-Contrast Body Text Color |
| `--text-sub` | `#7A4048` | Muted Secondary Subtitle Color |
| `--gold` | `#D97706` | AI Confidence Gauge & Warning Accent |
| `--green` | `#166534` | Standard Status & Hydration Target Accent |
| `--red` | `#991B1B` | Critical Triage Priority Alert Callout |
| `--radius` | `20px` | Container Rounded Corner Radius |

### Typography
- **Primary Font**: `Plus Jakarta Sans`, system-ui, sans-serif (imported from Google Fonts).
- **Weight Scale**: Regular `400`, Medium `500`, Semi-Bold `600`, Bold `700`, Extra-Bold `800`.

---

## 💻 2. Web Server Pages & Template Placeholder Matrix

The ESP32 web server renders 4 PROGMEM pages and 1 dynamically generated clinical report. All template substitution functions in `src/main.ino` rely on exact placeholder string matching.

### Page Template Overview

```
+-----------------------------------------------------------------------+
|                       POCKETDOCTOR MARK 4 WEB PORTAL                 |
+-----------------------------------------------------------------------+
|  [ / or /portal ]  --> DASHBOARD_HTML (%IP%, %UPTIME%)                |
|  [ /wifi ]         --> WIFI_HTML (%SSID%, %PASS%)                     |
|  [ /profiles ]     --> PROFILES_HTML (%PROFILES%)                     |
|  [ /profile/edit ] --> PROFILE_EDIT_HTML (%HEADER%, %INDEX%, %NAME%..)|
|  [ /report ]       --> createComprehensiveHTMLReport()                |
+-----------------------------------------------------------------------+
```

### Template Substitution Matrix

| PROGMEM Variable | Target Route | Required C++ Placeholders | Description |
| :--- | :--- | :--- | :--- |
| `DASHBOARD_HTML` | `GET /`, `GET /portal` | `%IP%`<br>`%UPTIME%` | Control portal homepage displaying live device status chips and navigation cards. |
| `WIFI_HTML` | `GET /wifi` | `%SSID%`<br>`%PASS%` | Wi-Fi network configuration form sending POST to `/wifi/update`. |
| `PROFILES_HTML` | `GET /profiles` | `%PROFILES%` | Profile registry displaying card list of EEPROM profiles with Edit/Delete controls. |
| `PROFILE_EDIT_HTML` | `GET /profile/edit` | `%HEADER%`<br>`%INDEX%`<br>`%NAME%`<br>`%AGE%`<br>`%HEIGHT%`<br>`%WEIGHT%`<br>`%SEL_NONE%`<br>`%SEL_ALCOHOL%`<br>`%SEL_TOBACCO%`<br>`%SEL_BOTH%`<br>`%SEL_HIST_YES%`<br>`%SEL_HIST_NO%`<br>`%SEL_ALLOPATHY%`<br>`%SEL_HOMEOPATHY%`<br>`%SEL_AYURVEDIC%`<br>`%SEL_ALLTYPES%` | Multi-field form for creating or editing patient biometric profiles in EEPROM. |

---

## 📄 3. Clinical Diagnostic Report Architecture

The report rendered at `GET /report` via `createComprehensiveHTMLReport()` incorporates the following sections:

1. **Gradient Hero Header**:
   - Patient Name (`currentProfile.name`), Age (`currentProfile.age`), Weight (`currentProfile.weight`), Height (`currentProfile.height`), Preferred Modality (`currentProfile.treatmentPreference`).
2. **Hero Metric Cards**:
   - Primary Diagnosis (`diagnosedDisease`)
   - AI Confidence Gauge (`accuracy`)
   - Priority Level Badge (`STANDARD` vs `URGENT`)
   - Modality Type (`currentProfile.treatmentPreference`)
3. **Clinical Sections**:
   - **Diagnostic Assessment**: Condition description and confidence progress bar.
   - **Medication Protocol**: Suggested medications, administration rules, side effect checklist.
   - **Essential Precautions**: Patient safety rules and precautions.
   - **Lifestyle & Dietary Guidance**: Hydration target (2.0–2.5L), exercise, sleep, diet recommendations.
   - **Warning Signs & Emergency Callouts**: Red emergency callout box for severe symptoms.
   - **Recovery Timeline**: Interactive multi-stage timeline (Days 1–3, Days 3–5, Week 1–2, Week 2-4).
   - **Prognosis**: Detailed recovery factors and expected timeline.
   - **Medical Disclaimer & Legal Notice**: Comprehensive safety notice.

---

## 🧪 4. Automated Test Suite Specification (`tests/`)

The repository includes an automated Python test suite located in `tests/`:

### Test Files

1. **`tests/test_ui_templates.py`**:
   - Verifies the presence of all PROGMEM HTML templates in `src/main.ino`.
   - Validates that `%IP%`, `%UPTIME%`, `%SSID%`, `%PASS%`, `%PROFILES%`, and all 16 profile edit placeholders exist in `src/main.ino`.
   - Verifies `createComprehensiveHTMLReport()` signature and structure.
   - Asserts modern CSS rule presence (`:root`, `grid-template-columns`, `@media`).

2. **`tests/test_profile_validation.py`**:
   - Tests boundary validation logic for profiles: Name non-empty, Age `1-120`, Height `50-250cm`, Weight `20-300kg`, Max profile capacity `3`.

3. **`tests/run_all_tests.py`**:
   - Master test runner executing all unit tests in `tests/`.

### Executing Tests
To execute the test suite locally:
```bash
python tests/run_all_tests.py
```
Expected output:
```text
Ran 10 tests in 0.007s

OK
```

---

## 🔒 5. Development & Contribution Rules

- **Do Not Change C++ Logic**: When tweaking HTML/CSS in `src/main.ino`, never change C++ function signatures, EEPROM structures, or template placeholder names.
- **Run Tests After Any UI Modification**: Always run `python tests/run_all_tests.py` after editing `src/main.ino` to ensure template placeholder compliance.
- **Keep Local Credentials Private**: Never commit raw API keys or passwords. Keep sensitive credentials in `credentials.txt` (which is gitignored).
