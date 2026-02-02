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
#ifndef CHARTSCREEN_H
#define CHARTSCREEN_H

#include "screen.h"
#include "../gauges/chartgauge.h"

#ifdef SCREEN_CHART

class ChartScreen : public Screen {
private:
    ChartGauge *gauge;

public:
    ChartScreen() : Screen() {
        gauge = new ChartGauge(scr, SCREEN_WIDTH, SCREEN_HEIGHT);
    }

    // Swipe up = zoom in
    void on_swipe_up() override {
        gauge->zoom_in();
    }

    // Swipe down = zoom out
    void on_swipe_down() override {
        gauge->zoom_out();
    }
};

#endif // SCREEN_CHART

#endif // CHARTSCREEN_H
