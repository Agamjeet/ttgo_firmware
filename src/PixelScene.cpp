#include "PixelScene.h"

#include <cstring>

namespace PixelScene {
namespace {

uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
    return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

// Night is the default. The animation button swaps these for sunny, sunset, and sea.
uint16_t kSkyTop = rgb(15, 10, 35);
uint16_t kSkyMid = rgb(31, 18, 58);
uint16_t kSkyLow = rgb(49, 28, 78);
const uint16_t kPaper = rgb(255, 244, 248);
const uint16_t kInk = rgb(42, 28, 52);
const uint16_t kInkShadow = rgb(210, 167, 195);
const uint16_t kCream = rgb(255, 245, 221);
uint16_t kPink = rgb(255, 128, 169);
const uint16_t kRose = rgb(222, 89, 135);
const uint16_t kBlush = rgb(255, 157, 183);
uint16_t kMoon = rgb(255, 229, 164);
uint16_t kLavender = rgb(195, 171, 237);
uint16_t kCloud = rgb(204, 190, 231);
const uint16_t kOutline = rgb(28, 67, 57);
const uint16_t kDino = rgb(119, 207, 153);
const uint16_t kDinoLight = rgb(164, 232, 181);
const uint16_t kDinoDark = rgb(72, 157, 119);
const uint16_t kBelly = rgb(245, 237, 190);
const uint16_t kWhite = rgb(255, 252, 248);
uint16_t kShadow = rgb(17, 9, 29);
const uint16_t kSun = rgb(255, 220, 112);
const uint16_t kSunSoft = rgb(255, 236, 170);
const uint16_t kSeed = rgb(112, 70, 30);

// Mood is shared by every scene so the mascot feels like one character.
enum class Mood { Normal, Blink, Happy, Sleepy, Blush, Surprised };

struct Star { int x; int y; int phase; };
const Star kStars[] = {
    {10, 8, 0}, {34, 20, 3}, {62, 6, 1}, {108, 14, 4}, {156, 8, 2},
    {214, 16, 5}, {258, 6, 1}, {292, 18, 3}, {306, 8, 0},
    {18, 156, 2}, {300, 150, 4}, {78, 160, 1}, {236, 158, 5},
};

int settle(uint32_t now, uint32_t arrived) {
    const uint32_t age = now - arrived;
    if (age >= 320) return 0;
    return static_cast<int>((320 - age) * 8 / 320);
}

Mood ambientMood(uint32_t now, uint32_t react) {
    const uint32_t since = now - react;
    if (since < 240) return Mood::Surprised;
    if (since < 780) return Mood::Blush;
    if (since < 1200) return Mood::Happy;
    if ((now % 3600) < 140) return Mood::Blink;
    return Mood::Normal;
}

void drawSky(lgfx::LovyanGFX &gfx, int width, int height, bool photo) {
    if (photo) return;
    gfx.fillScreen(kSkyTop);
    gfx.fillRect(0, height / 3, width, height / 3, kSkyMid);
    gfx.fillRect(0, (height * 2) / 3, width, height - (height * 2) / 3, kSkyLow);
}

void drawStars(lgfx::LovyanGFX &gfx, uint32_t now, bool cornersOnly) {
    for (const Star &star : kStars) {
        if (cornersOnly && star.x > 48 && star.x < 270 && star.y > 24 && star.y < 150) continue;
        const uint32_t tick = (now / (180 + static_cast<uint32_t>(star.phase) * 70) +
                               static_cast<uint32_t>(star.phase)) % 5;
        if (tick == 0) continue;
        const uint16_t color = (star.phase % 3 == 0) ? kMoon :
                               (star.phase % 3 == 1) ? kCream : kLavender;
        const int scale = tick > 3 ? 2 : 1;
        gfx.fillRect(star.x, star.y, scale, scale, color);
        if (tick > 2) {
            gfx.drawPixel(star.x - 2, star.y, color);
            gfx.drawPixel(star.x + 2, star.y, color);
            gfx.drawPixel(star.x, star.y - 2, color);
            gfx.drawPixel(star.x, star.y + 2, color);
        }
    }
}

void drawMoon(lgfx::LovyanGFX &gfx, int x, int y, int scale) {
    gfx.fillCircle(x + 8 * scale, y + 8 * scale, 7 * scale, kMoon);
    gfx.fillCircle(x + 12 * scale, y + 5 * scale, 6 * scale, kSkyTop);
    gfx.fillCircle(x + 5 * scale, y + 12 * scale, scale, kWhite);
}

void drawHeartAt(lgfx::LovyanGFX &gfx, int x, int y, int scale) {
    // Tiny blocky heart, intentionally pixel-like.
    gfx.fillRect(x + scale, y, 2 * scale, scale, kPink);
    gfx.fillRect(x + 4 * scale, y, 2 * scale, scale, kPink);
    gfx.fillRect(x, y + scale, 7 * scale, 2 * scale, kPink);
    gfx.fillRect(x + scale, y + 3 * scale, 5 * scale, scale, kPink);
    gfx.fillRect(x + 2 * scale, y + 4 * scale, 3 * scale, scale, kPink);
    gfx.fillRect(x + 3 * scale, y + 5 * scale, scale, scale, kPink);
}

void drawSparkle(lgfx::LovyanGFX &gfx, int x, int y, int scale, uint16_t color) {
    gfx.fillRect(x + 2 * scale, y, scale, 5 * scale, color);
    gfx.fillRect(x, y + 2 * scale, 5 * scale, scale, color);
    gfx.fillRect(x + scale, y + scale, 3 * scale, 3 * scale, color);
}

void drawRisingHearts(lgfx::LovyanGFX &gfx, uint32_t now, int count,
                      int x0, int x1, int y0, int y1) {
    for (int i = 0; i < count; ++i) {
        const uint32_t period = 2600 + static_cast<uint32_t>(i) * 380;
        const uint32_t age = (now + static_cast<uint32_t>(i) * 430) % period;
        if (age > period - 180) continue;
        const int span = x1 - x0;
        const int x = x0 + (i * 37 + static_cast<int>(age / 180)) % (span > 1 ? span : 1);
        const int travel = y1 - y0;
        const int y = y1 - static_cast<int>(age * static_cast<uint32_t>(travel) / period);
        if (y >= y0) drawHeartAt(gfx, x, y, 1);
    }
}

void drawBurst(lgfx::LovyanGFX &gfx, int cx, int cy, uint32_t now, uint32_t arrived) {
    const uint32_t age = now - arrived;
    if (age > 880) return;
    static const int steps[][2] = {{-4,-2},{4,-2},{-2,-4},{2,-3},{-5,1},{5,1},{-2,3},{3,2}};
    for (const auto &step : steps) {
        drawHeartAt(gfx, cx + step[0] * static_cast<int>(age) / 70,
                    cy + step[1] * static_cast<int>(age) / 80, 1);
    }
}

void drawDots(lgfx::LovyanGFX &gfx, int width, int height, int index) {
    const int gap = 12;
    const int total = kAnimationCount * gap;
    int x = (width - total) / 2;
    for (int i = 0; i < kAnimationCount; ++i) {
        const bool on = i == index;
        gfx.fillRoundRect(x, height - 7, on ? 8 : 3, 3, 1, on ? kPink : kDinoDark);
        x += gap;
    }
}
// . transparent   O outline   D green   L light green   K dark green   B belly
// One character is one pixel, drawn 2x2 on screen at scale 1. Facing right.
const char *const kSprite[] = {
    ".........................OOOOOOO",
    "........................OLLLLLLLOOO",
    "........................OLLLLLLLLKO",
    "........................OLLLLLLLLLO",
    "........................OLLLLLLLLLO",
    "........................OKDDDLLOOOO",
    ".......................OKDDDDLLO",
    ".......................OKDDDDLLO",
    "......................OKDDDDDLLO",
    "......................OKDDDDDLLO",
    ".....................OKDDDDDDLLO",
    ".....................OKDDDDDDLLO",
    "....................OKDDDDDDDLLO",
    "............OOOOOOOOKDDDDDDDDLLOOO",
    "..........OODDKKDDDDKKDDDDKKDDDLLO",
    "........OODDDDDDDDDDDDDDDDDDDDDDLLO",
    "OOOO....ODDDDDDDDDDDDDDDDDDDDDDDLLO",
    "OLDO...ODDDDDDDDDDDDDDDDDDDDDDDDLLO",
    "OLDO..ODDDDDDDDDDDDDDDDDDDDDDDDDLLO",
    "OLDO..ODDDDDDDDBBBBBBBBBBBBBDDDDLLO",
    "OLDO..ODDDDDDBBBBBBBBBBBBBBBBBDDLLO",
    "OLDOOODDDDDDDDBBBBBBBBBBBBBBBDDDLO",
    ".ODDDKKKKKKKKKKDDDDDDDDDDDDDDDLLO",
    "..OKKKKKKKKKKKKOKKDDDDOOOODDDDLLO",
    "...OOOOOOOOOOOOOKKDDDDO..ODDDDLLO",
    "...............OKKDDDDO..ODDDDLLO",
    "...............OKKDDDDO..ODDDDLLO",
    "...............OKKDDDDO..ODDDDLLO",
    "...............OLLLLLLO..OLLLLLLO",
    "...............OOOOOOOO..OOOOOOOO",
};
constexpr int kSpriteRows = sizeof(kSprite) / sizeof(kSprite[0]);
constexpr int kSpriteW = 35;

uint16_t spriteColor(char pixel) {
    switch (pixel) {
    case 'O':
        return kOutline;
    case 'D':
        return kDino;
    case 'L':
        return kDinoLight;
    case 'K':
        return kDinoDark;
    case 'B':
        return kBelly;
    default:
        return kDino;
    }
}

void drawBrachiosaurus(lgfx::LovyanGFX &gfx, int x, int y, int scale, Mood mood, bool faceLeft = false,
                       int bob = 0) {
    const int p = scale * 2;
    auto block = [&](int col, int row, int w, int h, uint16_t color) {
        const int left = faceLeft ? kSpriteW - col - w : col;
        gfx.fillRect(x + left * p, y + (row + bob) * p, w * p, h * p, color);
    };
    for (int r = 0; r < kSpriteRows; ++r) {
        const char *line = kSprite[r];
        int n = static_cast<int>(strlen(line));
        if (n > kSpriteW) {
            n = kSpriteW;
        }
        for (int c = 0; c < n;) {
            const char ch = line[c];
            int end = c;
            while (end < n && line[end] == ch) {
                ++end;
            }
            if (ch != '.') {
                block(c, r, end - c, 1, spriteColor(ch));
            }
            c = end;
        }
    }
    if (mood == Mood::Blink || mood == Mood::Sleepy) {
        block(28, 3, 2, 1, kOutline);
    } else if (mood == Mood::Happy) {
        block(27, 3, 1, 1, kOutline);
        block(28, 2, 2, 1, kOutline);
        block(30, 3, 1, 1, kOutline);
    } else if (mood == Mood::Surprised) {
        block(27, 2, 3, 3, kWhite);
        block(28, 3, 1, 1, kOutline);
    } else {
        block(28, 2, 2, 2, kOutline);
        block(28, 2, 1, 1, kWhite);
    }
    if (mood == Mood::Blush || mood == Mood::Happy) {
        block(26, 4, 2, 1, kBlush);
    }
}

void shadowText(lgfx::LovyanGFX &gfx, const char *text, int x, int y,
                uint16_t shadow, uint16_t color) {
    gfx.setTextColor(shadow);
    gfx.drawString(text, x + 1, y + 2);
    gfx.setTextColor(color);
    gfx.drawString(text, x, y);
}

void drawShootingStar(lgfx::LovyanGFX &gfx, uint32_t now, int yBase, uint32_t period) {
    const uint32_t age = now % period;
    if (age > 760) return;
    const int head = -16 + static_cast<int>(age * 360 / 760);
    for (int i = 0; i < 5; ++i) {
        const int sx = head - i * 8;
        const int sy = yBase + i;
        if (sx < -4 || sx > 324) continue;
        if (i == 0) drawSparkle(gfx, sx - 2, sy - 2, 1, kCream);
        else gfx.fillRect(sx, sy, 2, 2, i < 3 ? kCream : kLavender);
    }
}

void drawClouds(lgfx::LovyanGFX &gfx, int width, int y, uint32_t now) {
    const int drift = static_cast<int>((now / 50) % static_cast<uint32_t>(width + 80)) - 40;
    gfx.fillRoundRect(drift, y + 4, 56, 12, 6, kCloud);
    gfx.fillCircle(drift + 15, y + 4, 10, kCloud);
    gfx.fillCircle(drift + 31, y, 13, kCloud);
    gfx.fillCircle(drift + 43, y + 5, 9, kCloud);
    gfx.fillRoundRect(drift - 120, y + 10, 44, 9, 5, kLavender);
}

void drawMiniFlower(lgfx::LovyanGFX &gfx, int x, int y) {
    gfx.fillRect(x + 6, y + 11, 2, 7, kDinoDark);
    const int cx = x + 7;
    const int cy = y + 6;
    gfx.fillCircle(cx, cy - 5, 3, kSun);
    gfx.fillCircle(cx, cy + 4, 2, kSunSoft);
    gfx.fillCircle(cx - 5, cy, 3, kSun);
    gfx.fillCircle(cx + 5, cy, 3, kSunSoft);
    gfx.fillCircle(cx, cy, 3, kSeed);
}

void drawMiniSun(lgfx::LovyanGFX &gfx, int x, int y) {
    gfx.fillRect(x + 6, y, 2, 4, kSun);
    gfx.fillRect(x + 6, y + 12, 2, 4, kSun);
    gfx.fillRect(x, y + 6, 4, 2, kSun);
    gfx.fillRect(x + 12, y + 6, 4, 2, kSun);
    gfx.fillCircle(x + 7, y + 7, 5, kSun);
    gfx.fillCircle(x + 7, y + 7, 2, kSunSoft);
}

void drawMiniMoon(lgfx::LovyanGFX &gfx, int x, int y) {
    gfx.fillCircle(x + 7, y + 7, 6, kMoon);
    gfx.fillCircle(x + 11, y + 4, 5, kPaper);
}

void drawMiniNote(lgfx::LovyanGFX &gfx, int x, int y) {
    gfx.fillRect(x + 8, y + 1, 2, 11, kPink);
    gfx.fillRect(x + 8, y + 1, 6, 2, kPink);
    gfx.fillCircle(x + 5, y + 12, 3, kRose);
}

void drawStamp(lgfx::LovyanGFX &gfx, const char *style, const char *icon, int x, int y) {
    if (style != nullptr && strcmp(style, "text") == 0) return;
    const char *which = (style != nullptr && strcmp(style, "heart") == 0) ? "heart" : icon;
    if (which != nullptr && strcmp(which, "star") == 0) drawSparkle(gfx, x, y, 2, kMoon);
    else if (which != nullptr && strcmp(which, "sparkle") == 0) drawSparkle(gfx, x, y, 2, kLavender);
    else if (which != nullptr && strcmp(which, "smile") == 0) drawBrachiosaurus(gfx, x - 10, y - 5, 1, Mood::Happy);
    else if (which != nullptr && strcmp(which, "flower") == 0) drawMiniFlower(gfx, x, y);
    else if (which != nullptr && strcmp(which, "sun") == 0) drawMiniSun(gfx, x, y);
    else if (which != nullptr && strcmp(which, "moon") == 0) drawMiniMoon(gfx, x, y);
    else if (which != nullptr && strcmp(which, "note") == 0) drawMiniNote(gfx, x, y);
    else drawHeartAt(gfx, x, y, 2);
}

void drawEnvelope(lgfx::LovyanGFX &gfx, int x, int y, int lift) {
    gfx.fillRoundRect(x, y + 16, 104, 58, 6, kRose);
    gfx.fillRoundRect(x + 5, y + 24, 94, 44, 4, kPaper);
    const int tip = y + 20 - lift;
    gfx.fillTriangle(x + 4, y + 22, x + 52, tip, x + 100, y + 22, kLavender);
    gfx.fillTriangle(x + 14, y + 22, x + 52, tip + 16, x + 90, y + 22, kPink);
    drawHeartAt(gfx, x + 45, y + 36, 2);
}

int wrapLines(lgfx::LovyanGFX &gfx, const char *text, char lines[][48], int maxLines, int maxWidth) {
    int count = 0;
    int cursor = 0;
    const int len = static_cast<int>(strlen(text));
    while (cursor < len && count < maxLines) {
        while (cursor < len && text[cursor] == ' ') {
            ++cursor;
        }
        if (cursor >= len) {
            break;
        }
        int end = cursor;
        while (end < len && text[end] != ' ') {
            ++end;
        }
        int length = end - cursor;
        if (length > 39) {
            length = 39;
        }
        char trial[48];
        memcpy(trial, text + cursor, static_cast<size_t>(length));
        trial[length] = '\0';
        int last = (length < end - cursor) ? cursor + length : end;
        int scan = end;
        while (scan < len && length < 39) {
            while (scan < len && text[scan] == ' ') {
                ++scan;
            }
            if (scan >= len) {
                break;
            }
            int wordEnd = scan;
            while (wordEnd < len && text[wordEnd] != ' ') {
                ++wordEnd;
            }
            const int nextLen = length + 1 + (wordEnd - scan);
            if (nextLen > 39) {
                break;
            }
            char candidate[48];
            memcpy(candidate, trial, static_cast<size_t>(length));
            candidate[length] = ' ';
            memcpy(candidate + length + 1, text + scan, static_cast<size_t>(wordEnd - scan));
            candidate[nextLen] = '\0';
            if (gfx.textWidth(candidate) > maxWidth) {
                break;
            }
            memcpy(trial, candidate, static_cast<size_t>(nextLen) + 1);
            length = nextLen;
            last = wordEnd;
            scan = wordEnd;
        }
        memcpy(lines[count], trial, static_cast<size_t>(length) + 1);
        ++count;
        cursor = last;
    }
    return count;
}

void drawRevealed(lgfx::LovyanGFX &gfx, const char *text, int visible, int left, int top, int boxW, int boxH) {
    if (text == nullptr || text[0] == '\0' || visible <= 0) {
        return;
    }
    const int inner = boxW - 8;
    char lines[5][48] = {};
    const lgfx::GFXfont *font = &fonts::FreeSansBold18pt7b;
    int lineHeight = 34;
    gfx.setFont(font);
    int lineCount = 1;
    if (gfx.textWidth(text) <= inner && strlen(text) <= 39) {
        strncpy(lines[0], text, sizeof(lines[0]) - 1);
    } else {
        font = &fonts::FreeSansBold12pt7b;
        lineHeight = 24;
        gfx.setFont(font);
        lineCount = wrapLines(gfx, text, lines, 3, inner);
        if (lineCount >= 3) {
            font = &fonts::FreeSansBold9pt7b;
            lineHeight = 18;
            gfx.setFont(font);
            lineCount = wrapLines(gfx, text, lines, 5, inner);
        }
    }
    if (lineCount <= 0) {
        return;
    }
    gfx.setTextDatum(lgfx::middle_left);
    gfx.setFont(font);
    int y = top + boxH / 2 - ((lineCount - 1) * lineHeight) / 2;
    int shown = 0;
    for (int line = 0; line < lineCount; ++line) {
        if (y > top + boxH - 4) {
            break;
        }
        int x = left + (boxW - gfx.textWidth(lines[line])) / 2;
        const int n = static_cast<int>(strlen(lines[line]));
        for (int i = 0; i < n && shown < visible; ++i, ++shown) {
            char glyph[2] = {lines[line][i], '\0'};
            if (glyph[0] != ' ') {
                shadowText(gfx, glyph, x, y, kInkShadow, kInk);
            }
            x += gfx.textWidth(glyph);
        }
        y += lineHeight;
    }
}

void drawSunflower(lgfx::LovyanGFX &gfx, int x, int y) {
    gfx.fillRect(x + 12, y + 22, 3, 24, kDinoDark);
    gfx.fillTriangle(x + 12, y + 28, x - 2, y + 38, x + 12, y + 42, kDino);
    gfx.fillTriangle(x + 15, y + 32, x + 30, y + 42, x + 15, y + 46, kDinoLight);
    const int cx = x + 13;
    const int cy = y + 13;
    gfx.fillCircle(cx, cy - 10, 6, kSun);
    gfx.fillCircle(cx, cy + 10, 6, kSunSoft);
    gfx.fillCircle(cx - 10, cy, 6, kSun);
    gfx.fillCircle(cx + 10, cy, 6, kSunSoft);
    gfx.fillCircle(cx - 7, cy - 7, 5, kSunSoft);
    gfx.fillCircle(cx + 7, cy - 7, 5, kSun);
    gfx.fillCircle(cx - 7, cy + 7, 5, kSun);
    gfx.fillCircle(cx + 7, cy + 7, 5, kSunSoft);
    gfx.fillCircle(cx, cy, 5, kSeed);
    gfx.fillRect(cx - 2, cy - 2, 2, 2, kSunSoft);
}

void drawSunflowers(lgfx::LovyanGFX &gfx, int width, int height, uint32_t now) {
    const int sway = static_cast<int>((now / 480) % 3) - 1;
    const int base = height - 56;
    drawSunflower(gfx, -4, base - 18 + sway);
    drawSunflower(gfx, 34, base + 2 - sway);
    drawSunflower(gfx, 74, base - 12 + sway);
    drawSunflower(gfx, 116, base + 4 - sway);
    drawSunflower(gfx, 158, base - 8 + sway);
    drawSunflower(gfx, 200, base + 1 - sway);
    drawSunflower(gfx, 242, base - 16 + sway);
    drawSunflower(gfx, width - 34, base - 2 - sway);
}

void drawDaySun(lgfx::LovyanGFX &gfx, int x, int y) {
    gfx.fillRect(x + 11, y, 4, 7, kSun);
    gfx.fillRect(x + 11, y + 23, 4, 7, kSun);
    gfx.fillRect(x, y + 11, 7, 4, kSun);
    gfx.fillRect(x + 23, y + 11, 7, 4, kSun);
    gfx.fillRect(x + 4, y + 4, 4, 4, kSunSoft);
    gfx.fillRect(x + 20, y + 4, 4, 4, kSunSoft);
    gfx.fillRect(x + 4, y + 20, 4, 4, kSunSoft);
    gfx.fillRect(x + 20, y + 20, 4, 4, kSunSoft);
    gfx.fillCircle(x + 13, y + 13, 9, kSun);
    gfx.fillCircle(x + 13, y + 13, 5, kSunSoft);
}

void sceneGarden(lgfx::LovyanGFX &gfx, int width, int height, uint32_t now, uint32_t arrived, bool day, bool photo) {
    static const int hop[] = {0, 0, -2, -4, -2, 0, 0, 0};
    const int bob = hop[(now / 140) % 8];
    if (!photo && day) {
        drawDaySun(gfx, width - 44, 2);
        drawClouds(gfx, width, 34, now);
    } else if (!photo) {
        drawMoon(gfx, width - 38, 5, 1);
    }
    drawBrachiosaurus(gfx, width / 2 - 28, 48 + bob + settle(now, arrived), 1,
                      ambientMood(now, arrived), false, 0);
    drawRisingHearts(gfx, now, 4, 16, width - 18, 20, height - 16);
}

struct Rgb {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

struct Look {
    Rgb skyTop;
    Rgb skyMid;
    Rgb skyLow;
    Rgb pink;
    Rgb moon;
    Rgb lavender;
    Rgb cloud;
    Rgb shadow;
};

const Look kLooks[kAnimationCount] = {
    {{15, 10, 35}, {31, 18, 58}, {49, 28, 78}, {255, 128, 169}, {255, 229, 164}, {195, 171, 237}, {204, 190, 231}, {17, 9, 29}},
    {{120, 196, 255}, {186, 224, 255}, {255, 214, 148}, {255, 112, 148}, {255, 196, 64}, {255, 244, 210}, {255, 252, 248}, {48, 96, 140}},
    {{88, 28, 72}, {214, 96, 78}, {255, 176, 102}, {255, 120, 132}, {255, 214, 150}, {255, 186, 160}, {255, 176, 150}, {52, 18, 32}},
    {{8, 32, 64}, {18, 82, 112}, {36, 128, 136}, {255, 154, 176}, {190, 236, 226}, {150, 214, 220}, {176, 220, 228}, {6, 20, 36}},
    {{196, 59, 120}, {255, 122, 170}, {255, 208, 228}, {255, 61, 122}, {255, 240, 246}, {255, 224, 238}, {255, 230, 242}, {106, 24, 64}},
    {{90, 60, 140}, {160, 120, 210}, {220, 190, 245}, {230, 140, 200}, {255, 230, 250}, {220, 190, 245}, {230, 210, 250}, {30, 16, 50}},
    {{255, 160, 130}, {255, 196, 160}, {255, 224, 190}, {255, 110, 130}, {255, 236, 210}, {255, 210, 190}, {255, 230, 220}, {120, 50, 40}},
    {{8, 32, 24}, {16, 72, 48}, {32, 110, 70}, {255, 140, 170}, {220, 245, 200}, {160, 210, 170}, {180, 220, 190}, {4, 16, 12}},
    {{70, 36, 12}, {160, 100, 30}, {230, 180, 80}, {255, 120, 140}, {255, 230, 170}, {255, 210, 140}, {255, 220, 160}, {40, 20, 8}},
};

void useColor(uint16_t &slot, Rgb color) {
    slot = rgb(color.r, color.g, color.b);
}

void applyLook(int index) {
    if (index < 0 || index >= kAnimationCount) index = 0;
    const Look &look = kLooks[index];
    useColor(kSkyTop, look.skyTop);
    useColor(kSkyMid, look.skyMid);
    useColor(kSkyLow, look.skyLow);
    useColor(kPink, look.pink);
    useColor(kMoon, look.moon);
    useColor(kLavender, look.lavender);
    useColor(kCloud, look.cloud);
    useColor(kShadow, look.shadow);
}

const char *kNames[kAnimationCount] = {
    "night", "sunny", "sunset", "sea", "pink",
    "lilac", "dawn", "forest", "gold",
};

} // namespace

const char *animationName(int index) {
    if (index < 0 || index >= kAnimationCount) return kNames[0];
    return kNames[index];
}

void paintMessage(lgfx::LovyanGFX &gfx, int width, int height, bool photo,
                  const char *text, const char *style, const char *icon,
                  int visible, uint32_t now, uint32_t started, int look) {
    if (look < 0 || look >= kAnimationCount) look = 0;
    applyLook(look);
    const bool day = look == 1;
    drawSky(gfx, width, height, photo);
    if (!photo && day) {
        drawDaySun(gfx, width - 44, 2);
    } else if (!photo) {
        drawStars(gfx, now, false);
        drawMoon(gfx, width - 31, 2, 1);
    }
    const uint32_t age = now - started;
    const bool open = age >= kLetterOpenMs;
    const Mood mood = ambientMood(now, started);
    drawRisingHearts(gfx, now, 3, 8, 70, 8, 70);
    const int dinoY = height - 62;
    if (!open) {
        const int lift = static_cast<int>(age * 22 / kLetterOpenMs);
        drawEnvelope(gfx, (width - 104) / 2, 18, lift);
        drawBrachiosaurus(gfx, 4, dinoY, 1, mood);
        return;
    }
    const int cardX = 16;
    const int cardY = 6;
    const int cardW = width - 28;
    const int cardH = dinoY - cardY - 4;
    gfx.fillRoundRect(cardX - 3, cardY - 3, cardW + 6, cardH + 6, 9, kRose);
    gfx.fillRoundRect(cardX, cardY, cardW, cardH, 7, kPaper);
    gfx.fillTriangle(cardX + 7, cardY + 2, cardX + 30, cardY + 19, cardX + 7, cardY + 25, kLavender);
    drawStamp(gfx, style, icon, cardX + cardW - 24, cardY + 7);
    drawRevealed(gfx, text, visible, cardX + 8, cardY + 8, cardW - 36, cardH - 14);
    drawBrachiosaurus(gfx, 4, dinoY, 1, mood);
    drawBurst(gfx, width - 30, 16, now, started);
}

void paintClock(lgfx::LovyanGFX &gfx, int width, int height, bool photo,
                const char *timeText, const char *detail, bool waiting,
                uint32_t now, uint32_t arrived, int look) {
    if (look < 0 || look >= kAnimationCount) look = 0;
    applyLook(look);
    const bool day = look == 1;
    drawSky(gfx, width, height, photo);
    if (!photo && day) {
        drawDaySun(gfx, 4, 2);
        drawClouds(gfx, width, height - 17, now);
    } else if (!photo) {
        drawStars(gfx, now, true);
        drawMoon(gfx, 5, 2, 1);
        drawShootingStar(gfx, now, 12, 9000);
        drawClouds(gfx, width, height - 17, now);
    }
    drawRisingHearts(gfx, now, 2, 4, 35, 28, height - 8);
    drawRisingHearts(gfx, now, 2, width - 39, width - 8, 28, height - 8);
    drawBrachiosaurus(gfx, width - 72, 2 + settle(now, arrived), 1,
                      ambientMood(now, arrived), true);
    drawBurst(gfx, width - 24, 47, now, arrived);
    gfx.setTextDatum(lgfx::middle_center);
    if (waiting) {
        gfx.setFont(&fonts::FreeSansBold12pt7b);
        shadowText(gfx, timeText, width / 2, height / 2, kShadow, kCream);
        return;
    }
    gfx.setFont(&fonts::FreeSansBold24pt7b);
    if (gfx.textWidth(timeText) > width - 54) gfx.setFont(&fonts::FreeSansBold18pt7b);
    shadowText(gfx, timeText, width / 2, 69, kShadow, kCream);
    gfx.setFont(&fonts::FreeSansBold12pt7b);
    shadowText(gfx, detail, width / 2, 108, rgb(48, 16, 40), kPink);
    gfx.setFont(&fonts::FreeSansBold9pt7b);
    shadowText(gfx, "Boss", width / 2, 139, kShadow, kLavender);
}

void paintAnimation(lgfx::LovyanGFX &gfx, int width, int height, bool photo,
                    int index, uint32_t now, uint32_t arrived) {
    if (index < 0 || index >= kAnimationCount) index = 0;
    applyLook(index);
    const bool day = index == 1;
    drawSky(gfx, width, height, photo);
    if (!photo && !day) drawStars(gfx, now, false);
    drawSunflowers(gfx, width, height, now);
    sceneGarden(gfx, width, height, now, arrived, day, photo);
    drawDots(gfx, width, height, index);
}

void paintWelcome(lgfx::LovyanGFX &gfx, int width, int height, uint32_t now, int look) {
    if (look < 0 || look >= kAnimationCount) look = 0;
    applyLook(look);
    const bool day = look == 1;
    drawSky(gfx, width, height, false);
    if (day) {
        drawDaySun(gfx, width - 44, 2);
    } else {
        drawStars(gfx, now, false);
        drawMoon(gfx, width - 31, 2, 1);
    }
    const int scale = 8;
    const int heartW = 7 * scale;
    const int heartH = 6 * scale;
    drawHeartAt(gfx, (width - heartW) / 2, 36, scale);
    gfx.setFont(&fonts::FreeSansBold12pt7b);
    gfx.setTextDatum(lgfx::middle_center);
    shadowText(gfx, "Made just for you", width / 2, 36 + heartH + 26, kShadow, kCream);
}

void paintFrame(lgfx::LovyanGFX &gfx, int width, int height, lgfx::LGFX_Sprite *photo, uint32_t now,
                int look) {
    if (look < 0 || look >= kAnimationCount) look = 0;
    applyLook(look);
    const bool day = look == 1;
    drawSky(gfx, width, height, false);
    if (day) {
        drawDaySun(gfx, width - 44, 2);
    } else {
        drawStars(gfx, now, false);
    }
    const int photoX = (width - kFramePhotoW) / 2;
    const int photoY = (height - kFramePhotoH) / 2;
    const int x = photoX - kFrameBorder;
    const int y = photoY - kFrameBorder;
    const int frameW = kFramePhotoW + kFrameBorder * 2;
    const int frameH = kFramePhotoH + kFrameBorder * 2;
    gfx.fillRect(x, y, frameW, frameH, kShadow);
    gfx.fillRect(x + 4, y + 4, frameW - 8, frameH - 8, kCream);
    gfx.fillRect(x + 8, y + 8, frameW - 16, frameH - 16, rgb(92, 54, 36));
    gfx.fillRect(photoX - 3, photoY - 3, kFramePhotoW + 6, kFramePhotoH + 6, kInk);
    if (photo != nullptr && photo->width() == kFramePhotoW && photo->height() == kFramePhotoH) {
        photo->pushSprite(&gfx, photoX, photoY);
        return;
    }
    gfx.fillRect(photoX, photoY, kFramePhotoW, kFramePhotoH, kPaper);
    drawHeartAt(gfx, photoX + kFramePhotoW / 2 - 14, photoY + kFramePhotoH / 2 - 12, 4);
}

void paintWhisper(lgfx::LovyanGFX &gfx, int width, int height) {
    drawHeartAt(gfx, width / 2 - 62, height / 2 - 8, 2);
    drawHeartAt(gfx, width / 2 + 44, height / 2 - 8, 2);
    gfx.setFont(&fonts::FreeSansBold12pt7b);
    gfx.setTextDatum(lgfx::middle_center);
    shadowText(gfx, "I love you", width / 2, height / 2, kShadow, kCream);
}

} // namespace PixelScene
