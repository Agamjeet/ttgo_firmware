#include "Companion.h"

#include "PixelScene.h"
#include "config.h"

#include <WiFi.h>
#include <ctime>
#include <cstring>

static TDisplayS3 *screen = nullptr;
static DisplayScene scene = {"heart", "", "heart"};
static DeskMode mode = DeskMode::Message;
static bool sceneKnown = false;
static bool sceneChanged = true;
static bool modeChanged = true;
static bool ntpStarted = false;
static int drawnSecond = -1;
static int drawnMinute = -1;
static bool clockShowingTime = false;
static bool clockStatusDrawn = false;
static bool frameDirty = true;
static lgfx::LGFX_Sprite *backgrounds[kDeskFaceCount] = {};
static char backgroundNames[kDeskFaceCount][40] = {};
static char lastActive[12] = "";
static uint32_t lastShow = 0;
static char clockLine[19] = "Boss";
static char photoCaption[23] = "";

static lgfx::LGFX_Sprite *canvas = nullptr;
static int animationIndex = 0;
static uint32_t arrivedAt = 0;
static uint32_t messageStarted = 0;
static char cachedTime[8] = "";
static char cachedDetail[40] = "";
static int revealCount = 0;
static uint32_t nextReveal = 0;

struct Key {
    int pin;
    int stable;
    int last;
    uint32_t changedAt;
    uint32_t pressedAt;
    bool cycles;
};

static Key prevKey = {kButtonPrev, HIGH, HIGH, 0, 0, false};
static Key nextKey = {kButtonNext, HIGH, HIGH, 0, 0, false};

static lgfx::LovyanGFX *target() {
    if (canvas != nullptr) {
        return canvas;
    }
    return screen;
}

static void present() {
    if (canvas != nullptr && screen != nullptr) {
        canvas->pushSprite(screen, 0, 0);
    }
}

static bool paintPhoto(int index) {
    if (backgrounds[index] != nullptr && screen != nullptr && backgrounds[index]->width() == screen->width()) {
        backgrounds[index]->pushSprite(target(), 0, 0);
        return true;
    }
    return false;
}

static void paintSecret(uint32_t now);

static void cycleColor(uint32_t now, int step) {
    const int count = PixelScene::kAnimationCount;
    animationIndex = (animationIndex + step + count) % count;
    arrivedAt = now;
    frameDirty = true;
    Serial.printf("companion: color %s\n", PixelScene::animationName(animationIndex));
}

static bool readClock(struct tm &timeinfo);

static const char *setupStatus() {
    if (WiFi.status() != WL_CONNECTED) {
        return "Waiting for Wi-Fi";
    }
    struct tm timeinfo;
    if (!readClock(timeinfo)) {
        return "Setting the clock";
    }
    return "";
}

static void drawWelcomeFrame(uint32_t now) {
    if (screen == nullptr) {
        return;
    }
    PixelScene::paintWelcome(*target(), screen->width(), screen->height(), now, animationIndex, setupStatus());
    paintSecret(now);
    present();
}

static void drawMessageFrame(uint32_t now) {
    if (screen == nullptr) {
        return;
    }
    const bool photo = paintPhoto(0);
    PixelScene::paintMessage(*target(), screen->width(), screen->height(), photo, scene.message, scene.mode,
                             scene.icon, revealCount, now, messageStarted, animationIndex);
    paintSecret(now);
    present();
}

static void ensureClock() {
    if (ntpStarted || WiFi.status() != WL_CONNECTED) {
        return;
    }
    configTzTime(COMPANION_TZ, COMPANION_NTP_1, COMPANION_NTP_2);
    ntpStarted = true;
    Serial.println("companion: asking for the time");
}

static bool readClock(struct tm &timeinfo) {
    time_t now = 0;
    time(&now);
    localtime_r(&now, &timeinfo);
    return timeinfo.tm_year > (2016 - 1900);
}

static void drawClockFrame(uint32_t now) {
    if (screen == nullptr) {
        return;
    }
    ensureClock();
    struct tm timeinfo;
    const bool ready = readClock(timeinfo);
    if (!ready && cachedTime[0] == '\0') {
        drawWelcomeFrame(now);
        return;
    }
    const char *shownTime = cachedTime;
    const char *shownDetail = cachedDetail;
    if (ready) {
        int hour = timeinfo.tm_hour % 12;
        if (hour == 0) {
            hour = 12;
        }
        char timeText[8];
        char date[28];
        char detail[40];
        snprintf(timeText, sizeof(timeText), "%d:%02d", hour, timeinfo.tm_min);
        strftime(date, sizeof(date), "%a %d %b", &timeinfo);
        snprintf(detail, sizeof(detail), "%s   %s", timeinfo.tm_hour < 12 ? "AM" : "PM", date);
        strncpy(cachedTime, timeText, sizeof(cachedTime) - 1);
        cachedTime[sizeof(cachedTime) - 1] = '\0';
        strncpy(cachedDetail, detail, sizeof(cachedDetail) - 1);
        cachedDetail[sizeof(cachedDetail) - 1] = '\0';
        shownTime = cachedTime;
        shownDetail = cachedDetail;
        clockShowingTime = true;
        drawnMinute = timeinfo.tm_hour * 60 + timeinfo.tm_min;
        drawnSecond = timeinfo.tm_sec;
    }
    const bool photo = paintPhoto(1);
    PixelScene::paintClock(*target(), screen->width(), screen->height(), photo, shownTime, shownDetail, clockLine,
                           false, now, arrivedAt, animationIndex);
    paintSecret(now);
    present();
}

static void drawAnimationFrame(uint32_t now) {
    if (screen == nullptr) {
        return;
    }
    const bool photo = paintPhoto(2);
    PixelScene::paintAnimation(*target(), screen->width(), screen->height(), photo, animationIndex, now, arrivedAt);
    paintSecret(now);
    present();
}

static void drawPhotoFrame(uint32_t now) {
    if (screen == nullptr) {
        return;
    }
    PixelScene::paintFrame(*target(), screen->width(), screen->height(), backgrounds[3], now, animationIndex,
                           photoCaption);
    paintSecret(now);
    present();
}

static DeskMode nextMode(DeskMode current) {
    if (current == DeskMode::Message) {
        return DeskMode::Clock;
    }
    if (current == DeskMode::Clock) {
        return DeskMode::Animation;
    }
    if (current == DeskMode::Animation) {
        return DeskMode::Photo;
    }
    return DeskMode::Message;
}

static DeskMode previousMode(DeskMode current) {
    if (current == DeskMode::Message) {
        return DeskMode::Photo;
    }
    if (current == DeskMode::Photo) {
        return DeskMode::Animation;
    }
    if (current == DeskMode::Clock) {
        return DeskMode::Message;
    }
    return DeskMode::Clock;
}

static const char *modeName(DeskMode value) {
    if (value == DeskMode::Clock) {
        return "clock";
    }
    if (value == DeskMode::Animation) {
        return "animation";
    }
    if (value == DeskMode::Photo) {
        return "photo";
    }
    return "message";
}

static const bool kSecretUp[] = {true, true, false, false, true, false, true, false, true, false};
static int secretPos = 0;
static uint32_t secretLast = 0;
static uint32_t secretUntil = 0;

static void noteSecret(uint32_t now, bool up) {
    if (secretLast != 0 && (uint32_t)(now - secretLast) > 1800) {
        secretPos = 0;
    }
    if (up == kSecretUp[secretPos]) {
        secretPos += 1;
        secretLast = now;
        if (secretPos == 10) {
            secretPos = 0;
            secretUntil = now + 7000;
            frameDirty = true;
            Serial.println("companion: secret");
        }
        return;
    }
    secretPos = up == kSecretUp[0] ? 1 : 0;
    secretLast = now;
}

static void paintSecret(uint32_t now) {
    if (screen != nullptr && (int32_t)(secretUntil - now) > 0) {
        PixelScene::paintWhisper(*target(), screen->width(), screen->height());
    }
}

static void pollKey(Key &key, uint32_t now, bool theme) {
    const int read = digitalRead(key.pin);
    if (read != key.last) {
        key.last = read;
        key.changedAt = now;
    }
    if ((uint32_t)(now - key.changedAt) < 35 || read == key.stable) {
        return;
    }
    key.stable = read;
    if (read != LOW) {
        return;
    }
    noteSecret(now, theme);
    if (theme) {
        cycleColor(now, 1);
        return;
    }
    Companion::setMode(nextMode(mode));
}

void Companion::begin(TDisplayS3 &display) {
    screen = &display;
    pinMode(kButtonPrev, INPUT_PULLUP);
    pinMode(kButtonNext, INPUT_PULLUP);
    canvas = new lgfx::LGFX_Sprite(screen);
    canvas->setPsram(true);
    canvas->setColorDepth(16);
    if (!canvas->createSprite(screen->width(), screen->height())) {
        Serial.println("companion: drawing without a back buffer");
        delete canvas;
        canvas = nullptr;
    }
    arrivedAt = millis();
    Serial.println("companion: GPIO14 changes the screen");
    Serial.println("companion: GPIO0 changes the theme");
    drawWelcomeFrame(arrivedAt);
    modeChanged = false;
    sceneChanged = false;
    drawnSecond = -1;
}

void Companion::setScene(const DisplayScene &next) {
    const bool different = strcmp(scene.mode, next.mode) != 0 || strcmp(scene.message, next.message) != 0 ||
                           strcmp(scene.icon, next.icon) != 0;
    scene = next;
    if (different) {
        sceneChanged = true;
    }
    if (next.message[0] != '\0') {
        sceneKnown = true;
    }
}

static void adoptBackground(int index, BackgroundFile &file) {
    if (strcmp(backgroundNames[index], file.name) == 0) {
        free(file.jpeg);
        file.jpeg = nullptr;
        return;
    }
    if (file.name[0] == '\0') {
        if (backgrounds[index] != nullptr) {
            backgrounds[index]->deleteSprite();
        }
        backgroundNames[index][0] = '\0';
        free(file.jpeg);
        file.jpeg = nullptr;
        frameDirty = true;
        return;
    }
    if (file.jpeg == nullptr) {
        return;
    }
    if (backgrounds[index] == nullptr) {
        backgrounds[index] = new lgfx::LGFX_Sprite(screen);
        backgrounds[index]->setPsram(true);
        backgrounds[index]->setColorDepth(16);
    }
    backgrounds[index]->deleteSprite();
    const int spriteW = index == 3 ? PixelScene::kFramePhotoW : screen->width();
    const int spriteH = index == 3 ? PixelScene::kFramePhotoH : screen->height();
    const bool drawn = backgrounds[index]->createSprite(spriteW, spriteH) &&
                       backgrounds[index]->drawJpg(file.jpeg, file.length, 0, 0);
    free(file.jpeg);
    file.jpeg = nullptr;
    if (!drawn) {
        backgrounds[index]->deleteSprite();
        backgroundNames[index][0] = '\0';
        Serial.println("companion: picture could not be drawn");
        frameDirty = true;
        return;
    }
    strncpy(backgroundNames[index], file.name, sizeof(backgroundNames[index]) - 1);
    backgroundNames[index][sizeof(backgroundNames[index]) - 1] = '\0';
    frameDirty = true;
    if (index == 1) {
        clockShowingTime = false;
        clockStatusDrawn = false;
        drawnSecond = -1;
    }
}

static void copyLine(char *dest, size_t size, const char *value, const char *fallback) {
    const char *source = value != nullptr && value[0] != '\0' ? value : fallback;
    strncpy(dest, source, size - 1);
    dest[size - 1] = '\0';
}

void Companion::apply(DeskFaces &faces) {
    setScene(faces.message);
    sceneKnown = true;
    copyLine(clockLine, sizeof(clockLine), faces.clockLine, "Boss");
    copyLine(photoCaption, sizeof(photoCaption), faces.photoCaption, "");
    frameDirty = true;
    for (int i = 0; i < kDeskFaceCount; ++i) {
        adoptBackground(i, faces.backgrounds[i]);
    }
    const bool picked = faces.show != 0 && faces.show != lastShow;
    if (picked) {
        lastShow = faces.show;
    }
    if (faces.active[0] != '\0' && (picked || strcmp(faces.active, lastActive) != 0)) {
        strncpy(lastActive, faces.active, sizeof(lastActive) - 1);
        lastActive[sizeof(lastActive) - 1] = '\0';
        if (strcmp(faces.active, "clock") == 0) {
            setMode(DeskMode::Clock);
        } else if (strcmp(faces.active, "animation") == 0) {
            setMode(DeskMode::Animation);
        } else if (strcmp(faces.active, "photo") == 0) {
            setMode(DeskMode::Photo);
        } else {
            setMode(DeskMode::Message);
        }
    }
}

void Companion::setMode(DeskMode next) {
    if (next == mode) {
        return;
    }
    mode = next;
    modeChanged = true;
    arrivedAt = millis();
    drawnSecond = -1;
    drawnMinute = -1;
    clockShowingTime = false;
    clockStatusDrawn = false;
    if (mode == DeskMode::Animation) {
        Serial.printf("companion: mode animation %s\n", PixelScene::animationName(animationIndex));
        return;
    }
    if (mode == DeskMode::Photo) {
        Serial.println("companion: mode photo");
        return;
    }
    Serial.printf("companion: mode %s\n", modeName(mode));
}

void Companion::loop() {
    const uint32_t now = millis();
    pollKey(nextKey, now, false);
    pollKey(prevKey, now, true);
    ensureClock();

    if (!sceneKnown) {
        static uint32_t nextFrame = 0;
        if (frameDirty || (int32_t)(now - nextFrame) >= 0) {
            drawWelcomeFrame(now);
            nextFrame = now + 90;
            frameDirty = false;
        }
        return;
    }

    if (mode == DeskMode::Photo) {
        static uint32_t nextFrame = 0;
        if (modeChanged || frameDirty || (int32_t)(now - nextFrame) >= 0) {
            drawPhotoFrame(now);
            nextFrame = now + 90;
            modeChanged = false;
            sceneChanged = false;
            frameDirty = false;
        }
        return;
    }

    if (mode == DeskMode::Animation) {
        static uint32_t nextFrame = 0;
        if (modeChanged || frameDirty || (int32_t)(now - nextFrame) >= 0) {
            drawAnimationFrame(now);
            nextFrame = now + 90;
            modeChanged = false;
            sceneChanged = false;
            frameDirty = false;
        }
        return;
    }

    if (mode == DeskMode::Clock) {
        static uint32_t nextFrame = 0;
        if (modeChanged || frameDirty || (int32_t)(now - nextFrame) >= 0) {
            drawClockFrame(now);
            nextFrame = now + 90;
            modeChanged = false;
            sceneChanged = false;
            frameDirty = false;
        }
        return;
    }

    if (modeChanged || sceneChanged) {
        messageStarted = now;
        revealCount = 0;
        nextReveal = now + PixelScene::kLetterOpenMs;
        modeChanged = false;
        sceneChanged = false;
    }
    frameDirty = false;
    const int total = (int)strlen(scene.message);
    if (revealCount < total && (int32_t)(now - nextReveal) >= 0) {
        revealCount += 1;
        nextReveal = now + 70;
    }
    static uint32_t nextFrame = 0;
    if ((int32_t)(now - nextFrame) >= 0) {
        drawMessageFrame(now);
        nextFrame = now + 90;
    }
}
