#include "display.h"
// Display Constants
const int SCREEN_WIDTH = 128;
const int SCREEN_HEIGHT = 64;
const int YELLOW_SECTION_HEIGHT = 16;

// Pin Definitions for Display
const int SCL_PIN = 12;  // (D5/GPIO12)
const int SDA_PIN = 14;  // (D6/GPIO14)

// Display object
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, SCL_PIN, SDA_PIN, U8X8_PIN_NONE);

void setupDisplay() {
    Wire.setClock(400000);  // Set I2C to fast mode (400kHz)
    u8g2.begin();
    u8g2.setPowerSave(0);  // Ensure display is always on
    u8g2.setContrast(255); // Maximum contrast
}

void showMessage(const char *header, const char *body) {
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
        while (token != NULL && lineCount < MAX_LINES) {
            lines[lineCount++] = token;
            token = strtok(NULL, "\n");
        }

        u8g2.setFont(lineCount == 1 && bodyLength <= 6 ? u8g2_font_10x20_tf : u8g2_font_7x13_tf);

        int16_t lineHeight = (u8g2.getMaxCharHeight() > 13) ? 20 : 13;
        int16_t totalTextHeight = lineCount * lineHeight;
        int16_t startY = YELLOW_SECTION_HEIGHT + ((SCREEN_HEIGHT - YELLOW_SECTION_HEIGHT - totalTextHeight) / 2) + lineHeight;

        for (int i = 0; i < lineCount; i++) {
            int16_t lineWidth = u8g2.getStrWidth(lines[i]);
            int16_t lineX = (SCREEN_WIDTH - lineWidth) / 2;
            u8g2.drawStr(lineX, startY + (i * lineHeight), lines[i]);
        }

        u8g2.sendBuffer();
        free(bodyTemp);
    }
}

void renderOTPDisplay(const DisplayState& state) {
    u8g2.clearBuffer();

    // Draw progress bar
    const int PROGRESS_BAR_WIDTH = 120;
    const int PROGRESS_BAR_HEIGHT = 8;
    const int PROGRESS_BAR_X = (SCREEN_WIDTH - PROGRESS_BAR_WIDTH) / 2;
    const int PROGRESS_BAR_Y = (YELLOW_SECTION_HEIGHT - PROGRESS_BAR_HEIGHT) / 2;

    u8g2.drawFrame(PROGRESS_BAR_X, PROGRESS_BAR_Y, PROGRESS_BAR_WIDTH, PROGRESS_BAR_HEIGHT);
    int fillWidth = (PROGRESS_BAR_WIDTH * state.progressPercentage) / 100;
    u8g2.drawBox(PROGRESS_BAR_X, PROGRESS_BAR_Y, fillWidth, PROGRESS_BAR_HEIGHT);

    // Grid layout
    const int COLS = (state.totalItems <= 4) ? 2 : 3;
    const int ROWS = (state.totalItems <= 4) ? 2 : 2;
    const int CELL_WIDTH = SCREEN_WIDTH / COLS;
    const int CELL_HEIGHT = (SCREEN_HEIGHT - YELLOW_SECTION_HEIGHT) / ROWS;

    const uint8_t* labelFont = (state.totalItems <= 4) ? u8g2_font_6x10_tr : u8g2_font_5x7_tr;
    const uint8_t* codeFont = (state.totalItems <= 4) ? u8g2_font_profont17_tn : u8g2_font_profont11_tn;

    int startY = YELLOW_SECTION_HEIGHT + CELL_HEIGHT / 2;

    for (int i = 0; i < state.totalItems; i++) {
        int row = i / COLS;
        int col = i % COLS;
        int x = col * CELL_WIDTH;
        int y = startY + row * CELL_HEIGHT;

        // Draw label
        u8g2.setFont(labelFont);
        int labelWidth = u8g2.getStrWidth(state.entries[i].abbreviatedLabel);
        u8g2.drawStr(x + (CELL_WIDTH - labelWidth) / 2, y - 2, state.entries[i].abbreviatedLabel);

        // Draw code
        u8g2.setFont(codeFont);
        int codeWidth = u8g2.getStrWidth(state.entries[i].code);
        int yOffset = (state.totalItems <= 4) ? 12 : 10;
        u8g2.drawStr(x + (CELL_WIDTH - codeWidth) / 2, y + yOffset, state.entries[i].code);
    }

    u8g2.sendBuffer();
}
