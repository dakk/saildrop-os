#ifndef WINDGAUGE_H
#define WINDGAUGE_H

#include <lvgl.h>


class WindGauge {
  private:    
  public:
    lv_obj_t * meter;
    lv_meter_indicator_t * indic_direction;
    // lv_meter_indicator_t* speed_indic;
    lv_obj_t* speed_label;
    
    WindGauge(lv_obj_t *parent, int screenWidth, int screenHeight);

    void set_direction(int32_t v);
    void set_speed(int32_t speed);
    void showcase();
};


#endif
