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
#ifndef CHARTSTREAM_H
#define CHARTSTREAM_H

#include <WiFi.h>
#include "conf.h"

// Protocol constants
#define CHART_MAGIC 0xCA
#define CHART_HEADER_SIZE 4

// Message types (from OpenCPN)
#define MSG_CHART_IMAGE     0x01
#define MSG_CONFIG          0x02

// Message types (to OpenCPN)
#define MSG_ZOOM_CMD        0x81
#define MSG_REFRESH         0x82
#define MSG_HEARTBEAT       0x83

// Zoom directions
#define ZOOM_IN    0x01
#define ZOOM_OUT   0x02
#define ZOOM_RESET 0x03

// Buffer size for incoming JPEG (max ~40KB for 240x240 high quality)
#define CHART_IMAGE_BUFFER_SIZE (50 * 1024)

// Chart frame structure
struct ChartFrame {
    uint16_t seq_no;
    uint8_t zoom_level;
    uint8_t* jpeg_data;
    size_t jpeg_size;
    bool ready;
};

// Chart stream server class
class ChartStreamServer {
public:
    ChartStreamServer();
    ~ChartStreamServer();

    // Start the TCP server
    void begin();

    // Process incoming data (call from network loop)
    void loop();

    // Check if a client is connected
    bool isClientConnected();

    // Send commands to OpenCPN
    bool sendZoomCommand(uint8_t direction);
    bool sendRefreshRequest();
    bool sendHeartbeat();

    // Get the latest frame (check frame.ready first)
    ChartFrame* getFrame() { return &m_frame; }

    // Mark frame as consumed
    void clearFrame() { m_frame.ready = false; }

private:
    WiFiServer m_server;
    WiFiClient m_client;

    // Receive buffer
    uint8_t* m_rx_buffer;
    size_t m_rx_offset;

    // Current frame
    ChartFrame m_frame;
    uint8_t* m_jpeg_buffer;

    // State
    bool m_started;
    uint32_t m_last_heartbeat;

    // Process a complete packet
    void processPacket(uint8_t type, const uint8_t* payload, size_t len);

    // Send a packet
    bool sendPacket(uint8_t type, const uint8_t* payload, size_t len);
};

// Global accessor
ChartStreamServer* getChartStream();

#endif // CHARTSTREAM_H
