#ifndef SPEEDSCREEN_H
#define SPEEDSCREEN_H

#include "screen.h"
#include "../gauges/speedgauge.h"

class SpeedScreen : public Screen
{
private:
public:
    SpeedScreen() : Screen()
    {
        SpeedGauge *speed_gauge = new SpeedGauge(scr, SCREEN_WIDTH, SCREEN_HEIGHT);
        speed_gauge->showcase();
    }
};

#endif
