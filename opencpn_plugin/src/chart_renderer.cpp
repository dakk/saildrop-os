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
#include "chart_renderer.h"
#include "ocpn_plugin.h"

#include <wx/dcmemory.h>
#include <wx/dcscreen.h>
#include <wx/image.h>
#include <cmath>

ChartRenderer::ChartRenderer()
    : m_target_width(saildrop::DEFAULT_SCREEN_WIDTH)
    , m_target_height(saildrop::DEFAULT_SCREEN_HEIGHT)
    , m_jpeg_quality(saildrop::DEFAULT_JPEG_QUALITY)
    , m_zoom_level(DEFAULT_ZOOM)
    , m_zoom_scale(1.0)
{
    wxInitAllImageHandlers();
}

ChartRenderer::~ChartRenderer() {
}

void ChartRenderer::SetTargetSize(uint16_t width, uint16_t height) {
    m_target_width = width;
    m_target_height = height;
}

void ChartRenderer::SetJPEGQuality(int quality) {
    m_jpeg_quality = std::max(10, std::min(100, quality));
}

void ChartRenderer::UpdateZoomScale() {
    // Zoom level 10 = 1.0x, each level doubles/halves
    // Level 1 = 0.125x (zoomed out), Level 20 = 8x (zoomed in)
    m_zoom_scale = std::pow(2.0, (m_zoom_level - DEFAULT_ZOOM) / 3.0);
}

void ChartRenderer::ZoomIn() {
    if (m_zoom_level < MAX_ZOOM) {
        m_zoom_level++;
        UpdateZoomScale();
        wxLogMessage("Saildrop: Zoom in to level %d (scale %.2f)", m_zoom_level, m_zoom_scale);
    }
}

void ChartRenderer::ZoomOut() {
    if (m_zoom_level > MIN_ZOOM) {
        m_zoom_level--;
        UpdateZoomScale();
        wxLogMessage("Saildrop: Zoom out to level %d (scale %.2f)", m_zoom_level, m_zoom_scale);
    }
}

void ChartRenderer::ZoomReset() {
    m_zoom_level = DEFAULT_ZOOM;
    UpdateZoomScale();
    wxLogMessage("Saildrop: Zoom reset to level %d", m_zoom_level);
}

bool ChartRenderer::CaptureChart(std::vector<uint8_t>& jpeg_out) {
    jpeg_out.clear();

    // Get the OpenCPN canvas window
    wxWindow* canvas = GetOCPNCanvasWindow();
    if (!canvas) {
        wxLogError("Saildrop: Cannot get OpenCPN canvas window");
        return false;
    }

    // Get canvas dimensions
    wxSize canvas_size = canvas->GetClientSize();
    int cw = canvas_size.GetWidth();
    int ch = canvas_size.GetHeight();

    if (cw <= 0 || ch <= 0) {
        wxLogError("Saildrop: Invalid canvas size");
        return false;
    }

    // Calculate capture region (square, centered, zoom adjusted)
    int base_capture = std::min(cw, ch);
    int capture_size = static_cast<int>(base_capture / m_zoom_scale);

    // Clamp capture size to canvas bounds
    capture_size = std::min(capture_size, std::min(cw, ch));
    capture_size = std::max(capture_size, 1);

    int cx = (cw - capture_size) / 2;
    int cy = (ch - capture_size) / 2;

    // Create a bitmap for capture
    wxBitmap capture_bmp(capture_size, capture_size, 24);
    wxMemoryDC mem_dc;
    mem_dc.SelectObject(capture_bmp);

    // Capture from screen - canvas might be using OpenGL
    // so we need to capture from the screen coordinates
    wxPoint screen_pos = canvas->ClientToScreen(wxPoint(cx, cy));

    wxScreenDC screen_dc;
    mem_dc.Blit(0, 0, capture_size, capture_size,
                &screen_dc, screen_pos.x, screen_pos.y);
    mem_dc.SelectObject(wxNullBitmap);

    // Convert to image and scale to target size
    wxImage img = capture_bmp.ConvertToImage();

    if (!img.IsOk()) {
        wxLogError("Saildrop: Failed to convert bitmap to image");
        return false;
    }

    // Scale to target dimensions
    if (img.GetWidth() != m_target_width || img.GetHeight() != m_target_height) {
        img.Rescale(m_target_width, m_target_height, wxIMAGE_QUALITY_HIGH);
    }

    // Encode to JPEG
    img.SetOption(wxIMAGE_OPTION_QUALITY, m_jpeg_quality);

    wxMemoryOutputStream stream;
    if (!img.SaveFile(stream, wxBITMAP_TYPE_JPEG)) {
        wxLogError("Saildrop: Failed to encode JPEG");
        return false;
    }

    // Copy to output vector
    size_t len = stream.GetLength();
    jpeg_out.resize(len);
    stream.CopyTo(jpeg_out.data(), len);

    wxLogDebug("Saildrop: Captured %dx%d, encoded %zu bytes JPEG",
               m_target_width, m_target_height, len);

    return true;
}
