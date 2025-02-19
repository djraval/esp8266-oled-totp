#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include <vector>

// EEPROM Configuration
#define EEPROM_SIZE 512
#define LAST_WIFI_SSID_ADDR 0
#define LAST_WIFI_PASS_ADDR 64

#define MAX_SSID_LENGTH 32
#define MAX_PASS_LENGTH 64

// Wi-Fi credentials structure
struct WiFiCredentials {
    const char* ssid;
    const char* password;
};

// Known networks
const WiFiCredentials WIFI_CREDS[] = {
    {"SSID1", "PASSWORD1"},
    {"SSID2", "PASSWORD2"},
    // Add more networks as needed
};

const int WIFI_CREDS_COUNT = sizeof(WIFI_CREDS) / sizeof(WIFI_CREDS[0]);

// Network info structure for scanning
struct NetworkInfo {
    String ssid;
    int32_t rssi;
    int configIndex;
};

// Function declarations
void setupWiFi();
bool scanAndConnectWiFi();
bool connectToWiFi(const char *ssid, const char *password, int maxAttempts);
void saveWiFiCredentials(const char *ssid, const char *password);
bool isNetworkInConfig(const char* ssid);

#endif // WIFI_MANAGER_H
