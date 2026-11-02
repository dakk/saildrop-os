/*
 * battery.h - tiny battery-charge widget, shared by radar/ and saildrop/.
 * Header-only (common/ is included via relative paths). Uses LVGL's built-in
 * battery symbols (LV_SYMBOL_BATTERY_*), colored by level; shows LV_SYMBOL_USB
 * when no battery ADC is configured (shore power).
 *
 * Battery voltage source: define BATTERY_ADC_PIN (and BATTERY_ADC_DIVIDER for
 * the voltage divider ratio) in the board conf or sketch conf. Without a pin,
 * read_percent() returns -1 and the widget shows the USB/plug icon.
 */
#ifndef BATTERY_H
#define BATTERY_H

#include <lvgl.h>
#ifdef ARDUINO
#include <Wire.h>
#endif

#ifndef BATTERY_ADC_PIN
#define BATTERY_ADC_PIN -1
#endif
#ifndef BATTERY_ADC_DIVIDER
#define BATTERY_ADC_DIVIDER 2.0f   // typical on-board divider (e.g. 100k/100k)
#endif
#ifndef BATTERY_V_MIN
#define BATTERY_V_MIN 3.0f         // LiPo empty
#define BATTERY_V_MAX 4.2f         // LiPo full
#endif

// On the Waveshare 4" board the battery is managed by a SW6106 PMIC on the
// touch I2C bus (GPIO7/15, Wire is initialized by HAL). Battery % is register
// 0x0A at device 0x3C (see the board's Docs/SW6106 note). Returns -1 on I2C
// error so the caller falls back to the USB icon.
#if defined(ARDUINO) && defined(BOARD_LCD_4)
#define BATTERY_PMU_ADDR    0x3C
#define BATTERY_PMU_REG_PCT 0x0A
inline int battery_read_percent_pmu() {
    Wire.beginTransmission(BATTERY_PMU_ADDR);
    Wire.write(BATTERY_PMU_REG_PCT);
    if (Wire.endTransmission(false) != 0) return -1;
    if (Wire.requestFrom(BATTERY_PMU_ADDR, (uint8_t)1) != 1) return -1;
    int v = Wire.read();
    if (v < 0 || v > 100) return -1;  // 0xFF etc: no battery / read error
    return v;
}
#endif

// Read battery charge in percent (0-100), or -1 if unknown (shore power).
// Priority: BATTERY_ADC_PIN (GPIO ADC) > SW6106 PMU (4" board) >
// BATTERY_FORCE_PCT (fixed level) > -1 (USB icon).
inline int battery_read_percent() {
#if defined(ARDUINO) && BATTERY_ADC_PIN >= 0
    uint32_t mv = analogReadMilliVolts(BATTERY_ADC_PIN);
    float v = (mv / 1000.0f) * BATTERY_ADC_DIVIDER;
    int pct = (int)((v - BATTERY_V_MIN) / (BATTERY_V_MAX - BATTERY_V_MIN) * 100.0f);
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    return pct;
#elif defined(ARDUINO) && defined(BOARD_LCD_4)
    int pct = battery_read_percent_pmu();
    if (pct >= 0) return pct;
# if defined(BATTERY_FORCE_PCT)
    return BATTERY_FORCE_PCT;
# else
    return -1;
# endif
#elif defined(BATTERY_FORCE_PCT)
    return BATTERY_FORCE_PCT;
#else
    return -1;
#endif
}

class BatteryGauge {
public:
    // parent: container to attach to; align: where to put the icon
    BatteryGauge(lv_obj_t *parent, lv_align_t align = LV_ALIGN_TOP_LEFT,
                 int32_t x_ofs = 0, int32_t y_ofs = 0, bool show_percent = false) {
        icon = lv_label_create(parent);
        lv_obj_align(icon, align, x_ofs, y_ofs);
        lv_obj_set_style_text_font(icon, &lv_font_montserrat_16, 0);
        pct_label = nullptr;
        if (show_percent) {
            pct_label = lv_label_create(parent);
            lv_obj_set_style_text_font(pct_label, &lv_font_montserrat_14, 0);
            lv_obj_set_style_text_color(pct_label, lv_color_hex(0x9fb8a8), 0);
            lv_obj_align_to(pct_label, icon, LV_ALIGN_OUT_RIGHT_MID, 4, 0);
        }
        set_percent(battery_read_percent());
    }

    // start periodic refresh from the ADC (or shore-power icon)
    void start(uint32_t period_ms = 5000) {
        lv_timer_create(tick_cb, period_ms, this);
    }

    void set_percent(int pct) {
        last_pct = pct;
        const char *sym;
        lv_color_t col;
        if (pct < 0) {           // unknown: assume shore power
            sym = LV_SYMBOL_USB;
            col = lv_color_hex(0x9fb8a8);
        } else if (pct >= 85) { sym = LV_SYMBOL_BATTERY_FULL; col = lv_color_hex(0x6fd08c); }
        else if (pct >= 60)  { sym = LV_SYMBOL_BATTERY_3;    col = lv_color_hex(0x6fd08c); }
        else if (pct >= 35)  { sym = LV_SYMBOL_BATTERY_2;    col = lv_color_hex(0xffd27f); }
        else if (pct >= 10)  { sym = LV_SYMBOL_BATTERY_1;    col = lv_color_hex(0xff9040); }
        else                   { sym = LV_SYMBOL_BATTERY_EMPTY; col = lv_color_hex(0xd06060); }
        lv_label_set_text(icon, sym);
        lv_obj_set_style_text_color(icon, col, 0);
        if (pct_label) {
            if (pct < 0) {
                lv_label_set_text(pct_label, "");
            } else {
                char buf[8];
                lv_snprintf(buf, sizeof(buf), "%d%%", pct);
                lv_label_set_text(pct_label, buf);
            }
        }
    }

    int percent() const { return last_pct; }

private:
    lv_obj_t *icon;
    lv_obj_t *pct_label;
    int last_pct = -1;

    static void tick_cb(lv_timer_t *timer) {
        BatteryGauge *g = static_cast<BatteryGauge*>(lv_timer_get_user_data(timer));
        g->set_percent(battery_read_percent());
    }
};

#endif // BATTERY_H
