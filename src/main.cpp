#include <Arduino.h>

#include "Companion.h"
#include "OtaManager.h"
#include "TDisplayS3.hpp"
#include "firmware_version.h"

static TDisplayS3 tft;

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
    Companion::begin(tft);
    Serial.println("Welcome screen drawn.");
    OtaManager::begin();
}

void loop() {
    DeskFaces faces;
    if (OtaManager::takeFaces(faces)) {
        Companion::apply(faces);
    }
    Companion::loop();
}
