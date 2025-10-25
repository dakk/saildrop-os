/*
 * Copyright (C) 2024-2025 Davide Gessa
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef SCREEN_H
#define SCREEN_H

lv_obj_t *default_screen_create()
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(scr, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    return scr;
}

class Screen
{
public:
    lv_obj_t *scr;

    Screen()
    {
        scr = default_screen_create();
    }

    virtual void on_swipe_up() {}
    virtual void on_swipe_down() {}
};

class MultiScreen : public Screen
{
protected:
    lv_obj_t *screens[8];
    uint8_t current_screen;
    uint8_t num_screens;

public:
    MultiScreen(uint8_t ns)
    {
        num_screens = ns;

        for (int j; j < num_screens; j++)
        {
            screens[j] = default_screen_create();
        }
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
