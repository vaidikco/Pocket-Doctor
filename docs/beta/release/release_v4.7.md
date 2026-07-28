# PocketDoctor Release Notes
## Version: Beta v4.7 (Mk5 Test Release)

### New Features

* **Mode Selection Gateway:**
  * Replaced the immediate transition into "Symptom Selection" with a brand new Mode Selection UI upon starting the device.
  * Users can now select between two operating modes: **New Diagnosis** and **Daily Checkup**.
  * Added a **Location/Sync Indicator** at the bottom of the Mode Selection screen (e.g., `Loc Sync: 192.168.x.x`), allowing users to easily verify the device's network location status before initiating a session.

* **Daily Checkup Workflow:**
  * Introduced a fast-tracked Q&A workflow specifically designed for daily health tracking and recovery monitoring.
  * Preserves the core symptom and duration selection workflow to capture "symptoms today", ensuring consistent interaction patterns.
  * Instructs the Groq LLaMA 3.3 model with a specialized context to evaluate recovery, safety, and daily progression.
  * Reduced the question limit from 10 to **5 strategic Yes/No questions**, streamlining the checkup process.

* **Specialized Daily Checkup HTML Report:**
  * Added an entirely new, distinct HTML web report tailored for the Daily Checkup workflow.
  * Replaces the urgent "Clinical Triage" burgundy theme with a calming **blue and teal aesthetic** to reflect ongoing health monitoring.
  * Displays new customized metrics derived from the AI analysis:
    * **Status**
    * **Recovery Score (0-100%)**
    * **Condition Safety (Safe / Needs Attention)**
    * **Today's Focus (Daily Advice & Tips)**
    * **Next Steps**
  * The web server dynamically serves either the standard Diagnostic Report or the Daily Checkup Report based on the most recently completed session type.

### Improvements & Fixes
* Preserved all existing EEPROM profile handling and Wi-Fi configuration flows without interference.
* Optimized the Groq prompt system to seamlessly toggle final diagnosis parsing logic based on the selected operating mode.
* Enhanced UI screen transitions with proper debouncing and flow controls.
