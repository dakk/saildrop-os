#include <lvgl.h>
#include <TFT_eSPI.h>
#include "lv_conf.h"
#include "CST816S.h"

#ifndef LV_USE_TFT_ESPI
  #error "LVGL needs to be compiled with TFT_ESPI support"
#endif

#define DEBUG 1
#define LVGL_TICK_PERIOD_MS 2
#define LOOP_DELAY 2

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 240

#define DRAW_BUF_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT  / 10 * (LV_COLOR_DEPTH / 8))
uint32_t draw_buf[DRAW_BUF_SIZE / 4];

TFT_eSPI tft = TFT_eSPI(SCREEN_WIDTH, SCREEN_HEIGHT); /* TFT instance */
CST816S touch(6, 7, 13, 5);                         // sda, scl, rst, irq



uint32_t tick = 0;
uint32_t last_handled_gesture_tick = 0;



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

// /*Read the touchpad*/
// void my_touchpad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
// {
//     // uint16_t touchX, touchY;

//     bool touched = touch.available();
//     // touch.read_touch();
//     if (!touched)
//     // if( 0!=touch.data.points )
//     {
//         data->state = LV_INDEV_STATE_REL;
//     }
//     else
//     {
//         data->state = LV_INDEV_STATE_PR;
//         Serial.println(touch.gesture());

//         if (status != RUNNING || (tick - last_handled_gesture_tick) < 100)
//             return;

//         if (touch.data.gestureID == SWIPE_LEFT)
//         {
//             current_screen = abs((current_screen - 1) % num_screens);
//             lv_scr_load_anim(screens[current_screen]->scr, LV_SCR_LOAD_ANIM_MOVE_LEFT, 100, 0, false);
//             last_handled_gesture_tick = tick;
//         }
//         else if (touch.data.gestureID == SWIPE_RIGHT)
//         {
//             current_screen = (current_screen + 1) % num_screens;
//             lv_scr_load_anim(screens[current_screen]->scr, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, false);
//             last_handled_gesture_tick = tick;
//         }
//         else if (touch.data.gestureID == SWIPE_UP)
//         {
//             screens[current_screen]->on_swipe_up();
//             last_handled_gesture_tick = tick;
//         }
//         else if (touch.data.gestureID == SWIPE_DOWN)
//         {
//             screens[current_screen]->on_swipe_down();
//             last_handled_gesture_tick = tick;
//         }

//     }
// }


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

    touch.begin();

    Serial.println("Touch and TFT initialized");


    lv_display_t * disp;
    /*TFT_eSPI can be enabled lv_conf.h to initialize the display in a simple way*/
    disp = lv_tft_espi_create(SCREEN_WIDTH, SCREEN_HEIGHT, draw_buf, sizeof(draw_buf));
    lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_0);

    /*Initialize the (dummy) input device driver*/
    // static lv_indev_drv_t indev_drv;
    // lv_indev_drv_init(&indev_drv);
    // indev_drv.type = LV_INDEV_TYPE_POINTER;
    // indev_drv.read_cb = my_touchpad_read;
    // lv_indev_drv_register(&indev_drv);

    /*Change the active screen's background color*/
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x003a57), LV_PART_MAIN);

    /*Create a white label, set its text and align it to the center*/
    lv_obj_t * label = lv_label_create(lv_screen_active());
    lv_label_set_text(label, "Hello world");
    lv_obj_set_style_text_color(label, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

    Serial.println("Setup done");
}

void loop()
{
    lv_timer_handler(); /* let the GUI do its work */
    delay(5); /* let this time pass */
}
