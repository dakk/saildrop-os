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
#include "settings_dialog.h"
#include "saildrop_pi.h"
#include "protocol.h"

enum {
    ID_IP_CTRL = wxID_HIGHEST + 1,
    ID_WIDTH_CTRL,
    ID_HEIGHT_CTRL,
    ID_QUALITY_CTRL
};

wxBEGIN_EVENT_TABLE(SettingsDialog, wxDialog)
    EVT_BUTTON(wxID_OK, SettingsDialog::OnOK)
    EVT_BUTTON(wxID_CANCEL, SettingsDialog::OnCancel)
    EVT_SLIDER(ID_QUALITY_CTRL, SettingsDialog::OnQualityChanged)
wxEND_EVENT_TABLE()

SettingsDialog::SettingsDialog(wxWindow* parent, saildrop_pi* plugin)
    : wxDialog(parent, wxID_ANY, _("Saildrop Chart Stream Settings"),
               wxDefaultPosition, wxSize(350, 280))
    , m_plugin(plugin)
{
    wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);

    // ESP32 IP Address
    wxStaticBoxSizer* conn_sizer = new wxStaticBoxSizer(wxVERTICAL, this,
        _("Connection"));

    wxFlexGridSizer* ip_grid = new wxFlexGridSizer(2, 2, 5, 10);
    ip_grid->AddGrowableCol(1);

    ip_grid->Add(new wxStaticText(this, wxID_ANY, _("ESP32 IP Address:")),
        0, wxALIGN_CENTER_VERTICAL);
    m_ip_ctrl = new wxTextCtrl(this, ID_IP_CTRL, plugin->GetESP32IP());
    ip_grid->Add(m_ip_ctrl, 1, wxEXPAND);

    conn_sizer->Add(ip_grid, 0, wxEXPAND | wxALL, 5);
    main_sizer->Add(conn_sizer, 0, wxEXPAND | wxALL, 10);

    // Screen Size
    wxStaticBoxSizer* size_sizer = new wxStaticBoxSizer(wxVERTICAL, this,
        _("Screen Size"));

    wxFlexGridSizer* size_grid = new wxFlexGridSizer(2, 2, 5, 10);
    size_grid->AddGrowableCol(1);

    size_grid->Add(new wxStaticText(this, wxID_ANY, _("Width (pixels):")),
        0, wxALIGN_CENTER_VERTICAL);
    m_width_ctrl = new wxSpinCtrl(this, ID_WIDTH_CTRL, wxEmptyString,
        wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 64, 480,
        plugin->GetScreenWidth());
    size_grid->Add(m_width_ctrl, 1, wxEXPAND);

    size_grid->Add(new wxStaticText(this, wxID_ANY, _("Height (pixels):")),
        0, wxALIGN_CENTER_VERTICAL);
    m_height_ctrl = new wxSpinCtrl(this, ID_HEIGHT_CTRL, wxEmptyString,
        wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 64, 480,
        plugin->GetScreenHeight());
    size_grid->Add(m_height_ctrl, 1, wxEXPAND);

    size_sizer->Add(size_grid, 0, wxEXPAND | wxALL, 5);
    main_sizer->Add(size_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT, 10);

    // JPEG Quality
    wxStaticBoxSizer* quality_sizer = new wxStaticBoxSizer(wxVERTICAL, this,
        _("Image Quality"));

    wxBoxSizer* quality_row = new wxBoxSizer(wxHORIZONTAL);

    m_quality_ctrl = new wxSlider(this, ID_QUALITY_CTRL,
        plugin->GetJPEGQuality(), 30, 95,
        wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL);
    quality_row->Add(m_quality_ctrl, 1, wxEXPAND | wxRIGHT, 10);

    m_quality_label = new wxStaticText(this, wxID_ANY,
        wxString::Format("%d%%", plugin->GetJPEGQuality()));
    quality_row->Add(m_quality_label, 0, wxALIGN_CENTER_VERTICAL);

    quality_sizer->Add(quality_row, 0, wxEXPAND | wxALL, 5);

    wxStaticText* quality_hint = new wxStaticText(this, wxID_ANY,
        _("Lower quality = smaller images, faster transfer"));
    quality_hint->SetForegroundColour(wxColour(128, 128, 128));
    quality_sizer->Add(quality_hint, 0, wxLEFT | wxBOTTOM, 5);

    main_sizer->Add(quality_sizer, 0, wxEXPAND | wxALL, 10);

    // Buttons
    wxStdDialogButtonSizer* btn_sizer = new wxStdDialogButtonSizer();
    btn_sizer->AddButton(new wxButton(this, wxID_OK, _("OK")));
    btn_sizer->AddButton(new wxButton(this, wxID_CANCEL, _("Cancel")));
    btn_sizer->Realize();
    main_sizer->Add(btn_sizer, 0, wxEXPAND | wxALL, 10);

    SetSizer(main_sizer);
    main_sizer->SetSizeHints(this);
    Centre();
}

SettingsDialog::~SettingsDialog() {
}

void SettingsDialog::OnQualityChanged(wxCommandEvent& event) {
    m_quality_label->SetLabel(wxString::Format("%d%%", m_quality_ctrl->GetValue()));
}

void SettingsDialog::OnOK(wxCommandEvent& event) {
    // Validate IP address
    wxString ip = m_ip_ctrl->GetValue().Trim();
    if (ip.IsEmpty()) {
        wxMessageBox(_("Please enter an IP address"), _("Error"),
            wxOK | wxICON_ERROR, this);
        return;
    }

    // Save settings to plugin
    m_plugin->SetESP32IP(ip);
    m_plugin->SetScreenWidth(m_width_ctrl->GetValue());
    m_plugin->SetScreenHeight(m_height_ctrl->GetValue());
    m_plugin->SetJPEGQuality(m_quality_ctrl->GetValue());
    m_plugin->SaveConfig();

    EndModal(wxID_OK);
}

void SettingsDialog::OnCancel(wxCommandEvent& event) {
    EndModal(wxID_CANCEL);
}
