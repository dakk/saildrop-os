#ifndef SCREEN_H
#define SCREEN_H

lv_obj_t *default_screen_create() {
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(scr, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    return scr;
}

class Screen {
    public:
        lv_obj_t *scr; 

        virtual void on_swipe_up() {}
        virtual void on_swipe_down() {}
};

#endif
