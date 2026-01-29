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
#ifndef AISGAUGE_H
#define AISGAUGE_H

#include <lvgl.h>
#include <cmath>
#include "../data.h"
#include "../ais.h"
#include "../scale.h"
#include "styles.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

#define DEG_TO_RAD_F (M_PI / 180.0f)

class AISGauge {
private:
    lv_obj_t *bg;
    lv_obj_t *range_label;
    lv_obj_t *count_label;
    lv_obj_t *no_gps_label;
    lv_obj_t *own_boat_icon;
    lv_obj_t *north_marker;
    lv_point_precise_t north_pts[2];

    // Target icons (lines for triangles - 3 lines per target, max 8 targets)
    lv_obj_t *target_lines[AIS_MAX_TARGETS * 3];
    lv_point_precise_t target_pts[AIS_MAX_TARGETS * 3][2];

    // Range options in nautical miles
    static constexpr float RANGE_OPTIONS[] = {0.5f, 1.0f, 2.0f, 5.0f, 10.0f};
    static constexpr uint8_t NUM_RANGES = 5;
    uint8_t range_index = 2;  // Default to 2 NM
    float range_nm = 2.0f;

    static void tick_cb(lv_timer_t *timer) {
        AISGauge *gauge = static_cast<AISGauge*>(lv_timer_get_user_data(timer));
        gauge->update();
    }

    // Convert bearing/distance to screen coordinates (head-up mode)
    void target_to_screen(float bearing, float distance_nm, float own_heading,
                          int32_t *screen_x, int32_t *screen_y) {
        // Relative bearing (target bearing minus own heading)
        float rel_bearing = bearing - own_heading;
        float rel_rad = rel_bearing * DEG_TO_RAD_F;

        // Scale: max radius is ~95 pixels (leaving margin for icons)
        float max_radius = SCALE_PX(95);
        float scale = max_radius / range_nm;
        float pixel_dist = distance_nm * scale;

        // Clamp to visible area
        if (pixel_dist > max_radius) pixel_dist = max_radius;

        // Convert polar to cartesian
        // 0 degrees = up, clockwise positive
        // LVGL: y increases downward, x increases right
        int32_t cx = ui_scale::center_x();
        int32_t cy = ui_scale::center_y();
        *screen_x = cx + (int32_t)(pixel_dist * sinf(rel_rad));
        *screen_y = cy - (int32_t)(pixel_dist * cosf(rel_rad));
    }

    // Draw a triangle at position (x, y) pointing in direction cog_deg
    void draw_target_triangle(uint8_t target_idx, int32_t x, int32_t y,
                              float cog_deg, bool visible) {
        if (target_idx >= AIS_MAX_TARGETS) return;

        uint8_t base_idx = target_idx * 3;

        if (!visible) {
            // Hide all lines for this target
            for (int i = 0; i < 3; i++) {
                lv_obj_add_flag(target_lines[base_idx + i], LV_OBJ_FLAG_HIDDEN);
            }
            return;
        }

        // Triangle size
        const float size = SCALE_PX(8);

        // Convert COG to radians (0 = up, clockwise positive)
        float rad = (cog_deg - 90) * DEG_TO_RAD_F;

        // Triangle points: tip, base-left, base-right
        // Tip points in COG direction
        float tip_x = x + size * cosf(rad);
        float tip_y = y + size * sinf(rad);

        // Base is perpendicular to COG, half-size back
        float back_rad = rad + M_PI;
        float perp_rad_l = rad + M_PI / 2;
        float perp_rad_r = rad - M_PI / 2;
        float half_base = size * 0.6f;
        float back_dist = size * 0.5f;

        float base_cx = x + back_dist * cosf(back_rad);
        float base_cy = y + back_dist * sinf(back_rad);

        float bl_x = base_cx + half_base * cosf(perp_rad_l);
        float bl_y = base_cy + half_base * sinf(perp_rad_l);
        float br_x = base_cx + half_base * cosf(perp_rad_r);
        float br_y = base_cy + half_base * sinf(perp_rad_r);

        // Line 0: tip to base-left
        target_pts[base_idx][0].x = tip_x;
        target_pts[base_idx][0].y = tip_y;
        target_pts[base_idx][1].x = bl_x;
        target_pts[base_idx][1].y = bl_y;

        // Line 1: base-left to base-right
        target_pts[base_idx + 1][0].x = bl_x;
        target_pts[base_idx + 1][0].y = bl_y;
        target_pts[base_idx + 1][1].x = br_x;
        target_pts[base_idx + 1][1].y = br_y;

        // Line 2: base-right to tip
        target_pts[base_idx + 2][0].x = br_x;
        target_pts[base_idx + 2][0].y = br_y;
        target_pts[base_idx + 2][1].x = tip_x;
        target_pts[base_idx + 2][1].y = tip_y;

        // Update and show lines
        for (int i = 0; i < 3; i++) {
            lv_line_set_points(target_lines[base_idx + i], target_pts[base_idx + i], 2);
            lv_obj_remove_flag(target_lines[base_idx + i], LV_OBJ_FLAG_HIDDEN);
        }
    }

public:
    AISGauge(lv_obj_t *parent, int width, int height) {
        gauge_styles::init_styles();

        // Background container
        bg = gauge_styles::create_gauge_bg(parent, width, height);

        int32_t cx = ui_scale::center_x();
        int32_t cy = ui_scale::center_y();

        // Range rings (3 concentric circles at 33%, 66%, 100% of max radius)
        float max_radius = SCALE_PX(95);
        for (int r = 1; r <= 3; r++) {
            float radius = max_radius * r / 3.0f;
            lv_obj_t *ring = lv_arc_create(bg);
            lv_obj_set_size(ring, radius * 2, radius * 2);
            lv_obj_center(ring);
            lv_arc_set_rotation(ring, 0);
            lv_arc_set_bg_angles(ring, 0, 360);
            lv_arc_set_value(ring, 0);
            lv_obj_remove_style(ring, nullptr, LV_PART_KNOB);
            lv_obj_remove_style(ring, nullptr, LV_PART_INDICATOR);
            lv_obj_remove_flag(ring, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_arc_width(ring, 1, LV_PART_MAIN);
            lv_obj_set_style_arc_color(ring, lv_palette_darken(LV_PALETTE_GREY, 2), LV_PART_MAIN);
        }

        // Own boat icon at center (small triangle pointing up)
        own_boat_icon = lv_label_create(bg);
        lv_obj_align(own_boat_icon, LV_ALIGN_CENTER, 0, 0);
        lv_label_set_text(own_boat_icon, LV_SYMBOL_UP);
        lv_obj_set_style_text_color(own_boat_icon, lv_color_white(), 0);
        lv_obj_set_style_text_font(own_boat_icon, ui_scale::font_medium(), 0);

        // North marker (red line pointing to north, position updated in update())
        north_marker = lv_line_create(bg);
        north_pts[0].x = cx;
        north_pts[0].y = cy - max_radius + SCALE_PX(5);
        north_pts[1].x = cx;
        north_pts[1].y = cy - max_radius - SCALE_PX(5);
        lv_line_set_points(north_marker, north_pts, 2);
        lv_obj_set_style_line_width(north_marker, SCALE_PX(3), LV_PART_MAIN);
        lv_obj_set_style_line_color(north_marker, lv_palette_main(LV_PALETTE_RED), LV_PART_MAIN);
        lv_obj_set_style_line_rounded(north_marker, true, LV_PART_MAIN);

        // Create target lines (3 lines per target to form triangle)
        for (int i = 0; i < AIS_MAX_TARGETS * 3; i++) {
            target_lines[i] = lv_line_create(bg);
            lv_obj_set_style_line_width(target_lines[i], SCALE_PX(2), LV_PART_MAIN);
            lv_obj_set_style_line_color(target_lines[i], lv_palette_main(LV_PALETTE_ORANGE), LV_PART_MAIN);
            lv_obj_add_flag(target_lines[i], LV_OBJ_FLAG_HIDDEN);
        }

        // Range label (bottom center)
        range_label = lv_label_create(bg);
        lv_obj_align(range_label, LV_ALIGN_BOTTOM_MID, 0, -SCALE_PX(35));
        lv_obj_set_style_text_color(range_label, lv_palette_lighten(LV_PALETTE_GREY, 1), 0);
        lv_obj_set_style_text_font(range_label, ui_scale::font_small(), 0);
        update_range_label();

        // Target count label (top center)
        count_label = lv_label_create(bg);
        lv_obj_align(count_label, LV_ALIGN_TOP_MID, 0, SCALE_PX(35));
        lv_obj_set_style_text_color(count_label, lv_palette_lighten(LV_PALETTE_GREY, 1), 0);
        lv_obj_set_style_text_font(count_label, ui_scale::font_small(), 0);
        lv_label_set_text(count_label, "0");

        // No GPS warning (center, hidden by default)
        no_gps_label = lv_label_create(bg);
        lv_obj_align(no_gps_label, LV_ALIGN_BOTTOM_MID, 0, -SCALE_PX(20));
        lv_label_set_text(no_gps_label, "NO GPS");
        lv_obj_set_style_text_color(no_gps_label, lv_palette_main(LV_PALETTE_RED), 0);
        lv_obj_set_style_text_font(no_gps_label, ui_scale::font_small(), 0);
        lv_obj_add_flag(no_gps_label, LV_OBJ_FLAG_HIDDEN);

        lv_timer_create(tick_cb, 200, this);  // Update every 200ms
    }

    void update_range_label() {
        char buf[16];
        if (range_nm < 1.0f) {
            lv_snprintf(buf, sizeof(buf), "%.1f NM", range_nm);
        } else {
            lv_snprintf(buf, sizeof(buf), "%d NM", (int)range_nm);
        }
        lv_label_set_text(range_label, buf);
    }

    void update() {
        nmea_data *data = get_data();
        AISManager *ais = get_ais_manager();

        // Check for valid own position
        if (!data->position_valid) {
            lv_obj_remove_flag(no_gps_label, LV_OBJ_FLAG_HIDDEN);
            // Hide all targets
            for (int i = 0; i < AIS_MAX_TARGETS; i++) {
                draw_target_triangle(i, 0, 0, 0, false);
            }
            lv_label_set_text(count_label, "0");
            return;
        }

        lv_obj_add_flag(no_gps_label, LV_OBJ_FLAG_HIDDEN);

        float own_heading = data->hdg;  // Already in degrees
        uint8_t active_count = 0;

        // Update north marker position (in head-up mode, north is at -heading from top)
        float north_angle = -own_heading;
        float north_rad = north_angle * DEG_TO_RAD_F;
        int32_t cx = ui_scale::center_x();
        int32_t cy = ui_scale::center_y();
        float max_radius = SCALE_PX(95);
        float inner_r = max_radius - SCALE_PX(10);
        float outer_r = max_radius + SCALE_PX(5);
        north_pts[0].x = cx + inner_r * sinf(north_rad);
        north_pts[0].y = cy - inner_r * cosf(north_rad);
        north_pts[1].x = cx + outer_r * sinf(north_rad);
        north_pts[1].y = cy - outer_r * cosf(north_rad);
        lv_line_set_points(north_marker, north_pts, 2);

        // Draw each AIS target
        for (uint8_t i = 0; i < AIS_MAX_TARGETS; i++) {
            const ais_target *tgt = ais->get_target(i);

            if (!tgt->valid) {
                draw_target_triangle(i, 0, 0, 0, false);
                continue;
            }

            // Calculate bearing and distance from own boat to target
            float bearing = calculate_bearing(data->lat, data->lon, tgt->lat, tgt->lon);
            float distance = calculate_distance_nm(data->lat, data->lon, tgt->lat, tgt->lon);

            // Skip targets beyond range
            if (distance > range_nm * 1.2f) {
                draw_target_triangle(i, 0, 0, 0, false);
                continue;
            }

            // Convert to screen coordinates
            int32_t sx, sy;
            target_to_screen(bearing, distance, own_heading, &sx, &sy);

            // Draw target icon pointing in its COG direction
            float target_cog = tgt->cog / 10.0f;  // Convert from 0.1 deg units
            draw_target_triangle(i, sx, sy, target_cog, true);

            active_count++;
        }

        // Update target count
        char buf[8];
        lv_snprintf(buf, sizeof(buf), "%d", active_count);
        lv_label_set_text(count_label, buf);
    }

    void cycle_range() {
        range_index = (range_index + 1) % NUM_RANGES;
        range_nm = RANGE_OPTIONS[range_index];
        update_range_label();
        update();  // Refresh display immediately
    }

    void showcase() {
        // Demo mode - create fake targets
        // This would require modifying the AIS manager, so just update display
    }
};

// Static member initialization
constexpr float AISGauge::RANGE_OPTIONS[];

#endif
