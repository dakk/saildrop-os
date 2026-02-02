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
#ifndef SETTINGS_DIALOG_H
#define SETTINGS_DIALOG_H

#include <wx/wx.h>
#include <wx/spinctrl.h>

class saildrop_pi;

class SettingsDialog : public wxDialog {
public:
    SettingsDialog(wxWindow* parent, saildrop_pi* plugin);
    ~SettingsDialog();

private:
    void OnOK(wxCommandEvent& event);
    void OnCancel(wxCommandEvent& event);

    saildrop_pi* m_plugin;

    // Controls
    wxTextCtrl* m_ip_ctrl;
    wxSpinCtrl* m_width_ctrl;
    wxSpinCtrl* m_height_ctrl;
    wxSlider* m_quality_ctrl;
    wxStaticText* m_quality_label;

    void OnQualityChanged(wxCommandEvent& event);

    wxDECLARE_EVENT_TABLE();
};

#endif // SETTINGS_DIALOG_H
