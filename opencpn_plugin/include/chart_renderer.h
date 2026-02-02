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
#ifndef CHART_RENDERER_H
#define CHART_RENDERER_H

#include "protocol.h"

#include <wx/wx.h>
#include <wx/mstream.h>
#include <vector>

class ChartRenderer {
public:
    ChartRenderer();
    ~ChartRenderer();

    // Set target dimensions
    void SetTargetSize(uint16_t width, uint16_t height);
    uint16_t GetTargetWidth() const { return m_target_width; }
    uint16_t GetTargetHeight() const { return m_target_height; }

    // Set JPEG quality (0-100)
    void SetJPEGQuality(int quality);
    int GetJPEGQuality() const { return m_jpeg_quality; }

    // Capture current chart view
    // Returns JPEG data in output vector, returns false on failure
    bool CaptureChart(std::vector<uint8_t>& jpeg_out);

    // Zoom control
    void ZoomIn();
    void ZoomOut();
    void ZoomReset();
    uint8_t GetZoomLevel() const { return m_zoom_level; }

    // Get current zoom scale factor
    double GetZoomScale() const { return m_zoom_scale; }

private:
    uint16_t m_target_width;
    uint16_t m_target_height;
    int m_jpeg_quality;

    // Zoom state
    uint8_t m_zoom_level;   // 1-20, 10 = default
    double m_zoom_scale;    // Derived from zoom_level
    static constexpr uint8_t MIN_ZOOM = 1;
    static constexpr uint8_t MAX_ZOOM = 20;
    static constexpr uint8_t DEFAULT_ZOOM = 10;

    void UpdateZoomScale();
};

#endif // CHART_RENDERER_H
