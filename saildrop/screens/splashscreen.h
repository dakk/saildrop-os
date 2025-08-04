#ifndef SPLASHSCREEN_H
#define SPLASHSCREEN_H

#include "screen.h"
#include "../conn.h"
#include "../conf.h"

class SplashScreen : public Screen {
    private:
        void (*on_complete)();

    public:
        SplashScreen(void (*on_complete_f)()) {
            on_complete = on_complete_f;
            scr = lv_obj_create(NULL);
            lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_bg_color(scr, lv_color_hex(0x0), LV_PART_MAIN | LV_STATE_DEFAULT);


            lv_obj_t * spinner = lv_spinner_create(scr);
            lv_obj_center(spinner);
            lv_spinner_set_anim_params(spinner, 1000, 60);

            // lv_obj_set_style_arc_width(spinner, 20, LV_PART_MAIN );
            // lv_obj_set_style_arc_width(spinner, 20, LV_PART_INDICATOR);
            //lv_obj_set_style_arc_color(spinner, lv_palette_main(LV_PALETTE_ORANGE), LV_PART_MAIN);
            lv_obj_set_style_arc_color(spinner, lv_palette_darken(LV_PALETTE_ORANGE, 3), LV_PART_INDICATOR);
            lv_obj_set_size(spinner, SCREEN_WIDTH, SCREEN_HEIGHT);
            lv_obj_center(spinner);


            lv_obj_t* splash_label = lv_label_create(scr);
            lv_obj_set_width(splash_label, LV_SIZE_CONTENT);
            lv_obj_set_height(splash_label, LV_SIZE_CONTENT); 
            lv_obj_set_align(splash_label, LV_ALIGN_CENTER);
            lv_label_set_text(splash_label, "SAILDROP");
            lv_obj_set_style_text_color(splash_label, lv_color_white(), LV_PART_MAIN | LV_STATE_DEFAULT);
        }

        void load() {
            delay(2000);
            on_complete();
        }
};

#endif
