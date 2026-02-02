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
#include "../conf.h"
#include "../scale.h"
#include "styles.h"

class TimerGauge
{
private:
    bool started;
    bool finished;
    lv_obj_t *value_label;
    lv_obj_t *btn_label;
    lv_obj_t *value_arc;
    int32_t remaining;
    int32_t seconds;
    lv_timer_t *timer;
    lv_palette_t arc_color_orig;

public:
    TimerGauge(lv_obj_t *parent, int width, int height, int32_t secs, lv_palette_t arc_color);
    ~TimerGauge();
    void tick_handler();
    void start_stop();
    void reset(int32_t seconds = -1);
    void showcase();

    void increase_secs();
    void decrese_secs();
    void display_update();
};

void timer_gauge_tick_cb(lv_timer_t *timer)
{
    ((TimerGauge *) lv_timer_get_user_data(timer))->tick_handler();
}

void timer_gauge_btn_cb(lv_event_t *e)
{
    TimerGauge *gauge = (TimerGauge *) lv_event_get_user_data(e);
    gauge->start_stop();
}

TimerGauge::~TimerGauge()
{
    lv_timer_del(timer);
}

TimerGauge::TimerGauge(lv_obj_t *parent, int width, int height, int32_t secs = 60 * 5, lv_palette_t arc_color = LV_PALETTE_BLUE)
{
    gauge_styles::init_styles();

    started = false;
    finished = false;
    seconds = secs;
    remaining = secs;
    arc_color_orig = arc_color;

    // Background container
    lv_obj_t *bg = gauge_styles::create_gauge_bg(parent, width, height);

    // Arc indicator (full circle for timer)
    value_arc = lv_arc_create(bg);
    lv_obj_set_size(value_arc, width - SCALE_PX(10), height - SCALE_PX(10));
    lv_obj_center(value_arc);
    lv_arc_set_rotation(value_arc, 270);  // Start from top
    lv_arc_set_bg_angles(value_arc, 0, 360);
    lv_arc_set_value(value_arc, 100);
    lv_obj_remove_style(value_arc, nullptr, LV_PART_KNOB);
    lv_obj_remove_flag(value_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(value_arc, SCALE_PX(8), LV_PART_MAIN);
    lv_obj_set_style_arc_color(value_arc, lv_palette_darken(LV_PALETTE_GREY, 3), LV_PART_MAIN);
    lv_obj_set_style_arc_width(value_arc, SCALE_PX(8), LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(value_arc, lv_palette_main(arc_color), LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(value_arc, false, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(value_arc, false, LV_PART_INDICATOR);

    // Title label
    lv_obj_t *title = lv_label_create(bg);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -SCALE_PX(50));
    lv_label_set_text(title, "TIMER");
    lv_obj_set_style_text_color(title, lv_palette_lighten(LV_PALETTE_GREY, 1), 0);
    lv_obj_set_style_text_font(title, ui_scale::font_small(), 0);

    // Main value label (big digits)
    value_label = lv_label_create(bg);
    lv_obj_align(value_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_color(value_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(value_label, ui_scale::font_large(), 0);
    lv_label_set_text(value_label, "5:00");

    // Play/Stop button
    lv_obj_t *btn = lv_obj_create(bg);
    lv_obj_set_size(btn, SCALE_PX(50), SCALE_PX(30));
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, SCALE_PX(45));
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(btn, lv_palette_darken(LV_PALETTE_GREY, 2), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(btn, SCALE_PX(5), LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn, lv_palette_darken(LV_PALETTE_GREY, 1), LV_STATE_PRESSED);
    lv_obj_add_event_cb(btn, timer_gauge_btn_cb, LV_EVENT_CLICKED, this);

    btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, LV_SYMBOL_PLAY);
    lv_obj_set_style_text_color(btn_label, lv_color_white(), 0);
    lv_obj_center(btn_label);

    display_update();
    timer = lv_timer_create(timer_gauge_tick_cb, 1000, this);
}

void TimerGauge::increase_secs()
{
    if (started)
        return;

    // Reset if finished or partially elapsed
    if (finished || seconds != remaining)
    {
        reset();
    }

    if (seconds == 60 * 5)
        seconds = 60 * 10;

    remaining = seconds;
    display_update();
}

void TimerGauge::decrese_secs()
{
    if (started)
        return;

    // Reset if finished or partially elapsed
    if (finished || seconds != remaining)
    {
        reset();
    }

    if (seconds == 60 * 10)
        seconds = 60 * 5;

    remaining = seconds;
    display_update();
}

void TimerGauge::tick_handler()
{
    if (!started)
        return;

    if (remaining == 0)
    {
        if (!finished)
        {
            finished = true;
            started = false;
            lv_obj_set_style_arc_color(value_arc, lv_palette_main(LV_PALETTE_RED), LV_PART_INDICATOR);
            lv_label_set_text(btn_label, LV_SYMBOL_REFRESH);
        }
        return;
    }

    remaining--;
    display_update();
}

void TimerGauge::display_update() {
    // Arc shows remaining percentage (100 = full, 0 = empty)
    int32_t arc_val = (remaining * 100) / seconds;
    lv_arc_set_value(value_arc, arc_val);

    char buf[20];
    lv_snprintf(buf, sizeof(buf), "%d:%02d", remaining / 60, remaining % 60);
    lv_label_set_text(value_label, buf);
}

void TimerGauge::start_stop()
{
    if (finished)
    {
        reset();
        return;
    }

    started = !started;

    if (started)
        lv_label_set_text(btn_label, LV_SYMBOL_STOP);
    else
        lv_label_set_text(btn_label, LV_SYMBOL_PLAY);
}

void TimerGauge::reset(int32_t secs)
{
    if (secs != -1)
    {
        seconds = secs;
    }
    remaining = seconds;
    started = false;
    finished = false;
    lv_obj_set_style_arc_color(value_arc, lv_palette_main(arc_color_orig), LV_PART_INDICATOR);
    lv_label_set_text(btn_label, LV_SYMBOL_PLAY);
    display_update();
}

void TimerGauge::showcase()
{
}

#endif
