# PocketDoctor Mark 4 — Groq AI & Diagnostic Integration

This document describes the AI diagnostic workflow, Groq Cloud API integration, prompt architecture, JSON payload structures, string parsing rules, and HTML report synthesis implemented in **PocketDoctor Mark 4**.

---

## 1. Groq API Configuration

PocketDoctor interfaces with Groq's high-speed inference cloud via HTTPS.

- **API Endpoint**: `https://api.groq.com/openai/v1/chat/completions`
- **Model**: `llama-3.3-70b-versatile`
- **Authentication**: HTTP Header `Authorization: Bearer <GROQ_API_KEY>`
- **Content Type**: `application/json`
- **Temperature Setting**: `0.7`
- **HTTP Timeout**: Connect timeout = 15,000 ms; HTTP execution timeout = 20,000 ms.

---

## 2. Multi-Turn Diagnostic Workflow

The diagnostic engine follows a 10-question dynamic Q&A cycle:

```
 ┌─────────────────────────────────────────────────────────────┐
 │ 1. Build Patient Profile Context & Initial Symptom Payload  │
 └──────────────────────────────┬──────────────────────────────┘
                                │
                                ▼
 ┌─────────────────────────────────────────────────────────────┐
 │ 2. Query Groq API for Question #1                           │
 └──────────────────────────────┬──────────────────────────────┘
                                │
                                ▼
 ┌─────────────────────────────────────────────────────────────┐
 │ 3. Render Question on OLED; Capture Joystick YES/NO Input   │
 └──────────────────────────────┬──────────────────────────────┘
                                │
                                ▼
 ┌─────────────────────────────────────────────────────────────┐
 │ 4. Append Answer to Conversation History & Repeat Q2..Q10   │
 └──────────────────────────────┬──────────────────────────────┘
                                │ (After 10 Questions)
                                ▼
 ┌─────────────────────────────────────────────────────────────┐
 │ 5. Request Structured Final Diagnosis Summary               │
 └──────────────────────────────┬──────────────────────────────┘
                                │
                                ▼
 ┌─────────────────────────────────────────────────────────────┐
 │ 6. Parse Fields -> OLED Display -> Generate HTML Web Report │
 └─────────────────────────────────────────────────────────────┘
```

---

## 3. System Prompts & Payload Architecture

### 3.1 Initial Context Payload Construction
When starting a session (`startGroqDiagnosisWithData()`), PocketDoctor builds the prompt combining profile traits, symptoms, and duration:

```text
Patient Profile - Name: Vaidik, Age: 25 years, Height: 170cm, Weight: 70kg, Intoxication: None, Previous Medical Conditions: No, Preferred Treatment: Allopathy. Symptoms: Fever, Cough, Headache. Duration: 1-3 days. Ask 10 strategic yes/no questions (max 10 words each). Output one question only each time [I REPEAT ONE EACH TIME.], and cover all related aspects for efficiency. Cover different aspects: location, intensity, timing, triggers. No extra text, ONLY question.
```

### 3.2 Final Diagnosis Payload Instruction
After receiving the 10th answer, PocketDoctor injects the final extraction prompt:

```text
Based on all my answers and the initial symptoms I provided, provide a SHORT preliminary diagnosis in this EXACT format:

Diagnosed Disease: [disease name only]
Accuracy: [percentage]%
Medicines: [2-3 medicine names only, no dosages]
Precautions: [2-3 key precautions in one short sentence]
Critical: [Yes/No]

Keep it VERY brief. Follow this format exactly.
```

---

## 4. Response Parsing Algorithm (`parseDiagnosis()`)

The raw LLM string returned from Groq is parsed using index boundary extraction:

```cpp
void parseDiagnosis(String diagnosis) {
  int diseasePos = diagnosis.indexOf("Diagnosed Disease:");
  int accuracyPos = diagnosis.indexOf("Accuracy:");
  int medicinesPos = diagnosis.indexOf("Medicines:");
  int precautionsPos = diagnosis.indexOf("Precautions:");
  int criticalPos = diagnosis.indexOf("Critical:");

  if (diseasePos != -1 && accuracyPos != -1) {
    diagnosedDisease = diagnosis.substring(diseasePos + 18, accuracyPos);
    diagnosedDisease.trim();
  }
  // Extracts accuracy, medicines, precautions, and critical flags...
}
```

Extracted variables populate both the OLED short display card and the comprehensive web report.

---

## 5. HTML Medical Report Synthesis (`createComprehensiveHTMLReport()`)

Upon parsing the short diagnosis, PocketDoctor generates a full HTML document (~15 KB) incorporating:
1. **Patient Demographic Card**: Name, Age, Weight, Height, Treatment Preference.
2. **AI Confidence Gauge**: Dynamic CSS progress bar linked to AI accuracy percentage.
3. **Emergency Critical Banner**: Displayed prominently if `Critical: Yes`.
4. **Medication & Treatment Protocol**: Custom-tailored to the user's preferred treatment modality (Allopathy, Homeopathy, Ayurvedic, All types).
5. **Lifestyle & Dietary Guidance**: Hydration target (2.0 - 2.5L), physical activity, sleep, foods to prioritize/avoid.
6. **Timeline & Follow-Up Schedule**: Multi-phase recovery timeline (Days 1-3, Days 3-5, Week 1-2, Week 2-4).
7. **Legal & Clinical Disclaimer**: Clearly demarcates that the output is an AI preliminary assessment and not a replacement for clinical diagnosis.
