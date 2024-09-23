#ifndef COMPASSSCREEN_H
#define COMPASSSCREEN_H

#include "screen.h"
#include "../gauges/compass.h"

class CompassScreen : public Screen
{
private:
public:
    CompassScreen() : Screen()
    {
        Compass *compass = new Compass(scr, SCREEN_WIDTH, SCREEN_HEIGHT);
        compass->showcase();
    }
};

#endif
