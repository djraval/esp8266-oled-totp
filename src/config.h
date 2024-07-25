// config.h
#ifndef CONFIG_H
#define CONFIG_H
#include <EEPROM.h>

#define EEPROM_SIZE 512
#define LAST_WIFI_SSID_ADDR 0
#define LAST_WIFI_PASS_ADDR 64  // Increased from 32 to 64

#define MAX_SSID_LENGTH 32
#define MAX_PASS_LENGTH 64

// Wi-Fi credentials
struct WiFiCredentials {
    const char* ssid;
    const char* password;
};

const WiFiCredentials WIFI_CREDS[] = {
    {"SSID1", "PASSWORD1"},
    {"SSID2", "PASSWORD2"},
    // Add more networks as needed
};

const int WIFI_CREDS_COUNT = sizeof(WIFI_CREDS) / sizeof(WIFI_CREDS[0]);

// TOTP Secret Key (Base32 encoded)
const char *TOTP_SECRET = "YOUR_BASE32_ENCODED_SECRET";

#endif
