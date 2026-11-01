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

#define RADAR_FG_PERIOD_MS 100     // dynamic layer refresh
#define RADAR_SWEEP_DEG_PER_S 90.0f  // one revolution in 4 s
#define RADAR_TRAIL_DEG 60.0f      // phosphor trail length
#define RADAR_TRAIL_SEGS 20

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
        lv_obj_set_pos(bg_canvas, 0, 0);

        // ring labels sit between the two canvases
        for (int i = 0; i < 4; i++) {
            ring_label[i] = lv_label_create(bg);
            lv_obj_set_style_text_color(ring_label[i], lv_color_hex(0x4a7a5c), 0);
            lv_obj_set_style_text_font(ring_label[i], ui_scale::font_small(), 0);
        }

        fg_canvas = lv_canvas_create(bg);
        lv_canvas_set_buffer(fg_canvas, fg_buf, w, h, LV_COLOR_FORMAT_ARGB8888);
        lv_obj_set_pos(fg_canvas, 0, 0);

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
        lv_obj_set_style_text_font(info_label, ui_scale::font_small(), 0);
        lv_obj_set_style_text_color(info_label, lv_color_hex(0xffd27f), 0);
        lv_label_set_text(info_label, "");

        range_label = lv_label_create(bg);
        lv_obj_align(range_label, LV_ALIGN_BOTTOM_RIGHT, -SCALE_PX(6), -SCALE_PX(6));
        lv_obj_set_style_text_font(range_label, ui_scale::font_small(), 0);
        lv_obj_set_style_text_color(range_label, lv_color_hex(0x9fb8a8), 0);

        north_label = lv_label_create(bg);
        lv_obj_set_style_text_font(north_label, ui_scale::font_small(), 0);
        lv_obj_set_style_text_color(north_label, lv_color_hex(0xd06060), 0);
        lv_label_set_text(north_label, "N");

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
    lv_obj_t *house_label, *status_label, *info_label, *range_label, *north_label;
    lv_obj_t *ring_label[4];
    uint8_t *bg_buf = nullptr;
    uint8_t *fg_buf = nullptr;
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

    static lv_color_t col_sea()    { return lv_color_hex(0x0b1d26); }
    static lv_color_t col_land()   { return lv_color_hex(0x33301f); }
    static lv_color_t col_coast()  { return lv_color_hex(0x6b7261); }
    static lv_color_t col_ring()   { return lv_color_hex(0x1f5c38); }
    static lv_color_t col_sweep()  { return lv_color_hex(0x39ff6a); }
    static lv_color_t col_blip()   { return lv_color_hex(0xffb347); }
    static lv_color_t col_blip_b() { return lv_color_hex(0x6ac8ff); }

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

    // project a coastline point to screen px
    void project_pt(const coast_point &p, float &x, float &y) {
        geo_project(p.lat, p.lon, proj, x, y);
    }

    // scanline fill of a screen-space polygon on the bg canvas
    void fill_polygon(lv_point_precise_t *pts, int n, lv_color_t color) {
        if (n < 3) return;
        float ymin = pts[0].y, ymax = pts[0].y;
        for (int i = 1; i < n; i++) {
            if (pts[i].y < ymin) ymin = pts[i].y;
            if (pts[i].y > ymax) ymax = pts[i].y;
        }
        lv_draw_rect_dsc_t dsc;
        lv_draw_rect_dsc_init(&dsc);
        dsc.bg_color = color;
        dsc.bg_opa = LV_OPA_COVER;
        dsc.radius = 0;
        dsc.border_width = 0;

        float xs[64];
        for (int y = (int)ymin; y <= (int)ymax; y++) {
            int cnt = 0;
            for (int i = 0; i < n; i++) {
                lv_point_precise_t a = pts[i];
                lv_point_precise_t b = pts[(i + 1) % n];
                if ((a.y <= y && b.y > y) || (b.y <= y && a.y > y)) {
                    float t = (y - a.y) / (b.y - a.y);
                    if (cnt < 64) xs[cnt++] = a.x + t * (b.x - a.x);
                }
            }
            // sort intersections
            for (int i = 0; i < cnt; i++)
                for (int j = i + 1; j < cnt; j++)
                    if (xs[j] < xs[i]) { float tmp = xs[i]; xs[i] = xs[j]; xs[j] = tmp; }
            for (int i = 0; i + 1 < cnt; i += 2) {
                int x0 = (int)xs[i], x1 = (int)xs[i + 1];
                if (x1 > x0) lv_canvas_draw_rect(bg_canvas, x0, y, x1 - x0 + 1, 1, &dsc);
            }
        }
    }

    void draw_polygon_outline(lv_point_precise_t *pts, int n, lv_color_t color, int width) {
        lv_point_precise_t closed[65];
        if (n > 64) n = 64;
        memcpy(closed, pts, n * sizeof(lv_point_precise_t));
        closed[n] = closed[0];
        lv_draw_line_dsc_t dsc;
        lv_draw_line_dsc_init(&dsc);
        dsc.color = color;
        dsc.width = width;
        dsc.opa = LV_OPA_COVER;
        lv_canvas_draw_line(bg_canvas, closed, n + 1, &dsc);
    }

    void draw_coast(const coast_point *poly, int count) {
        lv_point_precise_t pts[64];
        if (count > 64) count = 64;
        for (int i = 0; i < count; i++)
            project_pt(poly[i], pts[i].x, pts[i].y);
        fill_polygon(pts, count, col_land());
        draw_polygon_outline(pts, count, col_coast(), SCALE_PX(1));
    }

    void draw_circle(int32_t cx, int32_t cy, float r, lv_color_t color, int width) {
        if (r < 2) return;
        lv_point_precise_t pts[73];
        for (int i = 0; i < 72; i++) {
            float a = i * 2.0f * (float)M_PI / 72.0f;
            pts[i].x = cx + r * sinf(a);
            pts[i].y = cy - r * cosf(a);
        }
        pts[72] = pts[0];
        lv_draw_line_dsc_t dsc;
        lv_draw_line_dsc_init(&dsc);
        dsc.color = color;
        dsc.width = width;
        dsc.opa = LV_OPA_COVER;
        lv_canvas_draw_line(bg_canvas, pts, 73, &dsc);
    }

    void draw_static() {
        lv_canvas_fill_bg(bg_canvas, col_sea(), LV_OPA_COVER);

        // coastline silhouette
        draw_coast(COAST_MAIN, COAST_MAIN_COUNT);
        draw_coast(COAST_SANTANTIOCO, COAST_SANTANTIOCO_COUNT);
        draw_coast(COAST_SANPIETRO, COAST_SANPIETRO_COUNT);

        // range rings centered on the house
        float range_nm = RANGES_NM[range_index];
        for (int r = 1; r <= 4; r++) {
            float rr = range_nm * r / 4.0f * proj.px_per_nm;
            draw_circle(origin_x, origin_y, rr, col_ring(), SCALE_PX(1));
            // label on the screen-up axis
            lv_label_set_text(ring_label[r - 1], "");
            char buf[8];
            lv_snprintf(buf, sizeof(buf), "%d", (int)(range_nm * r / 4.0f));
            lv_label_set_text(ring_label[r - 1], buf);
            lv_obj_set_pos(ring_label[r - 1], origin_x + SCALE_PX(4), origin_y - (int32_t)rr - SCALE_PX(7));
        }

        // spokes every 30 deg from the house
        float max_r = sqrtf((float)(w * w + h * h));
        lv_draw_line_dsc_t dsc;
        lv_draw_line_dsc_init(&dsc);
        dsc.color = col_ring();
        dsc.width = SCALE_PX(1);
        dsc.opa = LV_OPA_40;
        for (int a = 0; a < 360; a += 30) {
            float rad = a * (float)M_PI / 180.0f;
            lv_point_precise_t pts[2];
            pts[0].x = origin_x; pts[0].y = origin_y;
            pts[1].x = origin_x + max_r * sinf(rad);
            pts[1].y = origin_y - max_r * cosf(rad);
            lv_canvas_draw_line(bg_canvas, pts, 2, &dsc);
        }

        // house dot
        lv_draw_rect_dsc_t rdsc;
        lv_draw_rect_dsc_init(&rdsc);
        rdsc.bg_color = lv_color_hex(0xffb347);
        rdsc.bg_opa = LV_OPA_COVER;
        rdsc.radius = LV_RADIUS_CIRCLE;
        rdsc.border_width = 0;
        int hs = SCALE_PX(5);
        lv_canvas_draw_rect(bg_canvas, origin_x - hs / 2, origin_y - hs / 2, hs, hs, &rdsc);

        // north marker: small compass in the top-right corner, rotated so that
        // true north points in its real screen direction
        int32_t nx = w - SCALE_PX(34), ny = SCALE_PX(34);
        draw_circle(nx, ny, SCALE_PX(16), lv_color_hex(0xd06060), SCALE_PX(1));
        float nrad = (0.0f - RADAR_UP_BEARING) * (float)M_PI / 180.0f;  // north screen angle
        lv_draw_line_dsc_t ndsc;
        lv_draw_line_dsc_init(&ndsc);
        ndsc.color = lv_color_hex(0xd06060);
        ndsc.width = SCALE_PX(2);
        ndsc.opa = LV_OPA_COVER;
        lv_point_precise_t npts[2];
        npts[0].x = nx; npts[0].y = ny;
        npts[1].x = nx + SCALE_PX(13) * sinf(nrad);
        npts[1].y = ny - SCALE_PX(13) * cosf(nrad);
        lv_canvas_draw_line(bg_canvas, npts, 2, &ndsc);
        lv_obj_set_pos(north_label, (int32_t)npts[1].x - SCALE_PX(4), (int32_t)npts[1].y - SCALE_PX(14));
    }

    void redraw_fg() {
        lv_canvas_fill_bg(fg_canvas, lv_color_hex(0x000000), LV_OPA_TRANSP);

        float max_r = sqrtf((float)(w * w + h * h));
        lv_draw_line_dsc_t dsc;
        lv_draw_line_dsc_init(&dsc);
        dsc.color = col_sweep();
        dsc.width = SCALE_PX(2);
        dsc.rounded = true;

        // phosphor trail: fading segments behind the sweep line
        for (int k = RADAR_TRAIL_SEGS; k >= 1; k--) {
            float a = (sweep_angle - RADAR_TRAIL_DEG * k / RADAR_TRAIL_SEGS) * (float)M_PI / 180.0f;
            dsc.opa = (lv_opa_t)(200 * (RADAR_TRAIL_SEGS - k) / RADAR_TRAIL_SEGS);
            lv_point_precise_t pts[2];
            pts[0].x = origin_x; pts[0].y = origin_y;
            pts[1].x = origin_x + max_r * sinf(a);
            pts[1].y = origin_y - max_r * cosf(a);
            lv_canvas_draw_line(fg_canvas, pts, 2, &dsc);
        }

        // bright sweep line
        dsc.opa = LV_OPA_COVER;
        dsc.width = SCALE_PX(3);
        float a = sweep_angle * (float)M_PI / 180.0f;
        lv_point_precise_t pts[2];
        pts[0].x = origin_x; pts[0].y = origin_y;
        pts[1].x = origin_x + max_r * sinf(a);
        pts[1].y = origin_y - max_r * cosf(a);
        lv_canvas_draw_line(fg_canvas, pts, 2, &dsc);

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
            lv_opa_t opa = age < 30000 ? LV_OPA_COVER : (lv_opa_t)(LV_OPA_COVER - (LV_OPA_COVER - LV_OPA_30) * (age - 30000) / 150000);
            if (offscale) opa = opa / 3;

            lv_draw_rect_dsc_t rdsc;
            lv_draw_rect_dsc_init(&rdsc);
            rdsc.bg_color = t->class_b ? col_blip_b() : col_blip();
            rdsc.bg_opa = opa;
            rdsc.radius = LV_RADIUS_CIRCLE;
            rdsc.border_width = 0;
            int bs = SCALE_PX(7);
            lv_canvas_draw_rect(fg_canvas, (int32_t)cx - bs / 2, (int32_t)cy - bs / 2, bs, bs, &rdsc);

            // COG vector
            if (t->cog < 3600) {
                float crad = (t->cog / 10.0f - RADAR_UP_BEARING) * (float)M_PI / 180.0f;
                lv_draw_line_dsc_t ldsc;
                lv_draw_line_dsc_init(&ldsc);
                ldsc.color = t->class_b ? col_blip_b() : col_blip();
                ldsc.width = SCALE_PX(2);
                ldsc.opa = opa;
                ldsc.rounded = true;
                lv_point_precise_t vpts[2];
                vpts[0].x = cx; vpts[0].y = cy;
                vpts[1].x = cx + SCALE_PX(12) * sinf(crad);
                vpts[1].y = cy - SCALE_PX(12) * cosf(crad);
                lv_canvas_draw_line(fg_canvas, vpts, 2, &ldsc);
            }

            // selection box
            if (i == selected) {
                lv_draw_rect_dsc_t sdsc;
                lv_draw_rect_dsc_init(&sdsc);
                sdsc.bg_opa = LV_OPA_TRANSP;
                sdsc.border_color = lv_color_hex(0xff5050);
                sdsc.border_opa = LV_OPA_COVER;
                sdsc.border_width = SCALE_PX(2);
                sdsc.radius = SCALE_PX(2);
                int ss = SCALE_PX(15);
                lv_canvas_draw_rect(fg_canvas, (int32_t)cx - ss / 2, (int32_t)cy - ss / 2, ss, ss, &sdsc);
            }
        }
    }

    void update_info() {
        const ais_target *t = selected >= 0 ? get_ais_manager()->get_target(selected) : nullptr;
        if (!t || !t->valid) {
            lv_label_set_text(info_label, "");
            return;
        }
        float dist = geo_distance_nm(HOME_LAT, HOME_LON, t->lat, t->lon);
        float brg = geo_bearing_deg(HOME_LAT, HOME_LON, t->lat, t->lon);
        char buf[96];
        const char *name = t->name[0] ? t->name : "-----";
        lv_snprintf(buf, sizeof(buf), "%s %lu\n%.1fkn %03ddeg %.1fnm @%03d",
                    name, t->mmsi, t->sog / 10.0f, (int)(t->cog / 10.0f), dist, (int)brg);
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
            char buf[48];
            lv_snprintf(buf, sizeof(buf), "AIS %d tg%s  %s",
                        get_ais_manager()->get_active_count(),
                        get_ais_manager()->get_active_count() == 1 ? "" : "ts",
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

const float RadarGauge::RANGES_NM[] = { 5.0f, 10.0f, 20.0f, 40.0f };
const int RadarGauge::NUM_RANGES = 4;

#endif // RADARGAUGE_H
