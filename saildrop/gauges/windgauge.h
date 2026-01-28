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
#ifndef WINDGAUGE_H
#define WINDGAUGE_H

#include <lvgl.h>
#include <cmath>
#include "../data.h"
#include "styles.h"

class WindGauge {
private:
    lv_obj_t *bg;
    lv_obj_t *scale;
    lv_obj_t *needle;
    lv_obj_t *speed_label;
    lv_obj_t *angle_label;
    lv_obj_t *type_label;
    lv_point_precise_t needle_points[2];
    int32_t current_angle = 0;

    static const char* labels[];

    static void tick_cb(lv_timer_t *timer) {
        WindGauge *gauge = static_cast<WindGauge*>(lv_timer_get_user_data(timer));
        gauge->set_direction(get_data()->awa);
        gauge->set_speed(get_data()->aws);
    }

    static void anim_direction_cb(void *var, int32_t value) {
        static_cast<WindGauge*>(var)->set_direction(value);
    }

    static void anim_speed_cb(void *var, int32_t value) {
        static_cast<WindGauge*>(var)->set_speed(value);
    }

    void update_needle(int32_t angle) {
        // Calculate needle position
        // Angle: 0 = top (bow), positive = starboard, negative = port
        // Convert to radians, offset by -90° so 0 is at top
        float rad = (angle - 90) * 3.14159f / 180.0f;
        int32_t cx = 120;  // center x
        int32_t cy = 120;  // center y
        int32_t inner_r = 15;
        int32_t outer_r = 85;

        needle_points[0].x = cx + inner_r * cosf(rad);
        needle_points[0].y = cy + inner_r * sinf(rad);
        needle_points[1].x = cx + outer_r * cosf(rad);
        needle_points[1].y = cy + outer_r * sinf(rad);

        lv_line_set_points(needle, needle_points, 2);
    }

public:
    WindGauge(lv_obj_t *parent, int width, int height) {
        gauge_styles::init_styles();

        // Background container
        bg = gauge_styles::create_gauge_bg(parent, width, height);

        // Port (green) close-hauled arc - top left (20° to 60° from bow)
        // LVGL: 0°=right, 270°=top. Port is counter-clockwise from top: 270°-20°=250° to 270°-60°=210°
        lv_obj_t *port_arc = lv_arc_create(bg);
        lv_obj_set_size(port_arc, width - 20, height - 20);
        lv_obj_center(port_arc);
        lv_arc_set_rotation(port_arc, 0);
        lv_arc_set_bg_angles(port_arc, 210, 250);
        lv_arc_set_value(port_arc, 0);
        lv_obj_remove_style(port_arc, nullptr, LV_PART_KNOB);
        lv_obj_remove_style(port_arc, nullptr, LV_PART_INDICATOR);
        lv_obj_remove_flag(port_arc, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_arc_width(port_arc, 12, LV_PART_MAIN);
        lv_obj_set_style_arc_color(port_arc, lv_palette_main(LV_PALETTE_GREEN), LV_PART_MAIN);
        lv_obj_set_style_arc_rounded(port_arc, false, LV_PART_MAIN);

        // Starboard (red) close-hauled arc - top right (20° to 60° from bow)
        // LVGL: 0°=right, 270°=top. Starboard is clockwise from top: 270°+20°=290° to 270°+60°=330°
        lv_obj_t *stbd_arc = lv_arc_create(bg);
        lv_obj_set_size(stbd_arc, width - 20, height - 20);
        lv_obj_center(stbd_arc);
        lv_arc_set_rotation(stbd_arc, 0);
        lv_arc_set_bg_angles(stbd_arc, 290, 330);
        lv_arc_set_value(stbd_arc, 0);
        lv_obj_remove_style(stbd_arc, nullptr, LV_PART_KNOB);
        lv_obj_remove_style(stbd_arc, nullptr, LV_PART_INDICATOR);
        lv_obj_remove_flag(stbd_arc, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_arc_width(stbd_arc, 12, LV_PART_MAIN);
        lv_obj_set_style_arc_color(stbd_arc, lv_palette_main(LV_PALETTE_RED), LV_PART_MAIN);
        lv_obj_set_style_arc_rounded(stbd_arc, false, LV_PART_MAIN);

        // Scale
        scale = lv_scale_create(bg);
        lv_obj_set_size(scale, width - 5, height - 5);
        lv_scale_set_mode(scale, LV_SCALE_MODE_ROUND_INNER);
        lv_obj_center(scale);

        // Scale configuration: 0° at top, goes both ways to 180°
        lv_scale_set_total_tick_count(scale, 37);  // Every 10°
        lv_scale_set_major_tick_every(scale, 3);   // Every 30°
        lv_scale_set_range(scale, 0, 360);
        lv_scale_set_angle_range(scale, 360);
        lv_scale_set_rotation(scale, 270);  // 0° at top
        lv_scale_set_text_src(scale, labels);

        // Tick styling
        lv_obj_set_style_length(scale, 5, LV_PART_ITEMS);
        lv_obj_set_style_length(scale, 12, LV_PART_INDICATOR);
        lv_obj_set_style_line_color(scale, lv_palette_darken(LV_PALETTE_GREY, 2), LV_PART_ITEMS);
        lv_obj_set_style_line_width(scale, 2, LV_PART_ITEMS);
        lv_obj_set_style_line_color(scale, lv_palette_lighten(LV_PALETTE_GREY, 1), LV_PART_INDICATOR);
        lv_obj_set_style_line_width(scale, 3, LV_PART_INDICATOR);
        lv_obj_set_style_text_color(scale, lv_color_white(), LV_PART_INDICATOR);
        lv_obj_set_style_text_font(scale, &lv_font_montserrat_14, LV_PART_INDICATOR);

        // Boat bow marker at top
        lv_obj_t *bow_marker = lv_label_create(bg);
        lv_obj_align(bow_marker, LV_ALIGN_TOP_MID, 0, 15);
        lv_label_set_text(bow_marker, LV_SYMBOL_UP);
        lv_obj_set_style_text_color(bow_marker, lv_color_white(), 0);
        lv_obj_set_style_text_font(bow_marker, &lv_font_montserrat_14, 0);

        // Wind type label (AWA/TWA)
        type_label = lv_label_create(bg);
        lv_obj_align(type_label, LV_ALIGN_CENTER, 0, -35);
        lv_label_set_text(type_label, "AWA");
        lv_obj_set_style_text_color(type_label, lv_palette_lighten(LV_PALETTE_GREY, 1), 0);
        lv_obj_set_style_text_font(type_label, &lv_font_montserrat_14, 0);

        // Wind speed display (large)
        speed_label = lv_label_create(bg);
        lv_obj_align(speed_label, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_text_color(speed_label, lv_color_white(), 0);
        lv_obj_set_style_text_font(speed_label, &lv_font_montserrat_48, 0);
        lv_label_set_text(speed_label, "0");

        // Wind angle display
        angle_label = lv_label_create(bg);
        lv_obj_align(angle_label, LV_ALIGN_CENTER, 0, 45);
        lv_obj_set_style_text_color(angle_label, lv_palette_lighten(LV_PALETTE_GREY, 1), 0);
        lv_obj_set_style_text_font(angle_label, &lv_font_montserrat_20, 0);
        lv_label_set_text(angle_label, "0\xC2\xB0");

        // Needle
        needle = lv_line_create(bg);
        lv_obj_set_style_line_width(needle, 4, LV_PART_MAIN);
        lv_obj_set_style_line_color(needle, lv_palette_main(LV_PALETTE_ORANGE), LV_PART_MAIN);
        lv_obj_set_style_line_rounded(needle, true, LV_PART_MAIN);
        update_needle(0);

        // Needle center dot
        lv_obj_t *center_dot = lv_obj_create(bg);
        lv_obj_set_size(center_dot, 16, 16);
        lv_obj_center(center_dot);
        lv_obj_set_style_radius(center_dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(center_dot, lv_palette_main(LV_PALETTE_ORANGE), 0);
        lv_obj_set_style_border_width(center_dot, 0, 0);

        lv_timer_create(tick_cb, 100, this);
    }

    void set_direction(int32_t angle) {
        // angle: -180 to +180, where 0 = bow, positive = starboard, negative = port
        current_angle = angle;
        update_needle(angle);

        // Update angle label with port/starboard indicator
        char buf[16];
        if (angle < 0) {
            lv_snprintf(buf, sizeof(buf), "%d\xC2\xB0 P", -angle);
        } else if (angle > 0) {
            lv_snprintf(buf, sizeof(buf), "%d\xC2\xB0 S", angle);
        } else {
            lv_snprintf(buf, sizeof(buf), "0\xC2\xB0");
        }
        lv_label_set_text(angle_label, buf);
    }

    void set_speed(int32_t speed) {
        char buf[8];
        lv_snprintf(buf, sizeof(buf), "%d", speed / 10);
        lv_label_set_text(speed_label, buf);
    }

    void set_type(bool apparent) {
        lv_label_set_text(type_label, apparent ? "AWA" : "TWA");
    }

    void showcase() {
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, this);
        lv_anim_set_exec_cb(&a, anim_direction_cb);
        lv_anim_set_values(&a, -180, 180);
        lv_anim_set_time(&a, 4000);
        lv_anim_set_repeat_delay(&a, 100);
        lv_anim_set_playback_time(&a, 4000);
        lv_anim_set_playback_delay(&a, 100);
        lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
        lv_anim_start(&a);

        lv_anim_t b;
        lv_anim_init(&b);
        lv_anim_set_var(&b, this);
        lv_anim_set_exec_cb(&b, anim_speed_cb);
        lv_anim_set_values(&b, 0, 350);
        lv_anim_set_time(&b, 3000);
        lv_anim_set_repeat_delay(&b, 100);
        lv_anim_set_playback_time(&b, 2000);
        lv_anim_set_playback_delay(&b, 100);
        lv_anim_set_repeat_count(&b, LV_ANIM_REPEAT_INFINITE);
        lv_anim_start(&b);
    }
};

// Scale labels: bow at top (0), then 30, 60, 90, 120, 150, stern (180), and mirrored for port
const char* WindGauge::labels[] = {
    "", "30", "60", "90", "120", "150", "180", "150", "120", "90", "60", "30", nullptr
};

#endif
