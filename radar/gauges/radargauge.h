/*
 * radargauge.h - PPI radar renderer for the Gulf of Cagliari.
 *
 * Layout: house (HOME_LAT/HOME_LON) pinned at bottom center, display rotated
 * so that RADAR_UP_BEARING points to screen-up (the gulf is SSW of the house).
 *
 * Rendering: ONE full-screen RGB565 lv_canvas. The static picture (sea,
 * coastline, rings, house, north arrow) is drawn into bg_buf and copied to
 * bg_static (pristine backup). Blips are drawn directly into bg_buf; when the
 * blip set changes, only the affected rects are restored from bg_static,
 * redrawn and invalidated (lv_obj_invalidate_area) - the panel never sees a
 * full-screen flush except at boot / range change.
 *
 * All drawing writes the canvas buffer directly (no LVGL draw tasks: the 9.3
 * layer API deadlocks when called outside a running dispatch context).
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

#define RADAR_FG_PERIOD_MS 250       // blip layer poll
#define RADAR_MAX_POLY 1200          // coastline ring capacity (static buffers, not stack)
#define RADAR_MAX_RECTS 40           // blip rects tracked for area invalidation

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

        origin_x = w / 2;
        origin_y = h - SCALE_PX(42);
        top_margin = SCALE_PX(30);

        // buffers in PSRAM when available: canvas + pristine static copy
#ifdef ARDUINO
        bg_buf = (uint8_t*)heap_caps_malloc((size_t)w * h * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        bg_static = (uint8_t*)heap_caps_malloc((size_t)w * h * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!bg_buf) bg_buf = (uint8_t*)heap_caps_malloc((size_t)w * h * 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        if (!bg_static) bg_static = (uint8_t*)heap_caps_malloc((size_t)w * h * 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
#endif
        if (!bg_buf) bg_buf = (uint8_t*)malloc((size_t)w * h * 2);
        if (!bg_static) bg_static = (uint8_t*)malloc((size_t)w * h * 2);

        canvas = lv_canvas_create(bg);
        lv_canvas_set_buffer(canvas, bg_buf, w, h, LV_COLOR_FORMAT_RGB565);
        lv_obj_set_size(canvas, w, h);  // 9.3 does not size the obj from the buffer
        lv_obj_set_pos(canvas, 0, 0);
        stride = lv_draw_buf_width_to_stride(w, LV_COLOR_FORMAT_RGB565);

        for (int i = 0; i < 4; i++) {
            ring_label[i] = lv_label_create(bg);
            lv_obj_set_style_text_color(ring_label[i], lv_color_hex(0xa04040), 0);
            lv_obj_set_style_text_font(ring_label[i], ui_scale::font_small(), 0);
        }

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

        // vessel name labels (moving ships only, nearest first); tiny font
        for (int i = 0; i < 6; i++) {
            name_label[i] = lv_label_create(bg);
            lv_obj_set_style_text_font(name_label[i],
#if SCREEN_MIN_DIM <= 280
                &lv_font_montserrat_10,
#else
                &lv_font_montserrat_14,
#endif
                0);
            lv_obj_set_style_text_color(name_label[i], lv_color_hex(0xcfd8d3), 0);
            lv_obj_add_flag(name_label[i], LV_OBJ_FLAG_HIDDEN);
        }

        set_range(2);  // default 20 NM
        memset(bg_buf, 0, (size_t)stride * h);
        draw_static();
        redraw_blips();
        fg_signature = fg_signature_now();
        last_status_ms = 0;

        lv_timer_create(tick_cb, RADAR_FG_PERIOD_MS, this);
    }

    void cycle_range() {
        set_range((range_index + 1) % NUM_RANGES);
        draw_static();          // full repaint (rare: only on user swipe)
        redraw_blips();
        fg_signature = fg_signature_now();
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
        redraw_blips();
        fg_signature = fg_signature_now();
    }

private:
    lv_obj_t *bg;
    lv_obj_t *canvas;
    lv_obj_t *house_label, *status_label, *info_label, *range_label;
    lv_obj_t *ring_label[4];
    lv_obj_t *name_label[6];  // vessel names at closest range
    uint8_t *bg_buf = nullptr;     // canvas buffer (static picture + blips)
    uint8_t *bg_static = nullptr;  // pristine copy of the static picture
    uint32_t stride = 0;
    int w, h;
    int32_t origin_x, origin_y, top_margin;

    static const float RANGES_NM[];
    static const int NUM_RANGES;
    int range_index = 2;
    geo_projection proj;

    uint32_t last_status_ms = 0;
    int selected = -1;
    uint32_t fg_signature = 0;
    lv_area_t prev_rects[RADAR_MAX_RECTS];
    int prev_rect_count = 0;

    // scratch buffers for polygons (static: too big for the loop task stack)
    float poly_fx[RADAR_MAX_POLY], poly_fy[RADAR_MAX_POLY], poly_xs[RADAR_MAX_POLY];

    uint16_t c_sea, c_land, c_coast, c_ring;

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

    void px(int32_t x, int32_t y, uint16_t c) {
        if (x < 0 || y < 0 || x >= w || y >= h) return;
        ((uint16_t*)bg_buf)[y * (stride / 2) + x] = c;
    }

    void rect(int32_t x, int32_t y, int32_t wd, int32_t ht, uint16_t c) {
        if (x < 0) { wd += x; x = 0; }
        if (y < 0) { ht += y; y = 0; }
        if (x + wd > w) wd = w - x;
        if (y + ht > h) ht = h - y;
        if (wd <= 0 || ht <= 0) return;
        uint16_t *row = (uint16_t*)bg_buf + y * (stride / 2) + x;
        for (int32_t j = 0; j < ht; j++) {
            for (int32_t i = 0; i < wd; i++) row[i] = c;
            row += stride / 2;
        }
    }

    void line(float x1, float y1, float x2, float y2, uint16_t c, int width) {
        int32_t ix1 = (int32_t)x1, iy1 = (int32_t)y1, ix2 = (int32_t)x2, iy2 = (int32_t)y2;
        int32_t dx = abs(ix2 - ix1), sx = ix1 < ix2 ? 1 : -1;
        int32_t dy = -abs(iy2 - iy1), sy = iy1 < iy2 ? 1 : -1;
        int32_t err = dx + dy;
        int hw = width / 2;
        while (true) {
            rect(ix1 - hw, iy1 - hw, width, width, c);
            if (ix1 == ix2 && iy1 == iy2) break;
            int32_t e2 = 2 * err;
            if (e2 >= dy) { err += dy; ix1 += sx; }
            if (e2 <= dx) { err += dx; iy1 += sy; }
        }
    }

    void fill_circle(float cx, float cy, float r, uint16_t c) {
        for (int dy = -(int)r; dy <= (int)r; dy++) {
            float span = sqrtf(r * r - dy * dy);
            rect((int32_t)(cx - span), (int32_t)cy + dy, (int32_t)(2 * span) + 1, 1, c);
        }
    }

    void circle(float cx, float cy, float r, uint16_t c, int width) {
        if (r < 2) return;
        const int segs = 72;
        for (int i = 0; i < segs; i++) {
            float a0 = i * 2.0f * (float)M_PI / segs;
            float a1 = (i + 1) * 2.0f * (float)M_PI / segs;
            line(cx + r * sinf(a0), cy - r * cosf(a0),
                 cx + r * sinf(a1), cy - r * cosf(a1), c, width);
        }
    }

    void fill_polygon(const float *fx, const float *fy, int n, uint16_t c) {
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
                if (x1 > x0) rect(x0, y, x1 - x0 + 1, 1, c);
            }
        }
    }

    void draw_coast(const coast_point *poly, int count) {
        if (count > RADAR_MAX_POLY) count = RADAR_MAX_POLY;
        float *fx = poly_fx, *fy = poly_fy;
        for (int i = 0; i < count; i++) {
            geo_project(poly[i].lat, poly[i].lon, proj, fx[i], fy[i]);
            if (fx[i] < -w) fx[i] = -w; else if (fx[i] > 2 * w) fx[i] = 2 * w;
            if (fy[i] < -h) fy[i] = -h; else if (fy[i] > 2 * h) fy[i] = 2 * h;
        }
        fill_polygon(fx, fy, count, c_land);
        for (int i = 0; i < count; i++) {
            int j = (i + 1) % count;
            line(fx[i], fy[i], fx[j], fy[j], c_coast, SCALE_PX(1));
        }
    }

    static void clamp_area(lv_area_t *r, int w, int h) {
        if (r->x1 < 0) r->x1 = 0;
        if (r->y1 < 0) r->y1 = 0;
        if (r->x2 > w - 1) r->x2 = w - 1;
        if (r->y2 > h - 1) r->y2 = h - 1;
    }

    // copy a rect from the pristine static buffer back into the canvas buffer
    void restore_rect(const lv_area_t *r) {
        uint16_t *dst = (uint16_t*)bg_buf + (uint32_t)r->y1 * (stride / 2) + r->x1;
        const uint16_t *src = (const uint16_t*)bg_static + (uint32_t)r->y1 * (stride / 2) + r->x1;
        int32_t wd = r->x2 - r->x1 + 1;
        for (int32_t j = r->y1; j <= r->y2; j++) {
            memcpy(dst, src, (size_t)wd * 2);
            dst += stride / 2;
            src += stride / 2;
        }
    }

    void draw_static() {
        c_sea   = lv_color_to_u16(lv_color_hex(0x0b1d26));
        c_land  = lv_color_to_u16(lv_color_hex(0x33301f));
        c_coast = lv_color_to_u16(lv_color_hex(0x6b7261));
        c_ring  = lv_color_to_u16(lv_color_hex(0x8b1a1a));  // dark red rings

        lv_canvas_fill_bg(canvas, lv_color_hex(0x0b1d26), LV_OPA_COVER);

        for (int r = 0; r < COAST_RING_COUNT; r++)
            draw_coast(COAST_RINGS[r], COAST_RING_LENS[r]);

        float range_nm = RANGES_NM[range_index];
        for (int r = 1; r <= 4; r++) {
            float rr = range_nm * r / 4.0f * proj.px_per_nm;
            circle(origin_x, origin_y, rr, c_ring, SCALE_PX(1));
            char buf[8];
            lv_snprintf(buf, sizeof(buf), "%d", (int)(range_nm * r / 4.0f));
            lv_label_set_text(ring_label[r - 1], buf);
            // center the label exactly on the ring at the screen-up axis
            lv_obj_align(ring_label[r - 1], LV_ALIGN_CENTER,
                         origin_x - w / 2, (int32_t)(origin_y - rr) - h / 2);
        }

        int hs = SCALE_PX(5);
        rect(origin_x - hs / 2, origin_y - hs / 2, hs, hs, lv_color_to_u16(lv_color_hex(0xffb347)));

        // north arrow, top-right corner: filled arrow toward true north
        // (screen-up = RADAR_UP_BEARING; origin is fixed, so a static arrow suffices)
        {
            int32_t nx = w - SCALE_PX(26), ny = SCALE_PX(28);
            float sa = (0.0f - RADAR_UP_BEARING) * (float)M_PI / 180.0f;
            float ca = cosf(sa), sn = sinf(sa);
            float L = SCALE_PX(18), HL = SCALE_PX(6), HW = SCALE_PX(7), SW = SCALE_PX(2);
            float lx[7] = { 0, HW / 2, SW / 2, SW / 2, -SW / 2, -SW / 2, -HW / 2 };
            float ly[7] = { L, L - HL, L - HL, 0, 0, L - HL, L - HL };
            float fx[7], fy[7];
            for (int i = 0; i < 7; i++) {
                fx[i] = nx + lx[i] * ca + ly[i] * sn;
                fy[i] = ny - ly[i] * ca + lx[i] * sn;
            }
            fill_polygon(fx, fy, 7, lv_color_to_u16(lv_color_hex(0xff5050)));
        }

        memcpy(bg_static, bg_buf, (size_t)stride * h);  // pristine copy for blip restore
        lv_obj_invalidate(canvas);
    }

    // lerp between two RGB565 colors (for stale-fade), t in [0,255]
    static uint16_t lerp565(uint16_t a, uint16_t b, uint8_t t) {
        uint32_t ar = (a >> 11) & 31, ag = (a >> 5) & 63, ab = a & 31;
        uint32_t br = (b >> 11) & 31, bg = (b >> 5) & 63, bb = b & 31;
        uint32_t r = ar + (br - ar) * t / 255;
        uint32_t g = ag + (bg - ag) * t / 255;
        uint32_t bl = ab + (bb - ab) * t / 255;
        return (uint16_t)((r << 11) | (g << 5) | bl);
    }

    void redraw_blips() {
        AISManager *ais = get_ais_manager();
        uint32_t now = millis();

        lv_area_t rects[RADAR_MAX_RECTS];
        int nrects = 0;

        // restore previously drawn blip/trail areas from the pristine static layer
        for (int i = 0; i < prev_rect_count; i++) restore_rect(&prev_rects[i]);

        for (int i = 0; i < AIS_MAX_TARGETS; i++) {
            const ais_target *t = ais->get_target(i);
            if (!t->valid) continue;

            float tx, ty;
            geo_project(t->lat, t->lon, proj, tx, ty);
            float dist_nm = geo_distance_nm(HOME_LAT, HOME_LON, t->lat, t->lon);
            bool offscale = dist_nm > RANGES_NM[range_index];

            int m = SCALE_PX(6);
            float cx = tx, cy = ty;
            if (cx < m) cx = m; if (cx > w - m) cx = w - m;
            if (cy < m) cy = m; if (cy > h - m) cy = h - m;

            // fade with age (stale targets dim before the 3 min timeout)
            uint32_t age = now - t->last_update;
            uint8_t fade = age < 30000 ? 255
                         : (uint8_t)(255 - (255 - 76) * (age - 30000) / 150000);
            if (offscale) fade /= 3;

            // closest approach to the house: red if it will pass close soon
            float cpa_nm, tcpa_h;
            geo_cpa(HOME_LAT, HOME_LON, t->lat, t->lon, t->cog / 10.0f, t->sog / 10.0f, cpa_nm, tcpa_h);
            bool alert = (tcpa_h > 0.0f && tcpa_h < 0.25f && cpa_nm < 0.5f);

            uint16_t base = alert ? lv_color_to_u16(lv_color_hex(0xff5050))
                                  : lv_color_to_u16(lv_color_hex(ais_ship_type_color(t->ship_type)));
            uint16_t col = lerp565(c_sea, base, fade);

            // bounding box over trail + blip (for area invalidation)
            float minx = cx, maxx = cx, miny = cy, maxy = cy;

            // trail: fading polyline through the recorded positions, oldest first
            if (t->hist_count > 1) {
                int oldest = (t->hist_head - t->hist_count + AIS_HIST) % AIS_HIST;
                float px_prev = 0, py_prev = 0;
                for (int k = 0; k < t->hist_count; k++) {
                    int idx = (oldest + k) % AIS_HIST;
                    float hx, hy;
                    geo_project(t->hist_lat[idx], t->hist_lon[idx], proj, hx, hy);
                    if (hx < minx) minx = hx; if (hx > maxx) maxx = hx;
                    if (hy < miny) miny = hy; if (hy > maxy) maxy = hy;
                    if (k > 0) {
                        uint8_t seg_fade = (uint8_t)(40 + 160 * k / (t->hist_count - 1));
                        if (offscale) seg_fade /= 3;
                        line(px_prev, py_prev, hx, hy, lerp565(c_sea, col, seg_fade), SCALE_PX(1));
                    }
                    px_prev = hx; py_prev = hy;
                }
            }

            // icon: moving = filled triangle along COG; stationary = tiny filled circle
            bool moving = (t->nav_status != 1 && t->nav_status != 5 && t->sog >= 4);
            if (moving) {
                float deg = (t->cog < 3600 ? t->cog / 10.0f : 0.0f) - RADAR_UP_BEARING;
                float sa = deg * (float)M_PI / 180.0f;
                float ca = cosf(sa), sn = sinf(sa);
                float L = SCALE_PX(7), HW = SCALE_PX(4);
                float lx[3] = { 0, -HW, HW };
                float ly[3] = { L, -L * 0.6f, -L * 0.6f };
                float fx[3], fy[3];
                for (int i = 0; i < 3; i++) {
                    fx[i] = cx + lx[i] * ca + ly[i] * sn;
                    fy[i] = cy - ly[i] * ca + lx[i] * sn;
                    if (fx[i] < minx) minx = fx[i]; if (fx[i] > maxx) maxx = fx[i];
                    if (fy[i] < miny) miny = fy[i]; if (fy[i] > maxy) maxy = fy[i];
                }
                fill_polygon(fx, fy, 3, col);
            } else {
                fill_circle(cx, cy, SCALE_PX(4), col);
            }

            if (i == selected) {
                uint16_t red = lv_color_to_u16(lv_color_hex(0xff5050));
                int ss = SCALE_PX(15);
                int32_t sx = (int32_t)cx - ss / 2, sy = (int32_t)cy - ss / 2;
                rect(sx, sy, ss, SCALE_PX(2), red);
                rect(sx, sy + ss - SCALE_PX(2), ss, SCALE_PX(2), red);
                rect(sx, sy, SCALE_PX(2), ss, red);
                rect(sx + ss - SCALE_PX(2), sy, SCALE_PX(2), ss, red);
            }

            if (nrects < RADAR_MAX_RECTS) {
                lv_area_t r = { (int32_t)minx - SCALE_PX(4), (int32_t)miny - SCALE_PX(4),
                                (int32_t)maxx + SCALE_PX(4), (int32_t)maxy + SCALE_PX(4) };
                clamp_area(&r, w, h);
                if (r.x2 >= r.x1 && r.y2 >= r.y1) rects[nrects++] = r;
            }
        }

        // name labels next to the nearest named MOVING vessels (all ranges)
        int shown = 0;
        {
            const ais_target *near[6];
            float ndist[6];
            for (int i = 0; i < AIS_MAX_TARGETS && shown < 6; i++) {
                const ais_target *t = ais->get_target(i);
                if (!t->valid || !t->name[0]) continue;
                float d = geo_distance_nm(HOME_LAT, HOME_LON, t->lat, t->lon);
                if (d > RANGES_NM[range_index]) continue;
                bool moving = (t->nav_status != 1 && t->nav_status != 5 && t->sog >= 4);
                if (!moving) continue;
                int pos = shown;
                while (pos > 0 && ndist[pos - 1] > d) { ndist[pos] = ndist[pos - 1]; near[pos] = near[pos - 1]; pos--; }
                ndist[pos] = d; near[pos] = t; shown++;
            }
            for (int k = 0; k < shown; k++) {
                float nx, ny;
                geo_project(near[k]->lat, near[k]->lon, proj, nx, ny);
                lv_label_set_text(name_label[k], near[k]->name);
                lv_obj_set_pos(name_label[k], (int32_t)nx + SCALE_PX(10), (int32_t)ny - SCALE_PX(5));
                lv_obj_remove_flag(name_label[k], LV_OBJ_FLAG_HIDDEN);
            }
        }
        for (int k = shown; k < 6; k++) lv_obj_add_flag(name_label[k], LV_OBJ_FLAG_HIDDEN);

        // invalidate only blip/trail areas (old + new), never the whole canvas
        for (int i = 0; i < prev_rect_count; i++) lv_obj_invalidate_area(canvas, &prev_rects[i]);
        for (int i = 0; i < nrects; i++) lv_obj_invalidate_area(canvas, &rects[i]);
        memcpy(prev_rects, rects, nrects * sizeof(lv_area_t));
        prev_rect_count = nrects;
    }

    // cheap signature of the blip layer; 1 s time bucket keeps stale-fade alive
    uint32_t fg_signature_now() {
        AISManager *ais = get_ais_manager();
        uint32_t hsh = (uint32_t)range_index * 2654435761u + (uint32_t)(selected + 1) + millis() / 1000;
        for (int i = 0; i < AIS_MAX_TARGETS; i++) {
            const ais_target *t = ais->get_target(i);
            if (!t->valid) continue;
            hsh ^= (uint32_t)t->mmsi * 31u + (uint32_t)t->lat * 3u + (uint32_t)t->lon * 7u + t->sog + t->cog + t->last_update * 13u;
        }
        return hsh;
    }

    void set_label_text(lv_obj_t *label, const char *txt) {
        if (strcmp(lv_label_get_text(label), txt) != 0) lv_label_set_text(label, txt);
    }

    void update_info() {
        const ais_target *t = selected >= 0 ? get_ais_manager()->get_target(selected) : nullptr;
        if (!t || !t->valid) {
            set_label_text(info_label, "");
            return;
        }
        float dist = geo_distance_nm(HOME_LAT, HOME_LON, t->lat, t->lon);
        float brg = geo_bearing_deg(HOME_LAT, HOME_LON, t->lat, t->lon);
        float cpa_nm, tcpa_h;
        geo_cpa(HOME_LAT, HOME_LON, t->lat, t->lon, t->cog / 10.0f, t->sog / 10.0f, cpa_nm, tcpa_h);
        char buf[160];
        const char *name = t->name[0] ? t->name : "-----";
        // NOTE: lv_snprintf has no float support unless LV_USE_FLOAT is on,
        // so format tenths as integers: "%d.%d" (AIS already stores 0.1 units)
        int sog10 = t->sog;
        int dist10 = (int)(dist * 10.0f + 0.5f);
        int cpa10 = (int)(cpa_nm * 10.0f + 0.5f);
        int tcpa_min = (int)(tcpa_h * 60.0f + 0.5f);
        if (tcpa_h > 0.0f)
            lv_snprintf(buf, sizeof(buf), "%s %lu\n%s - %s\n%d.%dkn %03ddeg %d.%dnm @%03d\nclosest %d.%dnm in %dmin",
                        name, (unsigned long)t->mmsi, ais_ship_type_name(t->ship_type),
                        ais_nav_status_name(t->nav_status),
                        sog10 / 10, sog10 % 10, (int)(t->cog / 10.0f),
                        dist10 / 10, dist10 % 10, (int)brg, cpa10 / 10, cpa10 % 10, tcpa_min);
        else
            lv_snprintf(buf, sizeof(buf), "%s %lu\n%s - %s\n%d.%dkn %03ddeg %d.%dnm @%03d",
                        name, (unsigned long)t->mmsi, ais_ship_type_name(t->ship_type),
                        ais_nav_status_name(t->nav_status),
                        sog10 / 10, sog10 % 10, (int)(t->cog / 10.0f),
                        dist10 / 10, dist10 % 10, (int)brg);
        set_label_text(info_label, buf);
    }

    void update() {
        uint32_t now = millis();
        uint32_t sig = fg_signature_now();
        if (sig != fg_signature) {
            fg_signature = sig;
            redraw_blips();
        }

        if (now - last_status_ms > 500) {
            last_status_ms = now;
            uint8_t count = get_ais_manager()->get_active_count();
            char buf[48];
#ifdef SHOWCASE
            lv_snprintf(buf, sizeof(buf), "AIS %d ships - DEMO", count);
#else
            lv_snprintf(buf, sizeof(buf), "AIS %d ships - %lu msg/m", count,
                        (unsigned long)conn_msgs_per_min());
#endif
            set_label_text(status_label, buf);
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
