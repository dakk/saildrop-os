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
#include "saildrop_pi.h"
#include "chartstream_client.h"
#include "chart_renderer.h"
#include "settings_dialog.h"

#include <wx/filename.h>
#include <wx/stdpaths.h>

// Timer ID
enum {
    TIMER_FRAME = wxID_HIGHEST + 200
};

// PluginTimerOwner implementation
wxBEGIN_EVENT_TABLE(PluginTimerOwner, wxEvtHandler)
    EVT_TIMER(TIMER_FRAME, PluginTimerOwner::OnTimer)
wxEND_EVENT_TABLE()

PluginTimerOwner::PluginTimerOwner(saildrop_pi* plugin)
    : m_plugin(plugin)
    , m_timer(this, TIMER_FRAME)
{
}

PluginTimerOwner::~PluginTimerOwner() {
    Stop();
}

void PluginTimerOwner::Start(int interval_ms) {
    m_timer.Start(interval_ms);
}

void PluginTimerOwner::Stop() {
    m_timer.Stop();
}

bool PluginTimerOwner::IsRunning() const {
    return m_timer.IsRunning();
}

void PluginTimerOwner::OnTimer(wxTimerEvent& event) {
    if (m_plugin) {
        m_plugin->OnFrameTimer();
    }
}

// Plugin factory function
extern "C" DECL_EXP opencpn_plugin* create_pi(void* ppimgr) {
    return new saildrop_pi(ppimgr);
}

extern "C" DECL_EXP void destroy_pi(opencpn_plugin* p) {
    delete p;
}

saildrop_pi::saildrop_pi(void* ppimgr)
    : opencpn_plugin_118(ppimgr)
    , m_plugin_bitmap(nullptr)
    , m_toolbar_id(-1)
    , m_streaming(false)
    , m_esp32_ip("192.168.4.1")
    , m_screen_width(saildrop::DEFAULT_SCREEN_WIDTH)
    , m_screen_height(saildrop::DEFAULT_SCREEN_HEIGHT)
    , m_jpeg_quality(saildrop::DEFAULT_JPEG_QUALITY)
    , m_frame_seq(0)
    , m_zoom_level(10)
{
}

saildrop_pi::~saildrop_pi() {
}

int saildrop_pi::Init() {
    AddLocaleCatalog("opencpn-saildrop_pi");

    // Load configuration
    LoadConfig();

    // Create components
    m_client = std::make_unique<ChartStreamClient>(this);
    m_renderer = std::make_unique<ChartRenderer>();
    m_renderer->SetTargetSize(m_screen_width, m_screen_height);
    m_renderer->SetJPEGQuality(m_jpeg_quality);

    // Create timer owner
    m_timer_owner = std::make_unique<PluginTimerOwner>(this);

    // Load toolbar icon
    wxString plugin_dir = GetPluginDataDir("saildrop_pi");
    wxString icon_path = plugin_dir + wxFileName::GetPathSeparator() + "saildrop_pi.png";

    if (wxFileExists(icon_path)) {
        m_plugin_bitmap = new wxBitmap(icon_path, wxBITMAP_TYPE_PNG);
    } else {
        // Create a default bitmap
        m_plugin_bitmap = new wxBitmap(24, 24);
        wxMemoryDC dc(*m_plugin_bitmap);
        dc.SetBackground(*wxWHITE_BRUSH);
        dc.Clear();
        dc.SetPen(*wxBLACK_PEN);
        dc.SetBrush(*wxBLUE_BRUSH);
        dc.DrawCircle(12, 12, 10);
        dc.SelectObject(wxNullBitmap);
    }

    // Add toolbar button
    m_toolbar_id = InsertPlugInTool("", m_plugin_bitmap, m_plugin_bitmap,
        wxITEM_CHECK, _("Saildrop Chart Stream"),
        "", nullptr, -1, 0, this);

    wxLogMessage("Saildrop Chart Stream plugin initialized");
    return (WANTS_OVERLAY_CALLBACK |
            WANTS_OPENGL_OVERLAY_CALLBACK |
            WANTS_CURSOR_LATLON |
            WANTS_TOOLBAR_CALLBACK |
            INSTALLS_TOOLBAR_TOOL |
            WANTS_PREFERENCES);
}

bool saildrop_pi::DeInit() {
    StopStreaming();

    m_timer_owner.reset();
    m_client.reset();
    m_renderer.reset();

    if (m_plugin_bitmap) {
        delete m_plugin_bitmap;
        m_plugin_bitmap = nullptr;
    }

    wxLogMessage("Saildrop Chart Stream plugin deinitialized");
    return true;
}

wxString saildrop_pi::GetShortDescription() {
    return _("Stream charts to Saildrop ESP32 display");
}

wxString saildrop_pi::GetLongDescription() {
    return _("This plugin captures the current chart view and streams it "
             "to a Saildrop ESP32 device via TCP. The ESP32 can send zoom "
             "commands back to control the chart view.");
}

void saildrop_pi::LoadConfig() {
    wxFileConfig* config = GetOCPNConfigObject();
    if (!config) return;

    config->SetPath("/PlugIns/SaildropChartStream");
    m_esp32_ip = config->Read("ESP32_IP", "192.168.4.1");
    m_screen_width = config->Read("ScreenWidth", saildrop::DEFAULT_SCREEN_WIDTH);
    m_screen_height = config->Read("ScreenHeight", saildrop::DEFAULT_SCREEN_HEIGHT);
    m_jpeg_quality = config->Read("JPEGQuality", saildrop::DEFAULT_JPEG_QUALITY);
}

void saildrop_pi::SaveConfig() {
    wxFileConfig* config = GetOCPNConfigObject();
    if (!config) return;

    config->SetPath("/PlugIns/SaildropChartStream");
    config->Write("ESP32_IP", m_esp32_ip);
    config->Write("ScreenWidth", m_screen_width);
    config->Write("ScreenHeight", m_screen_height);
    config->Write("JPEGQuality", m_jpeg_quality);

    // Update renderer settings
    if (m_renderer) {
        m_renderer->SetTargetSize(m_screen_width, m_screen_height);
        m_renderer->SetJPEGQuality(m_jpeg_quality);
    }
}

void saildrop_pi::OnToolbarToolCallback(int id) {
    if (id != m_toolbar_id) return;

    if (m_streaming) {
        StopStreaming();
    } else {
        StartStreaming();
    }

    SetToolbarItemState(m_toolbar_id, m_streaming);
}

void saildrop_pi::ShowPreferencesDialog(wxWindow* parent) {
    SettingsDialog dlg(parent, this);
    dlg.ShowModal();
}

void saildrop_pi::StartStreaming() {
    if (m_streaming) return;

    wxLogMessage("Saildrop: Starting chart stream to %s", m_esp32_ip);

    m_frame_seq = 0;
    m_client->Connect(m_esp32_ip, saildrop::DEFAULT_PORT);
    m_timer_owner->Start(1000 / saildrop::DEFAULT_FPS);  // 1 FPS
    m_streaming = true;
}

void saildrop_pi::StopStreaming() {
    if (!m_streaming) return;

    wxLogMessage("Saildrop: Stopping chart stream");

    m_timer_owner->Stop();
    m_client->Disconnect();
    m_streaming = false;
}

void saildrop_pi::OnFrameTimer() {
    if (!m_streaming || !m_client->IsConnected()) {
        return;
    }

    SendChartFrame();
}

void saildrop_pi::SendChartFrame() {
    std::vector<uint8_t> jpeg_data;

    if (!m_renderer->CaptureChart(jpeg_data)) {
        wxLogWarning("Saildrop: Failed to capture chart");
        return;
    }

    if (jpeg_data.empty()) {
        return;
    }

    m_zoom_level = m_renderer->GetZoomLevel();
    m_client->SendChartImage(jpeg_data.data(), jpeg_data.size(),
                             m_frame_seq++, m_zoom_level);
}

void saildrop_pi::OnZoomCommand(uint8_t direction) {
    switch (direction) {
        case saildrop::ZOOM_IN:
            m_renderer->ZoomIn();
            break;
        case saildrop::ZOOM_OUT:
            m_renderer->ZoomOut();
            break;
        case saildrop::ZOOM_RESET:
            m_renderer->ZoomReset();
            break;
        default:
            wxLogWarning("Saildrop: Unknown zoom direction %d", direction);
            break;
    }

    // Send immediate frame after zoom change
    if (m_streaming && m_client->IsConnected()) {
        SendChartFrame();
    }
}

void saildrop_pi::SetCurrentViewPort(PlugIn_ViewPort& vp) {
    m_current_vp = vp;
}

bool saildrop_pi::RenderOverlay(wxDC& dc, PlugIn_ViewPort* vp) {
    return false;
}

bool saildrop_pi::RenderGLOverlay(wxGLContext* pcontext, PlugIn_ViewPort* vp) {
    return false;
}
