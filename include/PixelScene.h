#pragma once

#include <LovyanGFX.hpp>
#include <stdint.h>

// Original pixel art for this gift. No third-party sprites or fonts beyond the
// FreeSans faces already shipped with LovyanGFX.

namespace PixelScene {

constexpr int kAnimationCount = 5;
constexpr uint32_t kLetterOpenMs = 820;

const char *animationName(int index);

void paintMessage(lgfx::LovyanGFX &gfx, int width, int height, bool photo, const char *text,
                  const char *style, const char *icon, int visible, uint32_t now, uint32_t started,
                  int look);

void paintClock(lgfx::LovyanGFX &gfx, int width, int height, bool photo, const char *timeText,
                const char *detail, bool waiting, uint32_t now, uint32_t arrived, int look);

void paintAnimation(lgfx::LovyanGFX &gfx, int width, int height, bool photo, int index, uint32_t now,
                    uint32_t arrived);

}
