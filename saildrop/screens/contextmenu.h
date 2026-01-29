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
#ifndef CONTEXTMENU_H
#define CONTEXTMENU_H

#include <lvgl.h>
#include "screen.h"
#include "../settings.h"

enum ContextMenuAction {
    MENU_NONE = 0,
    MENU_RESET_SETTINGS,
    MENU_REBOOT,
    MENU_CLOSE
};

static ContextMenuAction contextMenuResult = MENU_NONE;
static lv_obj_t *contextMenuScreen = NULL;
static bool contextMenuVisible = false;

static void context_menu_btn_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        ContextMenuAction *action = (ContextMenuAction *)lv_event_get_user_data(e);
        contextMenuResult = *action;
    }
}

static lv_obj_t* create_menu_button(lv_obj_t *parent, const char *text, int y_offset, ContextMenuAction *action) {
    lv_obj_t *btn = lv_obj_create(parent);
    lv_obj_set_size(btn, 180, 45);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, y_offset);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);

    // Button styling
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(btn, 10, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x555555), LV_STATE_PRESSED);

    lv_obj_add_event_cb(btn, context_menu_btn_event_cb, LV_EVENT_CLICKED, action);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_center(label);

    return btn;
}

void context_menu_create() {
    if (contextMenuScreen != NULL) return;

    contextMenuScreen = lv_obj_create(NULL);
    lv_obj_clear_flag(contextMenuScreen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(contextMenuScreen, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(contextMenuScreen, LV_OPA_COVER, LV_PART_MAIN);

    // Title
    lv_obj_t *title = lv_label_create(contextMenuScreen);
    lv_label_set_text(title, "Menu");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);

    // Static action values for callbacks
    static ContextMenuAction resetAction = MENU_RESET_SETTINGS;
    static ContextMenuAction rebootAction = MENU_REBOOT;
    static ContextMenuAction closeAction = MENU_CLOSE;

    // Create buttons
    create_menu_button(contextMenuScreen, "Reset Settings", -25, &resetAction);
    create_menu_button(contextMenuScreen, "Reboot", 30, &rebootAction);
    create_menu_button(contextMenuScreen, "Close", 85, &closeAction);
}

void context_menu_show(lv_obj_t *previousScreen) {
    if (contextMenuScreen == NULL) {
        context_menu_create();
    }
    contextMenuResult = MENU_NONE;
    contextMenuVisible = true;
    lv_scr_load_anim(contextMenuScreen, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
}

void context_menu_hide(lv_obj_t *returnScreen) {
    contextMenuVisible = false;
    contextMenuResult = MENU_NONE;
    lv_scr_load_anim(returnScreen, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
}

bool context_menu_is_visible() {
    return contextMenuVisible;
}

ContextMenuAction context_menu_get_result() {
    return contextMenuResult;
}

void context_menu_clear_result() {
    contextMenuResult = MENU_NONE;
}

#endif
