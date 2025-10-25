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
#ifndef SPEEDSCREEN_H
#define SPEEDSCREEN_H

#include "screen.h"
#include "../conf.h"
#include "../gauges/speedgauge.h"

class SpeedScreen : public Screen
{
private:
public:
    SpeedScreen() : Screen()
    {
        SpeedGauge *speed_gauge = new SpeedGauge(scr, SCREEN_WIDTH, SCREEN_HEIGHT);
        #ifdef SHOWCASE
            speed_gauge->showcase();
        #endif
    }
};

#endif
