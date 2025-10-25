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

#define DEBUG 1
#define LVGL_TICK_PERIOD_MS 2
#define LOOP_DELAY 2

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 240

// #define SHOWCASE
// #define HOME_DEBUG

// #define AP_MODE
#define AP_SSID "SAILDROP_AP"
#define AP_PASS "12345678"

#ifdef HOME_DEBUG
    #define WIFI_DEFAULT_SSID "SD_NMEA"
    #define WIFI_DEFAULT_PASSWORD "saildropwashere!!!!"
    #define WIFI_DEFAULT_IP "192.168.4.2" //1.50"
    #define WIFI_DEFAULT_UDP_PORT 2000
    #define WIFI_DEFAULT_TCP_PORT 2001
#else
    #define WIFI_DEFAULT_SSID "DIVERSA_SAILING"
    #define WIFI_DEFAULT_PASSWORD "12345678"
    #define WIFI_DEFAULT_IP "192.168.4.1"
    #define WIFI_DEFAULT_UDP_PORT 2000
    #define WIFI_DEFAULT_TCP_PORT 2001
#endif

#endif