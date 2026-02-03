/*
 * Hardware Abstraction Layer (HAL)
 * Unified interface for display and touch across different boards
 *
 * Copyright (C) 2024-2025 Davide Gessa
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef HAL_H
#define HAL_H

#include <lvgl.h>
#include "boards/boards.h"

// Include appropriate drivers based on board
#if TOUCH_DRIVER_CST816S
    #include "CST816S.h"
#endif

#if TOUCH_DRIVER_GT911
    #include "GT911.h"
#endif

#if DISPLAY_USE_TFT_ESPI
    #include <TFT_eSPI.h>
#endif

#if DISPLAY_DRIVER_RGB
    #include <esp_lcd_panel_ops.h>
    #include <esp_lcd_panel_rgb.h>
    #include <driver/gpio.h>
    #include <Wire.h>
#endif

// Unified touch data structure
struct HAL_TouchData {
    uint8_t gestureID;
    uint8_t points;
    uint8_t event;
    int x;
    int y;
};

// Gesture constants (compatible across drivers)
#define HAL_GESTURE_NONE        0x00
#define HAL_GESTURE_SWIPE_UP    0x01
#define HAL_GESTURE_SWIPE_DOWN  0x02
#define HAL_GESTURE_SWIPE_LEFT  0x03
#define HAL_GESTURE_SWIPE_RIGHT 0x04
#define HAL_GESTURE_SINGLE_CLICK 0x05
#define HAL_GESTURE_DOUBLE_CLICK 0x0B
#define HAL_GESTURE_LONG_PRESS  0x0C

class HAL {
public:
    static HAL& instance() {
        static HAL hal;
        return hal;
    }

    // Initialize all hardware
    bool begin();

    // Display functions
    lv_display_t* initDisplay(void* draw_buf, size_t buf_size);
    void setBacklight(uint8_t brightness);  // 0-255

    // Touch functions
    bool touchAvailable();
    HAL_TouchData getTouchData();
    String getGestureName();

    // Get board info
    const char* getBoardName() { return BOARD_NAME; }
    int getScreenWidth() { return SCREEN_WIDTH; }
    int getScreenHeight() { return SCREEN_HEIGHT; }

private:
    HAL() : _initialized(false) {}

    bool _initialized;
    HAL_TouchData _lastTouch;

#if DISPLAY_USE_TFT_ESPI
    TFT_eSPI* _tft;
#endif

#if DISPLAY_DRIVER_RGB
    esp_lcd_panel_handle_t _panel;
    void initRGBPanel();
    void initIOExpander();
    static void rgb_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map);
#endif

#if TOUCH_DRIVER_CST816S
    CST816S* _touch_cst816s;
#endif

#if TOUCH_DRIVER_GT911
    GT911* _touch_gt911;
#endif
};

// Implementation
bool HAL::begin() {
    if (_initialized) return true;

    Serial.printf("HAL: Initializing board %s\n", BOARD_NAME);
    Serial.printf("HAL: Screen %dx%d\n", SCREEN_WIDTH, SCREEN_HEIGHT);

#if DISPLAY_USE_TFT_ESPI
    // Initialize TFT_eSPI display
    _tft = new TFT_eSPI(SCREEN_WIDTH, SCREEN_HEIGHT);
    _tft->begin();
    _tft->setRotation(0);
    Serial.println("HAL: TFT_eSPI display initialized");
#endif

#if DISPLAY_DRIVER_RGB
    // Initialize IO Expander for backlight and reset control
    initIOExpander();
    // Initialize RGB panel
    initRGBPanel();
    Serial.println("HAL: RGB display initialized");
#endif

#if TOUCH_DRIVER_CST816S
    // Initialize CST816S touch
    _touch_cst816s = new CST816S(TOUCH_SDA_PIN, TOUCH_SCL_PIN, TOUCH_RST_PIN, TOUCH_INT_PIN);
    _touch_cst816s->begin();
    Serial.println("HAL: CST816S touch initialized");
#endif

#if TOUCH_DRIVER_GT911
    // Initialize GT911 touch
    _touch_gt911 = new GT911(TOUCH_SDA_PIN, TOUCH_SCL_PIN, TOUCH_RST_PIN, TOUCH_INT_PIN);
    _touch_gt911->begin(&Wire);
    Serial.println("HAL: GT911 touch initialized");
#endif

    _initialized = true;
    return true;
}

lv_display_t* HAL::initDisplay(void* draw_buf, size_t buf_size) {
    lv_display_t* disp = nullptr;

#if DISPLAY_USE_TFT_ESPI
    // Use LVGL's TFT_eSPI integration
    disp = lv_tft_espi_create(SCREEN_WIDTH, SCREEN_HEIGHT, draw_buf, buf_size);
    lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_0);
#endif

#if DISPLAY_DRIVER_RGB
    // Create LVGL display for RGB panel
    disp = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_display_set_flush_cb(disp, rgb_flush_cb);
    lv_display_set_buffers(disp, draw_buf, NULL, buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_user_data(disp, this);
#endif

    return disp;
}

#if DISPLAY_DRIVER_RGB

void HAL::initIOExpander() {
    // Initialize second I2C bus for IO expander
    Wire1.begin(IO_EXPANDER_SDA_PIN, IO_EXPANDER_SCL_PIN);
    Wire1.setClock(400000);

    // Configure TCA9554 - all pins as outputs
    Wire1.beginTransmission(IO_EXPANDER_I2C_ADDR);
    Wire1.write(0x03);  // Configuration register
    Wire1.write(0x00);  // All outputs
    Wire1.endTransmission();

    // Set initial states: LCD reset high, backlight on, touch reset high
    Wire1.beginTransmission(IO_EXPANDER_I2C_ADDR);
    Wire1.write(0x01);  // Output register
    Wire1.write((1 << IO_EXP_PIN_LCD_RST) | (1 << IO_EXP_PIN_TOUCH_RST) | (1 << IO_EXP_PIN_LCD_BL));
    Wire1.endTransmission();

    delay(50);
    Serial.println("HAL: IO Expander initialized");
}

void HAL::initRGBPanel() {
    // Configure RGB panel
    esp_lcd_rgb_panel_config_t panel_config = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .timings = {
            .pclk_hz = LCD_PCLK_MHZ * 1000000,
            .h_res = LCD_H_RES,
            .v_res = LCD_V_RES,
            .hsync_pulse_width = LCD_HSYNC_PULSE_WIDTH,
            .hsync_back_porch = LCD_HSYNC_BACK_PORCH,
            .hsync_front_porch = LCD_HSYNC_FRONT_PORCH,
            .vsync_pulse_width = LCD_VSYNC_PULSE_WIDTH,
            .vsync_back_porch = LCD_VSYNC_BACK_PORCH,
            .vsync_front_porch = LCD_VSYNC_FRONT_PORCH,
            .flags = {
                .pclk_active_neg = 1,
            },
        },
        .data_width = 16,
        .bits_per_pixel = 16,
        .num_fbs = 1,
        .bounce_buffer_size_px = 0,
        .sram_trans_align = 8,
        .psram_trans_align = 64,
        .hsync_gpio_num = LCD_HSYNC_PIN,
        .vsync_gpio_num = LCD_VSYNC_PIN,
        .de_gpio_num = LCD_DE_PIN,
        .pclk_gpio_num = LCD_PCLK_PIN,
        .disp_gpio_num = -1,
        .data_gpio_nums = {
            LCD_DATA0_PIN, LCD_DATA1_PIN, LCD_DATA2_PIN, LCD_DATA3_PIN,
            LCD_DATA4_PIN, LCD_DATA5_PIN, LCD_DATA6_PIN, LCD_DATA7_PIN,
            LCD_DATA8_PIN, LCD_DATA9_PIN, LCD_DATA10_PIN, LCD_DATA11_PIN,
            LCD_DATA12_PIN, LCD_DATA13_PIN, LCD_DATA14_PIN, LCD_DATA15_PIN,
        },
        .flags = {
            .fb_in_psram = 1,
        },
    };

    ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&panel_config, &_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(_panel));
}

void HAL::rgb_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    HAL* hal = (HAL*)lv_display_get_user_data(disp);

    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;

    esp_lcd_panel_draw_bitmap(hal->_panel, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, px_map);

    lv_display_flush_ready(disp);
}

#endif // DISPLAY_DRIVER_RGB

void HAL::setBacklight(uint8_t brightness) {
#if DISPLAY_DRIVER_RGB
    // Control backlight via IO expander
    Wire1.beginTransmission(IO_EXPANDER_I2C_ADDR);
    Wire1.write(0x01);  // Output register
    uint8_t state = (1 << IO_EXP_PIN_LCD_RST) | (1 << IO_EXP_PIN_TOUCH_RST);
    if (brightness > 0) {
        state |= (1 << IO_EXP_PIN_LCD_BL);
    }
    Wire1.write(state);
    Wire1.endTransmission();
#endif
    // TFT_eSPI backlight control depends on hardware, usually via PWM pin
}

bool HAL::touchAvailable() {
#if TOUCH_DRIVER_CST816S
    bool avail = _touch_cst816s->available();
    if (avail) {
        _lastTouch.gestureID = _touch_cst816s->data.gestureID;
        _lastTouch.points = _touch_cst816s->data.points;
        _lastTouch.event = _touch_cst816s->data.event;
        _lastTouch.x = _touch_cst816s->data.x;
        _lastTouch.y = _touch_cst816s->data.y;
    }
    return avail;
#endif

#if TOUCH_DRIVER_GT911
    bool avail = _touch_gt911->available();
    if (avail) {
        _lastTouch.gestureID = _touch_gt911->data.gestureID;
        _lastTouch.points = _touch_gt911->data.points;
        _lastTouch.event = _touch_gt911->data.event;
        _lastTouch.x = _touch_gt911->data.x;
        _lastTouch.y = _touch_gt911->data.y;
    }
    return avail;
#endif

    return false;
}

HAL_TouchData HAL::getTouchData() {
    return _lastTouch;
}

String HAL::getGestureName() {
#if TOUCH_DRIVER_CST816S
    return _touch_cst816s->gesture();
#endif

#if TOUCH_DRIVER_GT911
    return _touch_gt911->gesture();
#endif

    return "NONE";
}

// Global accessor
inline HAL& hal() {
    return HAL::instance();
}

#endif // HAL_H
