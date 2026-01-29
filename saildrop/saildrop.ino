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
#include <lvgl.h>
#include <TFT_eSPI.h>
#include "lv_conf.h"
#include "CST816S.h"
#include "conn.h"
#include "conf.h"
#include "settings.h"
#include "wifiportal.h"

#include "screens/screen.h"
#include "screens/compassscreen.h"
#include "screens/speedscreen.h"
#include "screens/windscreen.h"
#include "screens/splashscreen.h"
#include "screens/valuesscreen.h"
#include "screens/portalscreen.h"
#include "screens/aisscreen.h"
// #include "screens/tackscreen.h"
// #include "screens/timerscreen.h"

#define DRAW_BUF_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT  / 10 * (LV_COLOR_DEPTH / 8))
uint32_t draw_buf[DRAW_BUF_SIZE / 4];

static Screen *screens[16];
int current_screen = 0;
uint8_t num_screens = 0;
Screen *splash;
Screen *portalScreen;

void add_screen(Screen *sc) {
    screens[num_screens] = sc;
    num_screens ++;
}

TFT_eSPI tft = TFT_eSPI(SCREEN_WIDTH, SCREEN_HEIGHT); /* TFT instance */
CST816S touch(6, 7, 13, 5);                         // sda, scl, rst, irq

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

// Long press detection for factory reset
#define BOOT_LONG_PRESS_MS 3000
bool factoryResetRequested = false;

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

/*Read the touchpad*/
void my_touchpad_read( lv_indev_t * indev, lv_indev_data_t * data )
{
    // uint16_t touchX, touchY;

    bool touched = touch.available();
    // touch.read_touch();
    if (!touched)
    // if( 0!=touch.data.points )
    {
        data->state = LV_INDEV_STATE_REL;
    }
    else
    {
        data->state = LV_INDEV_STATE_PR;
        Serial.printf("Gesture: %s, ID: %d, status: %d, tick: %lu, last: %lu\n",
                      touch.gesture(), touch.data.gestureID, status, tick, last_handled_gesture_tick);

        if (status != RUNNING || (tick - last_handled_gesture_tick) < 50)
            return;

        Serial.printf("Handling gesture ID: %d\n", touch.data.gestureID);
        if (touch.data.gestureID == SWIPE_LEFT)
        {
            current_screen = (current_screen + num_screens - 1) % num_screens;
            lv_scr_load_anim(screens[current_screen]->scr, LV_SCR_LOAD_ANIM_MOVE_LEFT, 100, 0, false);
            last_handled_gesture_tick = tick;
        }
        else if (touch.data.gestureID == SWIPE_RIGHT)
        {
            current_screen = (current_screen + 1) % num_screens;
            lv_scr_load_anim(screens[current_screen]->scr, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, false);
            last_handled_gesture_tick = tick;
        }
        else if (touch.data.gestureID == SWIPE_UP)
        {
            screens[current_screen]->on_swipe_up();
            last_handled_gesture_tick = tick;
        }
        else if (touch.data.gestureID == SWIPE_DOWN)
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

// Detect long press at boot for factory reset
bool detectBootLongPress() {
    Serial.println("Hold touch for 3 seconds to factory reset...");
    uint32_t start = millis();

    while (millis() - start < BOOT_LONG_PRESS_MS) {
        if (touch.available()) {
            if (touch.data.gestureID == LONG_PRESS || touch.data.points > 0) {
                // Touch detected, wait for full duration
                uint32_t touchStart = millis();
                while (millis() - touchStart < BOOT_LONG_PRESS_MS) {
                    if (!touch.available()) {
                        // Touch released too early
                        return false;
                    }
                    delay(50);
                }
                // Held for full duration
                Serial.println("Factory reset triggered!");
                return true;
            }
        }
        delay(50);
    }
    return false;
}

void setup()
{
    Serial.begin(115200);
    Serial.println("SaildropOS is booting.");

    String LVGL_Arduino = "LVGL: ";
    LVGL_Arduino += String('V') + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();

    Serial.println(LVGL_Arduino);

    // Initialize touch early for long-press detection
    touch.begin();

    // Check for factory reset (long press during boot)
    factoryResetRequested = detectBootLongPress();

    // Initialize settings
    getSettings()->begin();

    if (factoryResetRequested) {
        Serial.println("Clearing settings (factory reset)");
        getSettings()->clear();
    } else {
        getSettings()->load();
    }

    lv_init();
#if LV_USE_LOG != 0
    lv_log_register_print_cb(my_print); /* register print function for debugging */
#endif

    tft.begin();        /* TFT init */
    tft.setRotation(0); /* Landscape orientation, flipped */

    /*Set the touchscreen calibration data,
     the actual data for your display can be acquired using
     the Generic -> Touch_calibrate example from the TFT_eSPI library*/
    // uint16_t calData[5] = { 275, 3620, 264, 3532, 1 };
    // tft.setTouch( calData );

    Serial.println("Touch and TFT initialized");

    lv_display_t * disp;
    /*TFT_eSPI can be enabled lv_conf.h to initialize the display in a simple way*/
    disp = lv_tft_espi_create(SCREEN_WIDTH, SCREEN_HEIGHT, draw_buf, sizeof(draw_buf));
    lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_0);

    /*Initialize the (dummy) input device driver*/
    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER); /*Touchpad should have POINTER type*/
    lv_indev_set_read_cb(indev, my_touchpad_read);


    //////////////// Create screens
    Serial.println("LVGL initialized.\nCreating screens...");
    add_screen(new SpeedScreen());
    add_screen(new WindScreen());
    add_screen(new CompassScreen());
    add_screen(new ValuesScreen());
    add_screen(new AISScreen());
    // add_screen(new TackScreen());
    // add_screen(new TimerScreen());
    current_screen = 0;

    splash = new SplashScreen(&on_loading_completed);
    portalScreen = new PortalScreen();

    // Decide initial mode based on settings
    if (factoryResetRequested || !getSettings()->isConfigured()) {
        Serial.println("Starting in portal mode (first run or reset)");
        status = PORTAL_MODE;
        lv_disp_load_scr(portalScreen->scr);
    } else {
        Serial.println("Starting normal boot with saved settings");
        status = BOOT;
        lv_disp_load_scr(splash->scr);
    }

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
