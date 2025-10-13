#ifndef COMPASS_H
#define COMPASS_H

#include <lvgl.h>
#include <cmath>

// https://docs.lvgl.io/9.3/details/widgets/scale.html#a-round-scale-style-simulating-a-compass

class Compass
{
private:
    lv_obj_t *container;
    lv_obj_t *compass_rose;
    lv_obj_t *heading_indicator;
    lv_obj_t *heading_label;
    int center_x, center_y;
    int radius;

    void create_compass_rose()
    {
        compass_rose = lv_obj_create(container);
        lv_obj_remove_style_all(compass_rose);
        lv_obj_set_size(compass_rose, 2 * radius, 2 * radius);
        lv_obj_center(compass_rose);

        // Draw cardinal directions
        draw_direction("N", 0);
        draw_direction("E", 90);
        draw_direction("S", 180);
        draw_direction("W", 270);

        // Draw ordinal directions
        draw_direction("NE", 45);
        draw_direction("SE", 135);
        draw_direction("SW", 225);
        draw_direction("NW", 315);

        // Draw degree markers
        for (int i = 0; i < 360; i += 5)
        {
            int line_length = (i % 90 == 0) ? 15 : (i % 45 == 0) ? 10
                                                                 : 5;
            float angle_rad = (i - 90) * M_PI / 180;
            int x1 = center_x + (radius - line_length) * cos(angle_rad);
            int y1 = center_y + (radius - line_length) * sin(angle_rad);
            int x2 = center_x + radius * cos(angle_rad);
            int y2 = center_y + radius * sin(angle_rad);

            lv_obj_t *line = lv_line_create(compass_rose);
            static lv_point_t line_points[2];
            line_points[0].x = x1;
            line_points[0].y = y1;
            line_points[1].x = x2;
            line_points[1].y = y2;
            lv_line_set_points(line, line_points, 2);
            lv_obj_set_style_line_color(line, lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_line_width(line, 2, 0);
        }
    }

    void draw_direction(const char *text, int angle)
    {
        float angle_rad = (angle - 90) * M_PI / 180;
        int x = center_x + (radius - 25) * cos(angle_rad);
        int y = center_y + (radius - 25) * sin(angle_rad);

        lv_obj_t *label = lv_label_create(compass_rose);
        lv_label_set_text(label, text);
        lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_pos(label, x - 10, y - 10);
    }

    void create_heading_indicator()
    {
        heading_indicator = lv_obj_create(container);
        lv_obj_remove_style_all(heading_indicator);
        lv_obj_set_size(heading_indicator, 2 * radius, 2 * radius);
        lv_obj_center(heading_indicator);

        // Create heading arrow
        static lv_point_t arrow_points[] = {{0, -radius + 10}, {-10, -radius + 30}, {10, -radius + 30}};
        lv_obj_t *arrow = lv_line_create(heading_indicator);
        lv_line_set_points(arrow, arrow_points, 3);
        lv_obj_set_style_line_color(arrow, lv_color_hex(0xFF0000), 0); // Red color
        lv_obj_set_style_line_width(arrow, 3, 0);
        lv_obj_center(arrow);

        // Create heading lubber line
        static lv_point_t lubber_points[] = {{0, -radius}, {0, 0}};
        lv_obj_t *lubber_line = lv_line_create(heading_indicator);
        lv_line_set_points(lubber_line, lubber_points, 2);
        lv_obj_set_style_line_color(lubber_line, lv_color_hex(0xFF0000), 0); // Red color
        lv_obj_set_style_line_width(lubber_line, 2, 0);
        lv_obj_center(lubber_line);
    }

public:
    Compass(lv_obj_t *parent, int width, int height);

    void set_heading(int heading);
    void showcase();
};

static void _cp_set_angle(void *cp, int32_t hdg)
{
    ((Compass *)cp)->set_heading(hdg);
}

Compass::Compass(lv_obj_t *parent, int width, int height)
{
    container = lv_obj_create(parent);
    lv_obj_set_size(container, width, height);
    lv_obj_center(container);
    // lv_obj_set_style_bg_color(container, lv_color_hex(0x000000), 0); // Navy blue background
    lv_obj_set_style_border_width(container, 0, 0);

    center_x = width / 2;
    center_y = height / 2;
    radius = (width < height ? width : height) / 2 - 10;

    create_compass_rose();
    create_heading_indicator();

    // Create heading label
    heading_label = lv_label_create(container);
    lv_obj_set_style_text_color(heading_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(heading_label, LV_ALIGN_BOTTOM_MID, 0, -10);
}

void Compass::set_heading(int heading)
{
    lv_obj_set_style_transform_angle(heading_indicator, heading * 10, 0); // LVGL uses deci-degrees
    lv_obj_invalidate(heading_indicator);

    char buf[10];
    lv_snprintf(buf, sizeof(buf), "%d°", heading);
    lv_label_set_text(heading_label, buf);
}

void Compass::showcase()
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_exec_cb(&a, _cp_set_angle);
    lv_anim_set_var(&a, this);
    lv_anim_set_values(&a, 0, 360);
    lv_anim_set_time(&a, 2000);
    lv_anim_set_repeat_delay(&a, 100);
    lv_anim_set_playback_time(&a, 500);
    lv_anim_set_playback_delay(&a, 300);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
}

#endif