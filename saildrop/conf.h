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
#ifndef CONF_H
#define CONF_H

/*
 * Board Selection
 * ===============
 * Define ONE of these before including conf.h or via build flags:
 * - BOARD_LCD_128  : Waveshare ESP32-S3 1.28" Round Touch LCD (240x240)
 * - BOARD_LCD_4    : Waveshare ESP32-S3 4" Touch LCD (480x480)
 *
 * If neither is defined, BOARD_LCD_128 is used by default.
 * This can be set via Makefile: -DBOARD_LCD_4 or -DBOARD_LCD_128
 */

// Include board-specific configuration
// This defines SCREEN_WIDTH, SCREEN_HEIGHT, and hardware pins
#include "boards/boards.h"

#define DEBUG 1
#define LVGL_TICK_PERIOD_MS 2
#define LOOP_DELAY 2

// Screen dimensions are now defined in boards/board_*.h
// The UI will scale automatically via scale.h

// #define SHOWCASE

// Screen configuration - comment out to disable, reorder to change sequence
// The order here determines swipe order (left/right navigation)
// #define SCREEN_SPEED      1
#define SCREEN_VALUES     1
#define SCREEN_WIND       2
#define SCREEN_COMPASS    3
#define SCREEN_AIS        4
#define SCREEN_TACK       5
#define SCREEN_TIMER      6

// #define AP_MODE
#define AP_SSID "SAILDROP_AP"
#define AP_PASS "12345678"

// WiFiManager portal configuration
#define PORTAL_SSID "SAILDROP_SETUP"
#define PORTAL_PASS "saildrop"
#define PORTAL_TIMEOUT_SEC 180  // 3 minutes

#define WIFI_DEFAULT_SSID "NMEA3WIFI"
#define WIFI_DEFAULT_PASSWORD "12345678"
#define WIFI_DEFAULT_IP "192.168.4.1"
#define WIFI_DEFAULT_UDP_PORT 2000
#define WIFI_DEFAULT_TCP_PORT 2001

#endif