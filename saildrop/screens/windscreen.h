#ifndef WINDSCREEN_H
#define WINDSCREEN_H

#include "screen.h"
#include "../gauges/windgauge.h"

class WindScreen : public Screen
{
private:
public:
    WindScreen()
    {
        scr = lv_obj_create(NULL);
        lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE); /// Flags
        lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(scr, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

        WindGauge *wind_gauge = new WindGauge(scr, SCREEN_WIDTH, SCREEN_HEIGHT);
        wind_gauge->showcase();
    }
};

#endif
