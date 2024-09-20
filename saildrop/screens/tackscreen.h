#ifndef TACKSCREEN_H
#define TACKSCREEN_H

#include "screen.h"

class TackScreen : public Screen
{
private:
public:
    TackScreen()
    {
        scr = default_screen_create();
    }
};

#endif
