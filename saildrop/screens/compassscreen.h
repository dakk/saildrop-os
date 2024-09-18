#ifndef COMPASSSCREEN_H
#define COMPASSSCREEN_H

#include "screen.h"
#include "../gauges/compass.h"

class CompassScreen : public Screen
{
private:
public:
    CompassScreen()
    {
        scr = lv_obj_create(NULL);
        lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE); /// Flags
        lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(scr, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

        Compass *compass = new Compass(scr, SCREEN_WIDTH, SCREEN_HEIGHT);
        compass->showcase();
    }
};

#endif
