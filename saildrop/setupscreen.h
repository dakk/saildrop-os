#ifndef SETUPSCREEN_H
#define SETUPSCREEN_H

#include "screen.h"

class SetupScreen : public Screen
{
private:
public:
    SetupScreen()
    {
        scr = lv_obj_create(NULL);
        lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *cont_col = lv_obj_create(scr);
        lv_obj_set_size(cont_col, 200, 150);
        // lv_obj_align_to(cont_col, cont_row, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
        lv_obj_set_flex_flow(cont_col, LV_FLEX_FLOW_COLUMN);

        lv_obj_t * roller1 = lv_roller_create(cont_col);
        lv_roller_set_options(roller1,
                            "January\n"
                            "February\n"
                            "March\n"
                            "April\n"
                            "May\n"
                            "June\n"
                            "July\n"
                            "August\n"
                            "September\n"
                            "October\n"
                            "November\n"
                            "December",
                            LV_ROLLER_MODE_INFINITE);

        lv_roller_set_visible_row_count(roller1, 4);
        lv_obj_center(roller1);
        // lv_obj_add_event_cb(roller1, event_handler, LV_EVENT_ALL, NULL);
    }
};

#endif
