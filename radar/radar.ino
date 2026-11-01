/*
 * radar.ino - naval radar for the Gulf of Cagliari.
 *
 * Shows AIS boats in realtime relative to the house (HOME_LAT/HOME_LON in
 * conf.h), which is pinned at bottom center of the display. No map: a
 * stylized coastline silhouette (common/coastline.h).
 *
 * Core 0: WiFi + AIS feed (common/conn.h)   Core 1: LVGL rendering
 *
 * Touch: swipe up/down = radar range, tap = select nearest boat.
 */
#include "conf.h"
#include <Arduino.h>
#include <esp_system.h>
#include <esp_heap_caps.h>
#include <lvgl.h>
#include "../common/hal.h"
#include "../common/conn.h"
#include "../common/geo.h"
#include "gauges/radargauge.h"

// LVGL draw buffer (PSRAM on the 4" board, like saildrop)
#if defined(USE_PSRAM_BUFFER) && USE_PSRAM_BUFFER && defined(BOARD_LCD_4)
    static uint8_t* draw_buf = nullptr;
    #define DRAW_BUF_ALLOC_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT / 10 * (LV_COLOR_DEPTH / 8))
#else
    #define DRAW_BUF_ALLOC_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT / 10 * (LV_COLOR_DEPTH / 8))
    static uint32_t draw_buf[DRAW_BUF_ALLOC_SIZE / 4];
#endif

static lv_obj_t *scr;
static RadarGauge *radar;
static uint32_t tick = 0;
static uint32_t last_gesture_tick = 0;

/* ---------- connection task on core 0 ---------- */
void conn_task(void *arg) {
    conn_setup();
    while (1) {
        conn_loop();
        delay(100);
    }
}

/* ---------- touch ---------- */
void my_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data) {
    if (!hal().touchAvailable()) {
        data->state = LV_INDEV_STATE_REL;
        return;
    }
    HAL_TouchData td = hal().getTouchData();
    data->state = LV_INDEV_STATE_PR;
    data->point.x = td.x;
    data->point.y = td.y;

    if (tick - last_gesture_tick < 50) return;

    if (td.gestureID == HAL_GESTURE_SWIPE_UP || td.gestureID == HAL_GESTURE_SWIPE_DOWN) {
        radar->cycle_range();
        last_gesture_tick = tick;
    } else if (td.gestureID == HAL_GESTURE_SINGLE_CLICK) {
        radar->select_at(td.x, td.y);
        last_gesture_tick = tick;
    }
}

/* ---------- LVGL tick ---------- */
void on_tick(void *arg) {
    lv_tick_inc(LVGL_TICK_PERIOD_MS);
}

#if LV_USE_LOG != 0
void my_print(const char *buf) {
    Serial.printf(buf);
    Serial.flush();
}
#endif

/* ---------- SHOWCASE: synthetic traffic, no feed needed ---------- */
#ifdef SHOWCASE
struct demo_route {
    uint32_t mmsi;
    const char *name;
    int32_t lat, lon;      // start, microdeg
    float bearing;         // deg
    float speed_kn;
    float length_nm;       // route length (ping-pong)
};
// routes roughly like real gulf traffic (see testdata/ais_29_01_2026.nmea)
static const demo_route DEMO_ROUTES[] = {
    { 247141000, "FERRY",    39207000, 9115000, 170.0f, 14.0f, 8.0f  },
    { 247348700, "CARGO",    39050000, 9050000, 120.0f, 12.0f, 20.0f },
    { 247520250, "SAILBOAT", 39214000, 9200000,  30.0f,  6.0f, 3.0f  },
    { 247415200, "MOORED",   39210500, 9106700,   0.0f,  0.0f, 0.0f  },
};
static const int NUM_DEMO_ROUTES = sizeof(DEMO_ROUTES) / sizeof(DEMO_ROUTES[0]);

void showcase_update() {
    float t = millis() / 1000.0f;
    for (int i = 0; i < NUM_DEMO_ROUTES; i++) {
        const demo_route &r = DEMO_ROUTES[i];
        // ping-pong along the route
        float d = fmodf(t * r.speed_kn / 3600.0f * 60.0f, 2.0f * r.length_nm);  // nm traveled
        float leg = d > r.length_nm ? 2.0f * r.length_nm - d : d;
        float cog = d > r.length_nm ? fmodf(r.bearing + 180.0f, 360.0f) : r.bearing;
        int32_t lat, lon;
        geo_destination(r.lat, r.lon, r.bearing, leg, lat, lon);
        get_ais_manager()->debug_inject(r.mmsi, lat, lon,
                                        (uint16_t)(cog * 10), (uint16_t)(r.speed_kn * 10),
                                        511, r.name);
    }
}

void showcase_timer_cb(lv_timer_t *timer) {
    showcase_update();
}
#endif

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.printf("\n=== Cagliari radar booting (%s %dx%d) ===\n", BOARD_NAME, SCREEN_WIDTH, SCREEN_HEIGHT);
    Serial.printf("PSRAM: %d bytes, free heap: %d\n", ESP.getPsramSize(), ESP.getFreeHeap());

    lv_init();
#if LV_USE_LOG != 0
    lv_log_register_print_cb(my_print);
#endif

    hal().begin();

#if defined(USE_PSRAM_BUFFER) && USE_PSRAM_BUFFER && defined(BOARD_LCD_4)
    draw_buf = (uint8_t*)heap_caps_malloc(DRAW_BUF_ALLOC_SIZE, MALLOC_CAP_SPIRAM);
    if (!draw_buf) draw_buf = (uint8_t*)malloc(SCREEN_WIDTH * SCREEN_HEIGHT / 20 * (LV_COLOR_DEPTH / 8));
    if (draw_buf) memset(draw_buf, 0, DRAW_BUF_ALLOC_SIZE);
#endif

    lv_display_t *disp = hal().initDisplay(draw_buf, DRAW_BUF_ALLOC_SIZE);
    (void)disp;

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, my_touchpad_read);

    // single radar screen
    scr = lv_obj_create(NULL);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    radar = new RadarGauge(scr);
    lv_disp_load_scr(scr);

#ifdef SHOWCASE
    Serial.println("SHOWCASE mode: synthetic AIS targets");
    lv_timer_create(showcase_timer_cb, 500, nullptr);
#endif

    // LVGL tick timer
    const esp_timer_create_args_t tick_args = { .callback = &on_tick, .name = "lvgl_tick" };
    esp_timer_handle_t tick_timer = NULL;
    esp_timer_create(&tick_args, &tick_timer);
    esp_timer_start_periodic(tick_timer, LVGL_TICK_PERIOD_MS * 1000);

    // WiFi + AIS feed on core 0
    xTaskCreatePinnedToCore(conn_task, "conn", 10000, NULL, 0, NULL, 0);

    Serial.println("Setup done");
}

void loop() {
    tick++;
    lv_timer_handler();
    delay(LOOP_DELAY);
}
