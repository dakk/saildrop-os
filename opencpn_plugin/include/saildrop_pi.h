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
#ifndef SAILDROP_PI_H
#define SAILDROP_PI_H

#include "ocpn_plugin.h"
#include "protocol.h"

#include <wx/wx.h>
#include <wx/timer.h>
#include <memory>

class ChartStreamClient;
class ChartRenderer;
class SettingsDialog;
class PluginTimerOwner;

class saildrop_pi : public opencpn_plugin_118 {
public:
    saildrop_pi(void* ppimgr);
    ~saildrop_pi();

    // OpenCPN Plugin API
    int Init() override;
    bool DeInit() override;

    int GetAPIVersionMajor() override { return 1; }
    int GetAPIVersionMinor() override { return 18; }

    int GetPlugInVersionMajor() override { return 1; }
    int GetPlugInVersionMinor() override { return 0; }

    wxBitmap* GetPlugInBitmap() override { return m_plugin_bitmap; }

    wxString GetCommonName() override { return _("Saildrop Chart Stream"); }
    wxString GetShortDescription() override;
    wxString GetLongDescription() override;

    // Toolbar
    int GetToolbarToolCount() override { return 1; }
    void OnToolbarToolCallback(int id) override;

    // Preferences
    void ShowPreferencesDialog(wxWindow* parent) override;

    // Canvas notifications
    void SetCurrentViewPort(PlugIn_ViewPort& vp) override;
    bool RenderOverlay(wxDC& dc, PlugIn_ViewPort* vp) override;
    bool RenderGLOverlay(wxGLContext* pcontext, PlugIn_ViewPort* vp) override;

    // Configuration
    void LoadConfig();
    void SaveConfig();

    // Streaming control
    void StartStreaming();
    void StopStreaming();
    bool IsStreaming() const { return m_streaming; }

    // Timer callback (called from timer owner)
    void OnFrameTimer();

    // Zoom callback from TCP client
    void OnZoomCommand(uint8_t direction);

    // Settings accessors
    wxString GetESP32IP() const { return m_esp32_ip; }
    void SetESP32IP(const wxString& ip) { m_esp32_ip = ip; }

    uint16_t GetScreenWidth() const { return m_screen_width; }
    void SetScreenWidth(uint16_t w) { m_screen_width = w; }

    uint16_t GetScreenHeight() const { return m_screen_height; }
    void SetScreenHeight(uint16_t h) { m_screen_height = h; }

    int GetJPEGQuality() const { return m_jpeg_quality; }
    void SetJPEGQuality(int q) { m_jpeg_quality = q; }

private:
    void SendChartFrame();

    wxBitmap* m_plugin_bitmap;
    int m_toolbar_id;
    bool m_streaming;

    // Settings
    wxString m_esp32_ip;
    uint16_t m_screen_width;
    uint16_t m_screen_height;
    int m_jpeg_quality;

    // Components
    std::unique_ptr<ChartStreamClient> m_client;
    std::unique_ptr<ChartRenderer> m_renderer;
    std::unique_ptr<PluginTimerOwner> m_timer_owner;

    // State
    PlugIn_ViewPort m_current_vp;
    uint16_t m_frame_seq;
    uint8_t m_zoom_level;
};

// Helper class to own the timer and receive events
class PluginTimerOwner : public wxEvtHandler {
public:
    PluginTimerOwner(saildrop_pi* plugin);
    ~PluginTimerOwner();

    void Start(int interval_ms);
    void Stop();
    bool IsRunning() const;

private:
    void OnTimer(wxTimerEvent& event);

    saildrop_pi* m_plugin;
    wxTimer m_timer;

    wxDECLARE_EVENT_TABLE();
};

#endif // SAILDROP_PI_H
