/*
 * Copyright (C) 2024-2025 Davide Gessa
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef SPLASHSCREEN_H
#define SPLASHSCREEN_H

#include "screen.h"
#include "../conn.h"
#include "../conf.h"
#include "../scale.h"

class SplashScreen : public Screen {
private:
    void (*on_complete)();
    lv_obj_t *drop;
    lv_obj_t *ripple1;
    lv_obj_t *ripple2;
    lv_obj_t *ripple3;
    bool drop_falling = true;

    static void drop_fall_cb(void *var, int32_t value) {
        lv_obj_t *obj = static_cast<lv_obj_t*>(var);
        lv_obj_set_y(obj, value);
    }

    static void drop_complete_cb(lv_anim_t *anim) {
        SplashScreen *splash = static_cast<SplashScreen*>(anim->user_data);
        splash->on_drop_landed();
    }

    static void ripple_size_cb(void *var, int32_t value) {
        lv_obj_t *obj = static_cast<lv_obj_t*>(var);
        lv_obj_set_size(obj, value, value);
        lv_obj_center(obj);
        lv_obj_set_y(obj, SCALE_PX(20));
    }

    static void ripple_opa_cb(void *var, int32_t value) {
        lv_obj_t *obj = static_cast<lv_obj_t*>(var);
        lv_obj_set_style_border_opa(obj, value, 0);
    }

    void start_ripple(lv_obj_t *ripple, int delay_ms) {
        // Size animation
        lv_anim_t size_anim;
        lv_anim_init(&size_anim);
        lv_anim_set_var(&size_anim, ripple);
        lv_anim_set_exec_cb(&size_anim, ripple_size_cb);
        lv_anim_set_values(&size_anim, SCALE_PX(10), SCALE_PX(180));
        lv_anim_set_time(&size_anim, 1500);
        lv_anim_set_delay(&size_anim, delay_ms);
        lv_anim_set_repeat_count(&size_anim, LV_ANIM_REPEAT_INFINITE);
        lv_anim_set_repeat_delay(&size_anim, 500);
        lv_anim_start(&size_anim);

        // Opacity animation (fade out as it expands)
        lv_anim_t opa_anim;
        lv_anim_init(&opa_anim);
        lv_anim_set_var(&opa_anim, ripple);
        lv_anim_set_exec_cb(&opa_anim, ripple_opa_cb);
        lv_anim_set_values(&opa_anim, 255, 0);
        lv_anim_set_time(&opa_anim, 1500);
        lv_anim_set_delay(&opa_anim, delay_ms);
        lv_anim_set_repeat_count(&opa_anim, LV_ANIM_REPEAT_INFINITE);
        lv_anim_set_repeat_delay(&opa_anim, 500);
        lv_anim_start(&opa_anim);
    }

    void on_drop_landed() {
        // Hide the drop
        lv_obj_add_flag(drop, LV_OBJ_FLAG_HIDDEN);
        drop_falling = false;

        // Start ripple animations with staggered delays
        start_ripple(ripple1, 0);
        start_ripple(ripple2, 400);
        start_ripple(ripple3, 800);
    }

    lv_obj_t* create_ripple() {
        lv_obj_t *ripple = lv_obj_create(scr);
        lv_obj_set_size(ripple, SCALE_PX(10), SCALE_PX(10));
        lv_obj_center(ripple);
        lv_obj_set_y(ripple, SCALE_PX(20));
        lv_obj_set_style_radius(ripple, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(ripple, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(ripple, SCALE_PX(3), 0);
        lv_obj_set_style_border_color(ripple, lv_palette_main(LV_PALETTE_ORANGE), 0);
        lv_obj_set_style_border_opa(ripple, 0, 0);
        lv_obj_remove_flag(ripple, LV_OBJ_FLAG_SCROLLABLE);
        return ripple;
    }

public:
    SplashScreen(void (*on_complete_f)()) {
        on_complete = on_complete_f;
        scr = lv_obj_create(NULL);
        lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(scr, lv_color_hex(0x0), LV_PART_MAIN | LV_STATE_DEFAULT);

        // Create ripples (behind the drop)
        ripple1 = create_ripple();
        ripple2 = create_ripple();
        ripple3 = create_ripple();

        // Water drop
        drop = lv_obj_create(scr);
        lv_obj_set_size(drop, SCALE_PX(20), SCALE_PX(28));
        lv_obj_align(drop, LV_ALIGN_TOP_MID, 0, -SCALE_PX(30));
        lv_obj_set_style_radius(drop, SCALE_PX(10), 0);
        lv_obj_set_style_bg_color(drop, lv_palette_main(LV_PALETTE_ORANGE), 0);
        lv_obj_set_style_border_width(drop, 0, 0);
        lv_obj_remove_flag(drop, LV_OBJ_FLAG_SCROLLABLE);

        // Drop fall animation
        lv_anim_t fall_anim;
        lv_anim_init(&fall_anim);
        lv_anim_set_var(&fall_anim, drop);
        lv_anim_set_exec_cb(&fall_anim, drop_fall_cb);
        lv_anim_set_values(&fall_anim, -SCALE_PX(30), SCALE_PX(100));
        lv_anim_set_time(&fall_anim, 800);
        lv_anim_set_path_cb(&fall_anim, lv_anim_path_ease_in);
        lv_anim_set_user_data(&fall_anim, this);
        lv_anim_set_completed_cb(&fall_anim, drop_complete_cb);
        lv_anim_start(&fall_anim);

        // Title label
        lv_obj_t *title = lv_label_create(scr);
        lv_obj_align(title, LV_ALIGN_CENTER, 0, SCALE_PX(80));
        lv_label_set_text(title, "SAILDROP");
        lv_obj_set_style_text_color(title, lv_color_white(), 0);
        lv_obj_set_style_text_font(title, ui_scale::font_medium(), 0);
    }

    void load() {
        delay(4000);
        on_complete();
    }
};

#endif
