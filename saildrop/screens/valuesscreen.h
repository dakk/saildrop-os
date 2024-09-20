#ifndef VALUESSCREEN_H
#define VALUESSCREEN_H

#include "screen.h"
#include "../gauges/valuegauge.h"


class ValuesScreen : public MultiScreen
{
private:
    ValueGauge *gauges[8];

public:
    ValuesScreen() : MultiScreen(8)
    {
        uint8_t j = 0;

        gauges[j] = new ValueGauge(screens[j], SCREEN_WIDTH, SCREEN_HEIGHT,
                                             "DEPTH", "m", 0, 30, LV_PALETTE_BLUE);
        gauges[j]->showcase();

        j++;
        gauges[j] = new ValueGauge(screens[j], SCREEN_WIDTH, SCREEN_HEIGHT,
                                             "SOG", "kts", 0, 15, LV_PALETTE_GREEN);
        gauges[j]->showcase();

        j++;
        gauges[j] = new ValueGauge(screens[j], SCREEN_WIDTH, SCREEN_HEIGHT,
                                             "AWA", "\xC2\xB0", 0, 360, LV_PALETTE_RED);
        gauges[j]->showcase();

        j++;
        gauges[j] = new ValueGauge(screens[j], SCREEN_WIDTH, SCREEN_HEIGHT,
                                             "AWS", "kts", 0, 40, LV_PALETTE_ORANGE);
        gauges[j]->showcase();

        j++;
        gauges[j] = new ValueGauge(screens[j], SCREEN_WIDTH, SCREEN_HEIGHT,
                                             "TWA", "\xC2\xB0", 0, 360, LV_PALETTE_YELLOW);
        gauges[j]->showcase();

        j++;
        gauges[j] = new ValueGauge(screens[j], SCREEN_WIDTH, SCREEN_HEIGHT,
                                             "TWS", "kts", 0, 40, LV_PALETTE_INDIGO);
        gauges[j]->showcase();

        j++;
        gauges[j] = new ValueGauge(screens[j], SCREEN_WIDTH, SCREEN_HEIGHT,
                                             "HDG", "\xC2\xB0", 0, 360, LV_PALETTE_PURPLE);
        gauges[j]->showcase();
    }
};

#endif
