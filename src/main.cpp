#include <Arduino.h>

#include "OtaManager.h"
#include "TDisplayS3.hpp"
#include "firmware_version.h"

static TDisplayS3 tft;

static const uint16_t kHeartRows[] = {
    0b011000011000,
    0b111100111100,
    0b111111111110,
    0b111111111110,
    0b111111111110,
    0b011111111100,
    0b001111111000,
    0b000111110000,
    0b000011100000,
    0b000001000000,
};

static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

static void drawHeart(int left, int top, int scale, uint16_t color) {
    const int rows = sizeof(kHeartRows) / sizeof(kHeartRows[0]);
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < 12; ++x) {
            if (kHeartRows[y] & (1 << (11 - x))) {
                tft.fillRect(left + x * scale, top + y * scale, scale, scale, color);
            }
        }
    }
}

static void drawWelcome() {
    const uint16_t background = rgb565(42, 16, 36);
    const uint16_t heart = rgb565(255, 77, 109);
    const uint16_t text = rgb565(255, 244, 247);

    tft.fillScreen(background);

    const int scale = 7;
    const int heartW = 12 * scale;
    const int heartH = 10 * scale;
    drawHeart((tft.width() - heartW) / 2, 16, scale, heart);

    tft.setTextColor(text, background);
    tft.setFont(&fonts::Font4);
    const char *message = "Made just for you";
    if (tft.textWidth(message) > tft.width() - 16) {
        tft.setFont(&fonts::Font2);
    }
    tft.setTextDatum(lgfx::middle_center);
    tft.drawString(message, tft.width() / 2, 16 + heartH + 28);
}

void setup() {
    Serial.begin(115200);
    delay(400);
    Serial.println();
    Serial.println("Love Display bring-up");
    Serial.printf("Firmware version %s\n", FIRMWARE_VERSION);

    if (!tft.begin()) {
        Serial.printf("Display init failed. Reported size %d x %d (expected 320 x 170).\n",
                      tft.width(), tft.height());
        OtaManager::onDisplayFailed();
        return;
    }

    Serial.printf("Display ready: %d x %d landscape, ST7789.\n", tft.width(), tft.height());
    drawWelcome();
    Serial.println("Welcome screen drawn.");
    OtaManager::begin();
}

void loop() {}
