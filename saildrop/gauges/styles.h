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
#ifndef STYLES_H
#define STYLES_H

#include <lvgl.h>
#include "../conf.h"
#include "../scale.h"

namespace gauge_styles {

// Color accessor functions (must be called AFTER lv_init())
inline lv_color_t color_bg() { return lv_palette_darken(LV_PALETTE_GREY, 4); }
inline lv_color_t color_tick() { return lv_palette_darken(LV_PALETTE_GREY, 1); }
inline lv_color_t color_accent() { return lv_palette_main(LV_PALETTE_RED); }
inline lv_color_t color_text() { return lv_color_white(); }

// Style instances
static lv_style_t style_bg;
static lv_style_t style_tick_minor;
static lv_style_t style_tick_major;
static lv_style_t style_needle;
static lv_style_t style_label_value;
static lv_style_t style_label_small;

static bool styles_initialized = false;

inline void init_styles() {
    if (styles_initialized) return;
    styles_initialized = true;

    // Background style (circular gauge container)
    lv_style_init(&style_bg);
    lv_style_set_radius(&style_bg, LV_RADIUS_CIRCLE);
    lv_style_set_bg_color(&style_bg, color_bg());
    lv_style_set_bg_opa(&style_bg, LV_OPA_COVER);
    lv_style_set_pad_all(&style_bg, 0);
    lv_style_set_border_width(&style_bg, 0);

    // Minor tick style
    lv_style_init(&style_tick_minor);
    lv_style_set_line_color(&style_tick_minor, color_tick());
    lv_style_set_line_width(&style_tick_minor, SCALE_PX(2));
    lv_style_set_length(&style_tick_minor, SCALE_PX(5));

    // Major tick style
    lv_style_init(&style_tick_major);
    lv_style_set_line_color(&style_tick_major, color_tick());
    lv_style_set_line_width(&style_tick_major, SCALE_PX(3));
    lv_style_set_length(&style_tick_major, SCALE_PX(10));

    // Needle style
    lv_style_init(&style_needle);
    lv_style_set_line_width(&style_needle, SCALE_PX(6));
    lv_style_set_line_rounded(&style_needle, true);
    lv_style_set_line_color(&style_needle, color_tick());

    // Value label style (large text)
    lv_style_init(&style_label_value);
    lv_style_set_text_color(&style_label_value, color_text());
    lv_style_set_text_font(&style_label_value, ui_scale::font_medium());
    lv_style_set_text_align(&style_label_value, LV_TEXT_ALIGN_CENTER);

    // Small label style
    lv_style_init(&style_label_small);
    lv_style_set_text_color(&style_label_small, color_text());
    lv_style_set_text_font(&style_label_small, ui_scale::font_small());
    lv_style_set_text_align(&style_label_small, LV_TEXT_ALIGN_CENTER);
}

inline lv_obj_t* create_gauge_bg(lv_obj_t *parent, int width, int height) {
    init_styles();

    lv_obj_t *bg = lv_obj_create(parent);
    lv_obj_set_size(bg, width, height);
    lv_obj_center(bg);
    lv_obj_add_style(bg, &style_bg, 0);
    lv_obj_remove_flag(bg, LV_OBJ_FLAG_SCROLLABLE);
    return bg;
}

} // namespace gauge_styles

#endif
