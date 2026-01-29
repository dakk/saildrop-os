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
#ifndef PORTALSCREEN_H
#define PORTALSCREEN_H

#include "screen.h"
#include "../scale.h"
#include "../conf.h"

#ifndef PORTAL_SSID
#define PORTAL_SSID "SAILDROP_SETUP"
#endif

#ifndef PORTAL_PASS
#define PORTAL_PASS "saildrop"
#endif

class PortalScreen : public Screen {
private:
    lv_obj_t* spinner;
    lv_obj_t* statusLabel;

public:
    PortalScreen() : Screen() {
        // Black background
        lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

        // Title: "WiFi Setup"
        lv_obj_t* title = lv_label_create(scr);
        lv_label_set_text(title, "WiFi Setup");
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, SCALE_PX(25));
        lv_obj_set_style_text_color(title, lv_palette_main(LV_PALETTE_ORANGE), 0);
        lv_obj_set_style_text_font(title, ui_scale::font_medium(), 0);

        // Instruction line 1
        lv_obj_t* instr1 = lv_label_create(scr);
        lv_label_set_text(instr1, "Connect to WiFi:");
        lv_obj_align(instr1, LV_ALIGN_CENTER, 0, SCALE_PX(-50));
        lv_obj_set_style_text_color(instr1, lv_color_white(), 0);
        lv_obj_set_style_text_font(instr1, ui_scale::font_small(), 0);

        // AP SSID (highlighted)
        lv_obj_t* ssidLabel = lv_label_create(scr);
        lv_label_set_text(ssidLabel, PORTAL_SSID);
        lv_obj_align(ssidLabel, LV_ALIGN_CENTER, 0, SCALE_PX(-30));
        lv_obj_set_style_text_color(ssidLabel, lv_palette_main(LV_PALETTE_LIGHT_BLUE), 0);
        lv_obj_set_style_text_font(ssidLabel, ui_scale::font_medium(), 0);

        // Password
        lv_obj_t* passLabel = lv_label_create(scr);
        char passText[64];
        snprintf(passText, sizeof(passText), "Pass: %s", PORTAL_PASS);
        lv_label_set_text(passLabel, passText);
        lv_obj_align(passLabel, LV_ALIGN_CENTER, 0, SCALE_PX(-5));
        lv_obj_set_style_text_color(passLabel, lv_color_hex(0xAAAAAA), 0);
        lv_obj_set_style_text_font(passLabel, ui_scale::font_small(), 0);

        // Instruction line 2
        lv_obj_t* instr2 = lv_label_create(scr);
        lv_label_set_text(instr2, "Then open browser:");
        lv_obj_align(instr2, LV_ALIGN_CENTER, 0, SCALE_PX(20));
        lv_obj_set_style_text_color(instr2, lv_color_white(), 0);
        lv_obj_set_style_text_font(instr2, ui_scale::font_small(), 0);

        // IP Address
        lv_obj_t* ipLabel = lv_label_create(scr);
        lv_label_set_text(ipLabel, "192.168.4.1");
        lv_obj_align(ipLabel, LV_ALIGN_CENTER, 0, SCALE_PX(40));
        lv_obj_set_style_text_color(ipLabel, lv_palette_main(LV_PALETTE_LIGHT_BLUE), 0);
        lv_obj_set_style_text_font(ipLabel, ui_scale::font_medium(), 0);

        // Spinner at bottom
        spinner = lv_spinner_create(scr);
        lv_obj_set_size(spinner, SCALE_PX(30), SCALE_PX(30));
        lv_obj_align(spinner, LV_ALIGN_BOTTOM_MID, 0, SCALE_PX(-25));
        lv_spinner_set_anim_params(spinner, 1000, 200);

        // Status label (hidden by default)
        statusLabel = lv_label_create(scr);
        lv_label_set_text(statusLabel, "");
        lv_obj_align(statusLabel, LV_ALIGN_BOTTOM_MID, 0, SCALE_PX(-60));
        lv_obj_set_style_text_color(statusLabel, lv_color_hex(0x888888), 0);
        lv_obj_set_style_text_font(statusLabel, ui_scale::font_small(), 0);
    }

    void setStatus(const char* text) {
        lv_label_set_text(statusLabel, text);
    }

    void showConnecting() {
        setStatus("Connecting...");
        lv_obj_clear_flag(spinner, LV_OBJ_FLAG_HIDDEN);
    }

    void showSuccess() {
        setStatus("Connected!");
        lv_obj_add_flag(spinner, LV_OBJ_FLAG_HIDDEN);
    }

    void showTimeout() {
        setStatus("Timeout - Retrying...");
    }
};

#endif
