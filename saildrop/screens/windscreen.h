#ifndef WINDSCREEN_H
#define WINDSCREEN_H

#include "screen.h"
#include "../gauges/windgauge.h"

class WindScreen : public Screen
{
private:
public:
    WindScreen() : Screen()
    {
        WindGauge *wind_gauge = new WindGauge(scr, SCREEN_WIDTH, SCREEN_HEIGHT);
        wind_gauge->showcase();
    }
};

#endif
