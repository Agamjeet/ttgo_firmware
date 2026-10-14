#pragma once

// Board profile for the standard LILYGO T-Display-S3.
// Pins and panel offsets match LilyGo's LilyGo_T_Display_S3 class
// (ST7789, 170x320, 8-bit parallel, power on GPIO15, backlight GPIO38).
// Current LovyanGFX drives that bus with the ESP32-S3 LCD_CAM peripheral,
// which has no i2s_port field, so this profile is the official pin map
// adapted to LovyanGFX 1.2.x.

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

class TDisplayS3 : public lgfx::LGFX_Device {
public:
    TDisplayS3() {
        {
            auto cfg = _bus.config();
            cfg.port = 0;
            cfg.freq_write = 16000000;
            cfg.pin_wr = 8;
            cfg.pin_rd = 9;
            cfg.pin_rs = 7;
            cfg.pin_d0 = 39;
            cfg.pin_d1 = 40;
            cfg.pin_d2 = 41;
            cfg.pin_d3 = 42;
            cfg.pin_d4 = 45;
            cfg.pin_d5 = 46;
            cfg.pin_d6 = 47;
            cfg.pin_d7 = 48;
            _bus.config(cfg);
            _panel.setBus(&_bus);
        }
        {
            auto cfg = _panel.config();
            cfg.pin_cs = 6;
            cfg.pin_rst = 5;
            cfg.pin_busy = -1;
            cfg.memory_width = 240;
            cfg.memory_height = 320;
            cfg.panel_width = 170;
            cfg.panel_height = 320;
            cfg.offset_x = 35;
            cfg.offset_y = 0;
            cfg.offset_rotation = 0;
            cfg.dummy_read_pixel = 8;
            cfg.dummy_read_bits = 1;
            cfg.readable = false;
            cfg.invert = true;
            cfg.rgb_order = false;
            cfg.dlen_16bit = false;
            cfg.bus_shared = false;
            _panel.config(cfg);
        }
        {
            auto cfg = _light.config();
            cfg.pin_bl = 38;
            cfg.invert = false;
            cfg.freq = 12000;
            cfg.pwm_channel = 7;
            _light.config(cfg);
            _panel.setLight(&_light);
        }
        setPanel(&_panel);
    }

    bool begin() {
        pinMode(15, OUTPUT);
        digitalWrite(15, HIGH);
        if (!init()) {
            return false;
        }
        setRotation(1);
        setBrightness(210);
        fillScreen(TFT_BLACK);
        return width() == 320 && height() == 170;
    }

private:
    lgfx::Panel_ST7789 _panel;
    lgfx::Bus_Parallel8 _bus;
    lgfx::Light_PWM _light;
};
