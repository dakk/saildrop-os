#ifndef TIMERSCREEN_H
#define TIMERSCREEN_H

#include "../gauges/timergauge.h"
#include "screen.h"

class TimerScreen : public MultiScreen
{
private:
    TimerGauge *timer;

public:
    TimerScreen() : MultiScreen(2)
    {
        // Screen 1: Timer
        timer = new TimerGauge(screens[0], SCREEN_WIDTH, SCREEN_HEIGHT, 60*5, LV_PALETTE_BLUE);

        // Setup timers
        

        // Screen 2: Settings
    }
};

#endif
