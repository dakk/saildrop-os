/*
 * conf.h - configuration for the Cagliari naval radar (radar/ sketch)
 */
#ifndef CONF_H
#define CONF_H

// Board selection (BOARD_LCD_128 or BOARD_LCD_4) comes from the
// auto-generated common/board_config.h via the Makefile
#include "../common/boards/boards.h"

#define DEBUG 1
#define LVGL_TICK_PERIOD_MS 2
#define LOOP_DELAY 2

// --- WiFi (home network the radar connects to) ---
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// --- AIS feed: raw NMEA (!AIVDM/!AIVDO) over plain TCP ---
// Point this at your endpoint. For development you can replay the recorded
// gulf traffic from this repo on any machine on the LAN:
//   python3 tools/serve_testdata.py testdata/ais_29_01_2026.nmea --tcp-port 10110 --interval 0.05
// For live data, run a small bridge on the LAN that pulls AISHub (or your own
// receiver) and forwards only sentences inside AIS_BBOX_* below.
#define AIS_HOST "192.168.1.10"
#define AIS_PORT 10110
// Optional line sent right after connecting (e.g. AISHub API key):
// #define AIS_HELLO "..."

// --- House position: radar origin, drawn at bottom center ---
// 39.215977 N, 9.267019 E (Quartu Sant'Elena, Cagliari) - microdegrees
#define HOME_LAT 39215977
#define HOME_LON 9267019

// The gulf is SSW of the house (~195 deg true), so the radar is rotated:
// screen-up = RADAR_UP_BEARING. The north marker shows true north.
#define RADAR_UP_BEARING 195.0f

// --- Gulf of Cagliari AIS filter bounding box (microdegrees) ---
// Targets outside are dropped to save slots/bandwidth.
#define AIS_BBOX_LAT_MIN 38850000   // 38.85 N  (Capo Spartivento side)
#define AIS_BBOX_LAT_MAX 39300000   // 39.30 N
#define AIS_BBOX_LON_MIN 8800000    //  8.80 E  (Sant'Antioco side)
#define AIS_BBOX_LON_MAX 9600000    //  9.60 E  (Capo Carbonara side)

// --- Demo mode: synthetic moving targets, no feed needed ---
// #define SHOWCASE 1

#endif // CONF_H
