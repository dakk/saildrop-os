#ifndef TIMERSCREEN_H
#define TIMERSCREEN_H

#include "screen.h"

class TimerScreen : public Screen
{
private:
public:
    TimerScreen()
    {
        scr = default_screen_create();
    }
};

#endif
