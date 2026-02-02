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
#include "chartstream_client.h"
#include "saildrop_pi.h"

enum {
    SOCKET_ID = wxID_HIGHEST + 100
};

wxBEGIN_EVENT_TABLE(ChartStreamClient, wxEvtHandler)
    EVT_SOCKET(SOCKET_ID, ChartStreamClient::OnSocketEvent)
wxEND_EVENT_TABLE()

ChartStreamClient::ChartStreamClient(saildrop_pi* plugin)
    : m_plugin(plugin)
    , m_socket(nullptr)
    , m_rx_offset(0)
    , m_connected(false)
    , m_port(saildrop::DEFAULT_PORT)
{
    m_rx_buffer.resize(saildrop::HEADER_SIZE + saildrop::MAX_PAYLOAD_SIZE);
}

ChartStreamClient::~ChartStreamClient() {
    Disconnect();
}

bool ChartStreamClient::Connect(const wxString& host, uint16_t port) {
    if (m_socket) {
        Disconnect();
    }

    m_host = host;
    m_port = port;

    wxIPV4address addr;
    addr.Hostname(host);
    addr.Service(port);

    m_socket = new wxSocketClient(wxSOCKET_NOWAIT);
    m_socket->SetEventHandler(*this, SOCKET_ID);
    m_socket->SetNotify(wxSOCKET_CONNECTION_FLAG |
                        wxSOCKET_INPUT_FLAG |
                        wxSOCKET_LOST_FLAG);
    m_socket->Notify(true);

    wxLogMessage("Saildrop: Connecting to %s:%d", host, port);
    m_socket->Connect(addr, false);  // Non-blocking connect

    return true;
}

void ChartStreamClient::Disconnect() {
    if (m_socket) {
        m_socket->Destroy();
        m_socket = nullptr;
    }
    m_connected = false;
    m_rx_offset = 0;
    wxLogMessage("Saildrop: Disconnected");
}

bool ChartStreamClient::IsConnected() const {
    return m_connected && m_socket && m_socket->IsConnected();
}

void ChartStreamClient::OnSocketEvent(wxSocketEvent& event) {
    switch (event.GetSocketEvent()) {
        case wxSOCKET_CONNECTION:
            m_connected = true;
            wxLogMessage("Saildrop: Connected to ESP32");
            // Send initial config
            SendConfig(m_plugin->GetScreenWidth(), m_plugin->GetScreenHeight());
            break;

        case wxSOCKET_INPUT:
            ProcessReceivedData();
            break;

        case wxSOCKET_LOST:
            wxLogMessage("Saildrop: Connection lost");
            m_connected = false;
            m_rx_offset = 0;
            break;

        default:
            break;
    }
}

void ChartStreamClient::ProcessReceivedData() {
    if (!m_socket) return;

    // Read available data into buffer
    while (m_socket->IsData()) {
        size_t available = m_rx_buffer.size() - m_rx_offset;
        if (available == 0) {
            wxLogWarning("Saildrop: RX buffer overflow");
            m_rx_offset = 0;
            return;
        }

        m_socket->Read(&m_rx_buffer[m_rx_offset], available);
        size_t read = m_socket->LastCount();
        if (read == 0) break;
        m_rx_offset += read;

        // Process complete packets
        while (m_rx_offset >= saildrop::HEADER_SIZE) {
            // Check magic
            if (m_rx_buffer[0] != saildrop::PROTOCOL_MAGIC) {
                // Sync error - skip byte
                memmove(&m_rx_buffer[0], &m_rx_buffer[1], m_rx_offset - 1);
                m_rx_offset--;
                continue;
            }

            uint8_t type = m_rx_buffer[1];
            uint16_t len = saildrop::read_u16_be(&m_rx_buffer[2]);

            // Check if we have complete packet
            if (m_rx_offset < saildrop::HEADER_SIZE + len) {
                break;  // Wait for more data
            }

            // Process packet based on type
            const uint8_t* payload = &m_rx_buffer[saildrop::HEADER_SIZE];

            switch (type) {
                case saildrop::MSG_ZOOM_CMD:
                    if (len >= 1) {
                        m_plugin->OnZoomCommand(payload[0]);
                    }
                    break;

                case saildrop::MSG_REFRESH:
                    // ESP32 requesting a frame - handled by timer
                    wxLogDebug("Saildrop: Refresh request received");
                    break;

                case saildrop::MSG_HEARTBEAT:
                    wxLogDebug("Saildrop: Heartbeat received");
                    break;

                default:
                    wxLogWarning("Saildrop: Unknown message type 0x%02X", type);
                    break;
            }

            // Remove processed packet from buffer
            size_t packet_size = saildrop::HEADER_SIZE + len;
            memmove(&m_rx_buffer[0], &m_rx_buffer[packet_size],
                    m_rx_offset - packet_size);
            m_rx_offset -= packet_size;
        }
    }
}

bool ChartStreamClient::SendPacket(uint8_t type, const uint8_t* payload, size_t len) {
    if (!IsConnected()) return false;

    std::vector<uint8_t> packet(saildrop::HEADER_SIZE + len);
    packet[0] = saildrop::PROTOCOL_MAGIC;
    packet[1] = type;
    saildrop::write_u16_be(&packet[2], static_cast<uint16_t>(len));

    if (len > 0 && payload) {
        memcpy(&packet[saildrop::HEADER_SIZE], payload, len);
    }

    m_socket->Write(packet.data(), packet.size());
    return m_socket->LastCount() == packet.size();
}

bool ChartStreamClient::SendChartImage(const uint8_t* jpeg_data, size_t jpeg_size,
                                       uint16_t seq_no, uint8_t zoom_level) {
    if (!IsConnected()) return false;

    // Build payload: seq(2) + zoom(1) + jpeg_data
    std::vector<uint8_t> payload(3 + jpeg_size);
    saildrop::write_u16_be(&payload[0], seq_no);
    payload[2] = zoom_level;
    memcpy(&payload[3], jpeg_data, jpeg_size);

    return SendPacket(saildrop::MSG_CHART_IMAGE, payload.data(), payload.size());
}

bool ChartStreamClient::SendConfig(uint16_t width, uint16_t height) {
    uint8_t payload[4];
    saildrop::write_u16_be(&payload[0], width);
    saildrop::write_u16_be(&payload[2], height);
    return SendPacket(saildrop::MSG_CONFIG, payload, sizeof(payload));
}
