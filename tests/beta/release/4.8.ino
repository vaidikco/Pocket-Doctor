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
#include <NetworkClientSecure.h>
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
  uint16_t magic;      // 0xABCE
  char ssid[33];
  char password[33];
  char sosEmail[64];
  char sosFormId[64];
};

Config wifiConfig;
bool wifiConfigValid = false;

// EEPROM layout
#define CONFIG_START  0
#define CONFIG_MAGIC  0xABCE
#define PROFILES_START 150

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

// Modes
bool inModeSelection = false;
int modeIndex = 0;
bool isDailyCheckup = false;
bool inSOSLevelSelection = false;
int sosLevel = 1;
bool isSOSMode = false;

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

void dispatchSOSAlert() {
  showLoadingScreen("Fetching Loc...", "Standby");
  
  String locationData = "Unknown Location";
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin("http://ip-api.com/json/");
    int httpCode = http.GET();
    if (httpCode == 200) {
      String payload = http.getString();
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, payload);
      String city = doc["city"].as<String>();
      String lat = doc["lat"].as<String>();
      String lon = doc["lon"].as<String>();
      locationData = city + " (Lat: " + lat + ", Lon: " + lon + ")";
    }
    http.end();
  }

  showLoadingScreen("Sending SOS...", "Alerting Contact");

  String targetEmail = wifiConfigValid ? String(wifiConfig.sosEmail) : "none";
  if (targetEmail.length() < 3) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 20);
    display.println("SOS Failed!");
    display.println("No Email Configured");
    display.display();
    delay(3000);
    return;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("SOS: Connected to WiFi, initializing secure HTTPS client...");
    NetworkClientSecure client;
    client.setInsecure(); // Required for HTTPS endpoints on ESP32

    HTTPClient http;
    String endpoint = "https://formspree.io/f/";
    if (wifiConfigValid && strlen(wifiConfig.sosFormId) > 0) {
      String fid = String(wifiConfig.sosFormId);
      fid.trim();
      if (fid.startsWith("http")) {
        endpoint = fid;
      } else {
        endpoint += fid;
      }
    } else {
      endpoint += "dummy_form_id";
    }

    Serial.println("SOS Target Endpoint: " + endpoint);
    http.begin(client, endpoint);
    http.addHeader("Content-Type", "application/json");

    DynamicJsonDocument doc(2048);
    doc["email"] = targetEmail;
    doc["_replyto"] = targetEmail;
    doc["subject"] = "URGENT SOS ALERT from PocketDoctor";
    String message = "SOS ALERT ACTIVATED!\n\n";
    message += "Severity Level: " + String(sosLevel) + "/10\n";
    message += "Patient: " + String(currentProfile.name) + "\n";
    message += "Age: " + String(currentProfile.age) + "\n";
    message += "Weight: " + String(currentProfile.weight) + " kg\n";
    message += "Height: " + String(currentProfile.height) + " cm\n";
    message += "Location: " + locationData + "\n";
    doc["message"] = message;

    String requestBody;
    serializeJson(doc, requestBody);
    
    Serial.println("SOS Payload: " + requestBody);
    
    int httpCode = http.POST(requestBody);
    Serial.print("SOS HTTP Response Code: ");
    Serial.println(httpCode);
    
    if (httpCode > 0) {
      String response = http.getString();
      Serial.println("SOS HTTP Response: " + response);
    } else {
      Serial.printf("SOS POST Failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
    
    http.end();

    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 20);
    if (httpCode > 0) {
      display.println("SOS Sent");
      display.println("Successfully!");
    } else {
      display.println("SOS Send Failed");
    }
    display.display();
    delay(3000);
  } else {
    Serial.println("SOS: WiFi not connected, cannot send email.");
  }
}

// -------------------- EEPROM CONFIGURATION (Wi-Fi & SOS) --------------------
void loadWiFiConfig() {
  EEPROM.begin(EEPROM_SIZE);
  Config cfg;
  EEPROM.get(CONFIG_START, cfg);
  if (cfg.magic == CONFIG_MAGIC) {
    wifiConfig = cfg;
    wifiConfigValid = true;
    Serial.println("System config loaded from EEPROM");
  } else {
    wifiConfigValid = false;
    Serial.println("No valid config in EEPROM, using defaults");
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
  
  if (wifiConfigValid) {
    strncpy(cfg.sosEmail, wifiConfig.sosEmail, 63);
    cfg.sosEmail[63] = '\0';
    strncpy(cfg.sosFormId, wifiConfig.sosFormId, 63);
    cfg.sosFormId[63] = '\0';
  } else {
    cfg.sosEmail[0] = '\0';
    cfg.sosFormId[0] = '\0';
  }

  EEPROM.put(CONFIG_START, cfg);
  EEPROM.commit();
  EEPROM.end();
  wifiConfig = cfg;
  wifiConfigValid = true;
  Serial.println("WiFi config saved to EEPROM");
}

void saveSOSEmail(const char* email, const char* formId) {
  EEPROM.begin(EEPROM_SIZE);
  Config cfg;
  EEPROM.get(CONFIG_START, cfg);
  if (cfg.magic != CONFIG_MAGIC) {
    cfg.magic = CONFIG_MAGIC;
    cfg.ssid[0] = '\0';
    cfg.password[0] = '\0';
  }
  strncpy(cfg.sosEmail, email, 63);
  cfg.sosEmail[63] = '\0';
  strncpy(cfg.sosFormId, formId, 63);
  cfg.sosFormId[63] = '\0';
  EEPROM.put(CONFIG_START, cfg);
  EEPROM.commit();
  EEPROM.end();
  wifiConfig = cfg;
  wifiConfigValid = true;
  Serial.println("SOS Settings saved to EEPROM");
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
<html lang="en">
<head>
<meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>PocketDoctor Mark 4 — Control Portal</title>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link href="https://fonts.googleapis.com/css2?family=Plus+Jakarta+Sans:wght@400;500;600;700;800&display=swap" rel="stylesheet">
<style>
:root{--burg:#6B0F1A;--burg-dark:#4A0A12;--accent:#D4384B;--bg-start:#FDF6F7;--bg-end:#E8D8DA;--card-bg:rgba(255,255,255,0.85);--card-border:rgba(232,197,201,0.6);--shadow:0 12px 36px rgba(107,15,26,0.08);--text:#2D0A0E;--text-sub:#7A4048;--radius:20px;--pulse:#2E7D32}
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:'Plus Jakarta Sans',system-ui,sans-serif;background:linear-gradient(135deg,var(--bg-start) 0%,#F5EAEB 50%,var(--bg-end) 100%);color:var(--text);min-height:100vh;padding:24px 16px;line-height:1.6;display:flex;align-items:center;justify-content:center}
.app-window{width:100%;max-width:580px;background:var(--card-bg);backdrop-filter:blur(16px);-webkit-backdrop-filter:blur(16px);border:1px solid var(--card-border);border-radius:var(--radius);padding:32px 28px;box-shadow:var(--shadow);position:relative;overflow:hidden}
.app-window::before{content:'';position:absolute;top:-80px;right:-80px;width:240px;height:240px;background:radial-gradient(circle,rgba(212,56,75,0.12),transparent 70%);pointer-events:none}
.header{display:flex;align-items:center;justify-content:space-between;padding-bottom:20px;margin-bottom:24px;border-bottom:1.5px solid rgba(107,15,26,0.08)}
.brand{display:flex;align-items:center;gap:14px}
.logo-badge{width:48px;height:48px;border-radius:14px;background:linear-gradient(135deg,var(--burg),var(--accent));display:flex;align-items:center;justify-content:center;font-size:22px;color:#fff;box-shadow:0 6px 18px rgba(107,15,26,0.25)}
.brand-title{font-size:1.35rem;font-weight:800;letter-spacing:-0.5px;color:var(--burg-dark)}
.brand-sub{font-size:0.75rem;font-weight:600;color:var(--text-sub);text-transform:uppercase;letter-spacing:1px}
.status-pill{display:inline-flex;align-items:center;gap:8px;background:rgba(46,125,50,0.08);border:1px solid rgba(46,125,50,0.2);padding:6px 14px;border-radius:30px;font-size:0.78rem;font-weight:700;color:var(--pulse)}
.status-dot{width:8px;height:8px;border-radius:50%;background:var(--pulse);box-shadow:0 0 8px var(--pulse);animation:pulse 2s infinite}
@keyframes pulse{0%,100%{opacity:1;transform:scale(1)}50%{opacity:0.4;transform:scale(0.85)}}
.grid{display:flex;flex-direction:column;gap:14px;margin-bottom:28px}
.nav-card{display:flex;align-items:center;justify-content:space-between;background:rgba(255,255,255,0.7);border:1px solid rgba(232,197,201,0.5);border-radius:16px;padding:18px 20px;text-decoration:none;color:var(--text);transition:all 0.25s cubic-bezier(0.4,0,0.2,1);box-shadow:0 4px 16px rgba(0,0,0,0.02)}
.nav-card:hover{transform:translateY(-3px);background:#fff;border-color:var(--burg);box-shadow:0 10px 24px rgba(107,15,26,0.12)}
.card-left{display:flex;align-items:center;gap:16px}
.card-icon{width:42px;height:42px;border-radius:12px;background:rgba(107,15,26,0.06);display:flex;align-items:center;justify-content:center;font-size:1.2rem;color:var(--burg);transition:background 0.2s}
.nav-card:hover .card-icon{background:var(--burg);color:#fff}
.card-label{font-size:0.98rem;font-weight:700;color:var(--burg-dark)}
.card-desc{font-size:0.78rem;color:var(--text-sub);margin-top:2px}
.arrow-icon{font-size:1.2rem;color:var(--text-sub);transition:transform 0.2s,color 0.2s}
.nav-card:hover .arrow-icon{transform:translateX(4px);color:var(--burg)}
.meta-box{background:rgba(107,15,26,0.04);border:1px dashed rgba(107,15,26,0.18);border-radius:14px;padding:14px 18px;display:flex;align-items:center;justify-content:space-between;font-size:0.82rem;color:var(--text-sub);font-weight:600}
.meta-chip{background:#fff;padding:4px 10px;border-radius:8px;border:1px solid rgba(107,15,26,0.1);color:var(--burg-dark);font-family:monospace;font-size:0.85rem}
</style>
</head>
<body>
<div class="app-window">
<div class="header">
<div class="brand">
<div class="logo-badge">🩺</div>
<div>
<div class="brand-title">PocketDoctor</div>
<div class="brand-sub">Mark 4 Control Portal · By Vaidik Khurana</div>
</div>
</div>
<div class="status-pill"><span class="status-dot"></span> Online</div>
</div>
<div class="grid">
<a href="/wifi" class="nav-card">
<div class="card-left">
<div class="card-icon">📶</div>
<div>
<div class="card-label">Wi-Fi Configuration</div>
<div class="card-desc">Update network credentials and reboot system</div>
</div>
</div>
<span class="arrow-icon">→</span>
</a>
<a href="/profiles" class="nav-card">
<div class="card-left">
<div class="card-icon">👤</div>
<div>
<div class="card-label">Patient Profiles</div>
<div class="card-desc">Manage saved records, body metrics & preferences</div>
</div>
</div>
<span class="arrow-icon">→</span>
</a>
<a href="/report" class="nav-card">
<div class="card-left">
<div class="card-icon">📄</div>
<div>
<div class="card-label">Clinical Diagnostic Report</div>
<div class="card-desc">View and print the latest AI diagnostic analysis</div>
</div>
</div>
<span class="arrow-icon">→</span>
</a>
<a href="/sos" class="nav-card">
<div class="card-left">
<div class="card-icon">🚨</div>
<div>
<div class="card-label">SOS Configuration</div>
<div class="card-desc">Set emergency contact email address</div>
</div>
</div>
<span class="arrow-icon">→</span>
</a>
</div>
<div class="meta-box">
<span>IP: <span class="meta-chip">%IP%</span></span>
<span>Uptime: <span class="meta-chip">%UPTIME%</span></span>
</div>
<div style="text-align:center;margin-top:16px;font-size:0.75rem;color:var(--text-sub);font-weight:700">
Designed & Engineered by <span style="color:var(--burg-dark)">Vaidik Khurana</span>
</div>
</div>
</body>
</html>
)rawliteral";

const char SOS_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>SOS Configuration — PocketDoctor</title>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link href="https://fonts.googleapis.com/css2?family=Plus+Jakarta+Sans:wght@400;500;600;700;800&display=swap" rel="stylesheet">
<style>
:root{--burg:#6B0F1A;--burg-dark:#4A0A12;--accent:#D4384B;--bg-start:#FDF6F7;--bg-end:#E8D8DA;--card-bg:rgba(255,255,255,0.88);--card-border:rgba(232,197,201,0.6);--shadow:0 12px 36px rgba(107,15,26,0.08);--text:#2D0A0E;--text-sub:#7A4048;--radius:20px}
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:'Plus Jakarta Sans',system-ui,sans-serif;background:linear-gradient(135deg,var(--bg-start) 0%,#F5EAEB 50%,var(--bg-end) 100%);color:var(--text);min-height:100vh;padding:24px 16px;line-height:1.6;display:flex;align-items:center;justify-content:center}
.app-window{width:100%;max-width:500px;background:var(--card-bg);backdrop-filter:blur(16px);-webkit-backdrop-filter:blur(16px);border:1px solid var(--card-border);border-radius:var(--radius);padding:32px 28px;box-shadow:var(--shadow)}
.header{margin-bottom:24px;border-bottom:1.5px solid rgba(107,15,26,0.08);padding-bottom:16px}
.title{font-size:1.3rem;font-weight:800;color:var(--burg-dark);display:flex;align-items:center;gap:10px}
.subtitle{font-size:0.82rem;color:var(--text-sub);margin-top:4px}
.form-group{margin-bottom:18px}
label{display:block;font-size:0.85rem;font-weight:700;color:var(--burg-dark);margin-bottom:6px}
input[type=email]{width:100%;padding:12px 16px;font-size:0.95rem;font-family:inherit;background:rgba(255,255,255,0.9);border:1.5px solid rgba(232,197,201,0.8);border-radius:12px;color:var(--text);transition:all 0.2s}
input[type=email]:focus{outline:none;border-color:var(--burg);box-shadow:0 0 0 4px rgba(107,15,26,0.12);background:#fff}
.btn-submit{width:100%;margin-top:10px;padding:14px;font-size:0.98rem;font-weight:700;font-family:inherit;color:#fff;background:linear-gradient(135deg,var(--burg),var(--accent));border:none;border-radius:12px;cursor:pointer;box-shadow:0 6px 20px rgba(107,15,26,0.25);transition:all 0.2s}
.btn-submit:hover{transform:translateY(-2px);box-shadow:0 8px 24px rgba(107,15,26,0.35)}
.back-link{display:inline-flex;align-items:center;gap:6px;margin-top:20px;font-size:0.88rem;font-weight:700;color:var(--burg);text-decoration:none;transition:color 0.2s}
.back-link:hover{color:var(--accent);text-decoration:underline}
</style>
</head>
<body>
<div class="app-window">
<div class="header">
<div class="title">🚨 SOS Configuration</div>
<div class="subtitle">Set the emergency contact email address</div>
</div>
<form action="/sos/update" method="POST">
<div class="form-group">
<label>SOS Contact Email</label>
<input type="email" name="email" value="%EMAIL%" required placeholder="Enter SOS Email Address">
</div>
<div class="form-group">
<label>Formspree Form ID / Endpoint (Optional)</label>
<input type="text" name="formid" value="%FORMID%" placeholder="e.g. xeyrqwpo or full Formspree URL">
</div>
<button type="submit" class="btn-submit">Save SOS Configuration</button>
</form>
<a class="back-link" href="/">← Back to Control Portal</a>
</div>
</body>
</html>
)rawliteral";

const char WIFI_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Wi-Fi Configuration — PocketDoctor</title>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link href="https://fonts.googleapis.com/css2?family=Plus+Jakarta+Sans:wght@400;500;600;700;800&display=swap" rel="stylesheet">
<style>
:root{--burg:#6B0F1A;--burg-dark:#4A0A12;--accent:#D4384B;--bg-start:#FDF6F7;--bg-end:#E8D8DA;--card-bg:rgba(255,255,255,0.88);--card-border:rgba(232,197,201,0.6);--shadow:0 12px 36px rgba(107,15,26,0.08);--text:#2D0A0E;--text-sub:#7A4048;--radius:20px}
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:'Plus Jakarta Sans',system-ui,sans-serif;background:linear-gradient(135deg,var(--bg-start) 0%,#F5EAEB 50%,var(--bg-end) 100%);color:var(--text);min-height:100vh;padding:24px 16px;line-height:1.6;display:flex;align-items:center;justify-content:center}
.app-window{width:100%;max-width:500px;background:var(--card-bg);backdrop-filter:blur(16px);-webkit-backdrop-filter:blur(16px);border:1px solid var(--card-border);border-radius:var(--radius);padding:32px 28px;box-shadow:var(--shadow)}
.header{margin-bottom:24px;border-bottom:1.5px solid rgba(107,15,26,0.08);padding-bottom:16px}
.title{font-size:1.3rem;font-weight:800;color:var(--burg-dark);display:flex;align-items:center;gap:10px}
.subtitle{font-size:0.82rem;color:var(--text-sub);margin-top:4px}
.form-group{margin-bottom:18px}
label{display:block;font-size:0.85rem;font-weight:700;color:var(--burg-dark);margin-bottom:6px}
input[type=text],input[type=password]{width:100%;padding:12px 16px;font-size:0.95rem;font-family:inherit;background:rgba(255,255,255,0.9);border:1.5px solid rgba(232,197,201,0.8);border-radius:12px;color:var(--text);transition:all 0.2s}
input[type=text]:focus,input[type=password]:focus{outline:none;border-color:var(--burg);box-shadow:0 0 0 4px rgba(107,15,26,0.12);background:#fff}
.btn-submit{width:100%;margin-top:10px;padding:14px;font-size:0.98rem;font-weight:700;font-family:inherit;color:#fff;background:linear-gradient(135deg,var(--burg),var(--accent));border:none;border-radius:12px;cursor:pointer;box-shadow:0 6px 20px rgba(107,15,26,0.25);transition:all 0.2s}
.btn-submit:hover{transform:translateY(-2px);box-shadow:0 8px 24px rgba(107,15,26,0.35)}
.back-link{display:inline-flex;align-items:center;gap:6px;margin-top:20px;font-size:0.88rem;font-weight:700;color:var(--burg);text-decoration:none;transition:color 0.2s}
.back-link:hover{color:var(--accent);text-decoration:underline}
</style>
</head>
<body>
<div class="app-window">
<div class="header">
<div class="title">📶 Wi-Fi Configuration</div>
<div class="subtitle">Configure local network access credentials</div>
</div>
<form action="/wifi/update" method="POST">
<div class="form-group">
<label>Network SSID</label>
<input type="text" name="ssid" value="%SSID%" required placeholder="Enter Wi-Fi SSID">
</div>
<div class="form-group">
<label>Wi-Fi Password</label>
<input type="password" name="password" value="%PASS%" placeholder="Enter Wi-Fi Password">
</div>
<button type="submit" class="btn-submit">Save Settings & Reboot Device</button>
</form>
<a class="back-link" href="/">← Back to Control Portal</a>
</div>
</body>
</html>
)rawliteral";

const char PROFILES_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Patient Profiles — PocketDoctor</title>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link href="https://fonts.googleapis.com/css2?family=Plus+Jakarta+Sans:wght@400;500;600;700;800&display=swap" rel="stylesheet">
<style>
:root{--burg:#6B0F1A;--burg-dark:#4A0A12;--accent:#D4384B;--bg-start:#FDF6F7;--bg-end:#E8D8DA;--card-bg:rgba(255,255,255,0.88);--card-border:rgba(232,197,201,0.6);--shadow:0 12px 36px rgba(107,15,26,0.08);--text:#2D0A0E;--text-sub:#7A4048;--radius:20px}
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:'Plus Jakarta Sans',system-ui,sans-serif;background:linear-gradient(135deg,var(--bg-start) 0%,#F5EAEB 50%,var(--bg-end) 100%);color:var(--text);min-height:100vh;padding:24px 16px;line-height:1.6;display:flex;align-items:center;justify-content:center}
.app-window{width:100%;max-width:580px;background:var(--card-bg);backdrop-filter:blur(16px);-webkit-backdrop-filter:blur(16px);border:1px solid var(--card-border);border-radius:var(--radius);padding:32px 28px;box-shadow:var(--shadow)}
.header{display:flex;align-items:center;justify-content:space-between;margin-bottom:24px;border-bottom:1.5px solid rgba(107,15,26,0.08);padding-bottom:16px}
.title{font-size:1.3rem;font-weight:800;color:var(--burg-dark);display:flex;align-items:center;gap:10px}
.subtitle{font-size:0.82rem;color:var(--text-sub);margin-top:2px}
.profile-list{display:flex;flex-direction:column;gap:12px;margin-bottom:24px}
.profile{background:#fff;border:1.5px solid rgba(232,197,201,0.6);border-radius:14px;padding:14px 18px;display:flex;align-items:center;justify-content:space-between;transition:all 0.2s}
.profile:hover{border-color:var(--burg);box-shadow:0 6px 20px rgba(107,15,26,0.08)}
.profile .name{font-size:1rem;font-weight:800;color:var(--burg-dark)}
.actions{display:flex;gap:10px}
.actions a{text-decoration:none;font-size:0.82rem;font-weight:700;padding:6px 12px;border-radius:8px;transition:all 0.2s}
.actions a.edit{background:rgba(107,15,26,0.08);color:var(--burg)}
.actions a.edit:hover{background:var(--burg);color:#fff}
.actions a.delete{background:rgba(192,57,43,0.08);color:#C0392B}
.actions a.delete:hover{background:#C0392B;color:#fff}
.btn-new{display:inline-flex;align-items:center;justify-content:center;gap:8px;width:100%;padding:14px;font-size:0.95rem;font-weight:700;font-family:inherit;color:#fff;background:linear-gradient(135deg,var(--burg),var(--accent));border-radius:12px;text-decoration:none;box-shadow:0 6px 20px rgba(107,15,26,0.22);transition:all 0.2s}
.btn-new:hover{transform:translateY(-2px);box-shadow:0 8px 24px rgba(107,15,26,0.32)}
.back-link{display:inline-flex;align-items:center;gap:6px;margin-top:20px;font-size:0.88rem;font-weight:700;color:var(--burg);text-decoration:none}
.back-link:hover{color:var(--accent);text-decoration:underline}
.empty{color:var(--text-sub);font-style:italic;text-align:center;padding:20px;background:rgba(255,255,255,0.5);border-radius:12px}
</style>
</head>
<body>
<div class="app-window">
<div class="header">
<div>
<div class="title">👤 Patient Profiles</div>
<div class="subtitle">EEPROM Persistent Medical Profiles</div>
</div>
</div>
<div class="profile-list">
%PROFILES%
</div>
<a class="btn-new" href="/profile/edit?new=1">+ Create New Patient Profile</a>
<a class="back-link" href="/">← Back to Control Portal</a>
</div>
</body>
</html>
)rawliteral";

const char PROFILE_EDIT_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Edit Profile — PocketDoctor</title>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link href="https://fonts.googleapis.com/css2?family=Plus+Jakarta+Sans:wght@400;500;600;700;800&display=swap" rel="stylesheet">
<style>
:root{--burg:#6B0F1A;--burg-dark:#4A0A12;--accent:#D4384B;--bg-start:#FDF6F7;--bg-end:#E8D8DA;--card-bg:rgba(255,255,255,0.88);--card-border:rgba(232,197,201,0.6);--shadow:0 12px 36px rgba(107,15,26,0.08);--text:#2D0A0E;--text-sub:#7A4048;--radius:20px}
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:'Plus Jakarta Sans',system-ui,sans-serif;background:linear-gradient(135deg,var(--bg-start) 0%,#F5EAEB 50%,var(--bg-end) 100%);color:var(--text);min-height:100vh;padding:24px 16px;line-height:1.6;display:flex;align-items:center;justify-content:center}
.app-window{width:100%;max-width:520px;background:var(--card-bg);backdrop-filter:blur(16px);-webkit-backdrop-filter:blur(16px);border:1px solid var(--card-border);border-radius:var(--radius);padding:32px 28px;box-shadow:var(--shadow)}
.header{margin-bottom:24px;border-bottom:1.5px solid rgba(107,15,26,0.08);padding-bottom:14px}
.title{font-size:1.3rem;font-weight:800;color:var(--burg-dark)}
.form-grid{display:grid;grid-template-columns:1fr 1fr;gap:14px}
.full-width{grid-column:span 2}
.form-group{margin-bottom:14px}
label{display:block;font-size:0.82rem;font-weight:700;color:var(--burg-dark);margin-bottom:5px}
input,select{width:100%;padding:10px 14px;font-size:0.92rem;font-family:inherit;background:rgba(255,255,255,0.9);border:1.5px solid rgba(232,197,201,0.8);border-radius:12px;color:var(--text);transition:all 0.2s}
input:focus,select:focus{outline:none;border-color:var(--burg);box-shadow:0 0 0 4px rgba(107,15,26,0.12);background:#fff}
.btn-submit{width:100%;margin-top:12px;padding:14px;font-size:0.95rem;font-weight:700;font-family:inherit;color:#fff;background:linear-gradient(135deg,var(--burg),var(--accent));border:none;border-radius:12px;cursor:pointer;box-shadow:0 6px 20px rgba(107,15,26,0.25);transition:all 0.2s}
.btn-submit:hover{transform:translateY(-2px);box-shadow:0 8px 24px rgba(107,15,26,0.35)}
.back-link{display:inline-flex;align-items:center;gap:6px;margin-top:18px;font-size:0.88rem;font-weight:700;color:var(--burg);text-decoration:none}
.back-link:hover{color:var(--accent);text-decoration:underline}
</style>
</head>
<body>
<div class="app-window">
<div class="header">
<div class="title">%HEADER%</div>
</div>
<form action="/profile/update" method="POST">
<input type="hidden" name="index" value="%INDEX%">
<div class="form-group full-width">
<label>Patient Full Name</label>
<input type="text" name="name" value="%NAME%" required placeholder="Enter full name">
</div>
<div class="form-grid full-width">
<div class="form-group">
<label>Age (years)</label>
<input type="number" name="age" value="%AGE%" min="1" max="120" required>
</div>
<div class="form-group">
<label>Height (cm)</label>
<input type="number" name="height" value="%HEIGHT%" min="50" max="250" required>
</div>
</div>
<div class="form-group full-width">
<label>Weight (kg)</label>
<input type="number" name="weight" value="%WEIGHT%" min="20" max="300" required>
</div>
<div class="form-group full-width">
<label>Intoxication History</label>
<select name="intox">
<option value="None" %SEL_NONE%>None</option>
<option value="Alcohol" %SEL_ALCOHOL%>Alcohol</option>
<option value="Tobacco" %SEL_TOBACCO%>Tobacco</option>
<option value="Both" %SEL_BOTH%>Both</option>
</select>
</div>
<div class="form-group full-width">
<label>Chronic Medical History</label>
<select name="history">
<option value="1" %SEL_HIST_YES%>Yes</option>
<option value="0" %SEL_HIST_NO%>No</option>
</select>
</div>
<div class="form-group full-width">
<label>Preferred Medical Modality</label>
<select name="treatment">
<option value="Allopathy" %SEL_ALLOPATHY%>Allopathy</option>
<option value="Homeopathy" %SEL_HOMEOPATHY%>Homeopathy</option>
<option value="Ayurvedic" %SEL_AYURVEDIC%>Ayurvedic</option>
<option value="All types" %SEL_ALLTYPES%>All types</option>
</select>
</div>
<button type="submit" class="btn-submit full-width">Save Profile Record</button>
</form>
<a class="back-link" href="/profiles">← Back to Patient Profiles</a>
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

void handleSOS() {
  String email = wifiConfigValid ? String(wifiConfig.sosEmail) : "";
  String formid = wifiConfigValid ? String(wifiConfig.sosFormId) : "";
  String html = FPSTR(SOS_HTML);
  html.replace("%EMAIL%", email);
  html.replace("%FORMID%", formid);
  server.send(200, "text/html", html);
}

void handleSOSUpdate() {
  if (server.hasArg("email")) {
    String email = server.arg("email");
    email.trim();
    String formid = server.hasArg("formid") ? server.arg("formid") : "";
    formid.trim();
    if (email.length() > 0) {
      saveSOSEmail(email.c_str(), formid.c_str());
      server.sendHeader("Location", "/");
      server.send(303, "text/plain", "");
    } else {
      server.send(400, "text/plain", "Email cannot be empty");
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
  server.on("/sos", handleSOS);
  server.on("/sos/update", HTTP_POST, handleSOSUpdate);
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
    String finalInstruction = "";
    if (isDailyCheckup) {
      finalInstruction = "Based on the daily checkup, provide a SHORT summary in this EXACT format:\n\nStatus: [Safe/Needs Attention]\nRecovery Score: [0-100]%\nDaily Advice: [2-3 short tips]\nNext Steps: [Action to take]\nCritical: [Yes/No]\n\nKeep it VERY brief. Follow this format exactly.";
    } else {
      finalInstruction = "Based on all my answers and the initial symptoms I provided, provide a SHORT preliminary diagnosis in this EXACT format:\n\nDiagnosed Disease: [disease name only]\nAccuracy: [percentage]%\nMedicines: [2-3 medicine names only, no dosages]\nPrecautions: [2-3 key precautions in one short sentence]\nCritical: [Yes/No]\n\nKeep it VERY brief. Follow this format exactly.";
    }
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
  String html = "<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<title>PocketDoctor Mark 4 — Clinical Prescription & Triage Report</title>";
  html += "<link rel='preconnect' href='https://fonts.googleapis.com'><link rel='preconnect' href='https://fonts.gstatic.com' crossorigin>";
  html += "<link href='https://fonts.googleapis.com/css2?family=Outfit:wght@400;500;600;700;800&family=Plus+Jakarta+Sans:wght@400;500;600;700;800&display=swap' rel='stylesheet'>";
  html += "<style>";
  html += "*{margin:0;padding:0;box-sizing:border-box}";
  html += ":root{--burg:#5A0C16;--burg-mid:#7A1422;--burg-bright:#9E1B2C;--accent:#E63946;--cream:#FDF8F8;--bg-canvas:#F3E9EA;--card-bg:rgba(255,255,255,0.92);--card-border:rgba(158,27,44,0.18);--shadow:0 16px 48px rgba(90,12,22,0.12);--text:#2B090D;--text-sub:#6B3037;--gold:#D97706;--green:#15803D;--red:#DC2626;--radius:22px;}";
  html += "html{scroll-behavior:smooth}";
  html += "body{font-family:'Plus Jakarta Sans',system-ui,sans-serif;background:linear-gradient(135deg,var(--cream) 0%,#F0E2E4 50%,var(--bg-canvas) 100%);color:var(--text);line-height:1.7;min-height:100vh;padding:36px 18px 70px}";
  html += ".page{max-width:980px;margin:0 auto}";
  html += ".rx-header{background:linear-gradient(135deg,var(--burg) 0%,var(--burg-mid) 60%,var(--burg-bright) 100%);border-radius:var(--radius);padding:44px 40px 36px;margin-bottom:30px;position:relative;overflow:hidden;box-shadow:0 14px 40px rgba(90,12,22,0.32);color:#fff}";
  html += ".rx-header::before{content:'Rx';position:absolute;right:20px;bottom:-15px;font-family:'Outfit',sans-serif;font-size:12rem;font-weight:800;color:rgba(255,255,255,0.06);pointer-events:none;line-height:1}";
  html += ".header-top{display:flex;align-items:center;justify-content:space-between;gap:20px;margin-bottom:24px;border-bottom:1px solid rgba(255,255,255,0.15);padding-bottom:20px}";
  html += ".rx-badge{display:flex;align-items:center;gap:16px}";
  html += ".rx-logo{width:64px;height:64px;border-radius:20px;background:rgba(255,255,255,0.15);border:2px solid rgba(255,255,255,0.35);display:flex;align-items:center;justify-content:center;font-size:30px;backdrop-filter:blur(8px);box-shadow:0 8px 24px rgba(0,0,0,0.2)}";
  html += ".rx-header h1{font-family:'Outfit',sans-serif;font-size:clamp(1.6rem,4vw,2.4rem);font-weight:800;letter-spacing:-0.5px;color:#fff}";
  html += ".rx-header-sub{color:rgba(255,255,255,0.85);font-size:0.86rem;letter-spacing:1.5px;text-transform:uppercase;margin-top:2px;font-weight:700}";
  html += ".doc-stamp{background:rgba(255,255,255,0.12);border:1px solid rgba(255,255,255,0.25);border-radius:14px;padding:10px 18px;text-align:right;backdrop-filter:blur(6px)}";
  html += ".stamp-title{font-size:0.72rem;text-transform:uppercase;letter-spacing:1.2px;color:rgba(255,255,255,0.75);font-weight:700}";
  html += ".stamp-val{font-size:0.95rem;font-weight:800;color:#fff;font-family:monospace}";
  html += ".patient-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(140px,1fr));gap:12px;margin-top:16px}";
  html += ".p-chip{background:rgba(255,255,255,0.14);border:1px solid rgba(255,255,255,0.22);border-radius:30px;padding:8px 16px;font-size:0.84rem;color:#fff;font-weight:600;backdrop-filter:blur(6px);display:flex;align-items:center;gap:8px}";
  html += ".summary-row{display:grid;grid-template-columns:repeat(auto-fit,minmax(210px,1fr));gap:18px;margin-bottom:30px}";
  html += ".scard{background:var(--card-bg);backdrop-filter:blur(16px);border:1px solid var(--card-border);border-radius:var(--radius);padding:24px 22px;box-shadow:var(--shadow);transition:all 0.25s cubic-bezier(0.4,0,0.2,1);border-top:4px solid var(--burg)}";
  html += ".scard:hover{transform:translateY(-4px);box-shadow:0 20px 48px rgba(90,12,22,0.16)}";
  html += ".scard-label{font-size:0.75rem;text-transform:uppercase;letter-spacing:1.2px;color:var(--text-sub);margin-bottom:8px;font-weight:700}";
  html += ".scard-value{font-family:'Outfit',sans-serif;font-size:1.35rem;font-weight:800;color:var(--burg)}.scard-value.red{color:var(--red)}.scard-value.green{color:var(--green)}.scard-value.gold{color:var(--gold)}";
  html += ".scard-icon{font-size:1.8rem;margin-bottom:12px}";
  html += ".critical-banner{background:linear-gradient(135deg,rgba(220,38,38,0.12),rgba(220,38,38,0.04));border:1.5px solid var(--red);border-radius:var(--radius);padding:24px 28px;margin-bottom:30px;display:flex;align-items:flex-start;gap:20px;box-shadow:0 10px 32px rgba(220,38,38,0.15)}";
  html += ".critical-banner .icon{font-size:2.4rem;flex-shrink:0;animation:pulse 1.5s infinite}";
  html += "@keyframes pulse{0%,100%{opacity:1;transform:scale(1)}50%{opacity:0.4;transform:scale(0.9)} }";
  html += ".critical-banner h3{color:var(--red);font-size:1.15rem;font-weight:800;margin-bottom:4px}.critical-banner p{color:var(--text);font-size:0.94rem;font-weight:500}";
  html += ".section{background:var(--card-bg);backdrop-filter:blur(16px);border:1px solid var(--card-border);border-radius:var(--radius);padding:32px 30px;margin-bottom:26px;box-shadow:var(--shadow)}";
  html += ".section-header{display:flex;align-items:center;gap:16px;margin-bottom:24px;padding-bottom:16px;border-bottom:1.5px solid rgba(90,12,22,0.08)}";
  html += ".section-icon{width:46px;height:46px;border-radius:14px;background:linear-gradient(135deg,var(--burg),var(--burg-bright));display:flex;align-items:center;justify-content:center;font-size:1.3rem;color:#fff;flex-shrink:0;box-shadow:0 6px 18px rgba(90,12,22,0.22)}";
  html += ".section h2{font-family:'Outfit',sans-serif;font-size:1.25rem;font-weight:800;color:var(--burg)}.section h3{font-size:0.9rem;font-weight:700;color:var(--burg-mid);margin:22px 0 12px;text-transform:uppercase;letter-spacing:1px}";
  html += ".box-burg{background:rgba(90,12,22,0.05);border:1px solid rgba(90,12,22,0.18);border-left:4px solid var(--burg);border-radius:14px;padding:18px 22px;margin:14px 0}";
  html += ".box-gold{background:rgba(217,119,6,0.08);border:1px solid rgba(217,119,6,0.3);border-left:4px solid var(--gold);border-radius:14px;padding:18px 22px;margin:14px 0;color:#92400E}";
  html += ".box-red{background:rgba(220,38,38,0.08);border:1px solid rgba(220,38,38,0.3);border-left:4px solid var(--red);border-radius:14px;padding:18px 22px;margin:14px 0}";
  html += ".box-green{background:rgba(21,128,61,0.08);border:1px solid rgba(21,128,61,0.25);border-left:4px solid var(--green);border-radius:14px;padding:18px 22px;margin:14px 0;color:var(--green)}";
  html += "ul.styled{list-style:none;padding:0;margin:10px 0}ul.styled li{padding:10px 0 10px 30px;position:relative;font-size:0.94rem;color:var(--text);border-bottom:1px solid rgba(90,12,22,0.06)}ul.styled li:last-child{border-bottom:none}ul.styled li::before{content:'▸';position:absolute;left:4px;color:var(--burg-bright);font-size:1.1rem;font-weight:800;top:8px}ul.red-list li::before{color:var(--red)}";
  html += "table.rx-table{width:100%;border-collapse:separate;border-spacing:0;margin:16px 0;font-size:0.92rem;border-radius:14px;overflow:hidden;border:1px solid rgba(90,12,22,0.12)}table.rx-table thead th{background:var(--burg);color:#fff;padding:14px 18px;text-align:left;font-family:'Outfit',sans-serif;font-weight:700;letter-spacing:0.5px;font-size:0.82rem;text-transform:uppercase}table.rx-table tbody td{padding:14px 18px;border-bottom:1px solid rgba(90,12,22,0.06);color:var(--text);vertical-align:top;background:#fff}table.rx-table tbody tr:last-child td{border-bottom:none}table.rx-table tbody tr:hover td{background:rgba(90,12,22,0.03)}";
  html += ".timeline{display:flex;flex-direction:column;gap:0;margin:16px 0}.tl-item{display:flex;gap:20px;align-items:flex-start;padding:16px 0;border-bottom:1px solid rgba(90,12,22,0.06)}.tl-item:last-child{border-bottom:none}.tl-dot{width:14px;height:14px;border-radius:50%;background:var(--burg-bright);flex-shrink:0;margin-top:6px;box-shadow:0 0 12px rgba(158,27,44,0.5)}.tl-content .tl-time{font-family:'Outfit',sans-serif;font-size:0.82rem;color:var(--burg);font-weight:800;text-transform:uppercase;letter-spacing:1px}.tl-content .tl-text{font-size:0.92rem;color:var(--text);margin-top:3px}";
  html += ".acc-bar-wrap{background:rgba(90,12,22,0.08);border-radius:30px;height:14px;margin:10px 0;overflow:hidden;border:1px solid rgba(90,12,22,0.15)}.acc-bar{height:100%;border-radius:30px;background:linear-gradient(90deg,var(--burg),var(--burg-bright));transition:width 1s ease}";
  html += ".attestation{background:rgba(90,12,22,0.03);border:1.5px solid rgba(90,12,22,0.15);border-radius:var(--radius);padding:28px;margin-top:34px;display:flex;flex-direction:column;gap:14px}";
  html += ".att-title{font-family:'Outfit',sans-serif;font-size:1.05rem;font-weight:800;color:var(--burg);display:flex;align-items:center;gap:10px}";
  html += ".att-body{font-size:0.86rem;color:var(--text-sub);line-height:1.75}";
  html += ".sig-block{margin-top:12px;padding-top:14px;border-top:1px dashed rgba(90,12,22,0.2);display:flex;align-items:center;justify-content:space-between;font-size:0.82rem;color:var(--text-sub);font-weight:700}";
  html += ".footer{text-align:center;margin-top:44px;color:var(--text-sub);font-size:0.84rem;letter-spacing:0.5px;font-weight:600}.footer span{color:var(--burg);font-weight:800}";
  html += "@media(max-width:600px){.rx-header{padding:30px 22px}.section{padding:24px 20px}.summary-row{grid-template-columns:1fr 1fr}}";
  html += "</style></head><body><div class='page'>";

  html += "<div class='rx-header'><div class='header-top'><div class='rx-badge'><div class='rx-logo'>🩺</div><div><h1>Prescription & Triage Report</h1><div class='rx-header-sub'>PocketDoctor Mark 4 Autonomous Clinical Triage</div></div></div><div class='doc-stamp'><div class='stamp-title'>System Model</div><div class='stamp-val'>MK4-LLAMA-3.3</div></div></div>";
  html += "<div class='patient-grid'><div class='p-chip'>👤 <strong>" + currentProfile.name + "</strong></div><div class='p-chip'>🎂 <strong>" + String(currentProfile.age) + " yrs</strong></div><div class='p-chip'>⚖️ <strong>" + String(currentProfile.weight) + " kg</strong></div><div class='p-chip'>📏 <strong>" + String(currentProfile.height) + " cm</strong></div><div class='p-chip'>💊 <strong>" + currentProfile.treatmentPreference + "</strong></div></div></div>";

  html += "<div class='summary-row'>";
  html += "<div class='scard'><div class='scard-icon'>🔬</div><div class='scard-label'>Primary Diagnosis</div><div class='scard-value'>" + diagnosedDisease + "</div></div>";
  html += "<div class='scard'><div class='scard-icon'>📊</div><div class='scard-label'>AI Confidence</div><div class='scard-value gold'>" + accuracy + "</div></div>";
  bool isCritical = critical.indexOf("Yes") >= 0;
  html += "<div class='scard'><div class='scard-icon'>" + String(isCritical ? "🚨" : "✅") + "</div><div class='scard-label'>Priority Status</div><div class='scard-value " + String(isCritical ? "red" : "green") + "'>" + String(isCritical ? "URGENT TRIAGE" : "STANDARD CARE") + "</div></div>";
  html += "<div class='scard'><div class='scard-icon'>💊</div><div class='scard-label'>Modality</div><div class='scard-value'>" + currentProfile.treatmentPreference + "</div></div>";
  html += "</div>";

  if (isCritical) {
    html += "<div class='critical-banner'><div class='icon'>🚨</div><div><h3>CRITICAL TRIAGE ALERT — EMERGENCY CARE RECOMMENDED</h3><p>Patient symptoms indicate severe condition risk. Please proceed immediately to an emergency care facility or hospital for physical evaluation.</p></div></div>";
  }

  html += "<div class='section'><div class='section-header'><div class='section-icon'>🔬</div><h2>Clinical Diagnostic Assessment</h2></div>";
  html += "<h3>Primary Condition Identified</h3><div class='box-burg'><strong style='color:var(--burg);font-size:1.1rem'>" + diagnosedDisease + "</strong> &nbsp;—&nbsp; <span style='color:var(--text)'>Derived from symptom intake, EEPROM profile biometrics, and multi-turn LLM clinical reasoning.</span></div>";
  html += "<h3>AI Confidence Matrix</h3><div style='display:flex;align-items:center;gap:16px'><div class='acc-bar-wrap' style='flex:1'><div class='acc-bar' style='width:" + accuracy + "'></div></div><span style='color:var(--gold);font-weight:800;font-size:1.1rem;font-family:monospace'>" + accuracy + "</span></div>";
  html += "<p style='font-size:0.84rem;color:var(--text-sub);margin-top:10px'>Confidence score derived from symptom clarity, consistency of responses, and medical KB alignment.</p></div>";

  html += "<div class='section'><div class='section-header'><div class='section-icon'>💊</div><h2>Rx Prescribed Medication Protocol</h2></div>";
  html += "<div class='box-burg'><span style='color:var(--burg);font-weight:800'>Prescribed Medications (" + currentProfile.treatmentPreference + "):</span><br><span style='color:var(--burg);font-size:1.1rem;font-weight:800'>" + medicines + "</span></div>";
  html += "<h3>Rx Administration Schedule</h3><table class='rx-table'><thead><tr><th>Parameter</th><th>Clinical Recommendation</th></tr></thead><tbody>";
  html += "<tr><td>Dosing Schedule</td><td>Administer at regular intervals to maintain steady blood plasma levels</td></tr>";
  html += "<tr><td>Dietary Timing</td><td>Consume alongside meals unless contraindicated on prescription packaging</td></tr>";
  html += "<tr><td>Missed Dose Protocol</td><td>Administer upon recollection; do not double dosage under any circumstance</td></tr>";
  html += "<tr><td>Storage Conditions</td><td>Store in a cool, dry environment away from direct heat and moisture</td></tr>";
  html += "<tr><td>Course Duration</td><td>Complete the full course as recommended by your physician</td></tr>";
  html += "</tbody></table><h3>Side Effect Surveillance</h3><ul class='styled'><li>Anaphylactic signals — hives, dyspnea, facial/throat swelling</li><li>Severe vertigo, syncopal episodes, or motor unsteadiness</li><li>Unexplained lethargy, confusion, or disorientation</li><li>Gastrointestinal bleeding, persistent emesis, or severe abdominal pain</li></ul></div>";

  html += "<div class='section'><div class='section-header'><div class='section-icon'>⚡</div><h2>Essential Patient Precautions</h2></div>";
  html += "<div class='box-gold'>⚠️ &nbsp;" + precautions + "</div>";
  html += "<h3>Safety Directives</h3><ul class='styled'><li>Do not initiate additional non-prescribed pharmaceuticals without medical consultation</li><li>Disclose full medication history and known allergies to attending clinicians</li><li>Maintain infection control protocols and personal hygiene</li><li>Do not reassign or share prescribed pharmaceuticals</li></ul></div>";

  html += "<div class='section'><div class='section-header'><div class='section-icon'>🥗</div><h2>Dietary, Hydration & Activity Plan</h2></div>";
  html += "<h3>Physical Activity Limits</h3><ul class='styled'><li>Light ambulation (20–30 min daily) as tolerated</li><li>Avoid intense exertion, heavy lifting, or prolonged standing during acute phase</li><li>Prioritize physical rest when fatigued</li></ul>";
  html += "<h3>Sleep Hygiene</h3><ul class='styled'><li>Maintain 7–9 hours of sleep per night to support tissue recovery</li><li>Keep dark, quiet, temperature-regulated sleep environment</li></ul>";
  html += "<h3>Nutritional Guidelines</h3><ul class='styled'><li>Emphasize antioxidant-dense fruits, vegetables, and lean protein sources</li><li>Avoid processed, high-sodium, or fried foods</li><li>Limit caffeine, alcohol, and refined sugars</li></ul>";
  html += "<h3>Daily Hydration Goal</h3><div class='box-green'>💧 &nbsp;<strong>Target: 2.0 – 2.5 Litres daily fluid intake.</strong> Increase fluid intake if pyrexia, emesis, or diarrhoea is present.</div></div>";

  html += "<div class='section'><div class='section-header'><div class='section-icon'>🚨</div><h2>Emergency Red Flag Indicators</h2></div>";
  html += "<h3>Seek Immediate Emergency Services (ER) If:</h3><div class='box-red'><ul class='styled red-list'>";
  html += "<li>Severe dyspnea or acute respiratory distress at rest</li><li>Central chest pain, tightness, or pressure radiating to arm/jaw</li><li>Sudden severe thunderclap headache</li><li>Loss of consciousness, syncope, or acute confusion</li><li>High fever (>39.4°C / 103°F) refractory to antipyretics</li><li>Rapidly progressive allergic rash or airway compromise</li>";
  html += "</ul></div><h3>Contact Primary Care Physician Same-Day If:</h3><ul class='styled'><li>Symptoms rapidly deteriorate despite therapy</li><li>Fever persists beyond 72 hours</li><li>Inability to retain oral fluids</li></ul></div>";

  html += "<div class='section'><div class='section-header'><div class='section-icon'>📅</div><h2>Structured Recovery Timeline</h2></div><div class='timeline'>";
  html += "<div class='tl-item'><div class='tl-dot'></div><div class='tl-content'><div class='tl-time'>Stage 1: Days 1–3</div><div class='tl-text'>Initiate prescribed protocol. Rest, hydrate, and track initial symptom response.</div></div></div>";
  html += "<div class='tl-item'><div class='tl-dot'></div><div class='tl-content'><div class='tl-time'>Stage 2: Days 3–5</div><div class='tl-text'>Re-evaluate condition. Early resolution expected. Contact physician if symptoms escalate.</div></div></div>";
  html += "<div class='tl-item'><div class='tl-dot'></div><div class='tl-content'><div class='tl-time'>Stage 3: Week 1–2</div><div class='tl-text'>Clinical follow-up if incomplete recovery. Present this report to attending physician.</div></div></div>";
  html += "<div class='tl-item'><div class='tl-dot'></div><div class='tl-content'><div class='tl-time'>Stage 4: Week 2–4</div><div class='tl-text'>Conclude recovery phase and maintain preventative lifestyle guidelines.</div></div></div>";
  html += "</div></div>";

  html += "<div class='attestation'><div class='att-title'>📜 Official AI Triage Attestation & Medical Disclaimer</div>";
  html += "<div class='att-body'><p>This prescription and triage summary is generated by <strong>PocketDoctor Mark 4</strong> utilizing Groq LLaMA-3.3 70B inference. This document is intended solely for preliminary clinical decision-support and does <strong>NOT</strong> substitute for formal physical examination, laboratory diagnostic testing, or professional medical advice.</p><p style='margin-top:6px'>In a medical emergency, contact emergency medical services immediately.</p></div>";
  html += "<div class='sig-block'><span>Creator & Lead Engineer: <strong>Vaidik Khurana</strong></span><span>System: PocketDoctor Mark 4</span></div></div>";

  html += "<div class='footer'>Generated by <span>PocketDoctor Mark 4</span> · Designed & Engineered by <span>Vaidik Khurana</span> · For informational use only</div>";
  html += "</div></body></html>";
  return html;
}

String createDailyCheckupHTMLReport() {
  String html = "<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<title>PocketDoctor Mark 5 — Daily Checkup Report</title>";
  html += "<link rel='preconnect' href='https://fonts.googleapis.com'><link rel='preconnect' href='https://fonts.gstatic.com' crossorigin>";
  html += "<link href='https://fonts.googleapis.com/css2?family=Outfit:wght@400;500;600;700;800&family=Plus+Jakarta+Sans:wght@400;500;600;700;800&display=swap' rel='stylesheet'>";
  html += "<style>";
  html += "*{margin:0;padding:0;box-sizing:border-box}";
  html += ":root{--main:#0284C7;--main-mid:#0369A1;--main-bright:#0EA5E9;--accent:#38BDF8;--cream:#F0F9FF;--bg-canvas:#E0F2FE;--card-bg:rgba(255,255,255,0.92);--card-border:rgba(2,132,199,0.18);--shadow:0 16px 48px rgba(2,132,199,0.12);--text:#0C4A6E;--text-sub:#0369A1;--gold:#D97706;--green:#15803D;--red:#DC2626;--radius:22px;}";
  html += "html{scroll-behavior:smooth}";
  html += "body{font-family:'Plus Jakarta Sans',system-ui,sans-serif;background:linear-gradient(135deg,var(--cream) 0%,#E0F2FE 50%,var(--bg-canvas) 100%);color:var(--text);line-height:1.7;min-height:100vh;padding:36px 18px 70px}";
  html += ".page{max-width:980px;margin:0 auto}";
  html += ".rx-header{background:linear-gradient(135deg,var(--main) 0%,var(--main-mid) 60%,var(--main-bright) 100%);border-radius:var(--radius);padding:44px 40px 36px;margin-bottom:30px;position:relative;overflow:hidden;box-shadow:0 14px 40px rgba(2,132,199,0.32);color:#fff}";
  html += ".rx-header::before{content:'Daily';position:absolute;right:20px;bottom:-15px;font-family:'Outfit',sans-serif;font-size:12rem;font-weight:800;color:rgba(255,255,255,0.06);pointer-events:none;line-height:1}";
  html += ".header-top{display:flex;align-items:center;justify-content:space-between;gap:20px;margin-bottom:24px;border-bottom:1px solid rgba(255,255,255,0.15);padding-bottom:20px}";
  html += ".rx-badge{display:flex;align-items:center;gap:16px}";
  html += ".rx-logo{width:64px;height:64px;border-radius:20px;background:rgba(255,255,255,0.15);border:2px solid rgba(255,255,255,0.35);display:flex;align-items:center;justify-content:center;font-size:30px;backdrop-filter:blur(8px);box-shadow:0 8px 24px rgba(0,0,0,0.2)}";
  html += ".rx-header h1{font-family:'Outfit',sans-serif;font-size:clamp(1.6rem,4vw,2.4rem);font-weight:800;letter-spacing:-0.5px;color:#fff}";
  html += ".rx-header-sub{color:rgba(255,255,255,0.85);font-size:0.86rem;letter-spacing:1.5px;text-transform:uppercase;margin-top:2px;font-weight:700}";
  html += ".doc-stamp{background:rgba(255,255,255,0.12);border:1px solid rgba(255,255,255,0.25);border-radius:14px;padding:10px 18px;text-align:right;backdrop-filter:blur(6px)}";
  html += ".stamp-title{font-size:0.72rem;text-transform:uppercase;letter-spacing:1.2px;color:rgba(255,255,255,0.75);font-weight:700}";
  html += ".stamp-val{font-size:0.95rem;font-weight:800;color:#fff;font-family:monospace}";
  html += ".patient-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(140px,1fr));gap:12px;margin-top:16px}";
  html += ".p-chip{background:rgba(255,255,255,0.14);border:1px solid rgba(255,255,255,0.22);border-radius:30px;padding:8px 16px;font-size:0.84rem;color:#fff;font-weight:600;backdrop-filter:blur(6px);display:flex;align-items:center;gap:8px}";
  html += ".summary-row{display:grid;grid-template-columns:repeat(auto-fit,minmax(210px,1fr));gap:18px;margin-bottom:30px}";
  html += ".scard{background:var(--card-bg);backdrop-filter:blur(16px);border:1px solid var(--card-border);border-radius:var(--radius);padding:24px 22px;box-shadow:var(--shadow);transition:all 0.25s cubic-bezier(0.4,0,0.2,1);border-top:4px solid var(--main)}";
  html += ".scard:hover{transform:translateY(-4px);box-shadow:0 20px 48px rgba(2,132,199,0.16)}";
  html += ".scard-label{font-size:0.75rem;text-transform:uppercase;letter-spacing:1.2px;color:var(--text-sub);margin-bottom:8px;font-weight:700}";
  html += ".scard-value{font-family:'Outfit',sans-serif;font-size:1.35rem;font-weight:800;color:var(--main)}.scard-value.red{color:var(--red)}.scard-value.green{color:var(--green)}.scard-value.gold{color:var(--gold)}";
  html += ".scard-icon{font-size:1.8rem;margin-bottom:12px}";
  html += ".critical-banner{background:linear-gradient(135deg,rgba(220,38,38,0.12),rgba(220,38,38,0.04));border:1.5px solid var(--red);border-radius:var(--radius);padding:24px 28px;margin-bottom:30px;display:flex;align-items:flex-start;gap:20px;box-shadow:0 10px 32px rgba(220,38,38,0.15)}";
  html += ".critical-banner .icon{font-size:2.4rem;flex-shrink:0;animation:pulse 1.5s infinite}";
  html += "@keyframes pulse{0%,100%{opacity:1;transform:scale(1)}50%{opacity:0.4;transform:scale(0.9)} }";
  html += ".critical-banner h3{color:var(--red);font-size:1.15rem;font-weight:800;margin-bottom:4px}.critical-banner p{color:var(--text);font-size:0.94rem;font-weight:500}";
  html += ".section{background:var(--card-bg);backdrop-filter:blur(16px);border:1px solid var(--card-border);border-radius:var(--radius);padding:32px 30px;margin-bottom:26px;box-shadow:var(--shadow)}";
  html += ".section-header{display:flex;align-items:center;gap:16px;margin-bottom:24px;padding-bottom:16px;border-bottom:1.5px solid rgba(2,132,199,0.08)}";
  html += ".section-icon{width:46px;height:46px;border-radius:14px;background:linear-gradient(135deg,var(--main),var(--main-bright));display:flex;align-items:center;justify-content:center;font-size:1.3rem;color:#fff;flex-shrink:0;box-shadow:0 6px 18px rgba(2,132,199,0.22)}";
  html += ".section h2{font-family:'Outfit',sans-serif;font-size:1.25rem;font-weight:800;color:var(--main)}.section h3{font-size:0.9rem;font-weight:700;color:var(--main-mid);margin:22px 0 12px;text-transform:uppercase;letter-spacing:1px}";
  html += ".box-main{background:rgba(2,132,199,0.05);border:1px solid rgba(2,132,199,0.18);border-left:4px solid var(--main);border-radius:14px;padding:18px 22px;margin:14px 0}";
  html += ".box-gold{background:rgba(217,119,6,0.08);border:1px solid rgba(217,119,6,0.3);border-left:4px solid var(--gold);border-radius:14px;padding:18px 22px;margin:14px 0;color:#92400E}";
  html += ".box-red{background:rgba(220,38,38,0.08);border:1px solid rgba(220,38,38,0.3);border-left:4px solid var(--red);border-radius:14px;padding:18px 22px;margin:14px 0}";
  html += ".box-green{background:rgba(21,128,61,0.08);border:1px solid rgba(21,128,61,0.25);border-left:4px solid var(--green);border-radius:14px;padding:18px 22px;margin:14px 0;color:var(--green)}";
  html += "ul.styled{list-style:none;padding:0;margin:10px 0}ul.styled li{padding:10px 0 10px 30px;position:relative;font-size:0.94rem;color:var(--text);border-bottom:1px solid rgba(2,132,199,0.06)}ul.styled li:last-child{border-bottom:none}ul.styled li::before{content:'▸';position:absolute;left:4px;color:var(--main-bright);font-size:1.1rem;font-weight:800;top:8px}ul.red-list li::before{color:var(--red)}";
  html += ".acc-bar-wrap{background:rgba(2,132,199,0.08);border-radius:30px;height:14px;margin:10px 0;overflow:hidden;border:1px solid rgba(2,132,199,0.15)}.acc-bar{height:100%;border-radius:30px;background:linear-gradient(90deg,var(--main),var(--main-bright));transition:width 1s ease}";
  html += ".attestation{background:rgba(2,132,199,0.03);border:1.5px solid rgba(2,132,199,0.15);border-radius:var(--radius);padding:28px;margin-top:34px;display:flex;flex-direction:column;gap:14px}";
  html += ".att-title{font-family:'Outfit',sans-serif;font-size:1.05rem;font-weight:800;color:var(--main);display:flex;align-items:center;gap:10px}";
  html += ".att-body{font-size:0.86rem;color:var(--text-sub);line-height:1.75}";
  html += ".sig-block{margin-top:12px;padding-top:14px;border-top:1px dashed rgba(2,132,199,0.2);display:flex;align-items:center;justify-content:space-between;font-size:0.82rem;color:var(--text-sub);font-weight:700}";
  html += ".footer{text-align:center;margin-top:44px;color:var(--text-sub);font-size:0.84rem;letter-spacing:0.5px;font-weight:600}.footer span{color:var(--main);font-weight:800}";
  html += "@media(max-width:600px){.rx-header{padding:30px 22px}.section{padding:24px 20px}.summary-row{grid-template-columns:1fr 1fr}}";
  html += "</style></head><body><div class='page'>";

  html += "<div class='rx-header'><div class='header-top'><div class='rx-badge'><div class='rx-logo'>🌅</div><div><h1>Daily Checkup Report</h1><div class='rx-header-sub'>PocketDoctor Mark 5 Autonomous Recovery Tracker</div></div></div><div class='doc-stamp'><div class='stamp-title'>System Model</div><div class='stamp-val'>MK5-BETA</div></div></div>";
  html += "<div class='patient-grid'><div class='p-chip'>👤 <strong>" + currentProfile.name + "</strong></div><div class='p-chip'>🎂 <strong>" + String(currentProfile.age) + " yrs</strong></div><div class='p-chip'>⚖️ <strong>" + String(currentProfile.weight) + " kg</strong></div><div class='p-chip'>📏 <strong>" + String(currentProfile.height) + " cm</strong></div></div></div>";

  html += "<div class='summary-row'>";
  html += "<div class='scard'><div class='scard-icon'>📋</div><div class='scard-label'>Status</div><div class='scard-value'>" + diagnosedDisease + "</div></div>";
  html += "<div class='scard'><div class='scard-icon'>📈</div><div class='scard-label'>Recovery Score</div><div class='scard-value gold'>" + accuracy + "</div></div>";
  bool isCritical = critical.indexOf("Yes") >= 0;
  html += "<div class='scard'><div class='scard-icon'>" + String(isCritical ? "🚨" : "✅") + "</div><div class='scard-label'>Condition Safety</div><div class='scard-value " + String(isCritical ? "red" : "green") + "'>" + String(isCritical ? "NEEDS ATTENTION" : "SAFE") + "</div></div>";
  html += "</div>";

  if (isCritical) {
    html += "<div class='critical-banner'><div class='icon'>🚨</div><div><h3>ATTENTION REQUIRED</h3><p>Your responses indicate a potential safety concern or decline in recovery. Please consult a medical professional.</p></div></div>";
  }

  html += "<div class='section'><div class='section-header'><div class='section-icon'>💡</div><h2>Daily Advice & Tips</h2></div>";
  html += "<div class='box-main'><strong style='color:var(--main);font-size:1.1rem'>Today's Focus:</strong><br><span style='color:var(--text)'>" + medicines + "</span></div></div>";

  html += "<div class='section'><div class='section-header'><div class='section-icon'>🚀</div><h2>Next Steps</h2></div>";
  html += "<div class='box-green'>" + precautions + "</div></div>";

  html += "<div class='attestation'><div class='att-title'>📜 Official AI Checkup Attestation & Disclaimer</div>";
  html += "<div class='att-body'><p>This daily checkup summary is generated by <strong>PocketDoctor Mark 5</strong>. This document is intended solely for preliminary tracking and does <strong>NOT</strong> substitute for formal physical examination, laboratory diagnostic testing, or professional medical advice.</p><p style='margin-top:6px'>In a medical emergency, contact emergency medical services immediately.</p></div>";
  html += "<div class='sig-block'><span>Creator & Lead Engineer: <strong>Vaidik Khurana</strong></span><span>System: PocketDoctor Mark 5</span></div></div>";

  html += "<div class='footer'>Generated by <span>PocketDoctor Mark 5</span> · Designed & Engineered by <span>Vaidik Khurana</span> · For informational use only</div>";
  html += "</div></body></html>";
  return html;
}

void parseDiagnosis(String diagnosis) {
  if (isDailyCheckup) {
    int statusPos = diagnosis.indexOf("Status:");
    int recoveryPos = diagnosis.indexOf("Recovery Score:");
    int advicePos = diagnosis.indexOf("Daily Advice:");
    int stepsPos = diagnosis.indexOf("Next Steps:");
    int criticalPos = diagnosis.indexOf("Critical:");

    if (statusPos != -1 && recoveryPos != -1) {
      diagnosedDisease = diagnosis.substring(statusPos + 7, recoveryPos);
      diagnosedDisease.trim();
    }
    if (recoveryPos != -1 && advicePos != -1) {
      accuracy = diagnosis.substring(recoveryPos + 15, advicePos);
      accuracy.trim();
    }
    if (advicePos != -1 && stepsPos != -1) {
      medicines = diagnosis.substring(advicePos + 13, stepsPos);
      medicines.trim();
    }
    if (stepsPos != -1 && criticalPos != -1) {
      precautions = diagnosis.substring(stepsPos + 11, criticalPos);
      precautions.trim();
    }
    if (criticalPos != -1) {
      critical = diagnosis.substring(criticalPos + 9);
      critical.trim();
    }
  } else {
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
  String initialContext = profileContext + "Symptoms: " + symptomList + ". Duration: " + String(durations[selectedDurationIndex]);
  if (isDailyCheckup) {
    initialContext += ". This is a daily checkup. Ask up to 5 strategic yes/no questions to determine if the patient is safe, recovering well, or needs attention. Output one question only each time [I REPEAT ONE EACH TIME]. No extra text, ONLY question.";
  } else {
    initialContext += ". Ask 10 strategic yes/no questions (max 10 words each). Output one question only each time [I REPEAT ONE EACH TIME.], and cover all related aspects for efficiency. Cover different aspects: location, intensity, timing, triggers. No extra text, ONLY question.";
  }

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

    int currentMaxQuestions = isDailyCheckup ? 5 : 10;
    if (questionCount <= currentMaxQuestions) {
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
      if (isDailyCheckup) {
        htmlReport = createDailyCheckupHTMLReport();
      } else {
        htmlReport = createComprehensiveHTMLReport();
      }
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
  int currentMaxQuestions = isDailyCheckup ? 5 : 10;
  display.print("Q");
  display.print(questionCount);
  display.print("/");
  display.print(currentMaxQuestions);
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
  // Always handle web clients unconditionally
  server.handleClient();

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

  // Main menu – press to start mode selection
  if (!inSymptomSelection && !inDurationSelection && !inGeminiQuestions && !inModeSelection && !inSOSLevelSelection) {
    if (swState == LOW && (now - lastMove > debounceDelay)) {
      inModeSelection = true;
      modeIndex = 0;
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

  // Mode selection
  if (inModeSelection) {
    if (yValue < (2048 - joyThreshold) && (now - lastMove > debounceDelay)) {
      modeIndex--;
      if (modeIndex < 0) modeIndex = 0;
      lastMove = now;
    }
    if (yValue > (2048 + joyThreshold) && (now - lastMove > debounceDelay)) {
      modeIndex++;
      if (modeIndex > 2) modeIndex = 2;
      lastMove = now;
    }
    if (swState == LOW && (now - lastMove > debounceDelay)) {
      isDailyCheckup = (modeIndex == 1);
      isSOSMode = (modeIndex == 2);
      inModeSelection = false;
      if (isSOSMode) {
        inSOSLevelSelection = true;
        sosLevel = 1;
      } else {
        inSymptomSelection = true;
        symptomIndex = 0;
      }
      lastMove = now;
    }

    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("Select Mode:");
    display.setCursor(5, 15);
    if (modeIndex == 0) display.print("> ");
    else display.print("  ");
    display.println("New Diagnosis");
    
    display.setCursor(5, 27);
    if (modeIndex == 1) display.print("> ");
    else display.print("  ");
    display.println("Daily Checkup");

    display.setCursor(5, 39);
    if (modeIndex == 2) display.print("> ");
    else display.print("  ");
    display.println("SOS Alert");

    display.setCursor(0, 54);
    if (WiFi.status() == WL_CONNECTED) {
      display.print("Loc Sync: ");
      display.print(WiFi.localIP().toString());
    } else {
      display.print("Loc Sync: OFFLINE");
    }
    display.display();
    return;
  }

  // SOS Level Selection
  if (inSOSLevelSelection) {
    if (yValue < (2048 - joyThreshold) && (now - lastMove > debounceDelay)) {
      sosLevel++;
      if (sosLevel > 10) sosLevel = 10;
      lastMove = now;
    }
    if (yValue > (2048 + joyThreshold) && (now - lastMove > debounceDelay)) {
      sosLevel--;
      if (sosLevel < 1) sosLevel = 1;
      lastMove = now;
    }
    if (swState == LOW && (now - lastMove > debounceDelay)) {
      inSOSLevelSelection = false;
      dispatchSOSAlert();
      lastMove = now;
      
      inModeSelection = true;
      modeIndex = 0;
    }

    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("EMERGENCY SOS");
    display.drawLine(0, 10, 128, 10, SSD1306_WHITE);
    display.setCursor(0, 20);
    display.println("Select Severity:");
    display.setTextSize(2);
    display.setCursor(50, 40);
    display.print(sosLevel);
    display.display();
    return;
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