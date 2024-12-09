#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <ezTime.h>
#include <TOTP.h>
#include <EEPROM.h>
#include "config.h"
#include <vector>
#include <algorithm>

const int SCL_PIN = 12;
const int SDA_PIN = 14;
const int SCREEN_WIDTH = 128;
const int SCREEN_HEIGHT = 64;
const int YELLOW_SECTION_HEIGHT = 16;
const unsigned long NTP_SYNC_TIMEOUT = 30000; // 30 seconds timeout for NTP sync

U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, SCL_PIN, SDA_PIN, U8X8_PIN_NONE);

// Forward declarations
void setupDisplay();
bool connectToWiFi(const char *ssid, const char *password, int maxAttempts);
void saveWiFiCredentials(const char *ssid, const char *password);
void setupWiFi();
void setupTime();
void displayMultiTOTP();
int base32Decode(const char *input, uint8_t *output, int outputLength);
void showMessage(const char *header, const char *body);
bool scanAndConnectWiFi();

struct NetworkInfo {
    String ssid;
    int32_t rssi;
    int configIndex;  // Index in WIFI_CREDS array
};

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

void setupDisplay()
{
  Wire.setClock(400000);  // Set I2C to fast mode (400kHz)
  u8g2.begin();
  u8g2.setPowerSave(0);  // Ensure display is always on
  u8g2.setContrast(255); // Maximum contrast
  // showMessage("Initializing", "Display...");
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

void displayMultiTOTP()
{
  static unsigned long lastUpdateTime = 0;
  static long lastEpochTime = 0;
  static long lastTOTPTime = 0;  // Added this to track TOTP period changes
  const unsigned long UPDATE_INTERVAL = 100;

  unsigned long currentTime = millis();
  long epochTime = (long)now();
  long currentTOTPTime = epochTime / 30;  // Integer division to get current TOTP period

  if (epochTime != lastEpochTime || currentTime - lastUpdateTime >= UPDATE_INTERVAL)
  {
    lastUpdateTime = currentTime;
    lastEpochTime = epochTime;

    u8g2.clearBuffer();

    const int TOTP_PERIOD = 30;
    int secondsElapsed = epochTime % TOTP_PERIOD;
    int progressPercentage = 100 - ((secondsElapsed * 100) / TOTP_PERIOD);

    const int PROGRESS_BAR_WIDTH = 120;
    const int PROGRESS_BAR_HEIGHT = 8;
    const int PROGRESS_BAR_X = (SCREEN_WIDTH - PROGRESS_BAR_WIDTH) / 2;
    const int PROGRESS_BAR_Y = (YELLOW_SECTION_HEIGHT - PROGRESS_BAR_HEIGHT) / 2;

    u8g2.drawFrame(PROGRESS_BAR_X, PROGRESS_BAR_Y, PROGRESS_BAR_WIDTH, PROGRESS_BAR_HEIGHT);

    int fillWidth = (PROGRESS_BAR_WIDTH * progressPercentage) / 100;
    u8g2.drawBox(PROGRESS_BAR_X, PROGRESS_BAR_Y, fillWidth, PROGRESS_BAR_HEIGHT);

    // Determine grid layout based on number of TOTPs
    const int COLS = (TOTP_KEYS_COUNT <= 4) ? 2 : 3;
    const int ROWS = (TOTP_KEYS_COUNT <= 4) ? 2 : 2;
    const int CELL_WIDTH = SCREEN_WIDTH / COLS;
    const int CELL_HEIGHT = (SCREEN_HEIGHT - YELLOW_SECTION_HEIGHT) / ROWS;
    
    // Select font sizes based on number of TOTPs
    const uint8_t* labelFont = (TOTP_KEYS_COUNT <= 4) ? u8g2_font_6x10_tr : u8g2_font_5x7_tr;
    const uint8_t* codeFont = (TOTP_KEYS_COUNT <= 4) ? u8g2_font_profont17_tn : u8g2_font_profont11_tn;
    
    int startY = YELLOW_SECTION_HEIGHT + CELL_HEIGHT / 2;

    for (int i = 0; i < TOTP_KEYS_COUNT; i++)
    {
      int row = i / COLS;
      int col = i % COLS;

      int x = col * CELL_WIDTH;
      int y = startY + row * CELL_HEIGHT;

      u8g2.setFont(labelFont);
      char abbreviatedLabel[10]; // 9 chars + null terminator
      // Use 9 chars for 2x2 grid, 6 chars for 3x2 grid
      size_t maxLength = (TOTP_KEYS_COUNT <= 4) ? 9 : 6;
      abbreviateServiceName(TOTP_KEYS[i].label, abbreviatedLabel, maxLength);
      int labelWidth = u8g2.getStrWidth(abbreviatedLabel);
      u8g2.drawStr(x + (CELL_WIDTH - labelWidth) / 2, y - 2, abbreviatedLabel);

      uint8_t hmacKey[32];
      int keyLength = base32Decode(TOTP_KEYS[i].secret, hmacKey, sizeof(hmacKey));
      TOTP totp(hmacKey, keyLength);
      char *code = totp.getCode(epochTime);

      if (currentTOTPTime != lastTOTPTime) {  // Only print when TOTP period changes
        Serial.println(String(TOTP_KEYS[i].label) + " : " + String(code));
      }

      u8g2.setFont(codeFont);
      int codeWidth = u8g2.getStrWidth(code);
      // Adjust Y offset based on grid size
      int yOffset = (TOTP_KEYS_COUNT <= 4) ? 12 : 10;
      u8g2.drawStr(x + (CELL_WIDTH - codeWidth) / 2, y + yOffset, code);
    }

    u8g2.sendBuffer();

    if (currentTOTPTime != lastTOTPTime) {
      lastTOTPTime = currentTOTPTime;  // Update the last TOTP time 
    }
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

void showMessage(const char *header, const char *body)
{
  static char lastHeader[32] = "";
  static char lastBody[64] = "";
  
  // Only clear and redraw if the message has changed
  if (strcmp(lastHeader, header) != 0 || strcmp(lastBody, body) != 0) {
    strncpy(lastHeader, header, sizeof(lastHeader) - 1);
    strncpy(lastBody, body, sizeof(lastBody) - 1);
    lastHeader[sizeof(lastHeader) - 1] = '\0';
    lastBody[sizeof(lastBody) - 1] = '\0';
    
    u8g2.clearBuffer();

    u8g2.setFont(u8g2_font_7x13B_tr);
    int16_t headerWidth = u8g2.getStrWidth(header);
    int16_t headerX = (SCREEN_WIDTH - headerWidth) / 2;
    u8g2.drawStr(headerX, 12, header);

    int bodyLength = strlen(body);
    const int MAX_LINES = 3;
    char *lines[MAX_LINES];
    int lineCount = 0;

    char *bodyTemp = strdup(body);
    char *token = strtok(bodyTemp, "\n");
    while (token != NULL && lineCount < MAX_LINES)
    {
      lines[lineCount++] = token;
      token = strtok(NULL, "\n");
    }

    u8g2.setFont(lineCount == 1 && bodyLength <= 6 ? u8g2_font_10x20_tf : u8g2_font_7x13_tf);

    int16_t lineHeight = (u8g2.getMaxCharHeight() > 13) ? 20 : 13;
    int16_t totalTextHeight = lineCount * lineHeight;
    int16_t startY = YELLOW_SECTION_HEIGHT + ((SCREEN_HEIGHT - YELLOW_SECTION_HEIGHT - totalTextHeight) / 2) + lineHeight;

    for (int i = 0; i < lineCount; i++)
    {
      int16_t lineWidth = u8g2.getStrWidth(lines[i]);
      int16_t lineX = (SCREEN_WIDTH - lineWidth) / 2;
      u8g2.drawStr(lineX, startY + (i * lineHeight), lines[i]);
    }

    u8g2.sendBuffer();
    free(bodyTemp);
  }
}
