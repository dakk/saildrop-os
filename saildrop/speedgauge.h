#ifndef SPEEDGAUGE_H
#define SPEEDGAUGE_H

#include <lvgl.h>

class SpeedGauge {
  private:    
  public:
    lv_obj_t * meter;
    lv_meter_indicator_t * indic;
    
    SpeedGauge(lv_obj_t *parent, int screenWidth, int screenHeight) {
        meter = lv_meter_create(parent);
        lv_obj_center(meter);
        lv_obj_set_size(meter, screenWidth, screenHeight);
    
        /*Add a scale first*/
        lv_meter_scale_t * scale = lv_meter_add_scale(meter);
        lv_meter_set_scale_ticks(meter, scale, 41, 2, 10, lv_palette_main(LV_PALETTE_GREY));
        lv_meter_set_scale_major_ticks(meter, scale, 8, 4, 15, lv_color_black(), 10);
        lv_meter_set_scale_range(meter, scale, 0, 15, 270, 120);
    
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
    }

    void set_speed(int32_t v) {
      lv_meter_set_indicator_value(meter, indic, v);
    }
};


#endif
