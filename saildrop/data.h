#ifndef DATA_H
#define DATA_H

#include <lvgl.h>

struct nmea_data {
    uint32_t sog;
    uint32_t hdg;

    uint32_t tws;
    uint32_t twa;
    uint32_t aws;
    uint32_t awa;
};

nmea_data *get_data();

#endif