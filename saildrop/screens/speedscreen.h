#ifndef SPEEDSCREEN_H
#define SPEEDSCREEN_H

#include "screen.h"
#include "../gauges/speedgauge.h"

class SpeedScreen : public Screen
{
private:
public:
    SpeedScreen()
    {
        scr = lv_obj_create(NULL);
        lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE); /// Flags
        lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(scr, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

        SpeedGauge *speed_gauge = new SpeedGauge(scr, SCREEN_WIDTH, SCREEN_HEIGHT);
        speed_gauge->showcase();
    }
};

#endif
