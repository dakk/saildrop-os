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
// Include board configuration first (before LVGL)
#include "conf.h"
#include <Arduino.h>
#include <esp_system.h>
#include <lvgl.h>
#include "hal.h"
#include "conn.h"
#include "settings.h"
#include "wifiportal.h"

#include "screens/screen.h"
#include "screens/splashscreen.h"
#include "screens/portalscreen.h"
#include "screens/contextmenu.h"

// Conditionally include screens based on conf.h
#ifdef SCREEN_SPEED
#include "screens/speedscreen.h"
#endif
#ifdef SCREEN_WIND
#include "screens/windscreen.h"
#endif
#ifdef SCREEN_COMPASS
#include "screens/compassscreen.h"
#endif
#ifdef SCREEN_VALUES
#include "screens/valuesscreen.h"
#endif
#ifdef SCREEN_AIS
#include "screens/aisscreen.h"
#endif
#ifdef SCREEN_TACK
#include "screens/tackscreen.h"
#endif
#ifdef SCREEN_TIMER
#include "screens/timerscreen.h"
#endif

// Draw buffer - size defined by board configuration
// For RGB displays with PSRAM, allocate in PSRAM for larger buffer
#if USE_PSRAM_BUFFER && defined(BOARD_LCD_4)
    // Allocate from PSRAM for 4" display (larger buffer needed)
    static uint8_t* draw_buf = nullptr;
    #define DRAW_BUF_ALLOC_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT / 10 * (LV_COLOR_DEPTH / 8))
#else
    // Static allocation for SPI displays
    #define DRAW_BUF_ALLOC_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT / 10 * (LV_COLOR_DEPTH / 8))
    static uint32_t draw_buf[DRAW_BUF_ALLOC_SIZE / 4];
#endif

static Screen *screens[16];
int current_screen = 0;
uint8_t num_screens = 0;
Screen *splash;
Screen *portalScreen;

void add_screen(Screen *sc) {
    screens[num_screens] = sc;
    num_screens ++;
}

TaskHandle_t core2_loop_task;

enum SAILDROP_STATUS
{
    BOOT,
    PORTAL_MODE,
    LOADING_TRIGGERED,
    LOADING_COMPLETED,
    SETUP,
    SETUP_DONE,
    RUNNING
};

SAILDROP_STATUS status = BOOT;
uint32_t tick = 0;
uint32_t last_handled_gesture_tick = 0;


void core2_loop(void *arg)
{
    while (1)
    {
        if (status == PORTAL_MODE)
        {
            // Run WiFiManager portal (blocking until configured or timeout)
            Serial.println("Starting WiFi configuration portal...");
            getPortal()->begin();
            bool success = getPortal()->startPortal();

            if (success) {
                Serial.println("Portal configuration successful, restarting...");
                delay(1000);
                ESP.restart();  // Restart to apply new WiFi configuration cleanly
            } else {
                Serial.println("Portal timeout, retrying...");
                delay(1000);
            }
        }
        else if (status == BOOT)
        {
            status = LOADING_TRIGGERED;
            ((SplashScreen *)splash)->load();

            SaildropSettings* s = getSettings()->get();

            if (s->ap_mode) {
                // AP Mode: Create own WiFi network and listen for connections
                Serial.println("Starting in AP mode...");
                WiFi.mode(WIFI_AP);
                bool apStarted = WiFi.softAP(s->ap_ssid, s->ap_pass);

                if (!apStarted) {
                    Serial.println("Failed to start AP, going to portal");
                    status = PORTAL_MODE;
                    continue;
                }

                Serial.printf("AP started: %s\n", s->ap_ssid);
                Serial.printf("AP IP: %s\n", WiFi.softAPIP().toString().c_str());
                Serial.printf("Listening on port: %d (%s)\n", s->listen_port,
                             s->listen_protocol == PROTO_TCP ? "TCP" : "UDP");

                // Initialize connection module for listening mode
                initialize_connections_ap(s->listen_port, s->listen_protocol);
            } else {
                // Station Mode: Connect to existing WiFi network
                Serial.printf("Connecting to WiFi: %s\n", s->wifi_ssid);
                WiFi.mode(WIFI_STA);
                WiFi.begin(s->wifi_ssid, s->wifi_pass);

                // Wait for connection with timeout
                int timeout = 150;  // 15 seconds (100ms * 150)
                while (WiFi.status() != WL_CONNECTED && timeout > 0) {
                    delay(100);
                    timeout--;
                }

                if (WiFi.status() != WL_CONNECTED) {
                    Serial.println("WiFi connection failed, starting portal");
                    status = PORTAL_MODE;
                    continue;
                }

                Serial.printf("WiFi connected: %s\n", WiFi.localIP().toString().c_str());

                // Initialize connection module for client mode
                initialize_connections();
            }
        }
        else {
            conn_loop();
        }

        delay(100);
    }
}

#if LV_USE_LOG != 0
/* Serial debugging */
void my_print(const char *buf)
{
    Serial.printf(buf);
    Serial.flush();
}
#endif


/* Seconds timer */
void on_tick(void *arg)
{
    /* Tell LVGL how many milliseconds has elapsed */
    lv_tick_inc(LVGL_TICK_PERIOD_MS);
}

/*Read the touchpad - uses HAL for hardware abstraction*/
void my_touchpad_read( lv_indev_t * indev, lv_indev_data_t * data )
{
    bool touched = hal().touchAvailable();
    if (!touched)
    {
        data->state = LV_INDEV_STATE_REL;
    }
    else
    {
        HAL_TouchData touchData = hal().getTouchData();
        data->state = LV_INDEV_STATE_PR;
        data->point.x = touchData.x;
        data->point.y = touchData.y;

        Serial.printf("Gesture: %s, ID: %d, x: %d, y: %d, status: %d, tick: %lu, last: %lu\n",
                      hal().getGestureName().c_str(), touchData.gestureID, touchData.x, touchData.y, status, tick, last_handled_gesture_tick);

        if (status != RUNNING || (tick - last_handled_gesture_tick) < 50)
            return;

        Serial.printf("Handling gesture ID: %d\n", touchData.gestureID);

        // Handle long press globally (works even when context menu is visible)
        if (touchData.gestureID == HAL_GESTURE_LONG_PRESS)
        {
            if (!context_menu_is_visible()) {
                context_menu_show(screens[current_screen]->scr);
            }
            last_handled_gesture_tick = tick;
            return;
        }

        // When context menu is visible, let LVGL handle clicks but block swipe gestures
        if (context_menu_is_visible()) {
            // Allow single clicks to pass through to LVGL for button handling
            return;
        }

        if (touchData.gestureID == HAL_GESTURE_SWIPE_LEFT)
        {
            current_screen = (current_screen + num_screens - 1) % num_screens;
            lv_scr_load_anim(screens[current_screen]->scr, LV_SCR_LOAD_ANIM_MOVE_LEFT, 100, 0, false);
            last_handled_gesture_tick = tick;
        }
        else if (touchData.gestureID == HAL_GESTURE_SWIPE_RIGHT)
        {
            current_screen = (current_screen + 1) % num_screens;
            lv_scr_load_anim(screens[current_screen]->scr, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, false);
            last_handled_gesture_tick = tick;
        }
        else if (touchData.gestureID == HAL_GESTURE_SWIPE_UP)
        {
            screens[current_screen]->on_swipe_up();
            last_handled_gesture_tick = tick;
        }
        else if (touchData.gestureID == HAL_GESTURE_SWIPE_DOWN)
        {
            screens[current_screen]->on_swipe_down();
            last_handled_gesture_tick = tick;
        }

    }
}

void on_loading_completed()
{
    status = LOADING_COMPLETED;
}

void setup()
{
    // Very early initialization - before any other code runs
    Serial.begin(115200);
    delay(1000);  // Longer delay to catch serial output

    Serial.println("\n\n========================================");
    Serial.println("SaildropOS Early Boot");
    Serial.println("========================================");
    Serial.flush();

    // Print reset reason
    esp_reset_reason_t reason = esp_reset_reason();
    Serial.printf("Reset reason: %d\n", reason);
    Serial.flush();

    // Check PSRAM
    Serial.printf("PSRAM Size: %d bytes\n", ESP.getPsramSize());
    Serial.printf("Free PSRAM: %d bytes\n", ESP.getFreePsram());
    Serial.printf("Free Heap: %d bytes\n", ESP.getFreeHeap());
    Serial.flush();

    // Check for safe boot mode (hold BOOT button during startup to clear settings)
    // GPIO0 is the BOOT button on most ESP32-S3 boards
    pinMode(0, INPUT_PULLUP);
    delay(100);
    if (digitalRead(0) == LOW) {
        Serial.println("!!! SAFE BOOT: BOOT button held - clearing settings !!!");
        Serial.flush();
        getSettings()->begin();
        getSettings()->clear();
        Serial.println("Settings cleared. Release button and reboot.");
        Serial.flush();
        delay(3000);
        ESP.restart();
    }

    Serial.println("SaildropOS is booting.");
    Serial.flush();
    Serial.printf("Board: %s (%dx%d)\n", BOARD_NAME, SCREEN_WIDTH, SCREEN_HEIGHT);
    Serial.flush();

    String LVGL_Arduino = "LVGL: ";
    LVGL_Arduino += String('V') + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();

    Serial.println(LVGL_Arduino);
    Serial.flush();

    // Initialize settings
    Serial.println("Loading settings...");
    Serial.flush();
    getSettings()->begin();
    getSettings()->load();
    Serial.println("Settings loaded.");
    Serial.flush();

    Serial.println("Initializing LVGL...");
    Serial.flush();
    lv_init();
    Serial.println("LVGL initialized.");
    Serial.flush();
#if LV_USE_LOG != 0
    lv_log_register_print_cb(my_print); /* register print function for debugging */
#endif

    // Initialize Hardware Abstraction Layer (display + touch)
    hal().begin();

    Serial.println("HAL initialized");

#if USE_PSRAM_BUFFER && defined(BOARD_LCD_4)
    // Allocate draw buffer from PSRAM for 4" display
    draw_buf = (uint8_t*)heap_caps_malloc(DRAW_BUF_ALLOC_SIZE, MALLOC_CAP_SPIRAM);
    if (draw_buf == nullptr) {
        Serial.println("ERROR: Failed to allocate draw buffer from PSRAM!");
        // Fallback to internal RAM with smaller buffer
        draw_buf = (uint8_t*)malloc(SCREEN_WIDTH * SCREEN_HEIGHT / 20 * (LV_COLOR_DEPTH / 8));
    }
    // IMPORTANT: Zero the buffer to prevent garbage pixels in corners
    if (draw_buf != nullptr) {
        memset(draw_buf, 0, DRAW_BUF_ALLOC_SIZE);
    }
#endif

    // Initialize display using HAL
    lv_display_t * disp = hal().initDisplay(draw_buf, DRAW_BUF_ALLOC_SIZE);

    /*Initialize the input device driver*/
    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER); /*Touchpad should have POINTER type*/
    lv_indev_set_read_cb(indev, my_touchpad_read);


    //////////////// Create screens in order defined by conf.h
    Serial.println("LVGL initialized.\nCreating screens...");

    // Screen order array - add screens sorted by their order value
    struct ScreenEntry { int order; Screen* (*create)(); };
    ScreenEntry entries[] = {
        #ifdef SCREEN_SPEED
        { SCREEN_SPEED, []() -> Screen* { return new SpeedScreen(); } },
        #endif
        #ifdef SCREEN_WIND
        { SCREEN_WIND, []() -> Screen* { return new WindScreen(); } },
        #endif
        #ifdef SCREEN_COMPASS
        { SCREEN_COMPASS, []() -> Screen* { return new CompassScreen(); } },
        #endif
        #ifdef SCREEN_VALUES
        { SCREEN_VALUES, []() -> Screen* { return new ValuesScreen(); } },
        #endif
        #ifdef SCREEN_AIS
        { SCREEN_AIS, []() -> Screen* { return new AISScreen(); } },
        #endif
        #ifdef SCREEN_TACK
        { SCREEN_TACK, []() -> Screen* { return new TackScreen(); } },
        #endif
        #ifdef SCREEN_TIMER
        { SCREEN_TIMER, []() -> Screen* { return new TimerScreen(); } },
        #endif
    };

    // Sort by order value (simple bubble sort for small array)
    int n_entries = sizeof(entries) / sizeof(entries[0]);
    for (int i = 0; i < n_entries - 1; i++) {
        for (int j = 0; j < n_entries - i - 1; j++) {
            if (entries[j].order > entries[j + 1].order) {
                ScreenEntry tmp = entries[j];
                entries[j] = entries[j + 1];
                entries[j + 1] = tmp;
            }
        }
    }

    // Add screens in sorted order
    for (int i = 0; i < n_entries; i++) {
        add_screen(entries[i].create());
    }

    current_screen = 0;

    splash = new SplashScreen(&on_loading_completed);
    portalScreen = new PortalScreen();

    // Create context menu (long press menu)
    context_menu_create();

    // Decide initial mode based on settings
    if (!getSettings()->isConfigured()) {
        Serial.println("Starting in portal mode (first run or reset)");
        status = PORTAL_MODE;
        lv_disp_load_scr(portalScreen->scr);
    } else {
        Serial.println("Starting normal boot with saved settings");
        status = BOOT;
        lv_disp_load_scr(splash->scr);
    }

    // Force full screen refresh to clear any garbage in corners
    // This is needed for RGB displays with partial rendering mode
    lv_obj_invalidate(lv_scr_act());
    lv_refr_now(NULL);

    // Setup timers
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &on_tick,
        .name = "lvgl_tick"};

    esp_timer_handle_t lvgl_tick_timer = NULL;
    esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer);
    esp_timer_start_periodic(lvgl_tick_timer, LVGL_TICK_PERIOD_MS * 1000);


    // Start loop() on second core
    xTaskCreatePinnedToCore(
        core2_loop,
        "core2_loop",
        10000,            /* Stack size in words */
        NULL,             /* Task input parameter */
        0,                /* Priority of the task */
        &core2_loop_task, /* Task handle. */
        0);               /* Core where the task should run */

    Serial.println("Setup done");
}

void loop()
{
    tick++;

    // Handle context menu actions
    if (context_menu_is_visible()) {
        ContextMenuAction action = context_menu_get_result();
        if (action != MENU_NONE) {
            switch (action) {
                case MENU_RESET_SETTINGS:
                    Serial.println("Context menu: Reset settings requested");
                    getSettings()->clear();
                    delay(500);
                    ESP.restart();
                    break;
                case MENU_TOGGLE_AP_MODE:
                    {
                        bool currentApMode = getSettings()->get()->ap_mode;
                        Serial.printf("Context menu: Toggle AP mode %s -> %s\n",
                                     currentApMode ? "ON" : "OFF",
                                     !currentApMode ? "ON" : "OFF");
                        getSettings()->setApMode(!currentApMode);
                        getSettings()->save();
                        delay(500);
                        ESP.restart();
                    }
                    break;
                case MENU_REBOOT:
                    Serial.println("Context menu: Reboot requested");
                    delay(500);
                    ESP.restart();
                    break;
                case MENU_CLOSE:
                    Serial.println("Context menu: Close requested");
                    context_menu_hide(screens[current_screen]->scr);
                    break;
                default:
                    break;
            }
            context_menu_clear_result();
        }
    }

    // Handle screen transitions based on status changes
    static SAILDROP_STATUS lastStatus = BOOT;

    if (status != lastStatus) {
        if (status == PORTAL_MODE && lastStatus != PORTAL_MODE) {
            // Entering portal mode - show portal screen
            lv_disp_load_scr(portalScreen->scr);
        }
        else if (status == BOOT && lastStatus == PORTAL_MODE) {
            // Exiting portal mode - show splash for normal boot
            lv_disp_load_scr(splash->scr);
        }
        else if (status == LOADING_COMPLETED) {
            status = RUNNING;
            lv_disp_load_scr(screens[current_screen]->scr);
        }
        lastStatus = status;
    }

    lv_timer_handler();
    delay(LOOP_DELAY);
}
