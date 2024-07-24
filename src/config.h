// config.h
#ifndef CONFIG_H
#define CONFIG_H

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
