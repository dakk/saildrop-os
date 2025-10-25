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
#ifndef TIMERGAUGE_H
#define TIMERGAUGE_H

#include <lvgl.h>
#include <string.h>

class TimerGauge
{
private:
    bool started;
    lv_obj_t *value_label;
    lv_obj_t *btn_label;
    lv_obj_t *value_arc;
    int32_t remaining;
    int32_t seconds;
    lv_timer_t *timer;

public:
    TimerGauge(lv_obj_t *parent, int width, int height, int32_t secs, lv_palette_t arc_color);
    ~TimerGauge();
    void tick_handler();
    void start_stop();
    void reset(int32_t seconds);
    void showcase();

    void increase_secs();
    void decrese_secs();
    void display_update();
};

void timer_gauge_tick_cb(lv_timer_t *timer)
{
    ((TimerGauge *) lv_timer_get_user_data(timer))->tick_handler();
}

TimerGauge::~TimerGauge()
{
    lv_timer_del(timer);
}

TimerGauge::TimerGauge(lv_obj_t *parent, int width, int height, int32_t secs = 60 * 5, lv_palette_t arc_color = LV_PALETTE_BLUE)
{
    started = false;
    seconds = secs;
    remaining = secs;

    lv_obj_t *container = lv_obj_create(parent);
    lv_obj_set_size(container, width, height);
    lv_obj_center(container);
    // lv_obj_set_style_bg_color(container, lv_color_hex(0x000000), 0); // Navy blue background
    lv_obj_set_style_border_width(container, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_width(container, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_line_width(container, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *gauge_label = lv_label_create(container);
    lv_obj_align(gauge_label, LV_ALIGN_CENTER, 0, -40);
    lv_label_set_text(gauge_label, "TIMER");

    // Add value label
    value_label = lv_label_create(container);
    lv_obj_align(value_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_font(value_label, &lv_font_montserrat_48, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(value_label, "0:00");

    value_arc = lv_arc_create(container);

    lv_obj_set_style_arc_width(value_arc, 20, LV_PART_MAIN);
    lv_obj_set_style_arc_width(value_arc, 20, LV_PART_INDICATOR);
    // lv_obj_set_style_arc_color(spinner, lv_palette_main(LV_PALETTE_ORANGE), LV_PART_MAIN);
    lv_obj_set_style_arc_color(value_arc, lv_palette_darken(arc_color, 3), LV_PART_INDICATOR);

    lv_obj_remove_style(value_arc, NULL, LV_PART_KNOB);  /*Be sure the knob is not displayed*/
    lv_obj_clear_flag(value_arc, LV_OBJ_FLAG_CLICKABLE); /*To not allow adjusting by click*/

    lv_arc_set_rotation(value_arc, 0);
    lv_arc_set_bg_angles(value_arc, 0, 360);
    lv_arc_set_start_angle(value_arc, 0);

    lv_obj_set_size(value_arc, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_center(value_arc);

    lv_obj_t *btn2 = lv_btn_create(container);
    // lv_obj_add_event_cb(btn2, event_handler, LV_EVENT_ALL, NULL);
    lv_obj_align(btn2, LV_ALIGN_CENTER, 0, 40);
    lv_obj_add_flag(btn2, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_set_height(btn2, LV_SIZE_CONTENT);

    btn_label = lv_label_create(btn2);
    lv_label_set_text(btn_label, LV_SYMBOL_PLAY); // LV_SYMBOL_STOP
    lv_obj_center(btn_label);

    display_update();
    timer = lv_timer_create(timer_gauge_tick_cb, 1000, this);
}

void TimerGauge::increase_secs()
{
    if (started || seconds != remaining)
        return;
    // if (seconds == 60)
    //     seconds = 60 * 5;
    if (seconds == 60 * 5)
        seconds = 60 * 10;

    remaining = seconds;
    display_update();
}

void TimerGauge::decrese_secs()
{
    if (started || seconds != remaining)
        return;
    if (seconds == 60 * 10)
        seconds = 60 * 5;
    // else if (seconds == 60 * 5)
    //     seconds = 60;

    remaining = seconds;
    display_update();
}

void TimerGauge::tick_handler()
{
    if (!started)
        return;

    if (remaining == 0)
    {
        // TODO: Do some animation
        return;
    }

    remaining--;
    display_update();
}

void TimerGauge::display_update() {
    // 100 : seconds = x: elapsed
    // x = elapsed * 100 / seconds
    lv_arc_set_value(value_arc, 100 - ((seconds - remaining) * 100 / seconds));

    char buf[20];
    lv_snprintf(buf, sizeof(buf), "%d:%02d", remaining / 60, remaining % 60);
    lv_label_set_text(value_label, buf);
}

void TimerGauge::start_stop()
{
    started = !started;

    if (started)
        lv_label_set_text(btn_label, LV_SYMBOL_STOP);
    else
        lv_label_set_text(btn_label, LV_SYMBOL_PLAY);
}

void TimerGauge::reset(int32_t secs = -1)
{
    if (seconds != -1)
    {
        seconds = secs;
    }
    remaining = seconds;
    started = false;
}

void TimerGauge::showcase()
{
}

#endif
