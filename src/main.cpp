#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <ezTime.h>
#include <TOTP.h>
#include <EEPROM.h>
#include "config.h"

const int SCL_PIN = 12;
const int SDA_PIN = 14;
const int SCREEN_WIDTH = 128;
const int SCREEN_HEIGHT = 64;
const int YELLOW_SECTION_HEIGHT = 16;
const unsigned long NTP_SYNC_TIMEOUT = 30000; // 30 seconds timeout for NTP sync

U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, SCL_PIN, SDA_PIN, U8X8_PIN_NONE);

void setupDisplay();
bool connectToWiFi(const char *ssid, const char *password, int maxAttempts = 2);
void saveWiFiCredentials(const char *ssid, const char *password);
void setupWiFi();
void setupTime();
void displayMultiTOTP();
int base32Decode(const char *input, uint8_t *output, int outputLength);
void showMessage(const char *header, const char *body);

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
  u8g2.begin();
  showMessage("Initializing", "Display...");
}

void setupWiFi()
{
  EEPROM.begin(EEPROM_SIZE);

  char lastSSID[MAX_SSID_LENGTH + 1] = {0};
  char lastPass[MAX_PASS_LENGTH + 1] = {0};

  for (int i = 0; i < MAX_SSID_LENGTH; i++)
  {
    lastSSID[i] = EEPROM.read(LAST_WIFI_SSID_ADDR + i);
  }
  for (int i = 0; i < MAX_PASS_LENGTH; i++)
  {
    lastPass[i] = EEPROM.read(LAST_WIFI_PASS_ADDR + i);
  }

  lastSSID[MAX_SSID_LENGTH] = '\0';
  lastPass[MAX_PASS_LENGTH] = '\0';

  if (strlen(lastSSID) > 0 && connectToWiFi(lastSSID, lastPass, 2))
  {
    return;
  }

  for (int i = 0; i < WIFI_CREDS_COUNT; i++)
  {
    if (connectToWiFi(WIFI_CREDS[i].ssid, WIFI_CREDS[i].password, 2))
    {
      saveWiFiCredentials(WIFI_CREDS[i].ssid, WIFI_CREDS[i].password);
      return;
    }
  }

  Serial.println("Failed to connect to any WiFi network");
  showMessage("WiFi", "Connection\nFailed");
  delay(2000);
  ESP.restart();
}

bool connectToWiFi(const char *ssid, const char *password, int maxAttempts)
{
  for (int attempt = 0; attempt < maxAttempts; attempt++)
  {
    char displayMessage[64];
    char ssidTruncated[21] = {0};
    strncpy(ssidTruncated, ssid, 20);

    snprintf(displayMessage, sizeof(displayMessage), "Connecting to:\n%.20s\nAttempt %d/%d", ssidTruncated, attempt + 1, maxAttempts);
    showMessage("WiFi", displayMessage);

    Serial.printf("Attempting to connect to %s (Attempt %d/%d)\n", ssid, attempt + 1, maxAttempts);
    WiFi.begin(ssid, password);

    int timeout = 0;
    while (WiFi.status() != WL_CONNECTED && timeout < 20)
    {
      delay(500);
      Serial.print(".");
      timeout++;
    }

    if (WiFi.status() == WL_CONNECTED)
    {
      Serial.printf("\nConnected to %s\n", ssid);
      snprintf(displayMessage, sizeof(displayMessage), "Connected to:\n%.20s", ssidTruncated);
      showMessage("WiFi", displayMessage);
      delay(2000);
      return true;
    }

    Serial.printf("\nFailed to connect to %s (Attempt %d/%d)\n", ssid, attempt + 1, maxAttempts);
    WiFi.disconnect();
    delay(1000);
  }

  return false;
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
  const unsigned long UPDATE_INTERVAL = 100;

  unsigned long currentTime = millis();
  long epochTime = (long)now();

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

    int codeHeight = (SCREEN_HEIGHT - YELLOW_SECTION_HEIGHT) / ((TOTP_KEYS_COUNT + 1) / 2);
    int startY = YELLOW_SECTION_HEIGHT + codeHeight / 2;

    for (int i = 0; i < TOTP_KEYS_COUNT; i++)
    {
      int row = i / 2;
      int col = i % 2;

      int x = col * (SCREEN_WIDTH / 2);
      int y = startY + row * codeHeight;

      u8g2.setFont(u8g2_font_6x10_tr);
      int labelWidth = u8g2.getStrWidth(TOTP_KEYS[i].label);
      u8g2.drawStr(x + (SCREEN_WIDTH / 2 - labelWidth) / 2, y - 2, TOTP_KEYS[i].label);

      uint8_t hmacKey[32];
      int keyLength = base32Decode(TOTP_KEYS[i].secret, hmacKey, sizeof(hmacKey));
      TOTP totp(hmacKey, keyLength);
      char *code = totp.getCode(epochTime);

      u8g2.setFont(u8g2_font_profont17_tn);
      int codeWidth = u8g2.getStrWidth(code);
      u8g2.drawStr(x + (SCREEN_WIDTH / 2 - codeWidth) / 2, y + 12, code);
    }

    u8g2.sendBuffer();
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
