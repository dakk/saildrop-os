#ifndef VALUEGAUGE_H
#define VALUEGAUGE_H

#include <lvgl.h>
#include <string.h>

class ValueGauge
{
private:
public:
    lv_obj_t *value_label;
    lv_obj_t *value_arc;
    char unit_str[8];
    char label_str[16];
    int32_t min_val;
    int32_t max_val;

    ValueGauge(lv_obj_t *parent, int width, int height, const char *label, const char *unit, 
        int32_t min, int32_t max, lv_palette_t arc_color);
    void set_value(int32_t speed);
    void showcase();
};

static void _vg_set_value(void *vg, int32_t val)
{
    ((ValueGauge *)vg)->set_value(val);
}

ValueGauge::ValueGauge(lv_obj_t *parent, int width, int height, const char *label, const char *unit, 
        int32_t min=-1, int32_t max=-1, lv_palette_t arc_color=LV_PALETTE_BLUE)
{
    lv_obj_t *container = lv_obj_create(parent);
    lv_obj_set_size(container, width, height);
    lv_obj_center(container);
    // lv_obj_set_style_bg_color(container, lv_color_hex(0x000000), 0); // Navy blue background
    lv_obj_set_style_border_width(container, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_width(container, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_line_width(container, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    strcpy(label_str, label);
    strcpy(unit_str, unit);

    // Add value label
    lv_obj_t *gauge_label = lv_label_create(container);
    lv_obj_align(gauge_label, LV_ALIGN_CENTER, 0, -40);
    lv_label_set_text(gauge_label, label_str);

    value_label = lv_label_create(container);
    lv_obj_align(value_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_font(value_label, &lv_font_montserrat_48, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(value_label, "NO DATA");
    

    // Speed arc
    if (min != -1 && max != -1) {
        min_val = min;
        max_val = max;
        value_arc = lv_arc_create(container);

        lv_obj_set_style_arc_width(value_arc, 20, LV_PART_MAIN);
        lv_obj_set_style_arc_width(value_arc, 20, LV_PART_INDICATOR);
        // lv_obj_set_style_arc_color(spinner, lv_palette_main(LV_PALETTE_ORANGE), LV_PART_MAIN);
        lv_obj_set_style_arc_color(value_arc, lv_palette_darken(arc_color, 3), LV_PART_INDICATOR);

        lv_obj_remove_style(value_arc, NULL, LV_PART_KNOB);  /*Be sure the knob is not displayed*/
        lv_obj_clear_flag(value_arc, LV_OBJ_FLAG_CLICKABLE); /*To not allow adjusting by click*/

        lv_arc_set_rotation(value_arc, 0);
        lv_arc_set_bg_angles(value_arc, 0, 360);
        lv_arc_set_start_angle(value_arc, 0);

        lv_obj_set_size(value_arc, SCREEN_WIDTH, SCREEN_HEIGHT);
        lv_obj_center(value_arc);
    }
}

void ValueGauge::set_value(int32_t val)
{
    if(value_arc) {
        // max_val : 100 = val : y
        // y = 100 * val / max_val
        int32_t nval = 10 * val / max_val;
        if (nval > 100)
            nval = 100;
        lv_arc_set_value(value_arc, nval);
    }

    char buf[20];
    lv_snprintf(buf, sizeof(buf), "%d.%d %s", val / 10, val % 10, unit_str);
    lv_label_set_text(value_label, buf);
}

void ValueGauge::showcase()
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_exec_cb(&a, _vg_set_value);
    lv_anim_set_var(&a, this);
    lv_anim_set_values(&a, min_val*10, max_val*10);
    lv_anim_set_time(&a, 2000);
    lv_anim_set_repeat_delay(&a, 100);
    lv_anim_set_playback_time(&a, 500);
    lv_anim_set_playback_delay(&a, 300);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
}

#endif
