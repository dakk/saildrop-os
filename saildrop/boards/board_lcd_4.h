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
#ifndef BOARD_LCD_4_H
#define BOARD_LCD_4_H

/*
 * Waveshare ESP32-S3 4" Touch LCD
 * ===============================
 * - Display: 480x480, ST7701 (RGB Parallel)
 * - Touch: GT911 (I2C)
 * - IO Expander: TCA9554 for backlight control
 * - Uses ESP32_Display_Panel library
 */

#define BOARD_NAME "ESP32-S3-Touch-LCD-4"

// Screen dimensions
#define SCREEN_WIDTH  480
#define SCREEN_HEIGHT 480

// Display configuration
#define DISPLAY_DRIVER_SPI      0
#define DISPLAY_DRIVER_RGB      1
#define DISPLAY_USE_TFT_ESPI    0

// Touch configuration
#define TOUCH_DRIVER_CST816S    0
#define TOUCH_DRIVER_GT911      1

// Touch I2C pins (GT911)
#define TOUCH_SDA_PIN   15
#define TOUCH_SCL_PIN   7
#define TOUCH_RST_PIN   -1  // Connected via IO expander
#define TOUCH_INT_PIN   16

// IO Expander I2C pins (TCA9554)
#define IO_EXPANDER_SDA_PIN     8
#define IO_EXPANDER_SCL_PIN     9
#define IO_EXPANDER_I2C_ADDR    0x20

// IO Expander pin assignments
#define IO_EXP_PIN_TOUCH_RST    0
#define IO_EXP_PIN_LCD_RST      1
#define IO_EXP_PIN_LCD_CS       2
#define IO_EXP_PIN_LCD_SDA      3
#define IO_EXP_PIN_LCD_CLK      4
#define IO_EXP_PIN_LCD_BL       5  // Backlight control

// RGB Display pins
#define LCD_HSYNC_PIN   38
#define LCD_VSYNC_PIN   39
#define LCD_DE_PIN      40
#define LCD_PCLK_PIN    41

// RGB Data pins (active low directly)
// R0-R4: GPIO45, GPIO48, GPIO47, GPIO21, GPIO14
// G0-G5: GPIO13, GPIO12, GPIO11, GPIO10, GPIO9, GPIO46
// B0-B4: GPIO3, GPIO8, GPIO18, GPIO17, GPIO16
#define LCD_DATA0_PIN   45  // R0
#define LCD_DATA1_PIN   48  // R1
#define LCD_DATA2_PIN   47  // R2
#define LCD_DATA3_PIN   21  // R3
#define LCD_DATA4_PIN   14  // R4
#define LCD_DATA5_PIN   13  // G0
#define LCD_DATA6_PIN   12  // G1
#define LCD_DATA7_PIN   11  // G2
#define LCD_DATA8_PIN   10  // G3
#define LCD_DATA9_PIN   9   // G4
#define LCD_DATA10_PIN  46  // G5
#define LCD_DATA11_PIN  3   // B0
#define LCD_DATA12_PIN  8   // B1
#define LCD_DATA13_PIN  18  // B2
#define LCD_DATA14_PIN  17  // B3
#define LCD_DATA15_PIN  16  // B4

// RGB timing parameters for ST7701
#define LCD_H_RES       480
#define LCD_V_RES       480
#define LCD_HSYNC_BACK_PORCH    10
#define LCD_HSYNC_FRONT_PORCH   50
#define LCD_HSYNC_PULSE_WIDTH   8
#define LCD_VSYNC_BACK_PORCH    10
#define LCD_VSYNC_FRONT_PORCH   20
#define LCD_VSYNC_PULSE_WIDTH   8
#define LCD_PCLK_MHZ            16

// LVGL buffer size
// RGB displays need larger buffers, use PSRAM
// Full frame double buffer for best performance
#define DRAW_BUF_SIZE   (SCREEN_WIDTH * SCREEN_HEIGHT * (LV_COLOR_DEPTH / 8))
#define USE_PSRAM_BUFFER 1

// DPI for this display
#define DISPLAY_DPI     150

// Memory configuration (use more memory for larger display)
#define LV_MEM_SIZE_KB  128

#endif // BOARD_LCD_4_H
