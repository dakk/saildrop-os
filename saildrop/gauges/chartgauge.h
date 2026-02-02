/*
 * Copyright (C) 2024-2025 Davide Gessa
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef CHARTGAUGE_H
#define CHARTGAUGE_H

#include <lvgl.h>
#include <WiFi.h>
#include "../conf.h"
#include "../scale.h"
#include "../chartstream.h"
#include "styles.h"

#ifdef SCREEN_CHART

class ChartGauge;

// Timer callback forward declaration
static void chart_tick_cb(lv_timer_t *timer);

class ChartGauge {
private:
    lv_obj_t *bg;
    lv_obj_t *img_obj;
    lv_obj_t *status_label;
    lv_obj_t *zoom_label;
    lv_obj_t *info_label;  // Connection info when disconnected

    // Image descriptor for current frame
    lv_image_dsc_t img_dsc;

    // State
    uint16_t last_seq;
    bool connected;

    // Update the displayed image
    void updateImage(ChartFrame *frame) {
        if (!frame || !frame->jpeg_data || frame->jpeg_size == 0) {
            return;
        }

        // Update image descriptor to point to new JPEG data
        img_dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
        img_dsc.header.cf = LV_COLOR_FORMAT_RAW;
        img_dsc.header.w = SCREEN_WIDTH;
        img_dsc.header.h = SCREEN_HEIGHT;
        img_dsc.data_size = frame->jpeg_size;
        img_dsc.data = frame->jpeg_data;

        // Update the image object source
        lv_image_set_src(img_obj, &img_dsc);

#ifdef DEBUG
        Serial.printf("ChartGauge: Updated image, frame %d, %d bytes\n",
                      frame->seq_no, frame->jpeg_size);
#endif
    }

    // Update the info label with connection details
    void updateInfoLabel() {
        char buf[128];
        String ip = WiFi.localIP().toString();
        lv_snprintf(buf, sizeof(buf),
                    "Waiting for OpenCPN...\n\n"
                    "IP: %s\n"
                    "Port: %d\n"
                    "Size: %dx%d",
                    ip.c_str(),
                    CHART_STREAM_PORT,
                    SCREEN_WIDTH,
                    SCREEN_HEIGHT);
        lv_label_set_text(info_label, buf);
    }

public:
    ChartGauge(lv_obj_t *parent, int width, int height) {
        gauge_styles::init_styles();

        last_seq = 0;
        connected = false;

        // Initialize image descriptor
        memset(&img_dsc, 0, sizeof(img_dsc));

        // Background container
        bg = gauge_styles::create_gauge_bg(parent, width, height);

        // Create image object for chart display
        img_obj = lv_image_create(bg);
        lv_obj_set_size(img_obj, width, height);
        lv_obj_center(img_obj);

        // Connection info label (center) - shown when disconnected
        info_label = lv_label_create(bg);
        lv_obj_align(info_label, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_text_color(info_label, lv_color_white(), 0);
        lv_obj_set_style_text_font(info_label, ui_scale::font_small(), 0);
        lv_obj_set_style_text_align(info_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(info_label, "Initializing...");

        // Status label (top) - with semi-transparent background
        status_label = lv_label_create(bg);
        lv_obj_align(status_label, LV_ALIGN_TOP_MID, 0, SCALE_PX(5));
        lv_label_set_text(status_label, "CHART");
        lv_obj_set_style_text_color(status_label, lv_color_white(), 0);
        lv_obj_set_style_text_font(status_label, ui_scale::font_small(), 0);
        lv_obj_set_style_bg_color(status_label, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(status_label, LV_OPA_70, 0);
        lv_obj_set_style_pad_hor(status_label, SCALE_PX(4), 0);
        lv_obj_set_style_pad_ver(status_label, SCALE_PX(2), 0);
        lv_obj_set_style_radius(status_label, SCALE_PX(2), 0);

        // Zoom level indicator (bottom)
        zoom_label = lv_label_create(bg);
        lv_obj_align(zoom_label, LV_ALIGN_BOTTOM_MID, 0, -SCALE_PX(5));
        lv_label_set_text(zoom_label, "");
        lv_obj_set_style_text_color(zoom_label, lv_color_white(), 0);
        lv_obj_set_style_text_font(zoom_label, ui_scale::font_small(), 0);
        lv_obj_set_style_bg_color(zoom_label, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(zoom_label, LV_OPA_70, 0);
        lv_obj_set_style_pad_hor(zoom_label, SCALE_PX(4), 0);
        lv_obj_set_style_pad_ver(zoom_label, SCALE_PX(2), 0);
        lv_obj_set_style_radius(zoom_label, SCALE_PX(2), 0);

        // Create update timer (100ms)
        lv_timer_create(chart_tick_cb, 100, this);
    }

    void update() {
        ChartStreamServer *stream = getChartStream();

        // Check connection status
        bool is_connected = stream->isClientConnected();
        if (is_connected != connected) {
            connected = is_connected;
            if (connected) {
                // Hide info label, show image
                lv_obj_add_flag(info_label, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(img_obj, LV_OBJ_FLAG_HIDDEN);
            } else {
                // Show info label with connection details, hide image
                lv_obj_clear_flag(info_label, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(img_obj, LV_OBJ_FLAG_HIDDEN);
                updateInfoLabel();
                lv_label_set_text(zoom_label, "");
            }
        }

        if (!connected) {
            // Periodically update info label (IP might change)
            static uint32_t last_info_update = 0;
            uint32_t now = millis();
            if (now - last_info_update > 2000) {
                updateInfoLabel();
                last_info_update = now;
            }
            return;
        }

        // Check for new frame
        ChartFrame *frame = stream->getFrame();
        if (frame->ready && frame->seq_no != last_seq) {
            updateImage(frame);
            last_seq = frame->seq_no;

            // Update zoom label
            char buf[16];
            lv_snprintf(buf, sizeof(buf), "Z%d", frame->zoom_level);
            lv_label_set_text(zoom_label, buf);

            // Mark frame as consumed
            stream->clearFrame();

            // Request next frame
            stream->sendRefreshRequest();
        }
    }

    void zoom_in() {
        ChartStreamServer *stream = getChartStream();
        if (stream->isClientConnected()) {
            stream->sendZoomCommand(ZOOM_IN);
        }
    }

    void zoom_out() {
        ChartStreamServer *stream = getChartStream();
        if (stream->isClientConnected()) {
            stream->sendZoomCommand(ZOOM_OUT);
        }
    }
};

static void chart_tick_cb(lv_timer_t *timer) {
    ChartGauge *gauge = static_cast<ChartGauge *>(lv_timer_get_user_data(timer));
    gauge->update();
}

#endif // SCREEN_CHART

#endif // CHARTGAUGE_H
