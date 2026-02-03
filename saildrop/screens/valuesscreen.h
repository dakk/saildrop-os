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
#ifndef VALUESSCREEN_H
#define VALUESSCREEN_H

#include "screen.h"
#include "../data.h"
#include "../gauges/valuegauge.h"

#define VALUES_N            5
#define GAUGE_IDX_SOG       0
#define GAUGE_IDX_DEPTH     1
#define GAUGE_IDX_HDG       2
#define GAUGE_IDX_AWS       3
#define GAUGE_IDX_TWS       4
#define GAUGES_PER_PAGE     4



class ValuesScreen : public Screen
{
protected:
    lv_obj_t *screens[VALUES_N];  // For carousel mode or grid pages
    uint8_t current_screen;
    uint8_t num_pages;
    bool grid_mode;

public:
    ValuesScreen();
    ValueGauge *gauges[VALUES_N];

    virtual void on_swipe_up() override;
    virtual void on_swipe_down() override;
};

void values_tick_cb(lv_timer_t *timer)
{
    ValuesScreen *gauge = (ValuesScreen *) lv_timer_get_user_data(timer);
    gauge->gauges[GAUGE_IDX_DEPTH]->set_value(get_data()->depth);
    gauge->gauges[GAUGE_IDX_SOG]->set_value(get_data()->sog);
    gauge->gauges[GAUGE_IDX_HDG]->set_value(get_data()->hdg);
    gauge->gauges[GAUGE_IDX_AWS]->set_value(get_data()->aws);
    gauge->gauges[GAUGE_IDX_TWS]->set_value(get_data()->tws);
}


ValuesScreen::ValuesScreen() : Screen()
{
    current_screen = 0;
    grid_mode = (SCREEN_WIDTH > 240 || SCREEN_HEIGHT > 240);

    if (grid_mode) {
        // Grid mode: 2x2 grid per page, multiple pages if needed
        int cols = 2;
        int rows = 2;
        int gauge_w = SCREEN_WIDTH / cols;
        int gauge_h = SCREEN_HEIGHT / rows;
        num_pages = (VALUES_N + GAUGES_PER_PAGE - 1) / GAUGES_PER_PAGE;

        // Create pages (reuse base class scr for first page)
        screens[0] = scr;
        for (int page = 1; page < num_pages; page++) {
            screens[page] = default_screen_create();
        }

        // Create all grid cells (including empty ones) with background
        int total_cells = num_pages * GAUGES_PER_PAGE;
        for (int i = 0; i < total_cells; i++) {
            int page = i / GAUGES_PER_PAGE;
            int idx_in_page = i % GAUGES_PER_PAGE;
            int col = idx_in_page % cols;
            int row = idx_in_page / cols;

            lv_obj_t *cell = lv_obj_create(screens[page]);
            lv_obj_set_size(cell, gauge_w, gauge_h);
            lv_obj_set_style_bg_color(cell, lv_color_hex(0x000000), 0);
            lv_obj_set_style_bg_opa(cell, LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(cell, 0, 0);
            lv_obj_set_style_pad_all(cell, 0, 0);
            lv_obj_set_style_radius(cell, 0, 0);
            lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_pos(cell, col * gauge_w, row * gauge_h);
        }

        // Create gauge containers on top of background cells
        lv_obj_t *gauge_containers[VALUES_N];
        for (int i = 0; i < VALUES_N; i++) {
            int page = i / GAUGES_PER_PAGE;
            int idx_in_page = i % GAUGES_PER_PAGE;
            int col = idx_in_page % cols;
            int row = idx_in_page / cols;

            gauge_containers[i] = lv_obj_create(screens[page]);
            lv_obj_set_size(gauge_containers[i], gauge_w, gauge_h);
            lv_obj_set_style_bg_opa(gauge_containers[i], 0, 0);
            lv_obj_set_style_border_width(gauge_containers[i], 0, 0);
            lv_obj_set_style_pad_all(gauge_containers[i], 0, 0);
            lv_obj_clear_flag(gauge_containers[i], LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_pos(gauge_containers[i], col * gauge_w, row * gauge_h);
        }

        gauges[GAUGE_IDX_DEPTH] = new ValueGauge(gauge_containers[GAUGE_IDX_DEPTH], gauge_w, gauge_h,
                                                "DEPTH", "m", 0, 30, LV_PALETTE_BLUE);
        gauges[GAUGE_IDX_SOG] = new ValueGauge(gauge_containers[GAUGE_IDX_SOG], gauge_w, gauge_h,
                                                "SOG", "kts", 0, 15, LV_PALETTE_GREEN);
        gauges[GAUGE_IDX_HDG] = new ValueGauge(gauge_containers[GAUGE_IDX_HDG], gauge_w, gauge_h,
                                                "HDG", "\xC2\xB0", 0, 360, LV_PALETTE_PURPLE);
        gauges[GAUGE_IDX_AWS] = new ValueGauge(gauge_containers[GAUGE_IDX_AWS], gauge_w, gauge_h,
                                                "AWS", "kts", 0, 40, LV_PALETTE_ORANGE);
        gauges[GAUGE_IDX_TWS] = new ValueGauge(gauge_containers[GAUGE_IDX_TWS], gauge_w, gauge_h,
                                                "TWS", "kts", 0, 40, LV_PALETTE_CYAN);
    } else {
        // Carousel mode: one gauge per screen, swipe to navigate
        num_pages = VALUES_N;
        for (int j = 0; j < VALUES_N; j++) {
            screens[j] = default_screen_create();
        }
        scr = screens[0];

        gauges[GAUGE_IDX_DEPTH] = new ValueGauge(screens[GAUGE_IDX_DEPTH], SCREEN_WIDTH, SCREEN_HEIGHT,
                                                "DEPTH", "m", 0, 30, LV_PALETTE_BLUE);
        gauges[GAUGE_IDX_SOG] = new ValueGauge(screens[GAUGE_IDX_SOG], SCREEN_WIDTH, SCREEN_HEIGHT,
                                                "SOG", "kts", 0, 15, LV_PALETTE_GREEN);
        gauges[GAUGE_IDX_HDG] = new ValueGauge(screens[GAUGE_IDX_HDG], SCREEN_WIDTH, SCREEN_HEIGHT,
                                                "HDG", "\xC2\xB0", 0, 360, LV_PALETTE_PURPLE);
        gauges[GAUGE_IDX_AWS] = new ValueGauge(screens[GAUGE_IDX_AWS], SCREEN_WIDTH, SCREEN_HEIGHT,
                                                "AWS", "kts", 0, 40, LV_PALETTE_ORANGE);
        gauges[GAUGE_IDX_TWS] = new ValueGauge(screens[GAUGE_IDX_TWS], SCREEN_WIDTH, SCREEN_HEIGHT,
                                                "TWS", "kts", 0, 40, LV_PALETTE_CYAN);
    }

    #ifdef SHOWCASE
        for (uint8_t j = 0; j < VALUES_N; j++) {
            gauges[j]->showcase();
        }
    #endif

    lv_timer_t *timer = lv_timer_create(values_tick_cb, 100, this);
}

void ValuesScreen::on_swipe_up()
{
    if (num_pages <= 1) return;  // No navigation if only one page
    current_screen = (current_screen + num_pages - 1) % num_pages;
    lv_scr_load_anim(screens[current_screen], LV_SCR_LOAD_ANIM_FADE_ON, 100, 0, false);
    scr = screens[current_screen];
}

void ValuesScreen::on_swipe_down()
{
    if (num_pages <= 1) return;  // No navigation if only one page
    current_screen = (current_screen + 1) % num_pages;
    lv_scr_load_anim(screens[current_screen], LV_SCR_LOAD_ANIM_FADE_ON, 100, 0, false);
    scr = screens[current_screen];
}

#endif
