/*
 * radargauge.h - PPI radar renderer for the Gulf of Cagliari.
 *
 * Layout: house (HOME_LAT/HOME_LON) pinned at bottom center, display rotated
 * so that RADAR_UP_BEARING points to screen-up (the gulf is SSW of the house).
 * Two stacked LVGL canvases:
 *   - bg canvas (RGB565): sea, coastline silhouette, range rings, spokes,
 *     north marker - redrawn only when the range changes
 *   - fg canvas (ARGB8888, transparent): sweep line + phosphor trail + AIS
 *     blips - redrawn every RADAR_FG_PERIOD_MS
 *
 * IMPORTANT: all drawing is done by writing the canvas buffers directly.
 * The LVGL 9.3 layer API (lv_canvas_init_layer + lv_draw_* + finish_layer)
 * routes through the draw-task dispatcher, which deadlocks when called with
 * many primitives outside a running dispatch context (observed as a hang in
 * setup() -> black screen). Direct buffer writes have no such issue.
 */
#ifndef RADARGAUGE_H
#define RADARGAUGE_H

#include <lvgl.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#ifdef ARDUINO
#include <esp_heap_caps.h>
#endif
#include "../conf.h"
#include "common/scale.h"
#include "common/geo.h"
#include "common/ais.h"
#include "../conn.h"
#include "../coastline.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

#define RADAR_FG_PERIOD_MS 100       // dynamic layer refresh
#define RADAR_SWEEP_DEG_PER_S 90.0f  // one revolution in 4 s
#define RADAR_TRAIL_DEG 60.0f        // phosphor trail length
#define RADAR_TRAIL_SEGS 20
#define RADAR_MAX_POLY 1200  // coastline ring capacity (static buffers, not stack)

class RadarGauge {
public:
    RadarGauge(lv_obj_t *parent) {
        w = SCREEN_WIDTH;
        h = SCREEN_HEIGHT;

        bg = lv_obj_create(parent);
        lv_obj_set_size(bg, w, h);
        lv_obj_set_pos(bg, 0, 0);
        lv_obj_remove_flag(bg, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(bg, lv_color_hex(0x000000), 0);
        lv_obj_set_style_bg_opa(bg, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(bg, 0, 0);
        lv_obj_set_style_pad_all(bg, 0, 0);

        // geometry: house at bottom center
        origin_x = w / 2;
        origin_y = h - SCALE_PX(42);
        top_margin = SCALE_PX(30);

        // canvas buffers: prefer PSRAM (both boards can have it), then internal RAM
#ifdef ARDUINO
        bg_buf = (uint8_t*)heap_caps_malloc(w * h * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        fg_buf = (uint8_t*)heap_caps_malloc(w * h * 4, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!bg_buf) bg_buf = (uint8_t*)heap_caps_malloc(w * h * 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        if (!fg_buf) fg_buf = (uint8_t*)heap_caps_malloc(w * h * 4, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
#endif
        if (!bg_buf) bg_buf = (uint8_t*)malloc(w * h * 2);
        if (!fg_buf) fg_buf = (uint8_t*)malloc(w * h * 4);

        bg_canvas = lv_canvas_create(bg);
        lv_canvas_set_buffer(bg_canvas, bg_buf, w, h, LV_COLOR_FORMAT_RGB565);
        lv_obj_set_size(bg_canvas, w, h);  // 9.3 does not size the obj from the buffer
        lv_obj_set_pos(bg_canvas, 0, 0);
        stride_bg = lv_draw_buf_width_to_stride(w, LV_COLOR_FORMAT_RGB565);

        // ring labels sit between the two canvases
        for (int i = 0; i < 4; i++) {
            ring_label[i] = lv_label_create(bg);
            lv_obj_set_style_text_color(ring_label[i], lv_color_hex(0x4a7a5c), 0);
            lv_obj_set_style_text_font(ring_label[i], ui_scale::font_small(), 0);
        }

        fg_canvas = lv_canvas_create(bg);
        lv_canvas_set_buffer(fg_canvas, fg_buf, w, h, LV_COLOR_FORMAT_ARGB8888);
        lv_obj_set_size(fg_canvas, w, h);  // 9.3 does not size the obj from the buffer
        lv_obj_set_pos(fg_canvas, 0, 0);
        stride_fg = lv_draw_buf_width_to_stride(w, LV_COLOR_FORMAT_ARGB8888);

        // house marker on top of everything
        house_label = lv_label_create(bg);
        lv_label_set_text(house_label, LV_SYMBOL_HOME);
        lv_obj_set_style_text_color(house_label, lv_color_hex(0xffb347), 0);
        lv_obj_set_style_text_font(house_label, ui_scale::font_medium(), 0);
        lv_obj_align(house_label, LV_ALIGN_CENTER, 0, origin_y - h / 2 + SCALE_PX(2));

        status_label = lv_label_create(bg);
        lv_obj_align(status_label, LV_ALIGN_TOP_MID, 0, SCALE_PX(4));
        lv_obj_set_style_text_font(status_label, ui_scale::font_small(), 0);
        lv_obj_set_style_text_color(status_label, lv_color_hex(0x9fb8a8), 0);
        lv_label_set_text(status_label, "AIS --");

        info_label = lv_label_create(bg);
        lv_obj_align(info_label, LV_ALIGN_BOTTOM_LEFT, SCALE_PX(6), -SCALE_PX(6));
        lv_obj_set_width(info_label, SCALE_PX(230));
        lv_label_set_long_mode(info_label, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_font(info_label, ui_scale::font_small(), 0);
        lv_obj_set_style_text_color(info_label, lv_color_hex(0xffd27f), 0);
        lv_label_set_text(info_label, "");

        range_label = lv_label_create(bg);
        lv_obj_align(range_label, LV_ALIGN_BOTTOM_RIGHT, -SCALE_PX(6), -SCALE_PX(6));
        lv_obj_set_style_text_font(range_label, ui_scale::font_small(), 0);
        lv_obj_set_style_text_color(range_label, lv_color_hex(0x9fb8a8), 0);

        // (north arrow is drawn on the bg canvas in draw_static)

        set_range(2);  // default 20 NM
        draw_static();
        last_sweep_ms = millis();
        last_status_ms = 0;

        lv_timer_create(tick_cb, RADAR_FG_PERIOD_MS, this);
    }

    void cycle_range() {
        set_range((range_index + 1) % NUM_RANGES);
        draw_static();
        redraw_fg();
    }

    // tap: select nearest blip to (x, y), or deselect
    void select_at(int32_t x, int32_t y) {
        selected = -1;
        float best = SCALE_PX(22);
        AISManager *ais = get_ais_manager();
        for (int i = 0; i < AIS_MAX_TARGETS; i++) {
            const ais_target *t = ais->get_target(i);
            if (!t->valid) continue;
            float tx, ty;
            geo_project(t->lat, t->lon, proj, tx, ty);
            float d = sqrtf((tx - x) * (tx - x) + (ty - y) * (ty - y));
            if (d < best) { best = d; selected = i; }
        }
        update_info();
        redraw_fg();
    }

private:
    lv_obj_t *bg;
    lv_obj_t *bg_canvas;
    lv_obj_t *fg_canvas;
    lv_obj_t *house_label, *status_label, *info_label, *range_label;
    lv_obj_t *ring_label[4];
    uint8_t *bg_buf = nullptr;
    uint8_t *fg_buf = nullptr;
    uint32_t stride_bg = 0, stride_fg = 0;
    int w, h;
    int32_t origin_x, origin_y, top_margin;

    static const float RANGES_NM[];
    static const int NUM_RANGES;
    int range_index = 2;
    geo_projection proj;

    float sweep_angle = 0;
    uint32_t last_sweep_ms = 0;
    uint32_t last_status_ms = 0;
    int selected = -1;

    // polygon scratch buffers (static: too big for the 8 KB loop task stack)
    float poly_fx[RADAR_MAX_POLY], poly_fy[RADAR_MAX_POLY], poly_xs[RADAR_MAX_POLY];

    // colors: bg as RGB565, fg as RGBA
    uint16_t c_sea, c_land, c_coast, c_ring;
    uint8_t c_sweep[4], c_blip[4], c_blip_b[4], c_alert[4], c_house[4], c_north[4];

    static void hex_to_rgba(uint32_t hex, uint8_t out[4]) {
        out[0] = (hex >> 16) & 0xFF;  // r
        out[1] = (hex >> 8) & 0xFF;   // g
        out[2] = hex & 0xFF;          // b
        out[3] = 255;                 // a
    }

    static void tick_cb(lv_timer_t *timer) {
        RadarGauge *g = static_cast<RadarGauge*>(lv_timer_get_user_data(timer));
        g->update();
    }

    void set_range(int idx) {
        range_index = idx;
        float range_nm = RANGES_NM[idx];
        proj.home_lat = HOME_LAT;
        proj.home_lon = HOME_LON;
        proj.up_bearing_deg = RADAR_UP_BEARING;
        proj.px_per_nm = (float)(origin_y - top_margin) / range_nm;
        proj.origin_x = origin_x;
        proj.origin_y = origin_y;

        char buf[16];
        lv_snprintf(buf, sizeof(buf), "%d NM", (int)range_nm);
        lv_label_set_text(range_label, buf);
    }

    // ---- direct buffer raster helpers (no LVGL draw tasks) ----

    void bg_px(int32_t x, int32_t y, uint16_t c) {
        if (x < 0 || y < 0 || x >= w || y >= h) return;
        ((uint16_t*)bg_buf)[y * (stride_bg / 2) + x] = c;
    }

    // alpha-blended pixel on the ARGB8888 fg canvas (straight alpha "over")
    void fg_px(int32_t x, int32_t y, const uint8_t rgba[4]) {
        if (x < 0 || y < 0 || x >= w || y >= h) return;
        uint32_t *p = (uint32_t*)fg_buf + y * (stride_fg / 4) + x;
        uint32_t d = *p;
        uint32_t da = (d >> 24) & 0xFF;
        uint32_t sa = rgba[3];
        uint32_t oa = sa + (da * (255 - sa)) / 255;
        if (oa == 0) { *p = 0; return; }
        uint32_t inv = 255 - sa;
        uint32_t dr = (d >> 16) & 0xFF, dg = (d >> 8) & 0xFF, db = d & 0xFF;
        uint32_t r = (rgba[0] * sa * 255 + dr * da * inv) / (oa * 255);
        uint32_t g = (rgba[1] * sa * 255 + dg * da * inv) / (oa * 255);
        uint32_t b = (rgba[2] * sa * 255 + db * da * inv) / (oa * 255);
        *p = (oa << 24) | (r << 16) | (g << 8) | b;
    }

    void bg_rect(int32_t x, int32_t y, int32_t wd, int32_t ht, uint16_t c) {
        if (x < 0) { wd += x; x = 0; }
        if (y < 0) { ht += y; y = 0; }
        if (x + wd > w) wd = w - x;
        if (y + ht > h) ht = h - y;
        if (wd <= 0 || ht <= 0) return;
        uint16_t *row = (uint16_t*)bg_buf + y * (stride_bg / 2) + x;
        for (int32_t j = 0; j < ht; j++) {
            for (int32_t i = 0; i < wd; i++) row[i] = c;
            row += stride_bg / 2;
        }
    }

    void fg_rect(int32_t x, int32_t y, int32_t wd, int32_t ht, const uint8_t rgba[4]) {
        for (int32_t j = 0; j < ht; j++)
            for (int32_t i = 0; i < wd; i++)
                fg_px(x + i, y + j, rgba);
    }

    // Bresenham line with square brush of size `width`
    void bg_line(float x1, float y1, float x2, float y2, uint16_t c, int width) {
        int32_t ix1 = (int32_t)x1, iy1 = (int32_t)y1, ix2 = (int32_t)x2, iy2 = (int32_t)y2;
        int32_t dx = abs(ix2 - ix1), sx = ix1 < ix2 ? 1 : -1;
        int32_t dy = -abs(iy2 - iy1), sy = iy1 < iy2 ? 1 : -1;
        int32_t err = dx + dy;
        int hw = width / 2;
        while (true) {
            bg_rect(ix1 - hw, iy1 - hw, width, width, c);
            if (ix1 == ix2 && iy1 == iy2) break;
            int32_t e2 = 2 * err;
            if (e2 >= dy) { err += dy; ix1 += sx; }
            if (e2 <= dx) { err += dx; iy1 += sy; }
        }
    }

    void fg_line(float x1, float y1, float x2, float y2, const uint8_t rgba[4], int width) {
        int32_t ix1 = (int32_t)x1, iy1 = (int32_t)y1, ix2 = (int32_t)x2, iy2 = (int32_t)y2;
        int32_t dx = abs(ix2 - ix1), sx = ix1 < ix2 ? 1 : -1;
        int32_t dy = -abs(iy2 - iy1), sy = iy1 < iy2 ? 1 : -1;
        int32_t err = dx + dy;
        int hw = width / 2;
        while (true) {
            fg_rect(ix1 - hw, iy1 - hw, width, width, rgba);
            if (ix1 == ix2 && iy1 == iy2) break;
            int32_t e2 = 2 * err;
            if (e2 >= dy) { err += dy; ix1 += sx; }
            if (e2 <= dx) { err += dx; iy1 += sy; }
        }
    }

    void bg_circle(float cx, float cy, float r, uint16_t c, int width) {
        if (r < 2) return;
        const int segs = 72;
        for (int i = 0; i < segs; i++) {
            float a0 = i * 2.0f * (float)M_PI / segs;
            float a1 = (i + 1) * 2.0f * (float)M_PI / segs;
            bg_line(cx + r * sinf(a0), cy - r * cosf(a0),
                    cx + r * sinf(a1), cy - r * cosf(a1), c, width);
        }
    }

    // scanline fill of a float polygon into the bg buffer
    void bg_fill_polygon(const float *fx, const float *fy, int n, uint16_t c) {
        if (n < 3) return;
        float ymin = fy[0], ymax = fy[0];
        for (int i = 1; i < n; i++) {
            if (fy[i] < ymin) ymin = fy[i];
            if (fy[i] > ymax) ymax = fy[i];
        }
        float *xs = poly_xs;
        for (int y = (int)ymin; y <= (int)ymax; y++) {
            int cnt = 0;
            for (int i = 0; i < n; i++) {
                float ay = fy[i], by = fy[(i + 1) % n];
                float ax = fx[i], bx = fx[(i + 1) % n];
                if ((ay <= y && by > y) || (by <= y && ay > y)) {
                    float t = (y - ay) / (by - ay);
                    if (cnt < RADAR_MAX_POLY) xs[cnt++] = ax + t * (bx - ax);
                }
            }
            for (int i = 0; i < cnt; i++)
                for (int j = i + 1; j < cnt; j++)
                    if (xs[j] < xs[i]) { float tmp = xs[i]; xs[i] = xs[j]; xs[j] = tmp; }
            for (int i = 0; i + 1 < cnt; i += 2) {
                int x0 = (int)xs[i], x1 = (int)xs[i + 1];
                if (x1 > x0) bg_rect(x0, y, x1 - x0 + 1, 1, c);
            }
        }
    }

    void draw_coast(const coast_point *poly, int count) {
        if (count > RADAR_MAX_POLY) count = RADAR_MAX_POLY;
        float *fx = poly_fx, *fy = poly_fy;
        for (int i = 0; i < count; i++) {
            geo_project(poly[i].lat, poly[i].lon, proj, fx[i], fy[i]);
            // clamp far off-screen points so the scanline fill stays cheap at
            // short ranges (bg_rect clips to the buffer anyway)
            if (fx[i] < -w) fx[i] = -w; else if (fx[i] > 2 * w) fx[i] = 2 * w;
            if (fy[i] < -h) fy[i] = -h; else if (fy[i] > 2 * h) fy[i] = 2 * h;
        }
        bg_fill_polygon(fx, fy, count, c_land);
        for (int i = 0; i < count; i++) {
            int j = (i + 1) % count;
            bg_line(fx[i], fy[i], fx[j], fy[j], c_coast, SCALE_PX(1));
        }
    }

    void draw_static() {
        c_sea   = lv_color_to_u16(lv_color_hex(0x0b1d26));
        c_land  = lv_color_to_u16(lv_color_hex(0x33301f));
        c_coast = lv_color_to_u16(lv_color_hex(0x6b7261));
        c_ring  = lv_color_to_u16(lv_color_hex(0x1f5c38));
        hex_to_rgba(0xffb347, c_house);
        hex_to_rgba(0xd06060, c_north);

        lv_canvas_fill_bg(bg_canvas, lv_color_hex(0x0b1d26), LV_OPA_COVER);

        // coastline silhouette (all rings: mainland + islands)
        for (int r = 0; r < COAST_RING_COUNT; r++)
            draw_coast(COAST_RINGS[r], COAST_RING_LENS[r]);

        // range rings centered on the house
        float range_nm = RANGES_NM[range_index];
        for (int r = 1; r <= 4; r++) {
            float rr = range_nm * r / 4.0f * proj.px_per_nm;
            bg_circle(origin_x, origin_y, rr, c_ring, SCALE_PX(1));
            char buf[8];
            lv_snprintf(buf, sizeof(buf), "%d", (int)(range_nm * r / 4.0f));
            lv_label_set_text(ring_label[r - 1], buf);
            lv_obj_set_pos(ring_label[r - 1], origin_x + SCALE_PX(4), origin_y - (int32_t)rr - SCALE_PX(7));
        }

        // spokes every 30 deg from the house
        float max_r = sqrtf((float)(w * w + h * h));
        for (int a = 0; a < 360; a += 30) {
            float rad = a * (float)M_PI / 180.0f;
            bg_line(origin_x, origin_y,
                    origin_x + max_r * sinf(rad), origin_y - max_r * cosf(rad),
                    c_ring, SCALE_PX(1));
        }

        // house dot
        int hs = SCALE_PX(5);
        bg_rect(origin_x - hs / 2, origin_y - hs / 2, hs, hs, lv_color_to_u16(lv_color_hex(0xffb347)));

        // north arrow, top-right corner: a single filled arrow pointing toward
        // true north. Screen-up = RADAR_UP_BEARING, so north is at screen angle
        // (0 - UP). The origin (house) is fixed, so a static arrow is enough.
        {
            int32_t nx = w - SCALE_PX(26), ny = SCALE_PX(28);
            float sa = (0.0f - RADAR_UP_BEARING) * (float)M_PI / 180.0f;
            float ca = cosf(sa), sn = sinf(sa);
            // local arrow shape pointing "up" (y+), tapered shaft + solid head
            float L = SCALE_PX(18), HL = SCALE_PX(6), HW = SCALE_PX(7), SW = SCALE_PX(2);
            float lx[7] = { 0, HW / 2, SW / 2, SW / 2, -SW / 2, -SW / 2, -HW / 2 };
            float ly[7] = { L, L - HL, L - HL, 0, 0, L - HL, L - HL };
            float fx[7], fy[7];
            for (int i = 0; i < 7; i++) {
                fx[i] = nx + lx[i] * ca + ly[i] * sn;
                fy[i] = ny - ly[i] * ca + lx[i] * sn;
            }
            bg_fill_polygon(fx, fy, 7, lv_color_to_u16(lv_color_hex(0xff5050)));
        }

        lv_obj_invalidate(bg_canvas);
    }

    void redraw_fg() {
        hex_to_rgba(0x39ff6a, c_sweep);
        hex_to_rgba(0xffb347, c_blip);
        hex_to_rgba(0x6ac8ff, c_blip_b);
        hex_to_rgba(0xff5050, c_alert);

        memset(fg_buf, 0, (size_t)stride_fg * h);  // all transparent

        float max_r = sqrtf((float)(w * w + h * h));

        // phosphor trail: fading segments behind the sweep line
        for (int k = RADAR_TRAIL_SEGS; k >= 1; k--) {
            float a = (sweep_angle - RADAR_TRAIL_DEG * k / RADAR_TRAIL_SEGS) * (float)M_PI / 180.0f;
            uint8_t rgba[4] = { c_sweep[0], c_sweep[1], c_sweep[2],
                                (uint8_t)(200 * (RADAR_TRAIL_SEGS - k) / RADAR_TRAIL_SEGS) };
            fg_line(origin_x, origin_y,
                    origin_x + max_r * sinf(a), origin_y - max_r * cosf(a),
                    rgba, SCALE_PX(2));
        }

        // bright sweep line
        fg_line(origin_x, origin_y,
                origin_x + max_r * sinf(sweep_angle * (float)M_PI / 180.0f),
                origin_y - max_r * cosf(sweep_angle * (float)M_PI / 180.0f),
                c_sweep, SCALE_PX(3));

        // AIS blips
        AISManager *ais = get_ais_manager();
        uint32_t now = millis();
        for (int i = 0; i < AIS_MAX_TARGETS; i++) {
            const ais_target *t = ais->get_target(i);
            if (!t->valid) continue;

            float tx, ty;
            geo_project(t->lat, t->lon, proj, tx, ty);
            float dist_nm = geo_distance_nm(HOME_LAT, HOME_LON, t->lat, t->lon);
            bool offscale = dist_nm > RANGES_NM[range_index];

            // clamp into the visible area
            int m = SCALE_PX(6);
            float cx = tx, cy = ty;
            if (cx < m) cx = m; if (cx > w - m) cx = w - m;
            if (cy < m) cy = m; if (cy > h - m) cy = h - m;

            // fade with age (stale targets dim out before the 3 min timeout)
            uint32_t age = now - t->last_update;
            uint8_t opa = age < 30000 ? 255
                        : (uint8_t)(255 - (255 - 76) * (age - 30000) / 150000);
            if (offscale) opa /= 3;

            // CPA/TCPA: highlight targets that will pass close to the house soon
            float cpa_nm, tcpa_h;
            geo_cpa(HOME_LAT, HOME_LON, t->lat, t->lon, t->cog / 10.0f, t->sog / 10.0f, cpa_nm, tcpa_h);
            bool alert = (tcpa_h > 0.0f && tcpa_h < 0.25f && cpa_nm < 0.5f);

            const uint8_t *base = alert ? c_alert : (t->class_b ? c_blip_b : c_blip);
            uint8_t rgba[4] = { base[0], base[1], base[2], opa };
            int bs = SCALE_PX(7);
            fg_rect((int32_t)cx - bs / 2, (int32_t)cy - bs / 2, bs, bs, rgba);

            // COG vector
            if (t->cog < 3600) {
                float crad = (t->cog / 10.0f - RADAR_UP_BEARING) * (float)M_PI / 180.0f;
                fg_line(cx, cy, cx + SCALE_PX(12) * sinf(crad), cy - SCALE_PX(12) * cosf(crad),
                        rgba, SCALE_PX(2));
            }

            // selection box
            if (i == selected) {
                uint8_t red[4] = { 0xff, 0x50, 0x50, 255 };
                int ss = SCALE_PX(15);
                int32_t sx = (int32_t)cx - ss / 2, sy = (int32_t)cy - ss / 2;
                fg_rect(sx, sy, ss, SCALE_PX(2), red);
                fg_rect(sx, sy + ss - SCALE_PX(2), ss, SCALE_PX(2), red);
                fg_rect(sx, sy, SCALE_PX(2), ss, red);
                fg_rect(sx + ss - SCALE_PX(2), sy, SCALE_PX(2), ss, red);
            }
        }

        lv_obj_invalidate(fg_canvas);
    }

    void update_info() {
        const ais_target *t = selected >= 0 ? get_ais_manager()->get_target(selected) : nullptr;
        if (!t || !t->valid) {
            lv_label_set_text(info_label, "");
            return;
        }
        float dist = geo_distance_nm(HOME_LAT, HOME_LON, t->lat, t->lon);
        float brg = geo_bearing_deg(HOME_LAT, HOME_LON, t->lat, t->lon);
        float cpa_nm, tcpa_h;
        geo_cpa(HOME_LAT, HOME_LON, t->lat, t->lon, t->cog / 10.0f, t->sog / 10.0f, cpa_nm, tcpa_h);
        char buf[96];
        const char *name = t->name[0] ? t->name : "-----";
        if (tcpa_h > 0.0f)
            lv_snprintf(buf, sizeof(buf), "%s %lu\n%.1fkn %03ddeg %.1fnm @%03d\nCPA %.1fnm in %dmin",
                        name, (unsigned long)t->mmsi, t->sog / 10.0f, (int)(t->cog / 10.0f), dist, (int)brg,
                        cpa_nm, (int)(tcpa_h * 60.0f));
        else
            lv_snprintf(buf, sizeof(buf), "%s %lu\n%.1fkn %03ddeg %.1fnm @%03d",
                        name, (unsigned long)t->mmsi, t->sog / 10.0f, (int)(t->cog / 10.0f), dist, (int)brg);
        lv_label_set_text(info_label, buf);
    }

    void update() {
        uint32_t now = millis();
        float dt = (float)(now - last_sweep_ms);
        last_sweep_ms = now;
        sweep_angle = fmodf(sweep_angle + dt * RADAR_SWEEP_DEG_PER_S / 1000.0f, 360.0f);

        redraw_fg();

        if (now - last_status_ms > 500) {
            last_status_ms = now;
            uint8_t count = get_ais_manager()->get_active_count();
            char buf[48];
            lv_snprintf(buf, sizeof(buf), "AIS %d tg%s  %s", count, count == 1 ? "" : "ts",
#ifdef SHOWCASE
                        "DEMO"
#else
                        conn_feed_connected() ? "FEED OK" : "no feed"
#endif
                        );
            lv_label_set_text(status_label, buf);
            lv_obj_set_style_text_color(status_label,
#ifdef SHOWCASE
                lv_color_hex(0xffd27f)
#else
                conn_feed_connected() ? lv_color_hex(0x6fd08c) : lv_color_hex(0xd06060)
#endif
                , 0);
            update_info();
        }
    }
};

const float RadarGauge::RANGES_NM[] = { 5.0f, 10.0f, 20.0f };
const int RadarGauge::NUM_RANGES = 3;

#endif // RADARGAUGE_H
