/*
 * PocketDoctor v3 – AI-Powered Diagnostic Assistant
 * Uses Groq API (LLaMA 3.3 70B) for intelligent Q&A and preliminary diagnosis.
 * 
 * Features:
 * - Wi-Fi connection (configurable via web dashboard)
 * - EEPROM profile storage (up to 3 profiles)
 * - Joystick-driven UI on 128x64 OLED
 * - Symptom selection (35 symptoms)
 * - Duration selection
 * - 10‑question interactive session with Groq
 * - HTML report generation with treatment advice
 * - Built‑in web server with configuration dashboard
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <EEPROM.h>
#define EEPROM_SIZE 512
#include <Adafruit_SSD1306.h>
#include <WebServer.h>

// -------------------- DISPLAY --------------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// -------------------- NETWORK (fallback) --------------------
const char* default_ssid = "";
const char* default_password = "";

// -------------------- GROQ API --------------------
const char* groq_api_key = "";
const char* groq_url = "https://api.groq.com/openai/v1/chat/completions";
const char* groq_model = "llama-3.3-70b-versatile";

// -------------------- WEB SERVER --------------------
WebServer server(80);
String htmlReport = "";
bool reportReady = false;

// -------------------- JOYSTICK --------------------
#define VRx 34
#define VRy 35
#define SW 32
int joyThreshold = 1000;
unsigned long lastMove = 0;
int debounceDelay = 200;

// -------------------- EEPROM CONFIGURATION --------------------
struct Config {
  uint16_t magic;      // 0xABCD
  char ssid[33];
  char password[33];
};

Config wifiConfig;
bool wifiConfigValid = false;

// EEPROM layout
#define CONFIG_START  0
#define CONFIG_MAGIC  0xABCD
#define PROFILES_START 100

// -------------------- PROFILE SYSTEM --------------------
struct Profile {
  String name;
  int age;
  int height;  // cm
  int weight;  // kg
  String intoxStatus;          // "None", "Alcohol", "Tobacco", "Both"
  bool hasMedicalHistory;
  String treatmentPreference;  // "Allopathy", "Homeopathy", "Ayurvedic", "All types"
};

Profile currentProfile;
Profile savedProfiles[3];
int profileCount = 0;

bool inProfileMenu = false;
bool inProfileCreation = false;
int profileMenuIndex = 0;
int profileFieldIndex = 0;

// Text input helpers
const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ ";
int letterIndex = 0;
String inputBuffer = "";
int numericValue = 0;
unsigned long buttonHoldStart = 0;
bool buttonHeld = false;

// Intoxication options
const char* intoxOptions[] = {"None", "Alcohol", "Tobacco", "Both"};
int intoxCount = 4;
int intoxIndex = 0;

// Treatment options
const char* treatmentOptions[] = {"Allopathy", "Homeopathy", "Ayurvedic", "All types"};
int treatmentCount = 4;
int treatmentIndex = 0;

// Main menu
const char* menuItems[] = {"Start Diagnosis"};
int menuLength = 1;
int currentIndex = 0;
bool inSymptomSelection = false;
bool inDurationSelection = false;
bool inGeminiQuestions = false;

// Symptom lists
const char* symptoms[] = {
  "Fever", "Cough", "Fatigue", "Headache", "Nausea",
  "Dizziness", "Shortness of breath", "Chest pain", "Sore throat",
  "Diarrhea", "Muscle pain", "Loss of taste", "Loss of smell",
  "Rapid heartbeat", "Sweating", "Vomiting", "Runny nose",
  "Pain", "Constipation", "Gastral Issue", "Joint pain", "Chills",
  "Night sweats", "Weight loss", "Confusion", "Blurred vision",
  "Rash", "Itching", "Swelling", "Numbness", "Tingling",
  "Back pain", "Abdominal pain", "Ear pain", "Bloating"
};
int symptomCount = 35;
bool selectedSymptoms[35];
int symptomIndex = 0;

const char* durations[] = {"1-3 days","4-7 days","1-2 weeks","More than 2 weeks"};
int durationIndex = 0;
int selectedDurationIndex = 0;

// Conversation state for Groq
String conversationHistory = "";
int questionCount = 0;
const int maxQuestions = 10;
String currentQuestion = "";
int yesNoIndex = 0;

// Diagnosis results
String diagnosedDisease = "";
String accuracy = "";
String medicines = "";
String precautions = "";
String critical = "";

// Loading spinner
int spinnerFrame = 0;
const char spinnerChars[] = {'|', '/', '-', '\\'};

// -------------------- UTILITY FUNCTIONS --------------------
void drawSpinner(int x, int y) {
  display.setCursor(x, y);
  display.print(spinnerChars[spinnerFrame % 4]);
  spinnerFrame++;
}

void showLoadingScreen(const char* message, const char* submessage = "") {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 10);
  display.println(message);
  if (strlen(submessage) > 0) {
    display.setCursor(0, 30);
    display.println(submessage);
  }
  drawSpinner(110, 50);
  display.display();
}

// -------------------- EEPROM CONFIGURATION (Wi-Fi) --------------------
void loadWiFiConfig() {
  EEPROM.begin(EEPROM_SIZE);
  Config cfg;
  EEPROM.get(CONFIG_START, cfg);
  if (cfg.magic == CONFIG_MAGIC) {
    wifiConfig = cfg;
    wifiConfigValid = true;
    Serial.println("WiFi config loaded from EEPROM");
  } else {
    wifiConfigValid = false;
    Serial.println("No valid WiFi config in EEPROM, using defaults");
  }
  EEPROM.end();
}

void saveWiFiConfig(const char* ssid, const char* pass) {
  EEPROM.begin(EEPROM_SIZE);
  Config cfg;
  cfg.magic = CONFIG_MAGIC;
  strncpy(cfg.ssid, ssid, 32);
  cfg.ssid[32] = '\0';
  strncpy(cfg.password, pass, 32);
  cfg.password[32] = '\0';
  EEPROM.put(CONFIG_START, cfg);
  EEPROM.commit();
  EEPROM.end();
  wifiConfig = cfg;
  wifiConfigValid = true;
  Serial.println("WiFi config saved to EEPROM");
}

// -------------------- EEPROM PROFILES --------------------
void saveProfilesToEEPROM() {
  EEPROM.begin(EEPROM_SIZE);
  int addr = PROFILES_START;
  EEPROM.write(addr++, profileCount);
  for (int i = 0; i < profileCount; i++) {
    int nameLen = savedProfiles[i].name.length();
    EEPROM.write(addr++, nameLen);
    for (int j = 0; j < nameLen; j++) {
      EEPROM.write(addr++, savedProfiles[i].name[j]);
    }
    EEPROM.write(addr++, savedProfiles[i].age);
    EEPROM.write(addr++, savedProfiles[i].height);
    EEPROM.write(addr++, savedProfiles[i].weight);
    int intoxLen = savedProfiles[i].intoxStatus.length();
    EEPROM.write(addr++, intoxLen);
    for (int j = 0; j < intoxLen; j++) {
      EEPROM.write(addr++, savedProfiles[i].intoxStatus[j]);
    }
    EEPROM.write(addr++, savedProfiles[i].hasMedicalHistory ? 1 : 0);
    int treatLen = savedProfiles[i].treatmentPreference.length();
    EEPROM.write(addr++, treatLen);
    for (int j = 0; j < treatLen; j++) {
      EEPROM.write(addr++, savedProfiles[i].treatmentPreference[j]);
    }
  }
  EEPROM.commit();
  EEPROM.end();
  Serial.println("Profiles saved to EEPROM");
}

void loadProfilesFromEEPROM() {
  EEPROM.begin(EEPROM_SIZE);
  int addr = PROFILES_START;
  profileCount = EEPROM.read(addr++);
  if (profileCount > 3 || profileCount < 0) {
    profileCount = 0;
    EEPROM.end();
    return;
  }
  for (int i = 0; i < profileCount; i++) {
    int nameLen = EEPROM.read(addr++);
    savedProfiles[i].name = "";
    for (int j = 0; j < nameLen; j++) {
      savedProfiles[i].name += char(EEPROM.read(addr++));
    }
    savedProfiles[i].age = EEPROM.read(addr++);
    savedProfiles[i].height = EEPROM.read(addr++);
    savedProfiles[i].weight = EEPROM.read(addr++);
    int intoxLen = EEPROM.read(addr++);
    savedProfiles[i].intoxStatus = "";
    for (int j = 0; j < intoxLen; j++) {
      savedProfiles[i].intoxStatus += char(EEPROM.read(addr++));
    }
    savedProfiles[i].hasMedicalHistory = EEPROM.read(addr++) == 1;
    int treatLen = EEPROM.read(addr++);
    savedProfiles[i].treatmentPreference = "";
    for (int j = 0; j < treatLen; j++) {
      savedProfiles[i].treatmentPreference += char(EEPROM.read(addr++));
    }
  }
  EEPROM.end();
  Serial.println("Profiles loaded from EEPROM");
}

// -------------------- PROFILE MANAGEMENT (OLED) --------------------
void resetProfileCreation() {
  profileFieldIndex = 0;
  letterIndex = 0;
  inputBuffer = "";
  numericValue = 0;
  currentProfile.name = "";
  currentProfile.age = 0;
  currentProfile.height = 0;
  currentProfile.weight = 0;
  currentProfile.intoxStatus = "None";
  currentProfile.hasMedicalHistory = false;
  currentProfile.treatmentPreference = "Allopathy";
  buttonHoldStart = 0;
  buttonHeld = false;
}

void showProfileMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Profile Menu");
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

  int yPos = 15;
  for (int i = 0; i < profileCount; i++) {
    display.setCursor(5, yPos);
    if (profileMenuIndex == i) display.print("> ");
    else display.print("  ");
    display.print(savedProfiles[i].name);
    yPos += 10;
  }
  display.setCursor(5, yPos);
  if (profileMenuIndex == profileCount) display.print("> ");
  else display.print("  ");
  display.println("Create New");
  display.display();
}

void handleProfileMenu() {
  int xValue = analogRead(VRx);
  int yValue = analogRead(VRy);
  int swState = digitalRead(SW);
  unsigned long now = millis();

  int totalOptions = profileCount + 1;

  if (yValue < (2048 - joyThreshold) && (now - lastMove > debounceDelay)) {
    profileMenuIndex--;
    if (profileMenuIndex < 0) profileMenuIndex = 0;
    lastMove = now;
  }
  if (yValue > (2048 + joyThreshold) && (now - lastMove > debounceDelay)) {
    profileMenuIndex++;
    if (profileMenuIndex >= totalOptions) profileMenuIndex = totalOptions - 1;
    lastMove = now;
  }

  if (swState == LOW && (now - lastMove > debounceDelay)) {
    if (profileMenuIndex < profileCount) {
      currentProfile = savedProfiles[profileMenuIndex];
      inProfileMenu = false;
      Serial.println("Loading existing profile: " + currentProfile.name);
      display.clearDisplay();
      display.setCursor(0, 20);
      display.println("Profile loaded:");
      display.println(currentProfile.name);
      display.display();
      delay(1500);
    } else {
      Serial.println("Creating new profile...");
      resetProfileCreation();
      inProfileMenu = false;
      inProfileCreation = true;
    }
    lastMove = now;
  }
  showProfileMenu();
}

void handleProfileCreation() {
  int xValue = analogRead(VRx);
  int yValue = analogRead(VRy);
  int swState = digitalRead(SW);
  unsigned long now = millis();

  display.clearDisplay();
  display.setTextSize(1);

  // Field 0: Name
  if (profileFieldIndex == 0) {
    display.setCursor(0, 0);
    display.println("Enter Name:");
    display.drawLine(0, 10, 128, 10, SSD1306_WHITE);
    display.setCursor(0, 20);
    display.setTextSize(2);
    display.print("> ");
    display.println(alphabet[letterIndex]);
    display.setTextSize(1);
    display.setCursor(0, 40);
    display.print("Name: ");
    display.println(inputBuffer + "_");
    display.setCursor(0, 54);
    display.println("Hold 2s to finish");

    if (yValue < (2048 - joyThreshold) && (now - lastMove > debounceDelay)) {
      letterIndex--;
      if (letterIndex < 0) letterIndex = 0;
      lastMove = now;
    }
    if (yValue > (2048 + joyThreshold) && (now - lastMove > debounceDelay)) {
      letterIndex++;
      if (letterIndex >= 27) letterIndex = 26;
      lastMove = now;
    }

    if (swState == LOW) {
      if (!buttonHeld) {
        buttonHoldStart = now;
        buttonHeld = true;
      }
      if ((now - buttonHoldStart) > 2000 && inputBuffer.length() > 0) {
        currentProfile.name = inputBuffer;
        inputBuffer = "";
        profileFieldIndex = 1;
        numericValue = 25;
        buttonHeld = false;
        delay(300);
      }
    } else {
      if (buttonHeld && (now - buttonHoldStart) < 2000) {
        if (inputBuffer.length() < 10) {
          inputBuffer += alphabet[letterIndex];
        }
        delay(200);
      }
      buttonHeld = false;
    }
  }
  // Field 1: Age
  else if (profileFieldIndex == 1) {
    display.setCursor(0, 0);
    display.println("Enter Age:");
    display.drawLine(0, 10, 128, 10, SSD1306_WHITE);
    display.setCursor(20, 25);
    display.setTextSize(2);
    display.print(numericValue);
    display.println(" yrs");
    display.setTextSize(1);
    display.setCursor(0, 54);
    display.println("Press to confirm");

    if (yValue < (2048 - joyThreshold) && (now - lastMove > debounceDelay)) {
      numericValue--;
      if (numericValue < 1) numericValue = 1;
      lastMove = now;
    }
    if (yValue > (2048 + joyThreshold) && (now - lastMove > debounceDelay)) {
      numericValue++;
      if (numericValue > 120) numericValue = 120;
      lastMove = now;
    }
    if (swState == LOW && (now - lastMove > debounceDelay)) {
      currentProfile.age = numericValue;
      numericValue = 170;
      profileFieldIndex = 2;
      lastMove = now;
      delay(200);
    }
  }
  // Field 2: Height
  else if (profileFieldIndex == 2) {
    display.setCursor(0, 0);
    display.println("Enter Height:");
    display.drawLine(0, 10, 128, 10, SSD1306_WHITE);
    display.setCursor(20, 25);
    display.setTextSize(2);
    display.print(numericValue);
    display.println(" cm");
    display.setTextSize(1);
    display.setCursor(0, 54);
    display.println("Press to confirm");

    if (yValue < (2048 - joyThreshold) && (now - lastMove > debounceDelay)) {
      numericValue--;
      if (numericValue < 50) numericValue = 50;
      lastMove = now;
    }
    if (yValue > (2048 + joyThreshold) && (now - lastMove > debounceDelay)) {
      numericValue++;
      if (numericValue > 250) numericValue = 250;
      lastMove = now;
    }
    if (swState == LOW && (now - lastMove > debounceDelay)) {
      currentProfile.height = numericValue;
      numericValue = 70;
      profileFieldIndex = 3;
      lastMove = now;
      delay(200);
    }
  }
  // Field 3: Weight
  else if (profileFieldIndex == 3) {
    display.setCursor(0, 0);
    display.println("Enter Weight:");
    display.drawLine(0, 10, 128, 10, SSD1306_WHITE);
    display.setCursor(20, 25);
    display.setTextSize(2);
    display.print(numericValue);
    display.println(" kg");
    display.setTextSize(1);
    display.setCursor(0, 54);
    display.println("Press to confirm");

    if (yValue < (2048 - joyThreshold) && (now - lastMove > debounceDelay)) {
      numericValue--;
      if (numericValue < 20) numericValue = 20;
      lastMove = now;
    }
    if (yValue > (2048 + joyThreshold) && (now - lastMove > debounceDelay)) {
      numericValue++;
      if (numericValue > 300) numericValue = 300;
      lastMove = now;
    }
    if (swState == LOW && (now - lastMove > debounceDelay)) {
      currentProfile.weight = numericValue;
      intoxIndex = 0;
      profileFieldIndex = 4;
      lastMove = now;
      delay(200);
    }
  }
  // Field 4: Intoxication
  else if (profileFieldIndex == 4) {
    display.setCursor(0, 0);
    display.println("Intoxication:");
    display.drawLine(0, 10, 128, 10, SSD1306_WHITE);
    for (int i = 0; i < intoxCount; i++) {
      display.setCursor(10, 20 + i * 10);
      if (i == intoxIndex) display.print("> ");
      else display.print("  ");
      display.println(intoxOptions[i]);
    }
    if (yValue < (2048 - joyThreshold) && (now - lastMove > debounceDelay)) {
      intoxIndex--;
      if (intoxIndex < 0) intoxIndex = 0;
      lastMove = now;
    }
    if (yValue > (2048 + joyThreshold) && (now - lastMove > debounceDelay)) {
      intoxIndex++;
      if (intoxIndex >= intoxCount) intoxIndex = intoxCount - 1;
      lastMove = now;
    }
    if (swState == LOW && (now - lastMove > debounceDelay)) {
      currentProfile.intoxStatus = intoxOptions[intoxIndex];
      profileFieldIndex = 5;
      lastMove = now;
      delay(200);
    }
  }
  // Field 5: Medical History
  else if (profileFieldIndex == 5) {
    display.setCursor(0, 0);
    display.println("Medical History?");
    display.drawLine(0, 10, 128, 10, SSD1306_WHITE);
    display.setCursor(20, 25);
    display.setTextSize(2);
    if (currentProfile.hasMedicalHistory) {
      display.println("> YES");
      display.setTextSize(1);
      display.setCursor(20, 45);
      display.println("  NO");
    } else {
      display.println("  YES");
      display.setTextSize(1);
      display.setCursor(20, 45);
      display.println("> NO");
    }
    display.setTextSize(1);
    display.setCursor(0, 54);
    display.println("Press to confirm");

    if (yValue < (2048 - joyThreshold) && (now - lastMove > debounceDelay)) {
      currentProfile.hasMedicalHistory = true;
      lastMove = now;
    }
    if (yValue > (2048 + joyThreshold) && (now - lastMove > debounceDelay)) {
      currentProfile.hasMedicalHistory = false;
      lastMove = now;
    }
    if (swState == LOW && (now - lastMove > debounceDelay)) {
      treatmentIndex = 0;
      profileFieldIndex = 6;
      lastMove = now;
      delay(200);
    }
  }
  // Field 6: Treatment Preference
  else if (profileFieldIndex == 6) {
    display.setCursor(0, 0);
    display.println("Treatment Type:");
    display.drawLine(0, 10, 128, 10, SSD1306_WHITE);
    for (int i = 0; i < treatmentCount; i++) {
      display.setCursor(10, 20 + i * 10);
      if (i == treatmentIndex) display.print("> ");
      else display.print("  ");
      display.println(treatmentOptions[i]);
    }
    if (yValue < (2048 - joyThreshold) && (now - lastMove > debounceDelay)) {
      treatmentIndex--;
      if (treatmentIndex < 0) treatmentIndex = 0;
      lastMove = now;
    }
    if (yValue > (2048 + joyThreshold) && (now - lastMove > debounceDelay)) {
      treatmentIndex++;
      if (treatmentIndex >= treatmentCount) treatmentIndex = treatmentCount - 1;
      lastMove = now;
    }
    if (swState == LOW && (now - lastMove > debounceDelay)) {
      currentProfile.treatmentPreference = treatmentOptions[treatmentIndex];
      profileFieldIndex = 7;
      lastMove = now;
      delay(200);
    }
  }
  // Field 7: Save Profile
  else if (profileFieldIndex == 7) {
    if (profileCount < 3) {
      savedProfiles[profileCount] = currentProfile;
      profileCount++;
      saveProfilesToEEPROM();
    }
    inProfileCreation = false;
    display.clearDisplay();
    display.setCursor(0, 5);
    display.println("Profile saved!");
    display.println("");
    display.print("Name: ");
    display.println(currentProfile.name);
    display.print("Age: ");
    display.print(currentProfile.age);
    display.println(" yrs");
    display.print("Treatment: ");
    display.println(currentProfile.treatmentPreference);
    display.display();
    delay(4000);
    lastMove = now;
    delay(200);
  }
  display.display();
}

String buildProfileContext() {
  String context = "Patient Profile - ";
  context += "Name: " + currentProfile.name + ", ";
  context += "Age: " + String(currentProfile.age) + " years, ";
  context += "Height: " + String(currentProfile.height) + "cm, ";
  context += "Weight: " + String(currentProfile.weight) + "kg, ";
  context += "Intoxication: " + currentProfile.intoxStatus + ", ";
  context += "Previous Medical Conditions: " + String(currentProfile.hasMedicalHistory ? "Yes" : "No") + ", ";
  context += "Preferred Treatment: " + currentProfile.treatmentPreference + ". ";
  return context;
}

// -------------------- WEB SERVER ROUTE HANDLERS --------------------
// HTML pages stored in PROGMEM
const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head><meta name="viewport" content="width=device-width,initial-scale=1">
<title>PocketDoctor Dashboard</title>
<style>
body{font-family:sans-serif;background:#f4f4f9;margin:0;padding:20px;color:#333}
.container{max-width:600px;margin:auto;background:#fff;padding:25px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1)}
h1{color:#6B0F1A;border-bottom:2px solid #6B0F1A;padding-bottom:10px}
.card{background:#fafafa;border-left:4px solid #6B0F1A;padding:12px 18px;margin:15px 0;border-radius:6px;transition:0.2s}
.card:hover{background:#f0f0f0}
a{text-decoration:none;color:#6B0F1A;font-weight:bold;font-size:1.1em}
a:hover{text-decoration:underline}
.status{color:#2e7d32;font-weight:bold}
</style>
</head>
<body>
<div class="container">
<h1>🩺 PocketDoctor v3</h1>
<p><span class="status">●</span> System running</p>
<div class="card"><a href="/wifi">📶 Wi‑Fi Settings</a></div>
<div class="card"><a href="/profiles">👤 Profile Management</a></div>
<div class="card"><a href="/report">📄 Latest Report</a></div>
<div style="margin-top:30px;font-size:0.9em;color:#888">
IP: %IP%<br>Uptime: %UPTIME%
</div>
</div>
</body>
</html>
)rawliteral";

const char WIFI_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Wi‑Fi Settings</title>
<style>
body{font-family:sans-serif;background:#f4f4f9;margin:0;padding:20px;color:#333}
.container{max-width:500px;margin:auto;background:#fff;padding:25px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1)}
h1{color:#6B0F1A}
label{display:block;margin:12px 0 4px;font-weight:bold}
input[type=text],input[type=password]{width:100%;padding:10px;border:1px solid #ccc;border-radius:6px;box-sizing:border-box}
input[type=submit]{background:#6B0F1A;color:#fff;padding:12px 20px;border:none;border-radius:6px;cursor:pointer;font-size:1em;margin-top:10px}
input[type=submit]:hover{background:#8B1A2A}
.back{display:inline-block;margin-top:20px;color:#6B0F1A;text-decoration:none}
</style>
</head>
<body>
<div class="container">
<h1>📶 Wi‑Fi Configuration</h1>
<form action="/wifi/update" method="POST">
<label>SSID</label>
<input type="text" name="ssid" value="%SSID%" required>
<label>Password</label>
<input type="password" name="password" value="%PASS%">
<input type="submit" value="Save & Reboot">
</form>
<a class="back" href="/">← Back to Dashboard</a>
</div>
</body>
</html>
)rawliteral";

const char PROFILES_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Profiles</title>
<style>
body{font-family:sans-serif;background:#f4f4f9;margin:0;padding:20px;color:#333}
.container{max-width:600px;margin:auto;background:#fff;padding:25px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1)}
h1{color:#6B0F1A;border-bottom:2px solid #6B0F1A;padding-bottom:10px}
.profile{background:#fafafa;padding:12px;margin:8px 0;border-radius:6px;display:flex;justify-content:space-between;align-items:center}
.profile .name{font-weight:bold}
.actions a{margin-left:10px;color:#6B0F1A;text-decoration:none;font-size:0.9em}
.actions a:hover{text-decoration:underline}
.new{display:inline-block;margin:15px 0;background:#6B0F1A;color:#fff;padding:8px 16px;border-radius:6px;text-decoration:none}
.new:hover{background:#8B1A2A}
.back{display:inline-block;margin-top:20px;color:#6B0F1A;text-decoration:none}
.empty{color:#888;font-style:italic}
</style>
</head>
<body>
<div class="container">
<h1>👤 Profiles</h1>
%PROFILES%
<a class="new" href="/profile/edit?new=1">+ Create New</a>
<br><a class="back" href="/">← Back to Dashboard</a>
</div>
</body>
</html>
)rawliteral";

const char PROFILE_EDIT_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Edit Profile</title>
<style>
body{font-family:sans-serif;background:#f4f4f9;margin:0;padding:20px;color:#333}
.container{max-width:500px;margin:auto;background:#fff;padding:25px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1)}
h1{color:#6B0F1A}
label{display:block;margin:10px 0 4px;font-weight:bold}
input,select{width:100%;padding:8px;border:1px solid #ccc;border-radius:6px;box-sizing:border-box}
input[type=submit]{background:#6B0F1A;color:#fff;padding:12px;border:none;border-radius:6px;cursor:pointer;font-size:1em;margin-top:12px}
input[type=submit]:hover{background:#8B1A2A}
.back{display:inline-block;margin-top:20px;color:#6B0F1A;text-decoration:none}
</style>
</head>
<body>
<div class="container">
<h1>%HEADER%</h1>
<form action="/profile/update" method="POST">
<input type="hidden" name="index" value="%INDEX%">
<label>Name</label><input type="text" name="name" value="%NAME%">
<label>Age</label><input type="number" name="age" value="%AGE%">
<label>Height (cm)</label><input type="number" name="height" value="%HEIGHT%">
<label>Weight (kg)</label><input type="number" name="weight" value="%WEIGHT%">
<label>Intoxication</label>
<select name="intox">
<option value="None" %SEL_NONE%>None</option>
<option value="Alcohol" %SEL_ALCOHOL%>Alcohol</option>
<option value="Tobacco" %SEL_TOBACCO%>Tobacco</option>
<option value="Both" %SEL_BOTH%>Both</option>
</select>
<label>Medical History</label>
<select name="history">
<option value="1" %SEL_HIST_YES%>Yes</option>
<option value="0" %SEL_HIST_NO%>No</option>
</select>
<label>Treatment Preference</label>
<select name="treatment">
<option value="Allopathy" %SEL_ALLOPATHY%>Allopathy</option>
<option value="Homeopathy" %SEL_HOMEOPATHY%>Homeopathy</option>
<option value="Ayurvedic" %SEL_AYURVEDIC%>Ayurvedic</option>
<option value="All types" %SEL_ALLTYPES%>All types</option>
</select>
<input type="submit" value="Save Profile">
</form>
<a class="back" href="/profiles">← Back to Profiles</a>
</div>
</body>
</html>
)rawliteral";

// Helper to substitute placeholders in HTML
String processTemplate(const char* tmpl, const String& ip = "", const String& uptime = "") {
  String html = FPSTR(tmpl);
  if (ip.length()) html.replace("%IP%", ip);
  if (uptime.length()) html.replace("%UPTIME%", uptime);
  return html;
}

void handleDashboard() {
  String ip = WiFi.localIP().toString();
  unsigned long secs = millis() / 1000;
  String uptime = String(secs / 86400) + "d " + String((secs % 86400) / 3600) + "h " + String((secs % 3600) / 60) + "m";
  String html = processTemplate(DASHBOARD_HTML, ip, uptime);
  server.send(200, "text/html", html);
}

void handleWifi() {
  String ssid = wifiConfigValid ? String(wifiConfig.ssid) : "";
  String pass = wifiConfigValid ? String(wifiConfig.password) : "";
  String html = FPSTR(WIFI_HTML);
  html.replace("%SSID%", ssid);
  html.replace("%PASS%", pass);
  server.send(200, "text/html", html);
}

void handleWifiUpdate() {
  if (server.hasArg("ssid") && server.hasArg("password")) {
    String ssid = server.arg("ssid");
    String pass = server.arg("password");
    ssid.trim();
    pass.trim();
    if (ssid.length() > 0) {
      saveWiFiConfig(ssid.c_str(), pass.c_str());
      server.send(200, "text/html", "<html><body><h2>Wi‑Fi saved! Rebooting...</h2><p>Device will restart in 5 seconds.</p><script>setTimeout(function(){window.location.href='/';},5000);</script></body></html>");
      delay(500);
      ESP.restart();
    } else {
      server.send(400, "text/plain", "SSID cannot be empty");
    }
  } else {
    server.send(400, "text/plain", "Missing parameters");
  }
}

void handleProfiles() {
  String profileList = "";
  if (profileCount == 0) {
    profileList = "<div class='empty'>No profiles saved yet.</div>";
  } else {
    for (int i = 0; i < profileCount; i++) {
      profileList += "<div class='profile'>";
      profileList += "<span class='name'>" + savedProfiles[i].name + "</span>";
      profileList += "<div class='actions'>";
      profileList += "<a href='/profile/edit?index=" + String(i) + "'>Edit</a>";
      profileList += " | <a href='/profile/delete?index=" + String(i) + "' onclick='return confirm(\"Delete this profile?\")'>Delete</a>";
      profileList += "</div></div>";
    }
  }
  String html = FPSTR(PROFILES_HTML);
  html.replace("%PROFILES%", profileList);
  server.send(200, "text/html", html);
}

void handleProfileEdit() {
  int index = -1;
  bool isNew = false;
  if (server.hasArg("new")) {
    isNew = true;
  } else if (server.hasArg("index")) {
    index = server.arg("index").toInt();
    if (index < 0 || index >= profileCount) index = -1;
  }
  if (!isNew && index < 0) {
    server.send(404, "text/plain", "Profile not found");
    return;
  }

  Profile p;
  String header;
  if (isNew) {
    p.name = "";
    p.age = 30;
    p.height = 170;
    p.weight = 70;
    p.intoxStatus = "None";
    p.hasMedicalHistory = false;
    p.treatmentPreference = "Allopathy";
    header = "Create New Profile";
  } else {
    p = savedProfiles[index];
    header = "Edit Profile";
  }

  String html = FPSTR(PROFILE_EDIT_HTML);
  html.replace("%HEADER%", header);
  html.replace("%INDEX%", isNew ? "-1" : String(index));
  html.replace("%NAME%", p.name);
  html.replace("%AGE%", String(p.age));
  html.replace("%HEIGHT%", String(p.height));
  html.replace("%WEIGHT%", String(p.weight));

  // Intoxication selects
  html.replace("%SEL_NONE%", p.intoxStatus == "None" ? "selected" : "");
  html.replace("%SEL_ALCOHOL%", p.intoxStatus == "Alcohol" ? "selected" : "");
  html.replace("%SEL_TOBACCO%", p.intoxStatus == "Tobacco" ? "selected" : "");
  html.replace("%SEL_BOTH%", p.intoxStatus == "Both" ? "selected" : "");

  // Medical history
  html.replace("%SEL_HIST_YES%", p.hasMedicalHistory ? "selected" : "");
  html.replace("%SEL_HIST_NO%", p.hasMedicalHistory ? "" : "selected");

  // Treatment
  html.replace("%SEL_ALLOPATHY%", p.treatmentPreference == "Allopathy" ? "selected" : "");
  html.replace("%SEL_HOMEOPATHY%", p.treatmentPreference == "Homeopathy" ? "selected" : "");
  html.replace("%SEL_AYURVEDIC%", p.treatmentPreference == "Ayurvedic" ? "selected" : "");
  html.replace("%SEL_ALLTYPES%", p.treatmentPreference == "All types" ? "selected" : "");

  server.send(200, "text/html", html);
}

void handleProfileUpdate() {
  if (!server.hasArg("index") || !server.hasArg("name") || !server.hasArg("age") ||
      !server.hasArg("height") || !server.hasArg("weight") || !server.hasArg("intox") ||
      !server.hasArg("history") || !server.hasArg("treatment")) {
    server.send(400, "text/plain", "Missing parameters");
    return;
  }

  int index = server.arg("index").toInt();
  bool isNew = (index == -1);
  if (!isNew && (index < 0 || index >= profileCount)) {
    server.send(404, "text/plain", "Invalid profile index");
    return;
  }

  Profile p;
  p.name = server.arg("name");
  p.age = server.arg("age").toInt();
  p.height = server.arg("height").toInt();
  p.weight = server.arg("weight").toInt();
  p.intoxStatus = server.arg("intox");
  p.hasMedicalHistory = (server.arg("history") == "1");
  p.treatmentPreference = server.arg("treatment");

  // Validate
  if (p.name.length() == 0) { server.send(400, "text/plain", "Name cannot be empty"); return; }
  if (p.age < 1 || p.age > 120) { server.send(400, "text/plain", "Age out of range"); return; }

  if (isNew) {
    if (profileCount >= 3) {
      server.send(400, "text/plain", "Maximum 3 profiles reached");
      return;
    }
    savedProfiles[profileCount] = p;
    profileCount++;
  } else {
    savedProfiles[index] = p;
  }
  saveProfilesToEEPROM();

  server.sendHeader("Location", "/profiles");
  server.send(303, "text/plain", "");
}

void handleProfileDelete() {
  if (!server.hasArg("index")) {
    server.send(400, "text/plain", "Missing index");
    return;
  }
  int index = server.arg("index").toInt();
  if (index < 0 || index >= profileCount) {
    server.send(404, "text/plain", "Profile not found");
    return;
  }
  for (int i = index; i < profileCount - 1; i++) {
    savedProfiles[i] = savedProfiles[i+1];
  }
  profileCount--;
  saveProfilesToEEPROM();
  server.sendHeader("Location", "/profiles");
  server.send(303, "text/plain", "");
}

void handleReport() {
  if (reportReady && htmlReport.length() > 0) {
    server.send(200, "text/html", htmlReport);
  } else {
    String msg = "<html><body><h1>No report available</h1><p>Please perform a diagnosis first using the device.</p><a href='/'>Back to Dashboard</a></body></html>";
    server.send(200, "text/html", msg);
  }
}

void startWebServer() {
  server.on("/", handleDashboard);
  server.on("/portal", handleDashboard);  // alias
  server.on("/wifi", handleWifi);
  server.on("/wifi/update", HTTP_POST, handleWifiUpdate);
  server.on("/profiles", handleProfiles);
  server.on("/profile/edit", handleProfileEdit);
  server.on("/profile/update", HTTP_POST, handleProfileUpdate);
  server.on("/profile/delete", handleProfileDelete);
  server.on("/report", handleReport);
  server.begin();
  Serial.println("Web server started");
}

// -------------------- WIFI --------------------
void connectWiFi() {
  Serial.println("Connecting to WiFi...");
  String ssid, pass;
  if (wifiConfigValid) {
    ssid = String(wifiConfig.ssid);
    pass = String(wifiConfig.password);
    Serial.println("Using stored credentials");
  } else {
    ssid = default_ssid;
    pass = default_password;
    Serial.println("Using default credentials");
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Connecting WiFi...");
  display.display();

  WiFi.begin(ssid.c_str(), pass.c_str());
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Serial.println("IP: " + WiFi.localIP().toString());
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("WiFi Connected!");
    display.println(WiFi.localIP().toString());
    display.display();
    delay(2000);
  } else {
    Serial.println("\nWiFi failed!");
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("WiFi Failed!");
    display.println("Check credentials");
    display.display();
    delay(5000);
  }
}

// -------------------- JSON HELPER --------------------
String escapeJson(String str) {
  str.replace("\\", "\\\\");
  str.replace("\"", "\\\"");
  str.replace("\n", "\\n");
  str.replace("\r", "\\r");
  str.replace("\t", "\\t");
  return str;
}

// -------------------- GROQ API CALL --------------------
String sendToGroq(String userMessage, bool isFinalDiagnosis) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected!");
    WiFi.reconnect();
    delay(3000);
    if (WiFi.status() != WL_CONNECTED) {
      return "WiFi disconnected";
    }
  }

  HTTPClient http;
  http.setConnectTimeout(15000);
  http.setTimeout(20000);

  if (!http.begin(groq_url)) {
    Serial.println("Failed to initialize HTTP");
    delay(1000);
    return "Connection failed";
  }

  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", String("Bearer ") + groq_api_key);

  if (conversationHistory.length() > 0) {
    conversationHistory += ",";
  }
  conversationHistory += "{\"role\":\"user\",\"content\":\"" + escapeJson(userMessage) + "\"}";

  if (isFinalDiagnosis) {
    String finalInstruction = "Based on all my answers and the initial symptoms I provided, provide a SHORT preliminary diagnosis in this EXACT format:\n\nDiagnosed Disease: [disease name only]\nAccuracy: [percentage]%\nMedicines: [2-3 medicine names only, no dosages]\nPrecautions: [2-3 key precautions in one short sentence]\nCritical: [Yes/No]\n\nKeep it VERY brief. Follow this format exactly.";
    conversationHistory += ",{\"role\":\"user\",\"content\":\"" + escapeJson(finalInstruction) + "\"}";
  }

  String payload = "{";
  payload += "\"model\":\"" + String(groq_model) + "\",";
  payload += "\"messages\":[" + conversationHistory + "]";
  payload += ",\"temperature\":0.7";
  payload += "}";

  Serial.println("Sending to Groq...");
  int httpResponseCode = http.POST(payload);
  String response = "";

  if (httpResponseCode == 200) {
    String rawResponse = http.getString();
    Serial.println("Response received, size: " + String(rawResponse.length()));

    if (rawResponse.length() < 50) {
      Serial.println("Response too small: " + rawResponse);
      http.end();
      return "Invalid API response";
    }

    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, rawResponse);

    if (!error) {
      JsonObject choice = doc["choices"][0];
      if (choice.isNull()) {
        Serial.println("No choices in response");
        http.end();
        return "Empty API response";
      }
      JsonObject message = choice["message"];
      if (message.isNull()) {
        Serial.println("No message in choice");
        http.end();
        return "Empty message";
      }
      const char* textPtr = message["content"];
      if (textPtr != nullptr && strlen(textPtr) > 0) {
        response = String(textPtr);
        Serial.println("Got response: " + response.substring(0, 50) + "...");
        if (!isFinalDiagnosis) {
          conversationHistory += ",{\"role\":\"assistant\",\"content\":\"" + escapeJson(response) + "\"}";
        }
      } else {
        Serial.println("Text field empty or null");
        response = "No text in response";
      }
    } else {
      Serial.println("JSON parse error: " + String(error.c_str()));
      response = "JSON parse error";
    }
  } else {
    Serial.println("HTTP Error: " + String(httpResponseCode));
    String errorBody = http.getString();
    Serial.println("Error body: " + errorBody);

    if (httpResponseCode == -11 || httpResponseCode == -1) {
      Serial.println("Connection issue - retrying once...");
      http.end();
      delay(2000);
      if (http.begin(groq_url)) {
        http.addHeader("Content-Type", "application/json");
        http.addHeader("Authorization", String("Bearer ") + groq_api_key);
        httpResponseCode = http.POST(payload);
        if (httpResponseCode == 200) {
          String rawResponse = http.getString();
          DynamicJsonDocument doc(4096);
          deserializeJson(doc, rawResponse);
          const char* textPtr = doc["choices"][0]["message"]["content"];
          if (textPtr) {
            response = String(textPtr);
            if (!isFinalDiagnosis) {
              conversationHistory += ",{\"role\":\"assistant\",\"content\":\"" + escapeJson(response) + "\"}";
            }
          }
        }
      }
    }

    if (response.length() == 0) {
      response = "API Error " + String(httpResponseCode);
    }
  }

  http.end();
  delay(500);
  return response;
}

// -------------------- HTML REPORT --------------------
String createComprehensiveHTMLReport() {
  // (Same beautiful HTML report as original – unchanged)
  String html = "<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<title>PocketDoctor — Medical Report</title>";
  html += "<style>";
  html += "*{margin:0;padding:0;box-sizing:border-box}";
  html += ":root{--burg:#6B0F1A;--burg2:#8B1A2A;--burg3:#A52535;--rose:#C94050;--blush:#F2C4C8;--white:#FFFFFF;--cream:#FDF6F7;--offwhite:#F7ECED;--dark:#1A0508;--card:#FFFFFF;--card2:#FDF6F7;--border:#E8C5C9;--text:#2D0A0E;--text2:#7A4048;--gold:#C9860A;--green:#1A6B2A;--radius:14px;--shadow:0 4px 24px rgba(107,15,26,0.12);}";
  html += "html{scroll-behavior:smooth}";
  html += "body{font-family:'Segoe UI',system-ui,sans-serif;background:var(--offwhite);color:var(--text);line-height:1.7;min-height:100vh}";
  html += "body::before{content:'';position:fixed;inset:0;background-image:linear-gradient(rgba(107,15,26,0.04) 1px,transparent 1px),linear-gradient(90deg,rgba(107,15,26,0.04) 1px,transparent 1px);background-size:40px 40px;pointer-events:none;z-index:0}";
  html += ".page{position:relative;z-index:1;max-width:960px;margin:0 auto;padding:24px 16px 60px}";
  html += ".header{background:linear-gradient(135deg,var(--burg) 0%,var(--burg2) 60%,var(--burg3) 100%);border-radius:var(--radius);padding:40px 36px 32px;margin-bottom:28px;position:relative;overflow:hidden;box-shadow:0 8px 32px rgba(107,15,26,0.35)}";
  html += ".header::after{content:'';position:absolute;top:-60px;right:-60px;width:220px;height:220px;border-radius:50%;background:radial-gradient(circle,rgba(242,196,200,0.15),transparent 70%);pointer-events:none}";
  html += ".header::before{content:'';position:absolute;bottom:-40px;left:-40px;width:160px;height:160px;border-radius:50%;background:radial-gradient(circle,rgba(255,255,255,0.06),transparent 70%);pointer-events:none}";
  html += ".header-top{display:flex;align-items:center;gap:18px;margin-bottom:18px}";
  html += ".logo-ring{width:56px;height:56px;border-radius:50%;border:2px solid rgba(242,196,200,0.6);display:flex;align-items:center;justify-content:center;font-size:26px;flex-shrink:0;box-shadow:0 0 18px rgba(242,196,200,0.25);background:rgba(255,255,255,0.1)}";
  html += ".header h1{font-size:clamp(1.4rem,4vw,2rem);font-weight:700;color:var(--white);letter-spacing:-0.5px}";
  html += ".header-sub{color:var(--blush);font-size:0.85rem;letter-spacing:1.5px;text-transform:uppercase;margin-top:2px;opacity:0.85}";
  html += ".header-meta{display:flex;flex-wrap:wrap;gap:10px;margin-top:6px}";
  html += ".meta-chip{background:rgba(255,255,255,0.15);border:1px solid rgba(255,255,255,0.25);border-radius:30px;padding:4px 14px;font-size:0.78rem;color:var(--white);font-weight:500;backdrop-filter:blur(4px)}";
  html += ".summary-row{display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));gap:14px;margin-bottom:28px}";
  html += ".scard{background:var(--white);border:1px solid var(--border);border-radius:var(--radius);padding:20px 18px;box-shadow:var(--shadow);transition:transform 0.2s,box-shadow 0.2s;border-top:3px solid var(--burg)}";
  html += ".scard:hover{transform:translateY(-2px);box-shadow:0 8px 32px rgba(107,15,26,0.18)}";
  html += ".scard-label{font-size:0.72rem;text-transform:uppercase;letter-spacing:1.2px;color:var(--text2);margin-bottom:6px;font-weight:600}";
  html += ".scard-value{font-size:1.2rem;font-weight:700;color:var(--dark)}.scard-value.burg{color:var(--burg)}.scard-value.red{color:#C0392B}.scard-value.green{color:var(--green)}.scard-value.gold{color:var(--gold)}";
  html += ".scard-icon{font-size:1.5rem;margin-bottom:8px}";
  html += ".critical-banner{background:linear-gradient(135deg,rgba(192,57,43,0.1),rgba(192,57,43,0.04));border:1.5px solid #C0392B;border-radius:var(--radius);padding:20px 24px;margin-bottom:24px;display:flex;align-items:flex-start;gap:16px;box-shadow:0 0 24px rgba(192,57,43,0.15)}";
  html += ".critical-banner .icon{font-size:2rem;flex-shrink:0;animation:pulse 1.5s infinite}";
  html += "@keyframes pulse{0%,100%{opacity:1}50%{opacity:0.4}}";
  html += ".critical-banner h3{color:#C0392B;font-size:1rem;font-weight:700;margin-bottom:4px}.critical-banner p{color:var(--text);font-size:0.9rem}";
  html += ".section{background:var(--white);border:1px solid var(--border);border-radius:var(--radius);padding:28px 26px;margin-bottom:20px;box-shadow:var(--shadow)}";
  html += ".section-header{display:flex;align-items:center;gap:12px;margin-bottom:20px;padding-bottom:14px;border-bottom:2px solid var(--offwhite)}";
  html += ".section-icon{width:38px;height:38px;border-radius:10px;background:var(--burg);display:flex;align-items:center;justify-content:center;font-size:1.1rem;flex-shrink:0}";
  html += ".section h2{font-size:1.05rem;font-weight:700;color:var(--dark)}.section h3{font-size:0.85rem;font-weight:700;color:var(--burg);margin:18px 0 8px;text-transform:uppercase;letter-spacing:0.8px}";
  html += ".box-burg{background:linear-gradient(135deg,rgba(107,15,26,0.06),rgba(107,15,26,0.02));border:1px solid rgba(107,15,26,0.2);border-left:4px solid var(--burg);border-radius:8px;padding:14px 18px;margin:10px 0}";
  html += ".box-gold{background:rgba(201,134,10,0.07);border:1px solid rgba(201,134,10,0.3);border-left:4px solid var(--gold);border-radius:8px;padding:14px 18px;margin:10px 0;color:#7A5500}";
  html += ".box-red{background:rgba(192,57,43,0.07);border:1px solid rgba(192,57,43,0.3);border-left:4px solid #C0392B;border-radius:8px;padding:14px 18px;margin:10px 0}";
  html += ".box-green{background:rgba(26,107,42,0.07);border:1px solid rgba(26,107,42,0.25);border-left:4px solid var(--green);border-radius:8px;padding:14px 18px;margin:10px 0}";
  html += "ul.styled{list-style:none;padding:0;margin:6px 0}ul.styled li{padding:7px 0 7px 24px;position:relative;font-size:0.9rem;color:var(--text);border-bottom:1px solid var(--offwhite)}ul.styled li:last-child{border-bottom:none}ul.styled li::before{content:'▸';position:absolute;left:0;color:var(--burg);font-size:0.85rem;top:8px}ul.red-list li::before{color:#C0392B}";
  html += "table{width:100%;border-collapse:collapse;margin:10px 0;font-size:0.88rem}thead th{background:var(--burg);color:var(--white);padding:10px 14px;text-align:left;font-weight:600;letter-spacing:0.5px;font-size:0.8rem;text-transform:uppercase}tbody td{padding:10px 14px;border-bottom:1px solid var(--offwhite);color:var(--text);vertical-align:top}tbody tr:last-child td{border-bottom:none}tbody tr:hover td{background:var(--cream)}";
  html += ".timeline{display:flex;flex-direction:column;gap:0;margin:10px 0}.tl-item{display:flex;gap:16px;align-items:flex-start;padding:12px 0;border-bottom:1px solid var(--offwhite)}.tl-item:last-child{border-bottom:none}.tl-dot{width:10px;height:10px;border-radius:50%;background:var(--burg);flex-shrink:0;margin-top:6px;box-shadow:0 0 6px rgba(107,15,26,0.4)}.tl-content .tl-time{font-size:0.75rem;color:var(--burg);font-weight:700;text-transform:uppercase;letter-spacing:0.8px}.tl-content .tl-text{font-size:0.88rem;color:var(--text);margin-top:2px}";
  html += ".acc-bar-wrap{background:var(--offwhite);border-radius:30px;height:10px;margin:8px 0;overflow:hidden;border:1px solid var(--border)}.acc-bar{height:100%;border-radius:30px;background:linear-gradient(90deg,var(--burg),var(--rose));transition:width 1s ease}";
  html += ".disclaimer{background:rgba(201,134,10,0.05);border:1px solid rgba(201,134,10,0.3);border-radius:var(--radius);padding:24px;margin-top:28px}.disclaimer h3{color:var(--gold);font-size:0.95rem;margin-bottom:10px;font-weight:700}.disclaimer p{font-size:0.84rem;color:var(--text2);line-height:1.6;margin-bottom:6px}";
  html += ".footer{text-align:center;margin-top:40px;color:var(--text2);font-size:0.78rem;letter-spacing:0.5px}.footer span{color:var(--burg);font-weight:600}";
  html += "@media(max-width:600px){.header{padding:24px 18px}.section{padding:20px 16px}.summary-row{grid-template-columns:1fr 1fr}}";
  html += "</style></head><body><div class='page'>";

  html += "<div class='header'><div class='header-top'><div class='logo-ring'>🩺</div><div><h1>PocketDoctor Report</h1><div class='header-sub'>AI-Powered Preliminary Diagnostic Assessment</div></div></div>";
  html += "<div class='header-meta'><span class='meta-chip'>👤 " + currentProfile.name + "</span><span class='meta-chip'>🎂 " + String(currentProfile.age) + " yrs</span><span class='meta-chip'>⚖️ " + String(currentProfile.weight) + " kg</span><span class='meta-chip'>📏 " + String(currentProfile.height) + " cm</span><span class='meta-chip'>💊 " + currentProfile.treatmentPreference + "</span></div></div>";

  html += "<div class='summary-row'>";
  html += "<div class='scard'><div class='scard-icon'>🔬</div><div class='scard-label'>Primary Diagnosis</div><div class='scard-value burg'>" + diagnosedDisease + "</div></div>";
  html += "<div class='scard'><div class='scard-icon'>📊</div><div class='scard-label'>AI Confidence</div><div class='scard-value gold'>" + accuracy + "</div></div>";
  bool isCritical = critical.indexOf("Yes") >= 0;
  html += "<div class='scard'><div class='scard-icon'>" + String(isCritical ? "🚨" : "✅") + "</div><div class='scard-label'>Priority Level</div><div class='scard-value " + String(isCritical ? "red" : "green") + "'>" + String(isCritical ? "URGENT" : "STANDARD") + "</div></div>";
  html += "<div class='scard'><div class='scard-icon'>💊</div><div class='scard-label'>Treatment Type</div><div class='scard-value'>" + currentProfile.treatmentPreference + "</div></div>";
  html += "</div>";

  if (isCritical) {
    html += "<div class='critical-banner'><div class='icon'>🚨</div><div><h3>CRITICAL STATUS — IMMEDIATE ACTION REQUIRED</h3><p>This condition requires urgent medical evaluation. Do not delay — seek emergency care or visit a hospital immediately.</p></div></div>";
  }

  html += "<div class='section'><div class='section-header'><div class='section-icon'>🔬</div><h2>Diagnostic Assessment</h2></div>";
  html += "<h3>Condition</h3><div class='box-burg'><strong style='color:var(--burg);font-size:1.05rem'>" + diagnosedDisease + "</strong> &nbsp;—&nbsp; <span style='color:var(--text)'>diagnosed based on reported symptoms, patient profile, and 10-question AI follow-up.</span></div>";
  html += "<h3>Confidence Score</h3><div style='display:flex;align-items:center;gap:12px'><div class='acc-bar-wrap' style='flex:1'><div class='acc-bar' style='width:" + accuracy + "'></div></div><span style='color:var(--gold);font-weight:700;font-size:1rem'>" + accuracy + "</span></div>";
  html += "<p style='font-size:0.82rem;color:var(--text2);margin-top:8px'>Preliminary AI estimate. Accuracy reflects symptom clarity and answer consistency — not a laboratory result.</p></div>";

  html += "<div class='section'><div class='section-header'><div class='section-icon'>💊</div><h2>Medication Protocol</h2></div>";
  html += "<div class='box-burg'><span style='color:var(--burg);font-weight:700'>Suggested Medicines (" + currentProfile.treatmentPreference + "):</span><br><span style='color:var(--dark);font-size:1rem;font-weight:600'>" + medicines + "</span></div>";
  html += "<h3>Administration Guidelines</h3><table><thead><tr><th>Aspect</th><th>Recommendation</th></tr></thead><tbody>";
  html += "<tr><td>Timing</td><td>Take at the same time each day for consistent blood levels</td></tr>";
  html += "<tr><td>With Food</td><td>Take with meals unless the label specifies otherwise</td></tr>";
  html += "<tr><td>Missed Dose</td><td>Take as soon as remembered — never double up</td></tr>";
  html += "<tr><td>Storage</td><td>Cool, dry place — away from direct sunlight and moisture</td></tr>";
  html += "<tr><td>Duration</td><td>Complete the full prescribed course even if feeling better</td></tr>";
  html += "</tbody></table><h3>Side Effects to Watch</h3><ul class='styled'><li>Allergic reactions — hives, swelling, difficulty breathing</li><li>Severe dizziness or loss of balance</li><li>Unusual fatigue or cognitive changes</li><li>Gastrointestinal distress — severe nausea, vomiting, blood in stool</li></ul></div>";

  html += "<div class='section'><div class='section-header'><div class='section-icon'>⚡</div><h2>Essential Precautions</h2></div>";
  html += "<div class='box-gold'>⚠️ &nbsp;" + precautions + "</div>";
  html += "<h3>General Safety Rules</h3><ul class='styled'><li>No self-medication beyond what is listed — consult a provider first</li><li>Inform all healthcare providers of your current medications and allergies</li><li>Maintain hand hygiene and avoid close contact with sick individuals</li><li>Do not share medications with others under any circumstance</li></ul></div>";

  html += "<div class='section'><div class='section-header'><div class='section-icon'>🥗</div><h2>Lifestyle & Dietary Guidance</h2></div>";
  html += "<h3>Physical Activity</h3><ul class='styled'><li>Light walking 20–30 min daily is beneficial</li><li>Avoid strenuous exercise, heavy lifting, and prolonged standing until improved</li><li>Ensure adequate rest — do not push through fatigue</li></ul>";
  html += "<h3>Sleep</h3><ul class='styled'><li>Target 7–9 hours per night — sleep is critical for recovery</li><li>Maintain a consistent sleep/wake schedule</li><li>Keep the room cool, dark, and quiet</li></ul>";
  html += "<h3>Foods to Prioritize</h3><ul class='styled'><li>Fresh fruits and vegetables — rich in antioxidants and vitamins</li><li>Whole grains for sustained, stable energy</li><li>Lean proteins — fish, chicken, eggs, legumes</li><li>Healthy fats — avocado, olive oil, nuts</li></ul>";
  html += "<h3>Foods to Avoid</h3><ul class='styled'><li>Highly processed or deep-fried foods</li><li>Refined sugars and sugary drinks</li><li>Excessive caffeine and alcohol</li><li>High-sodium snacks and fast food</li></ul>";
  html += "<h3>Hydration Target</h3><div class='box-green'>💧 &nbsp;<strong>2.0 – 2.5 litres of water daily.</strong> Increase intake if experiencing fever, vomiting, or diarrhoea.</div></div>";

  html += "<div class='section'><div class='section-header'><div class='section-icon'>🚨</div><h2>Warning Signs</h2></div>";
  html += "<h3>Seek Emergency Care Immediately If:</h3><div class='box-red'><ul class='styled red-list'>";
  html += "<li>Difficulty breathing or shortness of breath at rest</li><li>Chest pain, tightness, or pressure</li><li>Sudden severe headache unlike anything before</li><li>Loss of consciousness or confusion</li><li>Fever above 39.4°C (103°F) unresponsive to medication</li><li>Signs of severe allergic reaction — throat swelling, rash spreading fast</li>";
  html += "</ul></div><h3>Contact Your Doctor the Same Day If:</h3><ul class='styled'><li>Symptoms worsen significantly or rapidly</li><li>New concerning symptoms develop</li><li>Fever persists beyond 3 days</li><li>You are unable to keep fluids down</li><li>No improvement after 5–7 days of treatment</li></ul></div>";

  html += "<div class='section'><div class='section-header'><div class='section-icon'>📅</div><h2>Follow-Up Schedule</h2></div><div class='timeline'>";
  html += "<div class='tl-item'><div class='tl-dot'></div><div class='tl-content'><div class='tl-time'>Days 1–3</div><div class='tl-text'>Begin medication. Rest, hydrate, and monitor initial symptom response. Note any side effects.</div></div></div>";
  html += "<div class='tl-item'><div class='tl-dot'></div><div class='tl-content'><div class='tl-time'>Days 3–5</div><div class='tl-text'>Self-assessment. Expect early signs of improvement. If worsening, contact provider.</div></div></div>";
  html += "<div class='tl-item'><div class='tl-dot'></div><div class='tl-content'><div class='tl-time'>Week 1–2</div><div class='tl-text'>Visit a healthcare provider if no clear improvement. Bring this report and list of medications.</div></div></div>";
  html += "<div class='tl-item'><div class='tl-dot'></div><div class='tl-content'><div class='tl-time'>Week 2–4</div><div class='tl-text'>Routine follow-up to confirm recovery. Discuss ongoing precautions with your doctor.</div></div></div>";
  html += "</div></div>";

  html += "<div class='section'><div class='section-header'><div class='section-icon'>📈</div><h2>Prognosis</h2></div>";
  html += "<div class='box-burg'>With consistent medication adherence and lifestyle adjustments, most patients with this diagnosis experience significant symptom improvement within <strong>1–2 weeks</strong> and full recovery within <strong>2–4 weeks</strong>.</div>";
  html += "<h3>Key Recovery Factors</h3><ul class='styled'><li>Starting treatment early in the symptom course</li><li>Full medication adherence without skipping doses</li><li>Adequate rest, hydration, and nutrition</li><li>Avoiding triggers (substance use, poor diet, stress) during recovery</li><li>Timely follow-up if improvement is not observed</li></ul></div>";

  html += "<div class='disclaimer'><h3>⚠️ Medical Disclaimer</h3>";
  html += "<p>This report is an <strong>AI-generated preliminary assessment</strong> produced by PocketDoctor and does <strong>NOT</strong> constitute professional medical advice, diagnosis, or treatment.</p>";
  html += "<p>All findings are based on self-reported symptoms and yes/no responses — not a physical examination, laboratory tests, or imaging. A qualified licensed healthcare professional must evaluate you before acting on this report.</p>";
  html += "<p>In a medical emergency, <strong>contact emergency services immediately</strong> — do not rely on this document for urgent decisions.</p></div>";

  html += "<div class='footer'>Generated by <span>PocketDoctor v3</span> · AI Diagnostic System · For informational use only</div>";
  html += "</div></body></html>";
  return html;
}

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
  if (accuracyPos != -1 && medicinesPos != -1) {
    accuracy = diagnosis.substring(accuracyPos + 9, medicinesPos);
    accuracy.trim();
  }
  if (medicinesPos != -1 && precautionsPos != -1) {
    medicines = diagnosis.substring(medicinesPos + 10, precautionsPos);
    medicines.trim();
  }
  if (precautionsPos != -1 && criticalPos != -1) {
    precautions = diagnosis.substring(precautionsPos + 12, criticalPos);
    precautions.trim();
  }
  if (criticalPos != -1) {
    critical = diagnosis.substring(criticalPos + 9);
    critical.trim();
  }
}

void displayShortDiagnosisWithSpinner() {
  const char* processingSteps[] = {
    "Analyzing data...",
    "Cross-checking...",
    "Calculating...",
    "Finalizing..."
  };
  for (int step = 0; step < 4; step++) {
    for (int i = 0; i < 3; i++) {
      showLoadingScreen(processingSteps[step], "Please wait");
      delay(300);
    }
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("SHORT DIAGNOSIS");
  display.println("===============");
  int yPos = 16;
  display.setCursor(0, yPos);
  display.println(diagnosedDisease.substring(0, 21));
  yPos += 10;
  display.setCursor(0, yPos);
  display.println(accuracy);
  yPos += 10;
  display.setCursor(0, yPos);
  display.println(medicines.substring(0, 21));
  display.display();
  delay(3000);
  showLoadingScreen("Generating", "full report...");
  delay(1000);
}

String buildSymptomList() {
  String symptomList = "";
  bool first = true;
  for (int i = 0; i < symptomCount; i++) {
    if (selectedSymptoms[i]) {
      if (!first) symptomList += ", ";
      symptomList += symptoms[i];
      first = false;
    }
  }
  return symptomList;
}

void startGroqDiagnosisWithData() {
  conversationHistory = "";
  questionCount = 0;
  yesNoIndex = 0;

  showLoadingScreen("Starting Groq...", "Initializing");
  delay(800);
  showLoadingScreen("Connecting to AI...", "Establishing link");
  delay(800);
  showLoadingScreen("Loading profile...", "Analyzing data");
  delay(800);
  showLoadingScreen("Preparing query...", "Almost ready");
  delay(800);

  String symptomList = buildSymptomList();
  String profileContext = buildProfileContext();
  String initialContext = profileContext + "Symptoms: " + symptomList + ". Duration: " + String(durations[selectedDurationIndex]) +
                         ". Ask 10 strategic yes/no questions (max 10 words each). Output one question only each time [I REPEAT ONE EACH TIME.], and cover all related aspects for efficiency. Cover different aspects: location, intensity, timing, triggers. No extra text, ONLY question.";

  Serial.println("\n=== INITIAL CONTEXT ===");
  Serial.println("Symptoms: " + symptomList);
  Serial.println("Duration: " + String(durations[selectedDurationIndex]));
  Serial.println("=======================\n");

  showLoadingScreen("Querying AI...", "Getting response");
  currentQuestion = sendToGroq(initialContext, false);

  if (currentQuestion.length() == 0 || currentQuestion == "WiFi disconnected" || currentQuestion == "Connection failed") {
    currentQuestion = "Is the pain constant or intermittent?";
  }

  questionCount = 1;
  inGeminiQuestions = true;
}

void handleGeminiQuestions() {
  int xValue = analogRead(VRx);
  int yValue = analogRead(VRy);
  int swState = digitalRead(SW);
  unsigned long now = millis();

  if (yValue < (2048 - joyThreshold) && (now - lastMove > debounceDelay)) {
    yesNoIndex = 0;
    lastMove = now;
  }
  if (yValue > (2048 + joyThreshold) && (now - lastMove > debounceDelay)) {
    yesNoIndex = 1;
    lastMove = now;
  }

  if (swState == LOW && (now - lastMove > debounceDelay)) {
    String answer = (yesNoIndex == 0) ? "Yes" : "No";
    Serial.println("Q" + String(questionCount) + ": " + currentQuestion);
    Serial.println("A: " + answer);

    showLoadingScreen("Sending answer...", "Processing");
    questionCount++;

    if (questionCount <= maxQuestions) {
      currentQuestion = sendToGroq(answer, false);
    } else {
      showLoadingScreen("Final analysis...", "Computing");
      delay(1000);
      String shortDiagnosis = sendToGroq(answer, true);
      parseDiagnosis(shortDiagnosis);

      Serial.println("\n╔════════════════════════════════════════╗");
      Serial.println("║          SHORT DIAGNOSIS               ║");
      Serial.println("╚════════════════════════════════════════╝");
      Serial.println(shortDiagnosis);
      Serial.println("══════════════════════════════════════════\n");

      displayShortDiagnosisWithSpinner();

      showLoadingScreen("Building report...", "Formatting");
      delay(500);
      htmlReport = createComprehensiveHTMLReport();
      reportReady = true;

      Serial.println("\n╔════════════════════════════════════════╗");
      Serial.println("║          FULL HTML REPORT              ║");
      Serial.println("╚════════════════════════════════════════╝");
      Serial.println("HTML Report Generated!");
      Serial.println("══════════════════════════════════════════\n");

      // Show IP on OLED for easy access
      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(0, 0);
      display.println("Report Ready!");
      display.println("Visit:");
      display.println(WiFi.localIP().toString());
      display.println("/ or /portal");
      display.println("Press button to exit");
      display.display();

      // Wait for button press to return to main menu
      while (digitalRead(SW) == HIGH) {
        server.handleClient();  // keep serving web
        delay(100);
      }
      delay(500);

      inGeminiQuestions = false;
      conversationHistory = "";
      questionCount = 0;
      currentQuestion = "";
      for (int i = 0; i < symptomCount; i++) {
        selectedSymptoms[i] = false;
      }
    }
    lastMove = now;
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Q");
  display.print(questionCount);
  display.print("/");
  display.print(maxQuestions);
  display.println(":");
  display.println("------------");
  String displayText = currentQuestion;
  if (displayText.length() > 70) {
    displayText = displayText.substring(0, 70) + "...";
  }
  display.setCursor(0, 16);
  display.println(displayText);
  display.setCursor(10, 50);
  if (yesNoIndex == 0) {
    display.print("> YES   NO");
  } else {
    display.print("  YES  > NO");
  }
  display.display();
}

void drawMedicalIcon() {
  display.fillRect(55, 10, 18, 5, SSD1306_WHITE);
  display.fillRect(58, 5, 12, 15, SSD1306_WHITE);
  int y = 35;
  display.drawLine(20, y, 30, y, SSD1306_WHITE);
  display.drawLine(30, y, 35, y-8, SSD1306_WHITE);
  display.drawLine(35, y-8, 40, y+8, SSD1306_WHITE);
  display.drawLine(40, y+8, 45, y, SSD1306_WHITE);
  display.drawLine(45, y, 55, y, SSD1306_WHITE);
  display.drawLine(55, y, 58, y-5, SSD1306_WHITE);
  display.drawLine(58, y-5, 61, y+5, SSD1306_WHITE);
  display.drawLine(61, y+5, 64, y, SSD1306_WHITE);
  display.drawLine(64, y, 75, y, SSD1306_WHITE);
  display.drawLine(75, y, 78, y-4, SSD1306_WHITE);
  display.drawLine(78, y-4, 81, y+4, SSD1306_WHITE);
  display.drawLine(81, y+4, 84, y, SSD1306_WHITE);
  display.drawLine(84, y, 108, y, SSD1306_WHITE);
}

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);
  delay(1000);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 allocation failed");
    for (;;);
  }
  display.clearDisplay();
  display.display();

  pinMode(SW, INPUT_PULLUP);

  for (int i = 0; i < symptomCount; i++) {
    selectedSymptoms[i] = false;
  }

  // Load Wi-Fi config from EEPROM
  loadWiFiConfig();

  connectWiFi();

  // Start web server if WiFi connected
  if (WiFi.status() == WL_CONNECTED) {
    startWebServer();
  }

  loadProfilesFromEEPROM();
  Serial.println("Pocket Doctor Started");

  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(10, 45);
  display.println("Pocket");
  display.setCursor(15, 55);
  display.setTextSize(1);
  display.println("Doctor v3.0");
  drawMedicalIcon();
  display.display();
  delay(3000);

  display.clearDisplay();
  display.display();
  delay(100);

  // Start with profile menu
  inProfileMenu = true;
  profileMenuIndex = 0;
}

// ==================== MAIN LOOP ====================
void loop() {
  // Always handle web clients
  if (WiFi.status() == WL_CONNECTED) {
    server.handleClient();
  }

  if (inProfileMenu) {
    handleProfileMenu();
    return;
  }

  if (inProfileCreation) {
    handleProfileCreation();
    return;
  }

  int xValue = analogRead(VRx);
  int yValue = analogRead(VRy);
  int swState = digitalRead(SW);
  unsigned long now = millis();

  // Main menu – press to start symptom selection
  if (!inSymptomSelection && !inDurationSelection && !inGeminiQuestions) {
    if (swState == LOW && (now - lastMove > debounceDelay)) {
      inSymptomSelection = true;
      symptomIndex = 0;
      lastMove = now;
    }

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.fillRect(5, 8, 10, 3, SSD1306_WHITE);
    display.fillRect(7, 5, 6, 9, SSD1306_WHITE);
    display.setCursor(20, 5);
    display.setTextSize(1);
    display.println("Pocket Doctor");
    display.drawLine(20, 15, 110, 15, SSD1306_WHITE);
    display.setCursor(10, 25);
    display.println("AI-Powered");
    display.setCursor(10, 35);
    display.println("Diagnostics");
    display.setCursor(10, 50);
    display.println("Press to Start");
    display.display();
  }

  // Groq Q&A session
  if (inGeminiQuestions) {
    handleGeminiQuestions();
    return;
  }

  // Symptom selection
  if (inSymptomSelection) {
    int totalOptions = symptomCount + 1;
    if (yValue < (2048 - joyThreshold) && (now - lastMove > debounceDelay)) {
      symptomIndex--;
      if (symptomIndex < 0) symptomIndex = 0;
      lastMove = now;
    }
    if (yValue > (2048 + joyThreshold) && (now - lastMove > debounceDelay)) {
      symptomIndex++;
      if (symptomIndex >= totalOptions) symptomIndex = totalOptions - 1;
      lastMove = now;
    }
    if (swState == LOW && (now - lastMove > debounceDelay)) {
      if (symptomIndex < symptomCount) {
        selectedSymptoms[symptomIndex] = !selectedSymptoms[symptomIndex];
      } else {
        bool hasSymptoms = false;
        for (int i = 0; i < symptomCount; i++) {
          if (selectedSymptoms[i]) {
            hasSymptoms = true;
            break;
          }
        }
        if (hasSymptoms) {
          inSymptomSelection = false;
          inDurationSelection = true;
          durationIndex = 0;
        } else {
          display.clearDisplay();
          display.setTextSize(1);
          display.setCursor(0, 20);
          display.println("Please select");
          display.println("at least one");
          display.println("symptom!");
          display.display();
          delay(2000);
        }
      }
      lastMove = now;
    }

    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("Select Symptoms:");
    int start = symptomIndex - 2;
    if (start < 0) start = 0;
    int end = start + 4;
    if (end > totalOptions) end = totalOptions;
    for (int i = start; i < end; i++) {
      display.setCursor(5, 12 + (i - start) * 12);
      if (i == symptomIndex) display.print("> ");
      else display.print("  ");
      if (i < symptomCount) {
        display.print(symptoms[i]);
        if (selectedSymptoms[i]) display.print(" [x]");
      } else {
        display.print("NEXT");
      }
    }
    display.display();
  }

  // Duration selection
  if (inDurationSelection) {
    int totalOptions = 4 + 1;
    if (yValue < (2048 - joyThreshold) && (now - lastMove > debounceDelay)) {
      durationIndex--;
      if (durationIndex < 0) durationIndex = 0;
      lastMove = now;
    }
    if (yValue > (2048 + joyThreshold) && (now - lastMove > debounceDelay)) {
      durationIndex++;
      if (durationIndex >= totalOptions) durationIndex = totalOptions - 1;
      lastMove = now;
    }
    if (swState == LOW && (now - lastMove > debounceDelay)) {
      if (durationIndex < 4) {
        selectedDurationIndex = durationIndex;
      } else {
        Serial.println("\n=== COLLECTED DATA ===");
        Serial.print("Symptoms: ");
        Serial.println(buildSymptomList());
        Serial.print("Duration: ");
        Serial.println(durations[selectedDurationIndex]);
        Serial.println("======================\n");
        inDurationSelection = false;
        startGroqDiagnosisWithData();
      }
      lastMove = now;
    }

    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("Duration:");
    for (int i = 0; i < totalOptions; i++) {
      display.setCursor(5, 12 + i * 10);
      if (i == durationIndex) display.print("> ");
      else display.print("  ");
      if (i < 4) display.print(durations[i]);
      else display.print("START DIAGNOSIS");
    }
    display.display();
  }
}