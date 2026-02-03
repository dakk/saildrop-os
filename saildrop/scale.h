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
#ifndef SCALE_H
#define SCALE_H

#include "conf.h"
#include <lvgl.h>

// Reference screen size (design baseline)
#define REF_SCREEN_WIDTH 240
#define REF_SCREEN_HEIGHT 240

// Calculate the minimum dimension for scaling (use smaller dimension for round screens)
#define SCREEN_MIN_DIM ((SCREEN_WIDTH < SCREEN_HEIGHT) ? SCREEN_WIDTH : SCREEN_HEIGHT)
#define REF_MIN_DIM 240

// Scaling factor (x1000 for integer math precision)
#define SCALE_FACTOR_X1000 ((SCREEN_MIN_DIM * 1000) / REF_MIN_DIM)

// Scale a pixel value from reference to actual screen size
// Use for dimensions, offsets, radii, etc.
#define SCALE_PX(px) (((px) * SCALE_FACTOR_X1000) / 1000)

// Scale for width specifically (useful for non-square screens)
#define SCALE_W(px) (((px) * SCREEN_WIDTH) / REF_SCREEN_WIDTH)

// Scale for height specifically (useful for non-square screens)
#define SCALE_H(px) (((px) * SCREEN_HEIGHT) / REF_SCREEN_HEIGHT)

// Font selection based on screen size
// Maps reference font sizes to appropriate fonts for current screen
namespace ui_scale {

// Font size categories based on reference 240px screen
enum FontSize {
    FONT_SMALL = 0,   // 14pt at 240px
    FONT_MEDIUM,      // 20pt at 240px
    FONT_LARGE        // 48pt at 240px
};

// Get appropriate font for current screen size
inline const lv_font_t* get_font(FontSize size) {
    // Calculate effective font scale
    // At 240px: small=14, medium=20, large=48
    // Scale proportionally for other sizes

#if SCREEN_MIN_DIM <= 128
    // Very small screens (128x128 or smaller)
    switch (size) {
        case FONT_SMALL:  return &lv_font_montserrat_10;
        case FONT_MEDIUM: return &lv_font_montserrat_12;
        case FONT_LARGE:  return &lv_font_montserrat_24;
        default:          return &lv_font_montserrat_12;
    }
#elif SCREEN_MIN_DIM <= 200
    // Small screens (129-200px)
    switch (size) {
        case FONT_SMALL:  return &lv_font_montserrat_12;
        case FONT_MEDIUM: return &lv_font_montserrat_16;
        case FONT_LARGE:  return &lv_font_montserrat_36;
        default:          return &lv_font_montserrat_14;
    }
#elif SCREEN_MIN_DIM <= 280
    // Medium screens (201-280px) - includes 240x240
    switch (size) {
        case FONT_SMALL:  return &lv_font_montserrat_14;
        case FONT_MEDIUM: return &lv_font_montserrat_20;
        case FONT_LARGE:  return &lv_font_montserrat_48;
        default:          return &lv_font_montserrat_14;
    }
#elif SCREEN_MIN_DIM <= 400
    // Large screens (281-400px)
    switch (size) {
        case FONT_SMALL:  return &lv_font_montserrat_18;
        case FONT_MEDIUM: return &lv_font_montserrat_26;
        case FONT_LARGE:  return &lv_font_montserrat_48;
        default:          return &lv_font_montserrat_18;
    }
#elif SCREEN_MIN_DIM <= 520
    // 480px screens (401-520px) - ESP32-S3-Touch-LCD-4
    switch (size) {
        case FONT_SMALL:  return &lv_font_montserrat_22;
        case FONT_MEDIUM: return &lv_font_montserrat_32;
        case FONT_LARGE:  return &lv_font_montserrat_48;
        default:          return &lv_font_montserrat_22;
    }
#else
    // Very large screens (520px+)
    switch (size) {
        case FONT_SMALL:  return &lv_font_montserrat_24;
        case FONT_MEDIUM: return &lv_font_montserrat_36;
        case FONT_LARGE:  return &lv_font_montserrat_48;
        default:          return &lv_font_montserrat_24;
    }
#endif
}

// Convenience functions
inline const lv_font_t* font_small()  { return get_font(FONT_SMALL); }
inline const lv_font_t* font_medium() { return get_font(FONT_MEDIUM); }
inline const lv_font_t* font_large()  { return get_font(FONT_LARGE); }

// Get screen center coordinates
inline int32_t center_x() { return SCREEN_WIDTH / 2; }
inline int32_t center_y() { return SCREEN_HEIGHT / 2; }

} // namespace ui_scale

#endif
