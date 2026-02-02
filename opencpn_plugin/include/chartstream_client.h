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
#ifndef CHARTSTREAM_CLIENT_H
#define CHARTSTREAM_CLIENT_H

#include "protocol.h"

#include <wx/wx.h>
#include <wx/socket.h>
#include <functional>
#include <vector>

class saildrop_pi;

class ChartStreamClient : public wxEvtHandler {
public:
    ChartStreamClient(saildrop_pi* plugin);
    ~ChartStreamClient();

    // Connection management
    bool Connect(const wxString& host, uint16_t port = saildrop::DEFAULT_PORT);
    void Disconnect();
    bool IsConnected() const;

    // Send chart frame
    bool SendChartImage(const uint8_t* jpeg_data, size_t jpeg_size,
                        uint16_t seq_no, uint8_t zoom_level);

    // Send configuration
    bool SendConfig(uint16_t width, uint16_t height);

private:
    void OnSocketEvent(wxSocketEvent& event);
    void ProcessReceivedData();
    bool SendPacket(uint8_t type, const uint8_t* payload, size_t len);

    saildrop_pi* m_plugin;
    wxSocketClient* m_socket;

    // Receive buffer
    std::vector<uint8_t> m_rx_buffer;
    size_t m_rx_offset;

    // Connection state
    bool m_connected;
    wxString m_host;
    uint16_t m_port;

    wxDECLARE_EVENT_TABLE();
};

#endif // CHARTSTREAM_CLIENT_H
