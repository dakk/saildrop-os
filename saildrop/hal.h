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
    #include <Arduino_GFX_Library.h>
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
    HAL() : _initialized(false)
#if DISPLAY_USE_TFT_ESPI
        , _tft(nullptr)
#endif
#if DISPLAY_DRIVER_RGB
        , _bus(nullptr), _rgbpanel(nullptr), _gfx(nullptr)
#endif
#if TOUCH_DRIVER_CST816S
        , _touch_cst816s(nullptr)
#endif
#if TOUCH_DRIVER_GT911
        , _touch_gt911(nullptr)
#endif
    {
        memset(&_lastTouch, 0, sizeof(_lastTouch));
    }

    bool _initialized;
    HAL_TouchData _lastTouch;

#if DISPLAY_USE_TFT_ESPI
    TFT_eSPI* _tft;
#endif

#if DISPLAY_DRIVER_RGB
    Arduino_DataBus* _bus;
    Arduino_ESP32RGBPanel* _rgbpanel;
    Arduino_RGB_Display* _gfx;
    void initRGBDisplay();
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
    Serial.flush();
    Serial.printf("HAL: Screen %dx%d\n", SCREEN_WIDTH, SCREEN_HEIGHT);
    Serial.flush();

#if DISPLAY_USE_TFT_ESPI
    // Initialize TFT_eSPI display
    Serial.println("HAL: Creating TFT_eSPI...");
    Serial.flush();
    _tft = new TFT_eSPI(SCREEN_WIDTH, SCREEN_HEIGHT);
    _tft->begin();
    _tft->setRotation(0);
    Serial.println("HAL: TFT_eSPI display initialized");
    Serial.flush();
#endif

#if DISPLAY_DRIVER_RGB
    // Initialize IO Expander for backlight and reset control
    Serial.println("HAL: Initializing IO Expander...");
    Serial.flush();
    initIOExpander();
    Serial.println("HAL: IO Expander done. Initializing RGB display...");
    Serial.flush();
    // Initialize RGB display via Arduino_GFX (handles ST7701 SPI init)
    initRGBDisplay();
    Serial.println("HAL: RGB display initialized");
    Serial.flush();
#endif

#if TOUCH_DRIVER_CST816S
    // Initialize CST816S touch
    Serial.println("HAL: Initializing CST816S touch...");
    Serial.flush();
    _touch_cst816s = new CST816S(TOUCH_SDA_PIN, TOUCH_SCL_PIN, TOUCH_RST_PIN, TOUCH_INT_PIN);
    _touch_cst816s->begin();
    Serial.println("HAL: CST816S touch initialized");
    Serial.flush();
#endif

#if TOUCH_DRIVER_GT911
    // Initialize GT911 touch
    // Note: Wire is already initialized by initIOExpander(), don't reinitialize
    Serial.println("HAL: Initializing GT911 touch...");
    Serial.flush();
    _touch_gt911 = new GT911(TOUCH_SDA_PIN, TOUCH_SCL_PIN, TOUCH_RST_PIN, TOUCH_INT_PIN);
    // Pass false to skip Wire.begin() - it's already initialized
    _touch_gt911->begin(&Wire, false);
    Serial.println("HAL: GT911 touch initialized");
    Serial.flush();
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
    // IO expander uses same I2C bus as touch (pins 15, 7)
    Wire.begin(TOUCH_SDA_PIN, TOUCH_SCL_PIN);
    Wire.setClock(100000);  // Use 100kHz for reliability during init

    Serial.println("HAL: Configuring IO Expander (PCA9557)...");
    Serial.flush();

    // PCA9557 IO Expander Pin mapping (Waveshare ESP32-S3-Touch-LCD-4):
    // P0 (0x01): LCD backlight enable
    // P2 (0x04): LCD reset (active low)
    //
    // Register 0x01: Output port register (actual pin values)
    // Register 0x02: Polarity inversion
    // Register 0x03: Configuration (0 = output, 1 = input)

    // IMPORTANT: Order matters! Polarity first, then configuration
    // This matches the official Waveshare example exactly

    // Step 1: Set polarity inversion FIRST (as per official Waveshare example)
    Wire.beginTransmission(IO_EXPANDER_I2C_ADDR);
    Wire.write(0x02);  // Polarity inversion register
    Wire.write(0xFF);  // Invert all pins
    Wire.endTransmission();
    delay(10);

    // Step 2: Configure pins as outputs
    // 0x3A = 0b00111010 means P0, P2, P6, P7 are outputs (bits = 0)
    Wire.beginTransmission(IO_EXPANDER_I2C_ADDR);
    Wire.write(0x03);  // Configuration register
    Wire.write(0x3A);
    Wire.endTransmission();
    delay(10);

    // Step 3: Hardware reset sequence - CRITICAL for restart stability
    // Assert LCD_RST (P2) LOW, backlight (P0) OFF
    Serial.println("HAL: Asserting LCD reset...");
    Serial.flush();
    Wire.beginTransmission(IO_EXPANDER_I2C_ADDR);
    Wire.write(0x01);  // Output port register
    Wire.write(0x00);  // All outputs low (LCD reset active, backlight off)
    Wire.endTransmission();
    delay(50);  // Hold reset for 50ms

    // Step 4: Release reset, keep backlight off initially
    Serial.println("HAL: Releasing LCD reset...");
    Serial.flush();
    Wire.beginTransmission(IO_EXPANDER_I2C_ADDR);
    Wire.write(0x01);
    Wire.write(0x04);  // P2 high (LCD reset released), P0 low (backlight off)
    Wire.endTransmission();
    delay(120);  // ST7701 needs 120ms after reset before initialization

    // Step 5: Enable backlight
    Wire.beginTransmission(IO_EXPANDER_I2C_ADDR);
    Wire.write(0x01);
    Wire.write(0x05);  // P0 high (backlight on), P2 high (reset released)
    Wire.endTransmission();
    delay(20);

    Serial.println("HAL: IO Expander initialized with reset sequence");
    Serial.flush();
}

void HAL::initRGBDisplay() {
    // Create SPI bus for ST7701 initialization commands
    _bus = new Arduino_SWSPI(
        GFX_NOT_DEFINED /* DC */, LCD_SPI_CS_PIN /* CS */,
        LCD_SPI_SCK_PIN /* SCK */, LCD_SPI_MOSI_PIN /* MOSI */, GFX_NOT_DEFINED /* MISO */);

    // Create RGB panel with timing parameters
    _rgbpanel = new Arduino_ESP32RGBPanel(
        LCD_DE_PIN /* DE */, LCD_VSYNC_PIN /* VSYNC */, LCD_HSYNC_PIN /* HSYNC */, LCD_PCLK_PIN /* PCLK */,
        LCD_DATA0_PIN /* R0 */, LCD_DATA1_PIN /* R1 */, LCD_DATA2_PIN /* R2 */, LCD_DATA3_PIN /* R3 */, LCD_DATA4_PIN /* R4 */,
        LCD_DATA5_PIN /* G0 */, LCD_DATA6_PIN /* G1 */, LCD_DATA7_PIN /* G2 */, LCD_DATA8_PIN /* G3 */, LCD_DATA9_PIN /* G4 */, LCD_DATA10_PIN /* G5 */,
        LCD_DATA11_PIN /* B0 */, LCD_DATA12_PIN /* B1 */, LCD_DATA13_PIN /* B2 */, LCD_DATA14_PIN /* B3 */, LCD_DATA15_PIN /* B4 */,
        LCD_HSYNC_POLARITY /* hsync_polarity */, LCD_HSYNC_FRONT_PORCH /* hsync_front_porch */, LCD_HSYNC_PULSE_WIDTH /* hsync_pulse_width */, LCD_HSYNC_BACK_PORCH /* hsync_back_porch */,
        LCD_VSYNC_POLARITY /* vsync_polarity */, LCD_VSYNC_FRONT_PORCH /* vsync_front_porch */, LCD_VSYNC_PULSE_WIDTH /* vsync_pulse_width */, LCD_VSYNC_BACK_PORCH /* vsync_back_porch */);

    // Create RGB display with ST7701 init sequence
    _gfx = new Arduino_RGB_Display(
        LCD_H_RES /* width */, LCD_V_RES /* height */, _rgbpanel, 0 /* rotation */, true /* auto_flush */,
        _bus, GFX_NOT_DEFINED /* RST */, st7701_type1_init_operations, sizeof(st7701_type1_init_operations));

    // Initialize the display (sends ST7701 init commands via SPI, then sets up RGB)
    if (!_gfx->begin()) {
        Serial.println("HAL: ERROR - gfx->begin() failed!");
    }

    // Fill screen with black
    _gfx->fillScreen(RGB565_BLACK);
}

void HAL::rgb_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    HAL* hal_inst = (HAL*)lv_display_get_user_data(disp);

    // Null check to prevent crash
    if (hal_inst == nullptr || hal_inst->_gfx == nullptr) {
        Serial.println("HAL: ERROR - flush_cb null pointer!");
        lv_display_flush_ready(disp);
        return;
    }

    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    // Use Arduino_GFX to draw the bitmap
    hal_inst->_gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t*)px_map, w, h);

    lv_display_flush_ready(disp);
}

#endif // DISPLAY_DRIVER_RGB

void HAL::setBacklight(uint8_t brightness) {
#if DISPLAY_DRIVER_RGB
    // Control backlight via IO expander (PCA9557)
    // P0 = backlight, P2 = LCD reset (keep high)
    // Register 0x01 is the output port register
    Wire.beginTransmission(IO_EXPANDER_I2C_ADDR);
    Wire.write(0x01);  // Output port register
    Wire.write(brightness > 0 ? 0x05 : 0x04);  // P2 always high, P0 = backlight
    Wire.endTransmission();
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
