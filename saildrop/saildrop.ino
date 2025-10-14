#include <lvgl.h>
#include <TFT_eSPI.h>
#include "lv_conf.h"
#include "CST816S.h"
#include "conn.h"
#include "conf.h"

#include "screens/screen.h"
// #include "screens/compassscreen.h"
#include "screens/speedscreen.h"
// #include "screens/windscreen.h"
#include "screens/splashscreen.h"
#include "screens/valuesscreen.h"
// #include "screens/tackscreen.h"
// #include "screens/timerscreen.h"

#define DRAW_BUF_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT  / 10 * (LV_COLOR_DEPTH / 8))
uint32_t draw_buf[DRAW_BUF_SIZE / 4];

static Screen *screens[16];
int current_screen = 0;
uint8_t num_screens = 0;
Screen *splash;

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
        if (status == BOOT)
        {
            status = LOADING_TRIGGERED;
            ((SplashScreen *)splash)->load();
            // Inizialize the connection module
            initialize_connections();
            #ifndef AP_MODE
                connect_wifi(WIFI_DEFAULT_SSID, WIFI_DEFAULT_PASSWORD);
            #endif
        } else {
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
        Serial.println(touch.gesture());

        if (status != RUNNING || (tick - last_handled_gesture_tick) < 100)
            return;

        if (touch.data.gestureID == SWIPE_LEFT)
        {
            current_screen = abs((current_screen - 1) % num_screens);
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

void setup()
{
    Serial.begin(115200);
    Serial.println("SaildropOS is booting.");

    String LVGL_Arduino = "LVGL: ";
    LVGL_Arduino += String('V') + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();

    Serial.println(LVGL_Arduino);

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
    touch.begin();

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
    // add_screen(new WindScreen());
    // add_screen(new CompassScreen());
    add_screen(new ValuesScreen());
    // add_screen(new TackScreen());
    // add_screen(new TimerScreen());
    current_screen = 0;

    splash = new SplashScreen(&on_loading_completed);
    lv_disp_load_scr(splash->scr);

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
    tick ++;
    if (status == LOADING_COMPLETED)
    {
        status = RUNNING;
        lv_disp_load_scr(screens[current_screen]->scr);
        // lv_scr_load_anim(screens[current_screen], LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, false);
    }

    lv_timer_handler();
    delay(LOOP_DELAY);
}
