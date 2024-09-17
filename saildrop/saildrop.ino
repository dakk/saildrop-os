#include <lvgl.h>
#include <TFT_eSPI.h>
#include "lv_conf.h"
#include "CST816S.h"
#include "speedgauge.h"
#include "windgauge.h"

#define EXAMPLE_LVGL_TICK_PERIOD_MS    2

/*Change to your screen resolution*/
static const uint16_t screenWidth  = 240;
static const uint16_t screenHeight = 240;

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[ screenWidth * screenHeight / 10 ];


static lv_obj_t* screens[2];
static const uint8_t num_screens = 2;
int current_screen = 0;

TFT_eSPI tft = TFT_eSPI(screenWidth, screenHeight); /* TFT instance */
CST816S touch(6, 7, 13, 5);  // sda, scl, rst, irq

#if LV_USE_LOG != 0
/* Serial debugging */
void my_print(const char * buf)
{
    Serial.printf(buf);
    Serial.flush();
}
#endif

/* Display flushing */
void my_disp_flush( lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p )
{
    uint32_t w = ( area->x2 - area->x1 + 1 );
    uint32_t h = ( area->y2 - area->y1 + 1 );

    tft.startWrite();
    tft.setAddrWindow( area->x1, area->y1, w, h );
    tft.pushColors( ( uint16_t * )&color_p->full, w * h, true );
    tft.endWrite();

    lv_disp_flush_ready( disp_drv );
}

void example_increase_lvgl_tick(void *arg)
{
    /* Tell LVGL how many milliseconds has elapsed */
    lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS);
}

/*Read the touchpad*/
void my_touchpad_read( lv_indev_drv_t * indev_drv, lv_indev_data_t * data )
{
    // uint16_t touchX, touchY;

    bool touched = touch.available();
    // touch.read_touch();
    if( !touched )
    // if( 0!=touch.data.points )
    {
        data->state = LV_INDEV_STATE_REL;
    }
    else
    {
        data->state = LV_INDEV_STATE_PR;

        /*Set the coordinates*/
        // data->point.x = touch.data.x;
        // data->point.y = touch.data.y;
        // Serial.print( "Data x " );
        // Serial.println( touch.data.x );

        // Serial.print( "Data y " );
        // Serial.println( touch.data.y );

        Serial.println(touch.gesture());

        if (touch.data.gestureID == SWIPE_LEFT) {
            current_screen = (current_screen - 1) % num_screens;
            lv_scr_load_anim(screens[current_screen], LV_SCR_LOAD_ANIM_MOVE_LEFT, 100, 0, false);
        } else if (touch.data.gestureID == SWIPE_RIGHT) {
            current_screen = (current_screen + 1) % num_screens;
            lv_scr_load_anim(screens[current_screen], LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, false);
        }
    }
}



void setup()
{
    Serial.begin( 115200 ); /* prepare for possible serial debug */

    String LVGL_Arduino = "Hello Arduino! ";
    LVGL_Arduino += String('V') + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();

    Serial.println( LVGL_Arduino );
    Serial.println( "I am LVGL_Arduino" );

    lv_init();
#if LV_USE_LOG != 0
    lv_log_register_print_cb( my_print ); /* register print function for debugging */
#endif

    tft.begin();          /* TFT init */
    tft.setRotation( 0 ); /* Landscape orientation, flipped */
    
    /*Set the touchscreen calibration data,
     the actual data for your display can be acquired using
     the Generic -> Touch_calibrate example from the TFT_eSPI library*/
    // uint16_t calData[5] = { 275, 3620, 264, 3532, 1 };
    // tft.setTouch( calData );
    touch.begin();

    lv_disp_draw_buf_init( &draw_buf, buf, NULL, screenWidth * screenHeight / 10 );

    /*Initialize the display*/
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init( &disp_drv );
    /*Change the following line to your display resolution*/
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register( &disp_drv );

    /*Initialize the (dummy) input device driver*/
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init( &indev_drv );
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register( &indev_drv );


    //////////////// Create screens

    // Wind screen
    lv_obj_t *ui_wind_scr = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_wind_scr, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_wind_scr, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_wind_scr, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    WindGauge *wind_gauge = new WindGauge(ui_wind_scr, screenWidth, screenHeight);
    wind_gauge->set_direction(33);
    wind_gauge->set_speed(20);
    wind_gauge->showcase();
    
    
    // Speed screen
    lv_obj_t *ui_speed_scr = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_speed_scr, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_speed_scr, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_speed_scr, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    
    SpeedGauge *speed_gauge = new SpeedGauge(ui_speed_scr, screenWidth, screenHeight);

    
    // Load first screen
    screens[0] = ui_wind_scr;
    screens[1] = ui_speed_scr;
    current_screen = 0;
    lv_disp_load_scr(ui_wind_scr);

    // Setup timers
    const esp_timer_create_args_t lvgl_tick_timer_args = {
      .callback = &example_increase_lvgl_tick,
      .name = "lvgl_tick"
    };

    esp_timer_handle_t lvgl_tick_timer = NULL;
    esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer);
    esp_timer_start_periodic(lvgl_tick_timer, EXAMPLE_LVGL_TICK_PERIOD_MS * 1000);
    
    Serial.println( "Setup done" );
}

void loop()
{
    lv_timer_handler(); /* let the GUI do its work */
    delay( 5 );
}
