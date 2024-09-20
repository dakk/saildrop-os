#ifndef COMPASSSCREEN_H
#define COMPASSSCREEN_H

#include "screen.h"
#include "../gauges/compass.h"

class CompassScreen : public Screen
{
private:
public:
    CompassScreen()
    {
        scr = default_screen_create();

        Compass *compass = new Compass(scr, SCREEN_WIDTH, SCREEN_HEIGHT);
        compass->showcase();
    }
};

#endif
