// config.h
#ifndef CONFIG_H
#define CONFIG_H
#include <EEPROM.h>

#define EEPROM_SIZE 512
#define LAST_WIFI_SSID_ADDR 0
#define LAST_WIFI_PASS_ADDR 64

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

// TOTP Secret Keys (Base32 encoded)
struct TOTPKey {
    const char* label;
    const char* secret;
};

const TOTPKey TOTP_KEYS[] = {
    {"SRVC1", "YOUR_BASE32_ENCODED_SECRET"},
    {"SRVC2", "YOUR_BASE32_ENCODED_SECRET"},
    // Add more TOTP keys as needed
};

const int TOTP_KEYS_COUNT = sizeof(TOTP_KEYS) / sizeof(TOTP_KEYS[0]);

#endif
