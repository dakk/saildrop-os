#ifndef TIMERSCREEN_H
#define TIMERSCREEN_H

#include "../gauges/timergauge.h"
#include "screen.h"

class TimerScreen : public Screen
{
private:
    TimerGauge *timer;

public:
    TimerScreen() : Screen()
    {
        timer = new TimerGauge(scr, SCREEN_WIDTH, SCREEN_HEIGHT, 60*5, LV_PALETTE_BLUE);
    }

    virtual void on_swipe_up() override
    {
        timer->increase_secs();
    }

    virtual void on_swipe_down() override
    {
        timer->decrese_secs();
    }
};

#endif
