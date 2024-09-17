#ifndef SPLASHSCREEN_H
#define SPLASHSCREEN_H

#include "screen.h"
#include "conn.h"

class SplashScreen : public Screen {
    private:
        void (*on_complete)();

    public:
        SplashScreen(void (*on_complete_f)()) {
            on_complete = on_complete_f;
            scr = lv_obj_create(NULL);
            lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
            // lv_obj_set_style_bg_color(scr, lv_color_hex(0xffffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            // lv_obj_set_style_bg_opa(scr, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

            // lv_obj_t* splash_bg = lv_img_create(scr);
            // lv_img_set_src(splash_bg, &ui_img_bg1_png);
            // lv_obj_set_width(splash_bg, LV_SIZE_CONTENT);
            // lv_obj_set_height(splash_bg, LV_SIZE_CONTENT);
            // lv_obj_set_align(splash_bg, LV_ALIGN_CENTER);
            // lv_obj_add_flag(splash_bg, LV_OBJ_FLAG_ADV_HITTEST);
            // lv_obj_clear_flag(splash_bg, LV_OBJ_FLAG_SCROLLABLE);

            lv_obj_t* splash_label = lv_label_create(scr);
            lv_obj_set_width(splash_label, LV_SIZE_CONTENT);
            lv_obj_set_height(splash_label, LV_SIZE_CONTENT); 
            lv_obj_set_x(splash_label, 2);
            lv_obj_set_y(splash_label, 40);
            lv_obj_set_align(splash_label, LV_ALIGN_TOP_MID);
            lv_label_set_text(splash_label, "SAILDROP");
            // lv_obj_set_style_text_color(splash_label, lv_color_hex(0x0), LV_PART_MAIN | LV_STATE_DEFAULT);
            // lv_obj_set_style_text_opa(splash_label, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            // lv_obj_set_style_text_font(splash_label, &lv_font_montserrat_44, LV_PART_MAIN | LV_STATE_DEFAULT);

            lv_obj_t * spinner = lv_spinner_create(scr, 1000, 60);
            lv_obj_set_size(spinner, 60, 60);
            lv_obj_center(spinner);
        }

        void load() {
            list_networks();
            on_complete();
        }

        void on_tick(uint32_t tick) {
        }
};

#endif
