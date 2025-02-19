// ===== Include Section =====
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ezTime.h>
#include <TOTP.h>
#include <EEPROM.h>
#include "config.h"
#include <vector>
#include <algorithm>
#include <Display.h>

// ===== Pin Definitions =====
const int FLASH_BTN_PIN = 0;

// ===== Timing Constants =====
const unsigned long NTP_SYNC_TIMEOUT = 30000;           // 30 seconds timeout for NTP sync
const unsigned long FLASH_BUTTON_DEBOUNCE_DELAY = 200;
unsigned long lastBtnPress = 0;

// ===== Data Structures =====
struct NetworkInfo {
    String ssid;
    int32_t rssi;
    int configIndex;  // Index in WIFI_CREDS array
};

// ===== Function Declarations =====
// Display Functions
void displayMultiTOTP();

// WiFi Functions
void setupWiFi();
bool scanAndConnectWiFi();
bool connectToWiFi(const char *ssid, const char *password, int maxAttempts);
void saveWiFiCredentials(const char *ssid, const char *password);
bool isNetworkInConfig(const char* ssid);

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

bool isNetworkInConfig(const char* ssid) {
  for (int i = 0; i < WIFI_CREDS_COUNT; i++) {
    if (strcmp(WIFI_CREDS[i].ssid, ssid) == 0) {
      return true;
    }
  }
  return false;
}

bool scanAndConnectWiFi()
{
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(100);

  showMessage("WiFi", "Scanning...");
  Serial.println("Scanning for networks...");
  unsigned long scanStart = millis();
  int n = WiFi.scanNetworks(true, true);

  int dots = 0;
  char scanMsg[32];
  while (n == WIFI_SCAN_RUNNING && millis() - scanStart < 10000) {
    snprintf(scanMsg, sizeof(scanMsg), "Scanning%.*s", dots + 1, "...");
    showMessage("WiFi", scanMsg);
    dots = (dots + 1) % 3;
    n = WiFi.scanComplete();
    delay(100);
  }

  if (n == WIFI_SCAN_FAILED || n == 0) {
    Serial.println("No networks found or scan failed");
    showMessage("WiFi", "No networks\nfound");
    WiFi.scanDelete();
    return false;
  }

  char statusMsg[64];
  Serial.println(String(n) + " WiFi networks in range");
  snprintf(statusMsg, sizeof(statusMsg), "%d\nnetworks inrange", n);

  for (int i = 0; i < n; i++) {
    Serial.println(WiFi.SSID(i));
  }
  showMessage("WiFi", statusMsg);
  delay(1000);

  // First try known networks
  std::vector<NetworkInfo> knownNetworks;
  for (int i = 0; i < n; i++) {
    String currentSSID = WiFi.SSID(i);
    for (int j = 0; j < WIFI_CREDS_COUNT; j++) {
      if (currentSSID.equals(WIFI_CREDS[j].ssid)) {
        NetworkInfo info = {
          .ssid = currentSSID,
          .rssi = WiFi.RSSI(i),
          .configIndex = j
        };
        knownNetworks.push_back(info);
        break;
      }
    }
  }

  // Sort and try known networks first
  if (!knownNetworks.empty()) {
    std::sort(knownNetworks.begin(), knownNetworks.end(),
      [](const NetworkInfo& a, const NetworkInfo& b) {
        return a.rssi > b.rssi;
      });

    for (const auto& network : knownNetworks) {
      char statusMsg[64];
      snprintf(statusMsg, sizeof(statusMsg), "Trying\n%s\nRSSI: %d dBm",
               network.ssid.c_str(), network.rssi);
      showMessage("WiFi", statusMsg);
      delay(500);

      if (connectToWiFi(WIFI_CREDS[network.configIndex].ssid,
                        WIFI_CREDS[network.configIndex].password, 1)) {
        saveWiFiCredentials(WIFI_CREDS[network.configIndex].ssid,
                           WIFI_CREDS[network.configIndex].password);
        WiFi.scanDelete();
        return true;
      }
    }
  }

  // If no known networks connected, try open networks
  Serial.println("Trying open networks...");
  showMessage("WiFi", "Trying open\nnetworks");

  // Sort all networks by signal strength
  std::vector<NetworkInfo> openNetworks;
  for (int i = 0; i < n; i++) {
    if (WiFi.encryptionType(i) == ENC_TYPE_NONE) {
      NetworkInfo info = {
        .ssid = WiFi.SSID(i),
        .rssi = WiFi.RSSI(i),
        .configIndex = -1  // Not used for open networks
      };
      openNetworks.push_back(info);
    }
  }

  if (!openNetworks.empty()) {
    std::sort(openNetworks.begin(), openNetworks.end(),
      [](const NetworkInfo& a, const NetworkInfo& b) {
        return a.rssi > b.rssi;
      });

    for (const auto& network : openNetworks) {
      char statusMsg[64];
      snprintf(statusMsg, sizeof(statusMsg), "Trying open\n%s\nRSSI: %d dBm",
               network.ssid.c_str(), network.rssi);
      showMessage("WiFi", statusMsg);
      delay(500);

      if (connectToWiFi(network.ssid.c_str(), "", 1)) {
        saveWiFiCredentials(network.ssid.c_str(), "");
        WiFi.scanDelete();
        return true;
      }
    }
  }

  WiFi.scanDelete();

  // If no networks connected, show failure message and clear EEPROM
  Serial.println("No networks available");
  showMessage("WiFi", "No networks\navailable");
  for (int i = 0; i < MAX_SSID_LENGTH + MAX_PASS_LENGTH; i++) {
    EEPROM.write(LAST_WIFI_SSID_ADDR + i, 0);
  }
  EEPROM.commit();

  return false;
}

bool connectToWiFi(const char *ssid, const char *password, int maxAttempts)
{
  // Add WiFi disconnect and mode setting before attempting connection
  WiFi.disconnect(true);  // true = disconnect and clear stored credentials
  delay(100);  // Add small delay after disconnect
  WiFi.mode(WIFI_STA);
  delay(100);  // Add small delay after mode change

  // Truncate SSID if too long and ensure it fits on display
  char ssidTruncated[17] = {0};
  strncpy(ssidTruncated, ssid, 16);

  char displayMessage[64];
  snprintf(displayMessage, sizeof(displayMessage), "Connecting to\n%s", ssidTruncated);
  showMessage("WiFi", displayMessage);

  Serial.printf("Attempting to connect to %s\n", ssid);
  WiFi.begin(ssid, password);

  const int CONNECT_DELAY_MS = 100;  // Delay between connection checks
  const int TIMEOUT_SECONDS = 10;    // Total timeout in seconds
  const int MAX_ATTEMPTS = (TIMEOUT_SECONDS * 1000) / CONNECT_DELAY_MS;  // Convert to iterations

  int attempts = 0;
  int dots = 0;
  unsigned long lastDisplayUpdate = 0;
  const unsigned long DISPLAY_UPDATE_INTERVAL = 250;

  while (WiFi.status() != WL_CONNECTED && attempts < MAX_ATTEMPTS)
  {
    unsigned long currentMillis = millis();
    if (currentMillis - lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL) {
      snprintf(displayMessage, sizeof(displayMessage), "Connecting to\n%s%.*s",
               ssidTruncated, dots + 1, "...");
      showMessage("WiFi", displayMessage);
      dots = (dots + 1) % 3;
      lastDisplayUpdate = currentMillis;
    }
    delay(CONNECT_DELAY_MS);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.printf("\nConnected to %s\n", ssid);
    snprintf(displayMessage, sizeof(displayMessage), "Connected to\n%s", ssidTruncated);
    showMessage("WiFi", displayMessage);
    delay(500);  // Reduced delay
    return true;
  }

  Serial.printf("\nFailed to connect to %s\n", ssid);
  snprintf(displayMessage, sizeof(displayMessage), "Failed to\nconnect to\n%s", ssidTruncated);
  showMessage("WiFi", displayMessage);
  delay(500);  // Reduced delay
  WiFi.disconnect();
  return false;
}

void setupWiFi()
{
  EEPROM.begin(EEPROM_SIZE);

  // Try scanning and connecting to available networks
  if (scanAndConnectWiFi()) {
    return;
  }

  // If scan and connect failed, restart the device
  Serial.println("Failed to connect to any WiFi network");
  showMessage("WiFi", "Connection\nFailed");
  delay(2000);
  ESP.restart();
}

void saveWiFiCredentials(const char *ssid, const char *password)
{
  for (int j = 0; j < MAX_SSID_LENGTH; j++)
  {
    EEPROM.write(LAST_WIFI_SSID_ADDR + j, ssid[j]);
  }
  for (int j = 0; j < MAX_PASS_LENGTH; j++)
  {
    EEPROM.write(LAST_WIFI_PASS_ADDR + j, password[j]);
  }
  EEPROM.commit();
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