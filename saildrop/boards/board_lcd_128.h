/*
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
#ifndef BOARD_LCD_128_H
#define BOARD_LCD_128_H

/*
 * Waveshare ESP32-S3 1.28" Round Touch LCD
 * ========================================
 * - Display: 240x240, GC9A01 (SPI)
 * - Touch: CST816S (I2C)
 * - Uses TFT_eSPI library
 */

#define BOARD_NAME "ESP32-S3-Touch-LCD-1.28"

// Screen dimensions
#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 240

// Display shape (0 = rectangular/square, 1 = circular/round)
#define DISPLAY_IS_ROUND 1

// Display configuration
#define DISPLAY_DRIVER_SPI      1
#define DISPLAY_DRIVER_RGB      0
#define DISPLAY_USE_TFT_ESPI    1

// Touch configuration
#define TOUCH_DRIVER_CST816S    1
#define TOUCH_DRIVER_GT911      0

// Touch I2C pins
#define TOUCH_SDA_PIN   6
#define TOUCH_SCL_PIN   7
#define TOUCH_RST_PIN   13
#define TOUCH_INT_PIN   5

// LVGL buffer size (smaller for SPI displays)
// 1/10 of screen in RGB565 (2 bytes per pixel)
#define DRAW_BUF_SIZE   (SCREEN_WIDTH * SCREEN_HEIGHT / 10 * (LV_COLOR_DEPTH / 8))

// DPI for this display
#define DISPLAY_DPI     130

// Memory configuration
#define LV_MEM_SIZE_KB  64

#endif // BOARD_LCD_128_H
