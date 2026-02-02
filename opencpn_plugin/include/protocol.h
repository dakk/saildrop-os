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
#ifndef SAILDROP_PROTOCOL_H
#define SAILDROP_PROTOCOL_H

#include <cstdint>
#include <cstddef>

namespace saildrop {

// Protocol constants
constexpr uint8_t PROTOCOL_MAGIC = 0xCA;
constexpr uint16_t DEFAULT_PORT = 2002;
constexpr uint16_t DEFAULT_SCREEN_WIDTH = 240;
constexpr uint16_t DEFAULT_SCREEN_HEIGHT = 240;
constexpr int DEFAULT_JPEG_QUALITY = 70;
constexpr int DEFAULT_FPS = 1;

// Message types: OpenCPN -> ESP32
constexpr uint8_t MSG_CHART_IMAGE = 0x01;   // seq(2) + zoom(1) + jpeg_data
constexpr uint8_t MSG_CONFIG = 0x02;        // width(2) + height(2)

// Message types: ESP32 -> OpenCPN
constexpr uint8_t MSG_ZOOM_CMD = 0x81;      // direction(1)
constexpr uint8_t MSG_REFRESH = 0x82;       // (empty)
constexpr uint8_t MSG_HEARTBEAT = 0x83;     // (empty)

// Zoom directions
constexpr uint8_t ZOOM_IN = 0x01;
constexpr uint8_t ZOOM_OUT = 0x02;
constexpr uint8_t ZOOM_RESET = 0x03;

// Packet header structure
// [MAGIC:1][TYPE:1][LEN:2 big-endian][PAYLOAD:LEN bytes]
constexpr size_t HEADER_SIZE = 4;
constexpr size_t MAX_PAYLOAD_SIZE = 65535;

// Helper functions for big-endian encoding/decoding
inline uint16_t read_u16_be(const uint8_t* data) {
    return (static_cast<uint16_t>(data[0]) << 8) | data[1];
}

inline void write_u16_be(uint8_t* data, uint16_t value) {
    data[0] = (value >> 8) & 0xFF;
    data[1] = value & 0xFF;
}

} // namespace saildrop

#endif // SAILDROP_PROTOCOL_H
