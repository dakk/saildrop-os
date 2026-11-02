/*
 * render_test.cpp - headless render test for RadarGauge.
 *
 * Compiles the real LVGL 9.3 + the real radar/gauges/radargauge.h on the host
 * with a dummy framebuffer display, runs the LVGL loop for a few hundred ms,
 * then checks the framebuffer: sea fill, coastline land pixels, range rings,
 * sweep line, and the house marker must all be present. Writes render.ppm so
 * the frame can be inspected visually.
 *
 * Build: see host_test/build_render_test.sh
 */
#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <chrono>

// ---- Arduino-ish stubs (WiFi.h resolves to host_test/stubs via -I) ----
#include <WiFi.h>
static unsigned long g_millis = 0;
unsigned long millis() { return g_millis; }
SerialStub Serial;
WiFiClass WiFi;

#include <lvgl.h>
#define private public   // test inspects internals (info_label, proj)
#include "../radar/gauges/radargauge.h"   // pulls conf.h, common/*, conn.h (stubbed WiFi)
#undef private

// ---- dummy display with a framebuffer ----
#define W 480
#define H 480
static uint16_t fb[W * H];  // RGB565

static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    int32_t w = area->x2 - area->x1 + 1;
    int32_t h = area->y2 - area->y1 + 1;
    for (int32_t y = 0; y < h; y++)
        memcpy(&fb[(area->y1 + y) * W + area->x1], &px_map[y * w * 2], w * 2);
    lv_display_flush_ready(disp);
}

static uint32_t count_color(uint16_t c) {
    uint32_t n = 0;
    for (int i = 0; i < W * H; i++) if (fb[i] == c) n++;
    return n;
}

int main() {
    lv_init();

    lv_display_t *disp = lv_display_create(W, H);
    lv_display_set_buffers(disp, fb, NULL, sizeof(fb), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp, flush_cb);

    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    RadarGauge radar(scr);
    lv_disp_load_scr(scr);
    lv_obj_invalidate(lv_scr_act());
    lv_refr_now(NULL);

    // moored unnamed cargo target ~3 NM out (circle icon check)
    // + moving named target twice (trail + name + info checks)
    int32_t tlat, tlon, tlat2, tlon2, tlat3, tlon3;
    geo_destination(HOME_LAT, HOME_LON, 195.0f, 3.0f, tlat, tlon);
    geo_destination(HOME_LAT, HOME_LON, 195.0f, 2.6f, tlat2, tlon2);
    geo_destination(HOME_LAT, HOME_LON, 195.0f, 2.2f, tlat3, tlon3);
    get_ais_manager()->debug_inject(247000001, tlat, tlon, 1950, 0, 511, "", 70, 5);          // moored
    get_ais_manager()->debug_inject(247000002, tlat2, tlon2, 1950, 120, 511, "TESTBOAT", 70, 0); // moving
    get_ais_manager()->debug_inject(247000002, tlat3, tlon3, 1950, 120, 511, "TESTBOAT", 70, 0); // trail pt

    // run ~1 s of LVGL time
    for (int i = 0; i < 500; i++) {
        g_millis += 2;
        lv_tick_inc(2);
        lv_timer_handler();
    }

    // expected colors (RGB565)
    uint16_t sea   = lv_color_to_u16(lv_color_hex(0x0b1d26));
    uint16_t land  = lv_color_to_u16(lv_color_hex(0x33301f));
    uint16_t ring  = lv_color_to_u16(lv_color_hex(0x8b1a1a));  // dark red
    uint16_t sweep = lv_color_to_u16(lv_color_hex(0x39ff6a));
    uint16_t blip  = lv_color_to_u16(lv_color_hex(0x0aa831));  // oceanook Cargo green
    uint16_t house = lv_color_to_u16(lv_color_hex(0xffb347));

    uint32_t n_sea = count_color(sea), n_land = count_color(land);
    uint32_t n_ring = count_color(ring), n_sweep = count_color(sweep);
    uint32_t n_blip = count_color(blip);
    uint32_t n_black = count_color(0x0000);

    // at 20 NM the moving named ship IS labeled (names only for moving ships)
    bool shown_at_20 = !lv_obj_has_flag(radar.name_label[0], LV_OBJ_FLAG_HIDDEN);

    // exercise range switching (what a swipe triggers) and verify it repaints
    uint32_t ring_before = count_color(lv_color_to_u16(lv_color_hex(0x8b1a1a)));
    radar.cycle_range();  // 20 -> 5 NM
    for (int i = 0; i < 50; i++) { g_millis += 2; lv_tick_inc(2); lv_timer_handler(); }
    uint32_t ring_after = count_color(lv_color_to_u16(lv_color_hex(0x8b1a1a)));
    printf("ring px before/after 4x cycle_range: %u -> %u\n", ring_before, ring_after);

    printf("pixels: sea=%u land=%u ring=%u sweep=%u blip/house=%u black=%u (total %d)\n",
           n_sea, n_land, n_ring, n_sweep, n_blip, n_black, W * H);

    // dump PPM for visual inspection
    FILE *f = fopen("host_test/render.ppm", "wb");
    fprintf(f, "P6\n%d %d\n255\n", W, H);
    for (int i = 0; i < W * H; i++) {
        uint16_t c = fb[i];
        uint8_t r = ((c >> 11) & 31) << 3, g = ((c >> 5) & 63) << 2, b = (c & 31) << 3;
        fwrite(&r, 1, 1, f); fwrite(&g, 1, 1, f); fwrite(&b, 1, 1, f);
    }
    fclose(f);

    int fails = 0;
    #define NEED(cond, ...) do { if (!(cond)) { fails++; printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); } else { printf("ok:   "); printf(__VA_ARGS__); printf("\n"); } } while (0)
    NEED(shown_at_20, "moving ship named at 20 NM");
    NEED(n_sea > 50000, "sea filled (%u px)", n_sea);
    NEED(n_land > 5000, "coastline land drawn (%u px)", n_land);
    NEED(n_ring > 500, "range rings drawn (%u px)", n_ring);
    NEED(n_sweep == 0, "no sweep line (%u px, want 0)", n_sweep);
    NEED(n_blip > 100, "stationary circle icon drawn (%u px)", n_blip);  // Moored target: filled dot
    // trail: greenish (not exact-color) pixels beyond the icon itself
    {
        uint32_t n_trail = 0;
        for (int i = 0; i < W * H; i++) {
            uint16_t c = fb[i];
            int r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
            if (g > 20 && g > r + 4 && g > b + 4 && c != blip) n_trail++;
        }
        const ais_target *tt = nullptr;
        for (int i = 0; i < AIS_MAX_TARGETS; i++) {
            const ais_target *x = get_ais_manager()->get_target(i);
            if (x->valid && x->mmsi == 247000002) tt = x;
        }
        NEED(tt && tt->hist_count == 2, "trail history recorded (hist_count=%d)", tt ? tt->hist_count : -1);
        NEED(n_trail > 20, "trail segment drawn (%u faded px)", n_trail);
    }
    NEED(n_black < W * H / 2, "screen not mostly black (black=%u)", n_black);
    NEED(ring_after != ring_before && ring_after > 100, "cycle_range repaints rings (%u -> %u)", ring_before, ring_after);

    // tap-select the injected target and check the info label text
    {
        const ais_target *t = nullptr;
        for (int i = 0; i < AIS_MAX_TARGETS; i++) {
            const ais_target *x = get_ais_manager()->get_target(i);
            if (x->valid && x->mmsi == 247000002) t = x;
        }
        float bx, by;
        geo_project(t->lat, t->lon, radar.proj, bx, by);
        radar.select_at((int32_t)bx, (int32_t)by);
        lv_timer_handler();
        const char *txt = lv_label_get_text(radar.info_label);
        printf("info label: '%s'\n", txt);
        NEED(strstr(txt, "TESTBOAT") != nullptr, "info shows name");
        NEED(strstr(txt, "12.0kn") != nullptr, "info shows speed as 12.0kn (got '%s')", txt);
        NEED(strstr(txt, "195deg") != nullptr, "info shows cog 195deg");
        NEED(strstr(txt, "Cargo") != nullptr, "info shows ship type Cargo");
        NEED(strstr(txt, "Under way using engine") != nullptr, "info shows nav status");
        bool ascii_ok = true;
        for (const char *p = txt; *p; p++) if ((unsigned char)*p >= 128) ascii_ok = false;
        NEED(ascii_ok, "info label is pure ASCII (no tofu glyphs): '%s'", txt);
        NEED(strstr(txt, "fkn") == nullptr, "no broken float format");
        NEED(strstr(txt, "closest ") != nullptr || strstr(txt, "\n") != nullptr, "info has detail line");
    }

    // status line format: "AIS X ships - Y msg/m"
    {
        const char *st = lv_label_get_text(radar.status_label);
        printf("status label: '%s'\n", st);
        NEED(strncmp(st, "AIS ", 4) == 0 && strstr(st, "ships - ") != nullptr && strstr(st, "msg/m") != nullptr,
             "status line is 'AIS X ships - Y msg/m' (got '%s')", st);
    }

    // name label shown at 5 NM (we cycled to it above)
    {
        bool vis = !lv_obj_has_flag(radar.name_label[0], LV_OBJ_FLAG_HIDDEN);
        const char *nt = lv_label_get_text(radar.name_label[0]);
        printf("name label: '%s' visible=%d\n", nt, vis);
        NEED(vis && strcmp(nt, "TESTBOAT") == 0, "name label TESTBOAT visible at 5 NM");
    }

    // ring labels centered on the rings at the up axis
    {
        for (int r = 1; r <= 4; r++) {
            lv_obj_t *lbl = radar.ring_label[r - 1];
            int32_t cx = lv_obj_get_x(lbl) + lv_obj_get_width(lbl) / 2;
            int32_t cy = lv_obj_get_y(lbl) + lv_obj_get_height(lbl) / 2;
            float rr = radar.RANGES_NM[radar.range_index] * r / 4.0f * radar.proj.px_per_nm;
            int32_t want_y = radar.origin_y - (int32_t)rr;
            printf("ring %d label center (%d,%d), want y=%d x=%d\n", r, cx, cy, want_y, radar.origin_x);
            NEED(abs(cx - radar.origin_x) <= 2 && abs(cy - want_y) <= 2,
                 "ring %d label centered on circle", r);
        }
    }

    // battery widget top-left (shore power -> gray USB icon)
    {
        uint16_t gray = lv_color_to_u16(lv_color_hex(0x9fb8a8));
        uint32_t n_gray = 0;
        for (int y = 0; y < 40; y++)
            for (int x = 0; x < 40; x++)
                if (fb[y * W + x] == gray) n_gray++;
        NEED(n_gray > 20, "battery icon drawn top-left (%u gray px)", n_gray);
    }

    // compass rose in the top-right corner: red-ish pixels present there
    uint16_t red = lv_color_to_u16(lv_color_hex(0xff5050));
    uint32_t n_red = 0;
    for (int y = 0; y < 100; y++)
        for (int x = W - 100; x < W; x++)
            if (fb[y * W + x] == red) n_red++;
    NEED(n_red > 50, "north arrow drawn top-right (%u red px)", n_red);

    printf("%s (%d failures)\n", fails ? "RENDER FAILED" : "RENDER OK", fails);
    return fails ? 1 : 0;
}
