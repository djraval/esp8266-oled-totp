// ===== Include Section =====
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ezTime.h>
#include <TOTP.h>
#include <EEPROM.h>
#include "config.h"
#include <vector>
#include <algorithm>

// Our Libs
#include <Display.h>
#include <WiFiManager.h>

// ===== Pin Definitions =====
const int FLASH_BTN_PIN = 0;

// ===== Timing Constants =====
const unsigned long NTP_SYNC_TIMEOUT = 30000;           // 30 seconds timeout for NTP sync
const unsigned long FLASH_BUTTON_DEBOUNCE_DELAY = 200;
unsigned long lastBtnPress = 0;

// ===== Function Declarations =====
// Display Functions
void displayMultiTOTP();

// Time Functions
void setupTime();

// Utility Functions
void abbreviateServiceName(const char* input, char* output, size_t maxLength);
int base32Decode(const char *input, uint8_t *output, int outputLength);
void updateOTPCodes(OTPEntry* entries, long epochTime);

// Helper function to abbreviate service names
void abbreviateServiceName(const char* input, char* output, size_t maxLength) {
  if (strlen(input) <= maxLength) {
    strncpy(output, input, maxLength);
    output[maxLength] = '\0';
    return;
  }

  // If longer than maxLength, remove vowels
  size_t j = 0;
  for (size_t i = 0; i < strlen(input) && j < maxLength; i++) {
    char c = toupper(input[i]);
    if (c != 'A' && c != 'E' && c != 'I' && c != 'O' && c != 'U') {
      output[j++] = c;
    }
  }
  output[j] = '\0';

  // If still too long, truncate
  if (strlen(output) > maxLength) {
    output[maxLength] = '\0';
  }
}

void setup()
{
  Serial.begin(115200);
  setupDisplay();
  setupWiFi();
  setupTime();
  Serial.println("Setup completed");
}

void loop()
{
  events(); // Handle ezTime events
  displayMultiTOTP();
}

void setupTime()
{
  showMessage("NTP Sync", "Syncing...");
  setInterval(30);

  unsigned long startAttemptTime = millis();

  while (timeStatus() != timeSet)
  {
    if (millis() - startAttemptTime > NTP_SYNC_TIMEOUT)
    {
      Serial.println("NTP sync failed");
      showMessage("NTP Sync", "NTP sync failed\nRestarting...");
      delay(2000);
      ESP.restart();
    }

    events();

    if (WiFi.status() != WL_CONNECTED)
    {
      Serial.println("WiFi disconnected during NTP sync");
      showMessage("NTP Sync", "WiFi disconnected\nRestarting...");
      delay(2000);
      ESP.restart();
    }

    delay(500);
  }

  Serial.println("Time synchronized");
  showMessage("NTP Sync", "Synchronized");
  delay(1000);
}

void updateOTPCodes(OTPEntry* entries, long epochTime) {
  for (int i = 0; i < TOTP_KEYS_COUNT; i++) {
      // Abbreviate service name
      abbreviateServiceName(TOTP_KEYS[i].label, 
                          entries[i].abbreviatedLabel, 
                          (TOTP_KEYS_COUNT <= 4) ? 9 : 6);

      // Generate TOTP code
      uint8_t hmacKey[32];
      int keyLength = base32Decode(TOTP_KEYS[i].secret, hmacKey, sizeof(hmacKey));
      TOTP totp(hmacKey, keyLength);
      char* code = totp.getCode(epochTime);
      
      // Copy code to entry
      strncpy(entries[i].code, code, 6);
      entries[i].code[6] = '\0';
  }
}

void displayMultiTOTP() {
  static unsigned long lastUpdateTime = 0;
  static long lastEpochTime = 0;
  static long lastTOTPTime = 0;
  static OTPEntry otpEntries[6]; // Max 6 TOTP keys for display
  static DisplayState displayState = {
      .progressPercentage = 0,
      .totalItems = TOTP_KEYS_COUNT,
      .entries = otpEntries
  };

  const unsigned long UPDATE_INTERVAL = 100;
  unsigned long currentTime = millis();
  long epochTime = (long)now();
  long currentTOTPTime = epochTime / 30;

  if (epochTime != lastEpochTime || currentTime - lastUpdateTime >= UPDATE_INTERVAL) {
      lastUpdateTime = currentTime;
      lastEpochTime = epochTime;

      // Update OTP codes if period changed
      if (currentTOTPTime != lastTOTPTime) {
          updateOTPCodes(otpEntries, epochTime);
          lastTOTPTime = currentTOTPTime;
      }

      // Update progress percentage
      displayState.progressPercentage = 100 - ((epochTime % 30) * 100) / 30;

      // Render display
      renderOTPDisplay(displayState);
  }
}

int base32Decode(const char *input, uint8_t *output, int outputLength)
{
  const char *base32Chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
  int buffer = 0;
  int bitsLeft = 0;
  int count = 0;

  for (const char *ptr = input; count < outputLength && *ptr; ++ptr)
  {
    char c = *ptr;
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '-')
      continue;

    buffer <<= 5;
    const char *pos = strchr(base32Chars, toupper(c));
    if (pos == nullptr)
      continue;

    buffer |= (pos - base32Chars);
    bitsLeft += 5;
    if (bitsLeft >= 8)
    {
      output[count++] = (buffer >> (bitsLeft - 8)) & 0xFF;
      bitsLeft -= 8;
    }
  }
  return count;
}