#ifndef SPEEDGAUGE_H
#define SPEEDGAUGE_H

#include <lvgl.h>
#include "../data.h"

class SpeedGauge
{
private:
    lv_obj_t *speed_label;
    lv_obj_t *scale_line;
    lv_obj_t *needle_line;
public:

    SpeedGauge(lv_obj_t *parent, int width, int height);
    void set_speed(int32_t speed);
    void showcase();
};



// void speed_gauge_tick_cb(lv_timer_t *timer)
// {
//     ((SpeedGauge *)timer->user_data)->set_speed(get_data()->sog);
// }

static void _sg_set_speed(void *sg, int32_t speed)
{
    ((SpeedGauge *)sg)->set_speed(speed);
}

SpeedGauge::SpeedGauge(lv_obj_t *parent, int width, int height)
{
    // Create a bg object (TODO: create an helper for everyone)
    lv_obj_t * bg = lv_obj_create(parent);
    lv_obj_set_size(bg, width, height);
    lv_obj_center(bg);
    lv_obj_set_style_radius(bg, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(bg, lv_palette_darken(LV_PALETTE_GREY, 4), 0);
    lv_obj_set_style_bg_opa(bg, LV_OPA_COVER, 0);
    lv_obj_remove_flag(bg, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(bg, 0, LV_PART_MAIN);


    scale_line = lv_scale_create(bg);

    lv_obj_set_size(scale_line, width, height);
    lv_scale_set_mode(scale_line, LV_SCALE_MODE_ROUND_INNER);
    // lv_obj_set_style_bg_opa(scale_line, LV_OPA_COVER, 0);
    // lv_obj_set_style_bg_color(scale_line, lv_palette_lighten(LV_PALETTE_RED, 5), 0);
    lv_obj_set_style_radius(scale_line, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_clip_corner(scale_line, true, 0);
    lv_obj_align(scale_line, LV_ALIGN_LEFT_MID, LV_PCT(2), 0);

    lv_scale_set_label_show(scale_line, true);

    lv_scale_set_total_tick_count(scale_line, 31);
    lv_scale_set_major_tick_every(scale_line, 5);

    lv_obj_set_style_length(scale_line, 5, LV_PART_ITEMS);
    lv_obj_set_style_length(scale_line, 10, LV_PART_INDICATOR);
    lv_scale_set_range(scale_line, 0, 20);

    lv_scale_set_angle_range(scale_line, 270);
    lv_scale_set_rotation(scale_line, 135);

    // Tick style
    static lv_style_t style_ticks;
    lv_style_init(&style_ticks);
    lv_style_set_line_color(&style_ticks, lv_palette_darken(LV_PALETTE_GREY, 1));
    lv_style_set_line_width(&style_ticks, 2);
    lv_style_set_width(&style_ticks, 10);
    lv_obj_add_style(scale_line, &style_ticks, LV_PART_INDICATOR);

    // Needle
    needle_line = lv_line_create(scale_line);
    lv_obj_set_style_line_width(needle_line, 6, LV_PART_MAIN);
    lv_obj_set_style_line_rounded(needle_line, true, LV_PART_MAIN);
    lv_obj_set_style_line_color(needle_line, lv_palette_darken(LV_PALETTE_GREY, 1), LV_PART_MAIN | LV_STATE_DEFAULT);

    // Speed label
    speed_label = lv_label_create(scale_line);
    lv_obj_align(speed_label, LV_ALIGN_CENTER, 0, 40);
    lv_obj_set_style_text_color(speed_label, lv_color_white(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(speed_label, &lv_font_montserrat_20, 0);
    lv_label_set_text(speed_label, "0.0 kts");

    // lv_timer_t *timer = lv_timer_create(speed_gauge_tick_cb, 100, this);
}

void SpeedGauge::set_speed(int32_t speed)
{
    char buf[20];
    lv_scale_set_line_needle_value(scale_line, needle_line, 60, speed / 10.0);

    lv_snprintf(buf, sizeof(buf), "%d.%d kts", speed / 10, speed % 10);
    lv_label_set_text(speed_label, buf);
}

void SpeedGauge::showcase()
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_exec_cb(&a, _sg_set_speed);
    lv_anim_set_var(&a, this);
    lv_anim_set_values(&a, 0, 200);
    lv_anim_set_time(&a, 2000);
    lv_anim_set_repeat_delay(&a, 100);
    lv_anim_set_playback_time(&a, 500);
    lv_anim_set_playback_delay(&a, 300);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
}

#endif
