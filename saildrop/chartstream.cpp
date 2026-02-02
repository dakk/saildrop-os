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
#include "chartstream.h"

#ifdef SCREEN_CHART

// Global instance
static ChartStreamServer* g_chart_stream = nullptr;

ChartStreamServer* getChartStream() {
    if (g_chart_stream == nullptr) {
        g_chart_stream = new ChartStreamServer();
    }
    return g_chart_stream;
}

ChartStreamServer::ChartStreamServer()
    : m_server(CHART_STREAM_PORT)
    , m_rx_buffer(nullptr)
    , m_rx_offset(0)
    , m_jpeg_buffer(nullptr)
    , m_started(false)
    , m_last_heartbeat(0)
{
    // Allocate buffers
    m_rx_buffer = new uint8_t[CHART_IMAGE_BUFFER_SIZE + CHART_HEADER_SIZE];
    m_jpeg_buffer = new uint8_t[CHART_IMAGE_BUFFER_SIZE];

    // Initialize frame
    m_frame.jpeg_data = m_jpeg_buffer;
    m_frame.jpeg_size = 0;
    m_frame.seq_no = 0;
    m_frame.zoom_level = 10;
    m_frame.ready = false;
}

ChartStreamServer::~ChartStreamServer() {
    if (m_rx_buffer) {
        delete[] m_rx_buffer;
        m_rx_buffer = nullptr;
    }
    if (m_jpeg_buffer) {
        delete[] m_jpeg_buffer;
        m_jpeg_buffer = nullptr;
    }
}

void ChartStreamServer::begin() {
    if (m_started) return;

    m_server.begin();
    m_started = true;

#ifdef DEBUG
    Serial.printf("ChartStream: Server started on port %d\n", CHART_STREAM_PORT);
#endif
}

void ChartStreamServer::loop() {
    if (!m_started) return;

    // Check for new client connections
    if (m_server.hasClient()) {
        // If we already have a client, disconnect it
        if (m_client && m_client.connected()) {
#ifdef DEBUG
            Serial.println("ChartStream: Disconnecting old client");
#endif
            m_client.stop();
        }

        m_client = m_server.available();
        m_rx_offset = 0;

#ifdef DEBUG
        Serial.printf("ChartStream: Client connected from %s\n",
                      m_client.remoteIP().toString().c_str());
#endif

        // Request first frame
        sendRefreshRequest();
    }

    // Process data from connected client
    if (m_client && m_client.connected()) {
        while (m_client.available()) {
            // Read into buffer
            size_t available = CHART_IMAGE_BUFFER_SIZE + CHART_HEADER_SIZE - m_rx_offset;
            if (available == 0) {
                // Buffer overflow - reset
#ifdef DEBUG
                Serial.println("ChartStream: Buffer overflow, resetting");
#endif
                m_rx_offset = 0;
                continue;
            }

            size_t read = m_client.read(&m_rx_buffer[m_rx_offset], available);
            if (read == 0) break;
            m_rx_offset += read;

            // Process complete packets
            while (m_rx_offset >= CHART_HEADER_SIZE) {
                // Check magic byte
                if (m_rx_buffer[0] != CHART_MAGIC) {
                    // Sync error - skip byte
                    memmove(&m_rx_buffer[0], &m_rx_buffer[1], m_rx_offset - 1);
                    m_rx_offset--;
                    continue;
                }

                uint8_t type = m_rx_buffer[1];
                uint16_t len = (m_rx_buffer[2] << 8) | m_rx_buffer[3];

                // Check if we have complete packet
                if (m_rx_offset < CHART_HEADER_SIZE + len) {
                    break;  // Wait for more data
                }

                // Process packet
                const uint8_t* payload = &m_rx_buffer[CHART_HEADER_SIZE];
                processPacket(type, payload, len);

                // Remove processed packet from buffer
                size_t packet_size = CHART_HEADER_SIZE + len;
                memmove(&m_rx_buffer[0], &m_rx_buffer[packet_size],
                        m_rx_offset - packet_size);
                m_rx_offset -= packet_size;
            }
        }

        // Send periodic heartbeat
        uint32_t now = millis();
        if (now - m_last_heartbeat > 5000) {
            sendHeartbeat();
            m_last_heartbeat = now;
        }
    }
}

bool ChartStreamServer::isClientConnected() {
    return m_client && m_client.connected();
}

void ChartStreamServer::processPacket(uint8_t type, const uint8_t* payload, size_t len) {
    switch (type) {
        case MSG_CHART_IMAGE:
            if (len >= 3) {
                // Parse: seq(2) + zoom(1) + jpeg_data
                m_frame.seq_no = (payload[0] << 8) | payload[1];
                m_frame.zoom_level = payload[2];
                m_frame.jpeg_size = len - 3;

                if (m_frame.jpeg_size <= CHART_IMAGE_BUFFER_SIZE) {
                    memcpy(m_frame.jpeg_data, &payload[3], m_frame.jpeg_size);
                    m_frame.ready = true;

#ifdef DEBUG
                    Serial.printf("ChartStream: Frame %d received, %d bytes, zoom %d\n",
                                  m_frame.seq_no, m_frame.jpeg_size, m_frame.zoom_level);
#endif
                }
            }
            break;

        case MSG_CONFIG:
            if (len >= 4) {
                uint16_t width = (payload[0] << 8) | payload[1];
                uint16_t height = (payload[2] << 8) | payload[3];
#ifdef DEBUG
                Serial.printf("ChartStream: Config received, %dx%d\n", width, height);
#endif
            }
            break;

        default:
#ifdef DEBUG
            Serial.printf("ChartStream: Unknown message type 0x%02X\n", type);
#endif
            break;
    }
}

bool ChartStreamServer::sendPacket(uint8_t type, const uint8_t* payload, size_t len) {
    if (!isClientConnected()) return false;

    uint8_t header[CHART_HEADER_SIZE];
    header[0] = CHART_MAGIC;
    header[1] = type;
    header[2] = (len >> 8) & 0xFF;
    header[3] = len & 0xFF;

    size_t written = m_client.write(header, CHART_HEADER_SIZE);
    if (written != CHART_HEADER_SIZE) return false;

    if (len > 0 && payload) {
        written = m_client.write(payload, len);
        if (written != len) return false;
    }

    return true;
}

bool ChartStreamServer::sendZoomCommand(uint8_t direction) {
#ifdef DEBUG
    Serial.printf("ChartStream: Sending zoom command %d\n", direction);
#endif
    return sendPacket(MSG_ZOOM_CMD, &direction, 1);
}

bool ChartStreamServer::sendRefreshRequest() {
    return sendPacket(MSG_REFRESH, nullptr, 0);
}

bool ChartStreamServer::sendHeartbeat() {
    return sendPacket(MSG_HEARTBEAT, nullptr, 0);
}

#endif // SCREEN_CHART
