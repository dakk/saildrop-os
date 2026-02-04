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
#ifndef VALUEGAUGE_H
#define VALUEGAUGE_H

#include <lvgl.h>
#include <string.h>
#include "../scale.h"
#include "styles.h"

class ValueGauge {
private:
    lv_obj_t *value_label;
    lv_obj_t *arc_indicator;
    char unit_str[8];
    char label_str[16];
    int32_t min_val;
    int32_t max_val;
    lv_color_t accent_color;

    static void anim_cb(void *var, int32_t value) {
        static_cast<ValueGauge*>(var)->set_value(value);
    }

public:
    ValueGauge(lv_obj_t *parent, int width, int height, const char *label, const char *unit,
               int32_t min = 0, int32_t max = 100, lv_palette_t arc_palette = LV_PALETTE_BLUE) {
        gauge_styles::init_styles();

        strcpy(label_str, label);
        strcpy(unit_str, unit);
        min_val = min;
        max_val = max;
        accent_color = lv_palette_main(arc_palette);

        // Background container
        lv_obj_t *bg = gauge_styles::create_gauge_bg(parent, width, height);

        // Arc indicator (background track)
        arc_indicator = lv_arc_create(bg);
        lv_obj_set_size(arc_indicator, width - SCALE_PX(10), height - SCALE_PX(10));
        lv_obj_center(arc_indicator);
        lv_arc_set_rotation(arc_indicator, 135);
        lv_arc_set_bg_angles(arc_indicator, 0, 270);
        lv_arc_set_value(arc_indicator, 0);
        lv_obj_remove_style(arc_indicator, nullptr, LV_PART_KNOB);
        lv_obj_remove_flag(arc_indicator, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_arc_width(arc_indicator, SCALE_PX(8), LV_PART_MAIN);
        lv_obj_set_style_arc_color(arc_indicator, lv_palette_darken(LV_PALETTE_GREY, 3), LV_PART_MAIN);
        lv_obj_set_style_arc_width(arc_indicator, SCALE_PX(8), LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(arc_indicator, accent_color, LV_PART_INDICATOR);
        lv_obj_set_style_arc_rounded(arc_indicator, false, LV_PART_MAIN);
        lv_obj_set_style_arc_rounded(arc_indicator, false, LV_PART_INDICATOR);

        // Title label
        lv_obj_t *title = lv_label_create(bg);
        lv_obj_align(title, LV_ALIGN_CENTER, 0, -SCALE_PX(25));
        lv_label_set_text(title, label_str);
        lv_obj_set_style_text_color(title, lv_palette_lighten(LV_PALETTE_GREY, 1), 0);
        lv_obj_set_style_text_font(title, ui_scale::font_small(), 0);

        // Main value label (big digits)
        value_label = lv_label_create(bg);
        lv_obj_align(value_label, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_text_color(value_label, lv_color_white(), 0);
        lv_obj_set_style_text_font(value_label, ui_scale::font_large(), 0);
        lv_label_set_text(value_label, "---");

        // Unit label
        lv_obj_t *unit_lbl = lv_label_create(bg);
        lv_obj_align(unit_lbl, LV_ALIGN_CENTER, 0, SCALE_PX(25));
        lv_label_set_text(unit_lbl, unit_str);
        lv_obj_set_style_text_color(unit_lbl, lv_palette_lighten(LV_PALETTE_GREY, 1), 0);
        lv_obj_set_style_text_font(unit_lbl, ui_scale::font_medium(), 0);
    }

    void set_value(int32_t val) {
        // Update arc indicator (0-100 range)
        int32_t range = max_val - min_val;
        int32_t arc_val = 0;
        if (range > 0) {
            arc_val = ((val - min_val * 10) * 100) / (range * 10);
            if (arc_val > 100) arc_val = 100;
            if (arc_val < 0) arc_val = 0;
        }
        lv_arc_set_value(arc_indicator, arc_val);

        // Update big digit display
        char buf[16];
        lv_snprintf(buf, sizeof(buf), "%d.%d", val / 10, val % 10);
        lv_label_set_text(value_label, buf);
    }

    void showcase() {
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, this);
        lv_anim_set_exec_cb(&a, anim_cb);
        lv_anim_set_values(&a, min_val * 10, max_val * 10);
        lv_anim_set_time(&a, 2000);
        lv_anim_set_repeat_delay(&a, 100);
        lv_anim_set_playback_time(&a, 500);
        lv_anim_set_playback_delay(&a, 300);
        lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
        lv_anim_start(&a);
    }
};

#endif
