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
