#pragma once

#include <LovyanGFX.hpp>
#include <stdint.h>

// Original pixel art for this gift. No third-party sprites or fonts beyond the
// FreeSans faces already shipped with LovyanGFX.

namespace PixelScene {

constexpr int kAnimationCount = 9;
constexpr uint32_t kLetterOpenMs = 820;
// Inner photograph. app/frameSize.js in the website must use these same pixels.
constexpr int kFramePhotoW = 256;
constexpr int kFramePhotoH = 118;
constexpr int kFrameBorder = 14;

const char *animationName(int index);

void paintMessage(lgfx::LovyanGFX &gfx, int width, int height, bool photo, const char *text,
                  const char *style, const char *icon, int visible, uint32_t now, uint32_t started,
                  int look);

void paintClock(lgfx::LovyanGFX &gfx, int width, int height, bool photo, const char *timeText,
                const char *detail, bool waiting, uint32_t now, uint32_t arrived, int look);

void paintAnimation(lgfx::LovyanGFX &gfx, int width, int height, bool photo, int index, uint32_t now,
                    uint32_t arrived);

void paintWelcome(lgfx::LovyanGFX &gfx, int width, int height, uint32_t now, int look);

void paintFrame(lgfx::LovyanGFX &gfx, int width, int height, lgfx::LGFX_Sprite *photo, uint32_t now,
                int look);

void paintWhisper(lgfx::LovyanGFX &gfx, int width, int height);

}
