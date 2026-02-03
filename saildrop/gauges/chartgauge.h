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
#ifndef CHARTGAUGE_H
#define CHARTGAUGE_H

#include "conf.h"

#ifdef BOARD_LCD_4

#include <lvgl.h>
#include <esp_heap_caps.h>
#include "../data.h"
#include "../scale.h"
#include "../tilemgr.h"
#include "../drivers/sdcard.h"
#include "styles.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

#define DEG_TO_RAD_F (M_PI / 180.0f)

class ChartGauge {
private:
    lv_obj_t *bg;
    lv_obj_t *canvas;
    lv_obj_t *boat_icon;
    lv_obj_t *no_gps_label;
    lv_obj_t *no_sd_label;
    lv_obj_t *zoom_label;
    lv_obj_t *loading_spinner;

    // Boat icon lines (triangle)
    lv_obj_t *boat_lines[3];
    lv_point_precise_t boat_pts[3][2];

    // Heading line
    lv_obj_t *heading_line;
    lv_point_precise_t heading_pts[2];

    TileManager *tileMgr;
    uint8_t *canvas_buf;

    uint8_t zoom_level;
    int32_t last_lat;
    int32_t last_lon;
    uint32_t last_tile_x;
    uint32_t last_tile_y;
    bool tiles_dirty;
    bool sd_available;

    int width;
    int height;

    static void tick_cb(lv_timer_t *timer) {
        ChartGauge *gauge = static_cast<ChartGauge*>(lv_timer_get_user_data(timer));
        gauge->update();
    }

    // Draw boat icon at screen center pointing in heading direction
    void draw_boat_icon(int32_t heading) {
        int32_t cx = width / 2;
        int32_t cy = height / 2;
        float rad = (heading - 90) * DEG_TO_RAD_F;
        const float size = SCALE_PX(12);

        // Triangle: tip, base-left, base-right
        float tip_x = cx + size * cosf(rad);
        float tip_y = cy + size * sinf(rad);

        float back_rad = rad + M_PI;
        float perp_rad_l = rad + M_PI / 2;
        float perp_rad_r = rad - M_PI / 2;
        float half_base = size * 0.6f;
        float back_dist = size * 0.5f;

        float base_cx = cx + back_dist * cosf(back_rad);
        float base_cy = cy + back_dist * sinf(back_rad);

        float bl_x = base_cx + half_base * cosf(perp_rad_l);
        float bl_y = base_cy + half_base * sinf(perp_rad_l);
        float br_x = base_cx + half_base * cosf(perp_rad_r);
        float br_y = base_cy + half_base * sinf(perp_rad_r);

        // Line 0: tip to base-left
        boat_pts[0][0].x = tip_x; boat_pts[0][0].y = tip_y;
        boat_pts[0][1].x = bl_x;  boat_pts[0][1].y = bl_y;

        // Line 1: base-left to base-right
        boat_pts[1][0].x = bl_x;  boat_pts[1][0].y = bl_y;
        boat_pts[1][1].x = br_x;  boat_pts[1][1].y = br_y;

        // Line 2: base-right to tip
        boat_pts[2][0].x = br_x;  boat_pts[2][0].y = br_y;
        boat_pts[2][1].x = tip_x; boat_pts[2][1].y = tip_y;

        for (int i = 0; i < 3; i++) {
            lv_line_set_points(boat_lines[i], boat_pts[i], 2);
        }

        // Heading line (extends from boat)
        float line_len = SCALE_PX(30);
        heading_pts[0].x = tip_x;
        heading_pts[0].y = tip_y;
        heading_pts[1].x = cx + (size + line_len) * cosf(rad);
        heading_pts[1].y = cy + (size + line_len) * sinf(rad);
        lv_line_set_points(heading_line, heading_pts, 2);
    }

    // Render tiles to canvas centered on position
    void render_tiles() {
        if (!tileMgr || !canvas_buf || !sd_available) return;

        nmea_data *data = get_data();
        if (!data->position_valid) return;

        // Get center tile
        uint32_t center_tile_x, center_tile_y;
        TileManager::lat_lon_to_tile_xy(data->lat, data->lon, zoom_level,
                                        &center_tile_x, &center_tile_y);

        // Get pixel offset within center tile
        int16_t offset_x, offset_y;
        tileMgr->get_pixel_offset(data->lat, data->lon, zoom_level,
                                  center_tile_x, center_tile_y,
                                  &offset_x, &offset_y);

        // Calculate how many tiles we need to cover the screen
        // With 128px tiles on 480px screen, we need at most 5 tiles per axis
        int tiles_x = (width / CHART_TILE_SIZE) + 2;
        int tiles_y = (height / CHART_TILE_SIZE) + 2;

        // Clear canvas to dark blue (water color)
        lv_canvas_fill_bg(canvas, lv_color_make(0x1a, 0x23, 0x2e), LV_OPA_COVER);

        // Calculate starting position for tiles
        int start_x = (width / 2) - offset_x - (tiles_x / 2) * CHART_TILE_SIZE;
        int start_y = (height / 2) - offset_y - (tiles_y / 2) * CHART_TILE_SIZE;

        // Render visible tiles
        for (int ty = 0; ty < tiles_y; ty++) {
            for (int tx = 0; tx < tiles_x; tx++) {
                uint32_t tile_x = center_tile_x - (tiles_x / 2) + tx;
                uint32_t tile_y = center_tile_y - (tiles_y / 2) + ty;

                const uint8_t *tile_data = tileMgr->get_tile(zoom_level, tile_x, tile_y);
                if (tile_data) {
                    int screen_x = start_x + tx * CHART_TILE_SIZE;
                    int screen_y = start_y + ty * CHART_TILE_SIZE;

                    // Copy tile data to canvas
                    for (int py = 0; py < CHART_TILE_SIZE; py++) {
                        int canvas_y = screen_y + py;
                        if (canvas_y < 0 || canvas_y >= height) continue;

                        for (int px = 0; px < CHART_TILE_SIZE; px++) {
                            int canvas_x = screen_x + px;
                            if (canvas_x < 0 || canvas_x >= width) continue;

                            // Read RGB565 pixel from tile (stored big-endian)
                            int tile_offset = (py * CHART_TILE_SIZE + px) * 2;
                            uint16_t pixel = (tile_data[tile_offset] << 8) | tile_data[tile_offset + 1];

                            // Write to canvas buffer (little-endian for ESP32/LVGL)
                            int canvas_offset = (canvas_y * width + canvas_x) * 2;
                            canvas_buf[canvas_offset] = pixel & 0xFF;         // Low byte first
                            canvas_buf[canvas_offset + 1] = pixel >> 8;       // High byte second
                        }
                    }
                }
            }
        }

        // Invalidate canvas to trigger redraw
        lv_obj_invalidate(canvas);
    }

public:
    ChartGauge(lv_obj_t *parent, int w, int h) : width(w), height(h) {
        gauge_styles::init_styles();
        tileMgr = nullptr;
        canvas_buf = nullptr;
        zoom_level = CHART_DEFAULT_ZOOM;
        last_lat = 0;
        last_lon = 0;
        last_tile_x = 0;
        last_tile_y = 0;
        tiles_dirty = true;
        sd_available = false;

        // Background container
        bg = gauge_styles::create_gauge_bg(parent, width, height);

        // SD card is already initialized and verified before ChartScreen creation
        sd_available = sdcard_is_mounted();
        no_sd_label = nullptr;

        // Allocate canvas buffer in PSRAM
        size_t buf_size = width * height * 2;  // RGB565
        canvas_buf = (uint8_t*)heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM);

        if (canvas_buf) {
            // Create canvas
            canvas = lv_canvas_create(bg);
            lv_canvas_set_buffer(canvas, canvas_buf, width, height, LV_COLOR_FORMAT_RGB565);
            lv_obj_center(canvas);

            // Fill with water color initially
            lv_canvas_fill_bg(canvas, lv_color_make(0x1a, 0x23, 0x2e), LV_OPA_COVER);
        }

        // Initialize tile manager
        tileMgr = new TileManager();
        if (!tileMgr->begin()) {
            #ifdef DEBUG
            Serial.println("TileManager init failed");
            #endif
        }

        // "NO GPS" label (shown when position invalid)
        no_gps_label = lv_label_create(bg);
        lv_obj_align(no_gps_label, LV_ALIGN_TOP_MID, 0, SCALE_PX(10));
        lv_label_set_text(no_gps_label, "NO GPS");
        lv_obj_set_style_text_color(no_gps_label, lv_palette_main(LV_PALETTE_RED), 0);
        lv_obj_set_style_text_font(no_gps_label, ui_scale::font_small(), 0);
        lv_obj_add_flag(no_gps_label, LV_OBJ_FLAG_HIDDEN);

        // Zoom level label
        zoom_label = lv_label_create(bg);
        lv_obj_align(zoom_label, LV_ALIGN_BOTTOM_LEFT, SCALE_PX(10), -SCALE_PX(10));
        lv_obj_set_style_text_color(zoom_label, lv_color_white(), 0);
        lv_obj_set_style_text_font(zoom_label, ui_scale::font_small(), 0);
        update_zoom_label();

        // Create boat icon lines (triangle)
        for (int i = 0; i < 3; i++) {
            boat_lines[i] = lv_line_create(bg);
            lv_obj_set_style_line_width(boat_lines[i], 2, 0);
            lv_obj_set_style_line_color(boat_lines[i], lv_palette_main(LV_PALETTE_RED), 0);
        }

        // Heading line
        heading_line = lv_line_create(bg);
        lv_obj_set_style_line_width(heading_line, 1, 0);
        lv_obj_set_style_line_color(heading_line, lv_palette_main(LV_PALETTE_RED), 0);
        lv_obj_set_style_line_dash_width(heading_line, 4, 0);
        lv_obj_set_style_line_dash_gap(heading_line, 4, 0);

        // Start update timer (500ms for tile check, boat position updated more frequently)
        lv_timer_create(tick_cb, 100, this);
    }

    ~ChartGauge() {
        if (tileMgr) delete tileMgr;
        if (canvas_buf) heap_caps_free(canvas_buf);
    }

    void update() {
        nmea_data *data = get_data();

        // Show/hide NO GPS label
        if (!data->position_valid) {
            lv_obj_remove_flag(no_gps_label, LV_OBJ_FLAG_HIDDEN);
            return;
        }
        lv_obj_add_flag(no_gps_label, LV_OBJ_FLAG_HIDDEN);

        if (!sd_available || !tileMgr) return;

        // Check if we crossed tile boundary or position changed significantly
        uint32_t current_tile_x, current_tile_y;
        TileManager::lat_lon_to_tile_xy(data->lat, data->lon, zoom_level,
                                        &current_tile_x, &current_tile_y);

        if (current_tile_x != last_tile_x || current_tile_y != last_tile_y || tiles_dirty) {
            render_tiles();
            last_tile_x = current_tile_x;
            last_tile_y = current_tile_y;
            tiles_dirty = false;
        }

        // Always update boat icon with current heading
        draw_boat_icon(data->hdg);

        last_lat = data->lat;
        last_lon = data->lon;
    }

    void update_zoom_label() {
        char buf[16];
        lv_snprintf(buf, sizeof(buf), "Z%d", zoom_level);
        lv_label_set_text(zoom_label, buf);
    }

    void zoom_in() {
        if (zoom_level < CHART_MAX_ZOOM) {
            zoom_level++;
            tiles_dirty = true;
            update_zoom_label();
            #ifdef DEBUG
            Serial.printf("Zoom in: %d\n", zoom_level);
            #endif
        }
    }

    void zoom_out() {
        if (zoom_level > CHART_MIN_ZOOM) {
            zoom_level--;
            tiles_dirty = true;
            update_zoom_label();
            #ifdef DEBUG
            Serial.printf("Zoom out: %d\n", zoom_level);
            #endif
        }
    }

    void set_zoom(uint8_t level) {
        if (level >= CHART_MIN_ZOOM && level <= CHART_MAX_ZOOM) {
            zoom_level = level;
            tiles_dirty = true;
            update_zoom_label();
        }
    }

    uint8_t get_zoom() { return zoom_level; }

    void showcase() {
        // Demo mode: simulate position in Mediterranean
        // Sardinia: 40.0°N, 9.5°E
        get_data()->lat = 40000000;  // 40.0 degrees in microdegrees
        get_data()->lon = 9500000;   // 9.5 degrees in microdegrees
        get_data()->position_valid = true;
        get_data()->hdg = 45;  // NE heading
        tiles_dirty = true;
    }
};

#endif // BOARD_LCD_4

#endif // CHARTGAUGE_H
