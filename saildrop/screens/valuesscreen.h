#ifndef VALUESSCREEN_H
#define VALUESSCREEN_H

#include "screen.h"
#include "../gauges/valuegauge.h"

// TODO: create a new screen with multiple internal pages

class ValuesScreen : public Screen
{
private:
    lv_obj_t *screens[8];
    ValueGauge *gauges[8];
    uint8_t current_screen;
    uint8_t num_screens;

public:
    ValuesScreen()
    {
        scr = screens[current_screen];

        num_screens = 0;
        screens[num_screens] = default_screen_create();
        gauges[num_screens] = new ValueGauge(screens[num_screens], SCREEN_WIDTH, SCREEN_HEIGHT,
                                             "DEPTH", "m", 0, 30, LV_PALETTE_BLUE);
        gauges[num_screens]->showcase();

        num_screens++;
        screens[num_screens] = default_screen_create();
        gauges[num_screens] = new ValueGauge(screens[num_screens], SCREEN_WIDTH, SCREEN_HEIGHT,
                                             "SOG", "kts", 0, 15, LV_PALETTE_GREEN);
        gauges[num_screens]->showcase();

        num_screens++;
        screens[num_screens] = default_screen_create();
        gauges[num_screens] = new ValueGauge(screens[num_screens], SCREEN_WIDTH, SCREEN_HEIGHT,
                                             "AWA", "\xC2\xB0", 0, 360, LV_PALETTE_RED);
        gauges[num_screens]->showcase();

        num_screens++;
        screens[num_screens] = default_screen_create();
        gauges[num_screens] = new ValueGauge(screens[num_screens], SCREEN_WIDTH, SCREEN_HEIGHT,
                                             "AWS", "kts", 0, 40, LV_PALETTE_ORANGE);
        gauges[num_screens]->showcase();

        num_screens++;
        screens[num_screens] = default_screen_create();
        gauges[num_screens] = new ValueGauge(screens[num_screens], SCREEN_WIDTH, SCREEN_HEIGHT,
                                             "TWA", "\xC2\xB0", 0, 360, LV_PALETTE_YELLOW);
        gauges[num_screens]->showcase();

        num_screens++;
        screens[num_screens] = default_screen_create();
        gauges[num_screens] = new ValueGauge(screens[num_screens], SCREEN_WIDTH, SCREEN_HEIGHT,
                                             "TWS", "kts", 0, 40, LV_PALETTE_INDIGO);
        gauges[num_screens]->showcase();

        num_screens++;
        screens[num_screens] = default_screen_create();
        gauges[num_screens] = new ValueGauge(screens[num_screens], SCREEN_WIDTH, SCREEN_HEIGHT,
                                             "HDG", "\xC2\xB0", 0, 360, LV_PALETTE_PURPLE);
        gauges[num_screens]->showcase();

        num_screens++;
        scr = screens[0];
    }

    virtual void on_swipe_up() override
    {
        current_screen = abs((current_screen - 1) % num_screens);
        lv_scr_load_anim(screens[current_screen], LV_SCR_LOAD_ANIM_FADE_ON, 100, 0, false);
        scr = screens[current_screen];
    }

    virtual void on_swipe_down() override
    {
        current_screen = (current_screen + 1) % num_screens;
        lv_scr_load_anim(screens[current_screen], LV_SCR_LOAD_ANIM_FADE_ON, 100, 0, false);
        scr = screens[current_screen];
    }
};

#endif
