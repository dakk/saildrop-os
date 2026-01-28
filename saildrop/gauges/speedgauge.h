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
#ifndef SPEEDGAUGE_H
#define SPEEDGAUGE_H

#include <lvgl.h>
#include "../data.h"
#include "styles.h"

class SpeedGauge {
private:
    lv_obj_t *speed_label;
    lv_obj_t *arc_bg;
    lv_obj_t *arc_indicator;
    int32_t max_speed = 200;  // 20.0 knots in tenths

    static void tick_cb(lv_timer_t *timer) {
        SpeedGauge *gauge = static_cast<SpeedGauge*>(lv_timer_get_user_data(timer));
        gauge->set_speed(get_data()->sog);
    }

    static void anim_cb(void *var, int32_t value) {
        static_cast<SpeedGauge*>(var)->set_speed(value);
    }

public:
    SpeedGauge(lv_obj_t *parent, int width, int height) {
        gauge_styles::init_styles();

        // Background container
        lv_obj_t *bg = gauge_styles::create_gauge_bg(parent, width, height);

        // Outer arc (background track)
        arc_bg = lv_arc_create(bg);
        lv_obj_set_size(arc_bg, width - 10, height - 10);
        lv_obj_center(arc_bg);
        lv_arc_set_rotation(arc_bg, 135);
        lv_arc_set_bg_angles(arc_bg, 0, 270);
        lv_arc_set_value(arc_bg, 0);
        lv_obj_remove_style(arc_bg, nullptr, LV_PART_KNOB);
        lv_obj_remove_flag(arc_bg, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_arc_width(arc_bg, 8, LV_PART_MAIN);
        lv_obj_set_style_arc_color(arc_bg, lv_palette_darken(LV_PALETTE_GREY, 3), LV_PART_MAIN);
        lv_obj_set_style_arc_width(arc_bg, 8, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(arc_bg, lv_palette_main(LV_PALETTE_BLUE), LV_PART_INDICATOR);
        lv_obj_set_style_arc_rounded(arc_bg, false, LV_PART_MAIN);
        lv_obj_set_style_arc_rounded(arc_bg, false, LV_PART_INDICATOR);

        // Title label "SOG"
        lv_obj_t *title = lv_label_create(bg);
        lv_obj_align(title, LV_ALIGN_CENTER, 0, -50);
        lv_label_set_text(title, "SOG");
        lv_obj_set_style_text_color(title, lv_palette_lighten(LV_PALETTE_GREY, 1), 0);
        lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);

        // Main speed label (big digits)
        speed_label = lv_label_create(bg);
        lv_obj_align(speed_label, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_text_color(speed_label, lv_color_white(), 0);
        lv_obj_set_style_text_font(speed_label, &lv_font_montserrat_48, 0);
        lv_label_set_text(speed_label, "0.0");

        // Unit label "kts"
        lv_obj_t *unit = lv_label_create(bg);
        lv_obj_align(unit, LV_ALIGN_CENTER, 0, 45);
        lv_label_set_text(unit, "kts");
        lv_obj_set_style_text_color(unit, lv_palette_lighten(LV_PALETTE_GREY, 1), 0);
        lv_obj_set_style_text_font(unit, &lv_font_montserrat_20, 0);

        lv_timer_create(tick_cb, 100, this);
    }

    void set_speed(int32_t speed) {
        // Update arc indicator (0-100 range)
        int32_t arc_val = (speed * 100) / max_speed;
        if (arc_val > 100) arc_val = 100;
        lv_arc_set_value(arc_bg, arc_val);

        // Update big digit display
        char buf[8];
        lv_snprintf(buf, sizeof(buf), "%d.%d", speed / 10, speed % 10);
        lv_label_set_text(speed_label, buf);
    }

    void showcase() {
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, this);
        lv_anim_set_exec_cb(&a, anim_cb);
        lv_anim_set_values(&a, 0, 200);
        lv_anim_set_time(&a, 2000);
        lv_anim_set_repeat_delay(&a, 100);
        lv_anim_set_playback_time(&a, 500);
        lv_anim_set_playback_delay(&a, 300);
        lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
        lv_anim_start(&a);
    }
};

#endif
