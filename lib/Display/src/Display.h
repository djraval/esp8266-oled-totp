#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>

// Display Constants
extern const int SCREEN_WIDTH;
extern const int SCREEN_HEIGHT;
extern const int YELLOW_SECTION_HEIGHT;

// Display object
extern U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2;

// Data structures for OTP display
struct OTPEntry {
    char abbreviatedLabel[10];
    char code[7];  // 6 digits + null terminator
};

struct DisplayState {
    int progressPercentage;
    int totalItems;
    OTPEntry* entries;
};

// Display Functions
void setupDisplay();
void showMessage(const char *header, const char *body);
void renderOTPDisplay(const DisplayState& state);
#endif // DISPLAY_H