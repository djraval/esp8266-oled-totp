// config.h
#ifndef CONFIG_H
#define CONFIG_H

// TOTP Secret Keys (Base32 encoded)
struct TOTPKey {
    const char* label;
    const char* secret;
};

const TOTPKey TOTP_KEYS[] = {
    {"SRVC1", "YOUR_BASE32_ENCODED_SECRET"},
    {"SRVC2", "YOUR_BASE32_ENCODED_SECRET"},
    // Add more TOTP keys as needed (up to 6)
};

const int TOTP_KEYS_COUNT = sizeof(TOTP_KEYS) / sizeof(TOTP_KEYS[0]);

#endif
