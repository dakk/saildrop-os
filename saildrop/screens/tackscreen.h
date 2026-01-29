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
#ifndef TACKSCREEN_H
#define TACKSCREEN_H

#include "screen.h"
#include "../data.h"
#include "../scale.h"
#include "../gauges/styles.h"

class TackScreen : public Screen
{
private:
    lv_obj_t *opp_hdg_label;
    lv_obj_t *curr_hdg_label;
    lv_obj_t *twa_label;

public:
    TackScreen();

    int32_t calculate_opposite_hdg(int32_t hdg, int32_t twa) {
        // Convert TWA from 0-3600 (0.1 deg units) to signed degrees
        int32_t twa_deg = twa / 10;
        int32_t hdg_deg = hdg / 10;
        int32_t signed_twa = (twa_deg > 180) ? twa_deg - 360 : twa_deg;
        // Calculate opposite tack heading
        int32_t opp = hdg_deg + 2 * signed_twa;
        // Normalize to 0-360
        opp = ((opp % 360) + 360) % 360;
        return opp;
    }

    void update(int32_t hdg, int32_t twa) {
        int32_t opp_hdg = calculate_opposite_hdg(hdg, twa);

        char buf[16];
        lv_snprintf(buf, sizeof(buf), "%d\xC2\xB0", opp_hdg);
        lv_label_set_text(opp_hdg_label, buf);

        lv_snprintf(buf, sizeof(buf), "HDG %d\xC2\xB0", hdg / 10);
        lv_label_set_text(curr_hdg_label, buf);

        int32_t twa_deg = twa / 10;
        int32_t signed_twa = (twa_deg > 180) ? twa_deg - 360 : twa_deg;
        lv_snprintf(buf, sizeof(buf), "TWA %d\xC2\xB0", signed_twa);
        lv_label_set_text(twa_label, buf);
    }
};

void tack_tick_cb(lv_timer_t *timer)
{
    TackScreen *screen = (TackScreen *)lv_timer_get_user_data(timer);
    screen->update(get_data()->hdg, get_twa());
}

TackScreen::TackScreen() : Screen()
{
    gauge_styles::init_styles();

    // Background container
    lv_obj_t *bg = gauge_styles::create_gauge_bg(scr, SCREEN_WIDTH, SCREEN_HEIGHT);

    // Title label
    lv_obj_t *title = lv_label_create(bg);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -SCALE_PX(70));
    lv_label_set_text(title, "OPP TACK");
    lv_obj_set_style_text_color(title, lv_palette_lighten(LV_PALETTE_GREY, 1), 0);
    lv_obj_set_style_text_font(title, ui_scale::font_small(), 0);

    // Main opposite heading value (large)
    opp_hdg_label = lv_label_create(bg);
    lv_obj_align(opp_hdg_label, LV_ALIGN_CENTER, 0, -SCALE_PX(15));
    lv_obj_set_style_text_color(opp_hdg_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(opp_hdg_label, ui_scale::font_large(), 0);
    lv_label_set_text(opp_hdg_label, "---");

    // Current heading (smaller, below)
    curr_hdg_label = lv_label_create(bg);
    lv_obj_align(curr_hdg_label, LV_ALIGN_CENTER, 0, SCALE_PX(40));
    lv_obj_set_style_text_color(curr_hdg_label, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_set_style_text_font(curr_hdg_label, ui_scale::font_medium(), 0);
    lv_label_set_text(curr_hdg_label, "HDG ---");

    // TWA (smaller, below heading)
    twa_label = lv_label_create(bg);
    lv_obj_align(twa_label, LV_ALIGN_CENTER, 0, SCALE_PX(70));
    lv_obj_set_style_text_color(twa_label, lv_palette_main(LV_PALETTE_ORANGE), 0);
    lv_obj_set_style_text_font(twa_label, ui_scale::font_medium(), 0);
    lv_label_set_text(twa_label, "TWA ---");

    lv_timer_create(tack_tick_cb, 100, this);
}

#endif
