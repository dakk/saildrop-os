#ifndef WINDGAUGE_H
#define WINDGAUGE_H

#include <lvgl.h>

class WindGauge
{
private:
public:
    lv_obj_t *meter;
    lv_meter_indicator_t *indic_direction;
    // lv_meter_indicator_t* speed_indic;
    lv_obj_t *speed_label;

    WindGauge(lv_obj_t *parent, int width, int height);

    void set_direction(int32_t v);
    void set_speed(int32_t speed);
    void showcase();
};

static void _wg_set_speed(void *wg, int32_t speed)
{
    ((WindGauge *)wg)->set_speed(speed);
}

static void _wg_set_direction(void *wg, int32_t direction)
{
    ((WindGauge *)wg)->set_direction(direction);
}

WindGauge::WindGauge(lv_obj_t *parent, int width, int height)
{
    meter = lv_meter_create(parent); // lv_scr_act()
    lv_obj_center(meter);
    lv_obj_set_size(meter, width, height);

    /*Add a scale first*/
    lv_meter_scale_t *direction_scale = lv_meter_add_scale(meter);
    lv_meter_set_scale_ticks(meter, direction_scale, 37, 2, 10, lv_palette_main(LV_PALETTE_GREY));
    lv_meter_set_scale_major_ticks(meter, direction_scale, 4, 4, 15, lv_color_black(), 10);
    lv_meter_set_scale_range(meter, direction_scale, -179, 180, 360, 90);

    // RED on left
    lv_meter_indicator_t *indic = lv_meter_add_arc(meter, direction_scale, 3, lv_palette_main(LV_PALETTE_RED), 0);
    lv_meter_set_indicator_start_value(meter, indic, -45);
    lv_meter_set_indicator_end_value(meter, indic, 0);

    indic = lv_meter_add_scale_lines(meter, direction_scale, lv_palette_main(LV_PALETTE_RED), lv_palette_main(LV_PALETTE_RED), false, 0);
    lv_meter_set_indicator_start_value(meter, indic, -45);
    lv_meter_set_indicator_end_value(meter, indic, 0);

    // GREEN on right
    indic = lv_meter_add_arc(meter, direction_scale, 3, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_meter_set_indicator_start_value(meter, indic, 0);
    lv_meter_set_indicator_end_value(meter, indic, 45);
    indic = lv_meter_add_scale_lines(meter, direction_scale, lv_palette_main(LV_PALETTE_GREEN), lv_palette_main(LV_PALETTE_GREEN), false, 0);
    lv_meter_set_indicator_start_value(meter, indic, 0);
    lv_meter_set_indicator_end_value(meter, indic, 45);

    // Needle
    indic_direction = lv_meter_add_needle_line(meter, direction_scale, 8, lv_palette_main(LV_PALETTE_RED), -10);

    // Add wind speed label
    speed_label = lv_label_create(meter);
    lv_obj_align(speed_label, LV_ALIGN_CENTER, 0, 40);
    lv_label_set_text(speed_label, "0.0 kts");
}

void WindGauge::set_direction(int32_t v)
{
    lv_meter_set_indicator_value(meter, indic_direction, v);
}

void WindGauge::set_speed(int32_t speed)
{
    // lv_meter_set_indicator_end_value(meter, speed_indic, speed);
    char buf[20];
    lv_snprintf(buf, sizeof(buf), "%d.%d kts", speed / 10, speed % 10);
    lv_label_set_text(speed_label, buf);
}

void WindGauge::showcase()
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_exec_cb(&a, _wg_set_direction);
    lv_anim_set_var(&a, this);
    lv_anim_set_values(&a, -180, 180);
    lv_anim_set_time(&a, 2000);
    lv_anim_set_repeat_delay(&a, 100);
    lv_anim_set_playback_time(&a, 500);
    lv_anim_set_playback_delay(&a, 100);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);

    lv_anim_t b;
    lv_anim_init(&b);
    lv_anim_set_exec_cb(&b, _wg_set_speed);
    lv_anim_set_var(&b, this);
    lv_anim_set_values(&b, 0, 400);
    lv_anim_set_time(&b, 2000);
    lv_anim_set_repeat_delay(&b, 100);
    lv_anim_set_playback_time(&b, 500);
    lv_anim_set_playback_delay(&b, 100);
    lv_anim_set_repeat_count(&b, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&b);
}

#endif
