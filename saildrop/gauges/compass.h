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
#ifndef COMPASS_H
#define COMPASS_H

#include <lvgl.h>
#include "../utils.h"
#include "../data.h"
#include "styles.h"

class Compass {
private:
    lv_obj_t *scale;
    lv_obj_t *heading_label;
    lv_obj_t *cardinal_label;

    static const char* labels[];

    static void tick_cb(lv_timer_t *timer) {
        Compass *compass = static_cast<Compass*>(lv_timer_get_user_data(timer));
        compass->set_heading(get_data()->hdg);
    }

    static void anim_cb(void *var, int32_t value) {
        static_cast<Compass*>(var)->set_heading(value);
    }

    static void draw_event_cb(lv_event_t *e) {
        lv_draw_task_t *draw_task = lv_event_get_draw_task(e);
        lv_draw_dsc_base_t *base_dsc = static_cast<lv_draw_dsc_base_t*>(lv_draw_task_get_draw_dsc(draw_task));
        lv_draw_label_dsc_t *label_dsc = lv_draw_task_get_label_dsc(draw_task);
        lv_draw_line_dsc_t *line_dsc = lv_draw_task_get_line_dsc(draw_task);

        if (base_dsc->part == LV_PART_INDICATOR) {
            // Highlight cardinal directions
            if (label_dsc) {
                // N, E, S, W are at positions 0, 3, 6, 9 in our 12-label array
                if (base_dsc->id1 == 0) {
                    label_dsc->color = lv_palette_main(LV_PALETTE_RED);
                } else if (base_dsc->id1 == 15 || base_dsc->id1 == 30 || base_dsc->id1 == 45) {
                    label_dsc->color = lv_color_white();
                }
            }
            // Highlight North tick
            if (line_dsc && base_dsc->id1 == 60) {
                line_dsc->color = lv_palette_main(LV_PALETTE_RED);
                line_dsc->width = 4;
            }
        }
    }

public:
    Compass(lv_obj_t *parent, int width, int height) {
        gauge_styles::init_styles();

        // Background container
        lv_obj_t *bg = gauge_styles::create_gauge_bg(parent, width, height);

        // Compass scale
        scale = lv_scale_create(bg);
        lv_obj_set_size(scale, width - 5, height - 5);
        lv_scale_set_mode(scale, LV_SCALE_MODE_ROUND_INNER);
        lv_obj_center(scale);

        // Scale configuration
        lv_scale_set_total_tick_count(scale, 61);
        lv_scale_set_major_tick_every(scale, 5);
        lv_scale_set_range(scale, 0, 360);
        lv_scale_set_angle_range(scale, 360);
        lv_scale_set_rotation(scale, 270);
        lv_scale_set_text_src(scale, labels);

        // Tick styling
        lv_obj_set_style_length(scale, 4, LV_PART_ITEMS);
        lv_obj_set_style_length(scale, 12, LV_PART_INDICATOR);
        lv_obj_set_style_line_color(scale, lv_palette_darken(LV_PALETTE_GREY, 2), LV_PART_ITEMS);
        lv_obj_set_style_line_width(scale, 2, LV_PART_ITEMS);
        lv_obj_set_style_line_color(scale, lv_palette_lighten(LV_PALETTE_GREY, 1), LV_PART_INDICATOR);
        lv_obj_set_style_line_width(scale, 3, LV_PART_INDICATOR);
        lv_obj_set_style_text_color(scale, lv_palette_darken(LV_PALETTE_GREY, 1), LV_PART_INDICATOR);
        lv_obj_set_style_text_font(scale, &lv_font_montserrat_14, LV_PART_INDICATOR);

        // Draw event for highlighting
        lv_obj_add_event_cb(scale, draw_event_cb, LV_EVENT_DRAW_TASK_ADDED, nullptr);
        lv_obj_add_flag(scale, LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS);

        // North indicator (red triangle at top)
        lv_obj_t *north_marker = lv_label_create(bg);
        lv_obj_align(north_marker, LV_ALIGN_TOP_MID, 0, 8);
        lv_label_set_text(north_marker, LV_SYMBOL_DOWN);
        lv_obj_set_style_text_color(north_marker, lv_palette_main(LV_PALETTE_RED), 0);
        lv_obj_set_style_text_font(north_marker, &lv_font_montserrat_20, 0);

        // Heading display (large number)
        heading_label = lv_label_create(bg);
        lv_obj_align(heading_label, LV_ALIGN_CENTER, 0, 10);
        lv_obj_set_style_text_color(heading_label, lv_color_white(), 0);
        lv_obj_set_style_text_font(heading_label, &lv_font_montserrat_48, 0);
        lv_label_set_text(heading_label, "0");

        // Cardinal direction label
        cardinal_label = lv_label_create(bg);
        lv_obj_align(cardinal_label, LV_ALIGN_CENTER, 0, 55);
        lv_obj_set_style_text_color(cardinal_label, lv_palette_lighten(LV_PALETTE_GREY, 1), 0);
        lv_obj_set_style_text_font(cardinal_label, &lv_font_montserrat_20, 0);
        lv_label_set_text(cardinal_label, "N");

        // Degree symbol
        lv_obj_t *degree = lv_label_create(bg);
        lv_obj_align(degree, LV_ALIGN_CENTER, 45, -5);
        lv_obj_set_style_text_color(degree, lv_palette_lighten(LV_PALETTE_GREY, 1), 0);
        lv_obj_set_style_text_font(degree, &lv_font_montserrat_20, 0);
        lv_label_set_text(degree, "\xC2\xB0");  // UTF-8 degree symbol

        set_heading(0);
        lv_timer_create(tick_cb, 100, this);
    }

    void set_heading(int heading) {
        lv_scale_set_rotation(scale, 270 - heading);

        char buf[8];
        lv_snprintf(buf, sizeof(buf), "%d", heading);
        lv_label_set_text(heading_label, buf);
        lv_label_set_text(cardinal_label, heading_to_cardinal(heading));
    }

    void showcase() {
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, this);
        lv_anim_set_exec_cb(&a, anim_cb);
        lv_anim_set_values(&a, 0, 360);
        lv_anim_set_time(&a, 5000);
        lv_anim_set_repeat_delay(&a, 500);
        lv_anim_set_reverse_duration(&a, 5000);
        lv_anim_set_reverse_delay(&a, 500);
        lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
        lv_anim_start(&a);
    }
};

// Compass labels - cardinals and degrees
const char* Compass::labels[] = {
    "N", "", "60", "E", "", "150", "S", "", "240", "W", "", "330", nullptr
};

#endif
