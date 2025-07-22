#ifndef SPEEDGAUGE_H
#define SPEEDGAUGE_H

#include <lvgl.h>
#include "../data.h"

class SpeedGauge
{
private:
public:
    lv_obj_t *meter;
    lv_meter_indicator_t *indic;
    lv_obj_t *speed_label;
    lv_obj_t * speed_arc;

    SpeedGauge(lv_obj_t *parent, int width, int height);
    void set_speed(int32_t speed);
    void showcase();
};



void speed_gauge_tick_cb(lv_timer_t *timer)
{
    ((SpeedGauge *)timer->user_data)->set_speed(get_data()->sog);
}

static void _sg_set_speed(void *sg, int32_t speed)
{
    ((SpeedGauge *)sg)->set_speed(speed);
}

SpeedGauge::SpeedGauge(lv_obj_t *parent, int width, int height)
{
    meter = lv_meter_create(parent);
    lv_obj_center(meter);
    lv_obj_set_size(meter, width, height);

    /*Add a scale first*/
    lv_meter_scale_t *scale = lv_meter_add_scale(meter);
    lv_meter_set_scale_ticks(meter, scale, 41, 2, 10, lv_palette_main(LV_PALETTE_GREY));
    lv_meter_set_scale_major_ticks(meter, scale, 8, 4, 15, lv_color_black(), 10);
    lv_meter_set_scale_range(meter, scale, 0, 15, 270, 0);

    /*Add a blue arc to the start*/
    indic = lv_meter_add_arc(meter, scale, 3, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_meter_set_indicator_start_value(meter, indic, 0);
    lv_meter_set_indicator_end_value(meter, indic, 3);

    /*Make the tick lines blue at the start of the scale*/
    indic = lv_meter_add_scale_lines(meter, scale, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_BLUE),
                                     false, 0);
    lv_meter_set_indicator_start_value(meter, indic, 0);
    lv_meter_set_indicator_end_value(meter, indic, 3);

    /*Add a red arc to the end*/
    indic = lv_meter_add_arc(meter, scale, 3, lv_palette_main(LV_PALETTE_RED), 0);
    lv_meter_set_indicator_start_value(meter, indic, 10);
    lv_meter_set_indicator_end_value(meter, indic, 15);

    /*Make the tick lines red at the end of the scale*/
    indic = lv_meter_add_scale_lines(meter, scale, lv_palette_main(LV_PALETTE_RED), lv_palette_main(LV_PALETTE_RED), false,
                                     0);
    lv_meter_set_indicator_start_value(meter, indic, 10);
    lv_meter_set_indicator_end_value(meter, indic, 15);

    /*Add a needle line indicator*/
    indic = lv_meter_add_needle_line(meter, scale, 4, lv_palette_main(LV_PALETTE_GREY), -10);

    // Add speed label
    speed_label = lv_label_create(meter);
    lv_obj_align(speed_label, LV_ALIGN_CENTER, 0, 40);
    lv_label_set_text(speed_label, "0.0 kts");


    // Speed arc
    speed_arc = lv_arc_create(parent);

    lv_obj_set_style_arc_width(speed_arc, 10, LV_PART_MAIN );
    lv_obj_set_style_arc_width(speed_arc, 10, LV_PART_INDICATOR);
    // lv_obj_set_style_arc_color(spinner, lv_palette_main(LV_PALETTE_ORANGE), LV_PART_MAIN);
    lv_obj_set_style_arc_color(speed_arc, lv_palette_darken(LV_PALETTE_BLUE, 3), LV_PART_INDICATOR);

    lv_obj_remove_style(speed_arc, NULL, LV_PART_KNOB);   /*Be sure the knob is not displayed*/
    lv_obj_clear_flag(speed_arc, LV_OBJ_FLAG_CLICKABLE);  /*To not allow adjusting by click*/
    
    lv_arc_set_rotation(speed_arc, 0);
    lv_arc_set_bg_angles(speed_arc, 0, 270);
    lv_arc_set_start_angle(speed_arc, 0);

    lv_obj_set_size(speed_arc, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_center(speed_arc);

    lv_timer_t *timer = lv_timer_create(speed_gauge_tick_cb, 100, this);
}

void SpeedGauge::set_speed(int32_t speed)
{
    lv_arc_set_end_angle(speed_arc, 270 * speed / 150);

    if (speed < 30) 
        lv_obj_set_style_arc_color(speed_arc, lv_palette_darken(LV_PALETTE_BLUE, 3), LV_PART_INDICATOR);
    else if (speed > 100) 
        lv_obj_set_style_arc_color(speed_arc, lv_palette_darken(LV_PALETTE_RED, 3), LV_PART_INDICATOR);
    else 
        lv_obj_set_style_arc_color(speed_arc, lv_palette_darken(LV_PALETTE_GREEN, 3), LV_PART_INDICATOR);


    lv_meter_set_indicator_value(meter, indic, speed / 10);

    char buf[20];
    lv_snprintf(buf, sizeof(buf), "%d.%d kts", speed / 10, speed % 10);
    lv_label_set_text(speed_label, buf);
}

void SpeedGauge::showcase()
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_exec_cb(&a, _sg_set_speed);
    lv_anim_set_var(&a, this);
    lv_anim_set_values(&a, 0, 150);
    lv_anim_set_time(&a, 2000);
    lv_anim_set_repeat_delay(&a, 100);
    lv_anim_set_playback_time(&a, 500);
    lv_anim_set_playback_delay(&a, 300);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
}

#endif
