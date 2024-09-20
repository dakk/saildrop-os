#ifndef SPEEDSCREEN_H
#define SPEEDSCREEN_H

#include "screen.h"
#include "../gauges/speedgauge.h"

class SpeedScreen : public Screen
{
private:
public:
    SpeedScreen()
    {
        scr = default_screen_create();

        SpeedGauge *speed_gauge = new SpeedGauge(scr, SCREEN_WIDTH, SCREEN_HEIGHT);
        speed_gauge->showcase();
    }
};

#endif
