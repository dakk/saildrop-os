#ifndef CONF_H
#define CONF_H

#define DEBUG 1
#define LVGL_TICK_PERIOD_MS 2
#define LOOP_DELAY 2

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 240

#define SHOWCASE 0
#define HOME_DEBUG

#ifdef HOME_DEBUG
    #define WIFI_DEFAULT_SSID "SD_NMEA"
    #define WIFI_DEFAULT_PASSWORD "saildropwashere!!!!"
    #define WIFI_DEFAULT_IP "192.168.1.50"
    #define WIFI_DEFAULT_UDP_PORT 2000
    #define WIFI_DEFAULT_TCP_PORT 2001
#else
    #define WIFI_DEFAULT_SSID "NMEA3WIFI"
    #define WIFI_DEFAULT_PASSWORD "12345678"
    #define WIFI_DEFAULT_IP "192.168.4.1"
    #define WIFI_DEFAULT_UDP_PORT 2000
    #define WIFI_DEFAULT_TCP_PORT 2001
#endif

#endif