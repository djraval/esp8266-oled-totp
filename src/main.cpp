// src/main.cpp
#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <ezTime.h>
#include <TOTP.h>
#include <EEPROM.h>
#include "config.h"


// Constants
const int SCL_PIN = 12;
const int SDA_PIN = 14;
const int SCREEN_WIDTH = 128;
const int SCREEN_HEIGHT = 64;
const int YELLOW_SECTION_HEIGHT = 16;
const int SECRET_LENGTH = 16;
const int LOOP_DELAY = 1000;
const int MAX_LINES = 3;
const int LARGE_FONT_HEIGHT = 20;
const int SMALL_FONT_HEIGHT = 13;
const unsigned long NTP_SYNC_TIMEOUT = 30000; // 30 seconds timeout for NTP sync


// OLED Display
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, SCL_PIN, SDA_PIN, U8X8_PIN_NONE);

// TOTP
uint8_t hmacKey[SECRET_LENGTH];
TOTP totp(hmacKey, SECRET_LENGTH);

// Function prototypes
void setupWiFi();
void setupTime();
void setupDisplay();
void setupTOTP();
void updateDisplay(const char* header, const char* body, char hAlign = 'C', char vAlign = 'M');
void updateTOTP();
void base32Decode(const char* input, uint8_t* output, int outputLength);
void drawProgressBar(int percentage);
bool tryWiFiConnection(const char* ssid, const char* password, int maxAttempts = 2);
void saveWiFiCredentials(const char* ssid, const char* password);


void setup() {
  Serial.begin(115200);
  
  setupDisplay();
  setupWiFi();
  setupTime();
  setupTOTP();

  Serial.println("Setup completed");
}

void loop() {
  events(); // Handle ezTime events
  updateTOTP();
  delay(LOOP_DELAY);
}

void setupDisplay() {
  u8g2.begin();
  updateDisplay("Initializing", "Display...");
}

void setupWiFi() {
  EEPROM.begin(EEPROM_SIZE);
  
  char lastSSID[MAX_SSID_LENGTH + 1] = {0};
  char lastPass[MAX_PASS_LENGTH + 1] = {0};
  
  // Read last successful SSID and password from EEPROM
  for (int i = 0; i < MAX_SSID_LENGTH; i++) {
    lastSSID[i] = EEPROM.read(LAST_WIFI_SSID_ADDR + i);
  }
  for (int i = 0; i < MAX_PASS_LENGTH; i++) {
    lastPass[i] = EEPROM.read(LAST_WIFI_PASS_ADDR + i);
  }
  
  // Ensure null termination
  lastSSID[MAX_SSID_LENGTH] = '\0';
  lastPass[MAX_PASS_LENGTH] = '\0';
  
  // Try connecting with last successful credentials first
  if (strlen(lastSSID) > 0 && tryWiFiConnection(lastSSID, lastPass, 2)) {
    return;
  }
  
  // If last successful connection fails, try the configured credentials
  for (int i = 0; i < WIFI_CREDS_COUNT; i++) {
    if (tryWiFiConnection(WIFI_CREDS[i].ssid, WIFI_CREDS[i].password, 2)) {
      saveWiFiCredentials(WIFI_CREDS[i].ssid, WIFI_CREDS[i].password);
      return;
    }
  }
  
  Serial.println("Failed to connect to any WiFi network");
  updateDisplay("WiFi", "Connection\nFailed");
  delay(2000);
  ESP.restart();
}

bool tryWiFiConnection(const char* ssid, const char* password, int maxAttempts) {
  for (int attempt = 0; attempt < maxAttempts; attempt++) {
    char displayMessage[64];
    char ssidTruncated[21] = {0}; // 20 characters + null terminator
    strncpy(ssidTruncated, ssid, 20);
    ssidTruncated[20] = '\0'; // Ensure null termination
    
    // Format the message in parts to avoid potential overflow
    snprintf(displayMessage, sizeof(displayMessage), "Connecting to:");
    snprintf(displayMessage + strlen(displayMessage), 
             sizeof(displayMessage) - strlen(displayMessage),
             "\n%.20s", ssidTruncated);
    snprintf(displayMessage + strlen(displayMessage), 
             sizeof(displayMessage) - strlen(displayMessage),
             "\nAttempt %d/%d", attempt + 1, maxAttempts);
    
    updateDisplay("WiFi", displayMessage);
    
    Serial.printf("Attempting to connect to %s (Attempt %d/%d)\n", ssid, attempt + 1, maxAttempts);
    WiFi.begin(ssid, password);
    
    int timeout = 0;
    while (WiFi.status() != WL_CONNECTED && timeout < 20) {
      delay(500);
      Serial.print(".");
      timeout++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("\nConnected to %s\n", ssid);
      snprintf(displayMessage, sizeof(displayMessage), "Connected to:\n%.20s", ssidTruncated);
      updateDisplay("WiFi", displayMessage);
      delay(2000);
      return true;
    }
    
    Serial.printf("\nFailed to connect to %s (Attempt %d/%d)\n", ssid, attempt + 1, maxAttempts);
    WiFi.disconnect();
    delay(1000); // Wait for a second before trying again
  }
  
  return false;
}


void saveWiFiCredentials(const char* ssid, const char* password) {
  for (int j = 0; j < MAX_SSID_LENGTH; j++) {
    EEPROM.write(LAST_WIFI_SSID_ADDR + j, ssid[j]);
  }
  for (int j = 0; j < MAX_PASS_LENGTH; j++) {
    EEPROM.write(LAST_WIFI_PASS_ADDR + j, password[j]);
  }
  EEPROM.commit();
}

void setupTime() {
  updateDisplay("Syncing", "Time...");
  setInterval(30); // Set NTP update interval to 30 seconds
  
  unsigned long startAttemptTime = millis();
  
  while (timeStatus() != timeSet) {
    if (millis() - startAttemptTime > NTP_SYNC_TIMEOUT) {
      Serial.println("NTP sync failed");
      updateDisplay("Error", "NTP sync failed\nRestarting...");
      delay(2000);
      ESP.restart();
    }
    
    events();
    
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi disconnected during NTP sync");
      updateDisplay("Error", "WiFi disconnected\nRestarting...");
      delay(2000);
      ESP.restart();
    }
    
    delay(500);
  }
  
  Serial.println("Time synchronized");
  updateDisplay("Time", "Synchronized");
  delay(1000);
}


void setupTOTP() {
  base32Decode(TOTP_SECRET, hmacKey, SECRET_LENGTH);
  updateDisplay("TOTP", "Initialized");
  delay(1000);
}

void drawProgressBar(int percentage) {
  int barWidth = SCREEN_WIDTH - 4;
  int barHeight = 6;
  int y = SCREEN_HEIGHT - barHeight - 2;

  u8g2.drawFrame(2, y, barWidth, barHeight);
  u8g2.drawBox(2, y, (barWidth * percentage) / 100, barHeight);
}

void updateTOTP() {
  static char lastTOTPCode[7] = {0};
  static unsigned long lastUpdateTime = 0;
  unsigned long currentTime = millis();
  unsigned long epochTime = now();
  
  int secondsInPeriod = epochTime % 30;
  int progress = 100 - ((secondsInPeriod * 100) / 30);

  if (currentTime - lastUpdateTime >= 1000 || strcmp(totp.getCode(epochTime), lastTOTPCode) != 0) {
    lastUpdateTime = currentTime;
    
    char* newTOTPCode = totp.getCode(epochTime);
    
    if (strcmp(newTOTPCode, lastTOTPCode) != 0) {
      Serial.printf("TOTP Code: %s\n", newTOTPCode);
      strcpy(lastTOTPCode, newTOTPCode);
    }

    u8g2.clearBuffer();
    
    // Draw header
    u8g2.setFont(u8g2_font_7x13B_tr);
    int16_t headerWidth = u8g2.getStrWidth("TOTP Code");
    int16_t headerX = (SCREEN_WIDTH - headerWidth) / 2;
    u8g2.drawStr(headerX, 12, "TOTP Code");

    // Draw TOTP code
    u8g2.setFont(u8g2_font_inr24_mn);
    int16_t digitWidth = u8g2.getStrWidth("0");
    int16_t codeWidth = digitWidth * 6;
    int16_t codeX = (SCREEN_WIDTH - codeWidth) / 2;
    int16_t codeY = 45;
    u8g2.drawStr(codeX, codeY, lastTOTPCode);

    // Draw progress bar
    drawProgressBar(progress);

    u8g2.sendBuffer();
  }
}

void updateDisplay(const char* header, const char* body, char hAlign, char vAlign) {
  u8g2.clearBuffer();

  // Header (yellow section)
  u8g2.setFont(u8g2_font_7x13B_tr);
  int16_t headerWidth = u8g2.getStrWidth(header);
  int16_t headerX = (SCREEN_WIDTH - headerWidth) / 2;
  u8g2.drawStr(headerX, 12, header);

  // Body (blue section)
  int bodyLength = strlen(body);
  char* lines[MAX_LINES];
  int lineCount = 0;

  // Split body into lines
  char* bodyTemp = strdup(body);
  char* token = strtok(bodyTemp, "\n");
  while (token != NULL && lineCount < MAX_LINES) {
    lines[lineCount++] = token;
    token = strtok(NULL, "\n");
  }

  // Choose font based on line count
  u8g2.setFont(lineCount == 1 && bodyLength <= 6 ? u8g2_font_10x20_tf : u8g2_font_7x13_tf);

  int16_t lineHeight = (u8g2.getMaxCharHeight() > SMALL_FONT_HEIGHT) ? LARGE_FONT_HEIGHT : SMALL_FONT_HEIGHT;
  int16_t totalTextHeight = lineCount * lineHeight;
  int16_t startY = YELLOW_SECTION_HEIGHT + ((SCREEN_HEIGHT - YELLOW_SECTION_HEIGHT - totalTextHeight) / 2) + lineHeight;

  for (int i = 0; i < lineCount; i++) {
    int16_t lineWidth = u8g2.getStrWidth(lines[i]);
    int16_t lineX = (SCREEN_WIDTH - lineWidth) / 2;
    u8g2.drawStr(lineX, startY + (i * lineHeight), lines[i]);
  }

  u8g2.sendBuffer();
  free(bodyTemp);
}

void base32Decode(const char* input, uint8_t* output, int outputLength) {
  const char* base32Chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
  int buffer = 0;
  int bitsLeft = 0;
  int count = 0;

  for (const char* ptr = input; count < outputLength && *ptr; ++ptr) {
    char c = *ptr;
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '-') continue;
    
    buffer <<= 5;
    const char* pos = strchr(base32Chars, toupper(c));
    if (pos == nullptr) continue;
    
    buffer |= (pos - base32Chars);
    bitsLeft += 5;
    if (bitsLeft >= 8) {
      output[count++] = (buffer >> (bitsLeft - 8)) & 0xFF;
      bitsLeft -= 8;
    }
  }
}
