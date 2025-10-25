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

#define VALUES_N            3
#define GAUGE_IDX_DEPTH     0
#define GAUGE_IDX_SOG       1
#define GAUGE_IDX_HDG       2
// #define GAUGE_IDX_AWA       3
// #define GAUGE_IDX_AWS       4
// #define GAUGE_IDX_TWA       5
// #define GAUGE_IDX_TWS       6



class ValuesScreen : public MultiScreen
{
public:
    ValuesScreen();
    ValueGauge *gauges[VALUES_N];
};

void values_tick_cb(lv_timer_t *timer)
{
    ValuesScreen *gauge = (ValuesScreen *) lv_timer_get_user_data(timer);
    gauge->gauges[GAUGE_IDX_SOG]->set_value(get_data()->sog);
    gauge->gauges[GAUGE_IDX_HDG]->set_value(get_data()->hdg);
}


ValuesScreen::ValuesScreen() : MultiScreen(VALUES_N)
{
    gauges[GAUGE_IDX_DEPTH] = new ValueGauge(screens[GAUGE_IDX_DEPTH], SCREEN_WIDTH, SCREEN_HEIGHT,
                                            "DEPTH", "m", 0, 30, LV_PALETTE_BLUE);
    gauges[GAUGE_IDX_SOG] = new ValueGauge(screens[GAUGE_IDX_SOG], SCREEN_WIDTH, SCREEN_HEIGHT,
                                            "SOG", "kts", 0, 15, LV_PALETTE_GREEN);
    // gauges[GAUGE_IDX_AWA] = new ValueGauge(screens[GAUGE_IDX_AWA], SCREEN_WIDTH, SCREEN_HEIGHT,
    //                                      "AWA", "\xC2\xB0", 0, 360, LV_PALETTE_RED);
    // gauges[GAUGE_IDX_AWS] = new ValueGauge(screens[GAUGE_IDX_AWS], SCREEN_WIDTH, SCREEN_HEIGHT,
    //                                      "AWS", "kts", 0, 40, LV_PALETTE_ORANGE);
    // gauges[GAUGE_IDX_TWA] = new ValueGauge(screens[GAUGE_IDX_TWA], SCREEN_WIDTH, SCREEN_HEIGHT,
    //                                      "TWA", "\xC2\xB0", 0, 360, LV_PALETTE_YELLOW);
    // gauges[GAUGE_IDX_TWS] = new ValueGauge(screens[GAUGE_IDX_TWS], SCREEN_WIDTH, SCREEN_HEIGHT,
    //                                      "TWS", "kts", 0, 40, LV_PALETTE_INDIGO);
    gauges[GAUGE_IDX_HDG] = new ValueGauge(screens[GAUGE_IDX_HDG], SCREEN_WIDTH, SCREEN_HEIGHT,
                                            "HDG", "\xC2\xB0", 0, 360, LV_PALETTE_PURPLE);

    #ifdef SHOWCASE 
        for (uint8_t j = 0; j < VALUES_N; i++) {
            gauges[j]->showcase();
        }
    #endif


    lv_timer_t *timer = lv_timer_create(values_tick_cb, 100, this);
}

#endif
