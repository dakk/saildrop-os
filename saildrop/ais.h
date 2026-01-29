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
#ifndef AIS_H
#define AIS_H

#include <stdint.h>
#include <Arduino.h>

#define AIS_MAX_TARGETS 8
#define AIS_TARGET_TIMEOUT_MS 180000  // 3 minutes stale timeout

// AIS Target data structure
struct ais_target {
    uint32_t mmsi;          // Maritime Mobile Service Identity
    int32_t lat;            // Latitude in microdegrees
    int32_t lon;            // Longitude in microdegrees
    uint16_t cog;           // Course over ground (0.1 degree units, 0-3599)
    uint16_t sog;           // Speed over ground (0.1 knot units)
    uint16_t heading;       // True heading (degrees, 511=unavailable)
    uint8_t nav_status;     // Navigation status (0-15)
    uint32_t last_update;   // millis() timestamp
    bool valid;             // Target slot in use
};

// AIS target manager - handles parsing and target storage
class AISManager {
private:
    ais_target targets[AIS_MAX_TARGETS];

    // Multi-part message assembly buffer
    char fragment_buffer[168];  // Max payload for multi-part
    uint8_t expected_fragments;
    uint8_t received_fragments;
    uint8_t fragment_msg_id;

    // 6-bit ASCII decoding helper
    uint8_t decode_char(char c);

    // Bit extraction helpers
    uint32_t extract_bits(const uint8_t* payload, int start, int len);
    int32_t extract_signed_bits(const uint8_t* payload, int start, int len);

    // Parse decoded message (Types 1, 2, 3)
    void parse_position_report(const uint8_t* payload, int payload_bits);

    // Find target by MMSI or get free slot
    int find_or_create_slot(uint32_t mmsi);

public:
    AISManager();

    // Process raw AIVDM/AIVDO sentence
    void process_sentence(const char* sentence);

    // Get target by index (0 to AIS_MAX_TARGETS-1)
    const ais_target* get_target(uint8_t index) const;

    // Get count of active (valid) targets
    uint8_t get_active_count() const;

    // Remove stale targets (call periodically)
    void cleanup_stale();
};

// Global AIS manager singleton
AISManager* get_ais_manager();

// Navigation calculations for display
float calculate_bearing(int32_t own_lat, int32_t own_lon, int32_t tgt_lat, int32_t tgt_lon);
float calculate_distance_nm(int32_t own_lat, int32_t own_lon, int32_t tgt_lat, int32_t tgt_lon);

#endif
