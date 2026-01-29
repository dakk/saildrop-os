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
#include "ais.h"
#include <cstring>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define DEG_TO_RAD (M_PI / 180.0f)
#define RAD_TO_DEG (180.0f / M_PI)

// Global AIS manager instance
static AISManager ais_manager_instance;

AISManager* get_ais_manager() {
    return &ais_manager_instance;
}

AISManager::AISManager() {
    memset(targets, 0, sizeof(targets));
    memset(fragment_buffer, 0, sizeof(fragment_buffer));
    expected_fragments = 0;
    received_fragments = 0;
    fragment_msg_id = 0;
}

// Decode 6-bit ASCII armored character to 6-bit value
uint8_t AISManager::decode_char(char c) {
    uint8_t val = c - 48;
    if (val > 40) val -= 8;
    return val & 0x3F;
}

// Extract unsigned bits from decoded payload
uint32_t AISManager::extract_bits(const uint8_t* payload, int start, int len) {
    uint32_t result = 0;
    for (int i = 0; i < len; i++) {
        int bit_pos = start + i;
        int byte_idx = bit_pos / 8;
        int bit_idx = 7 - (bit_pos % 8);
        if (payload[byte_idx] & (1 << bit_idx)) {
            result |= (1 << (len - 1 - i));
        }
    }
    return result;
}

// Extract signed bits (two's complement)
int32_t AISManager::extract_signed_bits(const uint8_t* payload, int start, int len) {
    uint32_t raw = extract_bits(payload, start, len);
    // Check sign bit
    if (raw & (1 << (len - 1))) {
        // Negative: sign extend
        return (int32_t)(raw | (~0U << len));
    }
    return (int32_t)raw;
}

// Find existing target by MMSI or allocate new slot
int AISManager::find_or_create_slot(uint32_t mmsi) {
    int oldest_slot = -1;
    uint32_t oldest_time = UINT32_MAX;

    for (int i = 0; i < AIS_MAX_TARGETS; i++) {
        if (targets[i].valid && targets[i].mmsi == mmsi) {
            return i;  // Found existing
        }
        if (!targets[i].valid) {
            return i;  // Found empty slot
        }
        // Track oldest for replacement
        if (targets[i].last_update < oldest_time) {
            oldest_time = targets[i].last_update;
            oldest_slot = i;
        }
    }

    // All slots full, replace oldest
    return oldest_slot;
}

// Parse AIS message types 1, 2, 3 (Class A position reports)
void AISManager::parse_position_report(const uint8_t* payload, int payload_bits) {
    if (payload_bits < 168) return;  // Minimum for type 1/2/3

    uint8_t msg_type = extract_bits(payload, 0, 6);
    if (msg_type < 1 || msg_type > 3) return;

    uint32_t mmsi = extract_bits(payload, 8, 30);
    uint8_t nav_status = extract_bits(payload, 38, 4);
    // int8_t rot = extract_signed_bits(payload, 42, 8);  // Rate of turn (unused)
    uint16_t sog = extract_bits(payload, 50, 10);  // 0.1 knot units
    // uint8_t pos_accuracy = extract_bits(payload, 60, 1);
    int32_t lon_raw = extract_signed_bits(payload, 61, 28);  // 1/10000 min
    int32_t lat_raw = extract_signed_bits(payload, 89, 27);  // 1/10000 min
    uint16_t cog = extract_bits(payload, 116, 12);  // 0.1 degree units
    uint16_t heading = extract_bits(payload, 128, 9);

    // Validate coordinates (181 deg = not available)
    if (lon_raw == 0x6791AC0 || lat_raw == 0x3412140) return;

    // Convert from 1/10000 minutes to microdegrees
    // 1 minute = 1/60 degree
    // microdeg = (raw / 10000) / 60 * 1000000 = raw * 1000000 / 600000 = raw * 5 / 3
    int32_t lat = (int32_t)((int64_t)lat_raw * 5 / 3);
    int32_t lon = (int32_t)((int64_t)lon_raw * 5 / 3);

    // Find or create slot
    int slot = find_or_create_slot(mmsi);
    if (slot < 0) return;

    // Update target
    targets[slot].mmsi = mmsi;
    targets[slot].lat = lat;
    targets[slot].lon = lon;
    targets[slot].cog = cog;
    targets[slot].sog = sog;
    targets[slot].heading = heading;
    targets[slot].nav_status = nav_status;
    targets[slot].last_update = millis();
    targets[slot].valid = true;

    Serial.printf("AIS Target: MMSI=%lu, lat=%ld, lon=%ld, COG=%.1f, SOG=%.1f\n",
                  mmsi, lat, lon, cog / 10.0f, sog / 10.0f);
}

// Process AIVDM/AIVDO sentence
void AISManager::process_sentence(const char* sentence) {
    if (sentence == nullptr) return;
    if (strncmp(sentence, "!AIVDM", 6) != 0 && strncmp(sentence, "!AIVDO", 6) != 0) return;

    // Parse sentence fields
    // Format: !AIVDM,frag_count,frag_num,seq_id,channel,payload,fill*checksum
    const char* p = sentence + 7;  // Skip "!AIVDM,"

    // Field 1: Fragment count
    int frag_count = atoi(p);
    while (*p && *p != ',') p++;
    if (*p) p++;

    // Field 2: Fragment number
    int frag_num = atoi(p);
    while (*p && *p != ',') p++;
    if (*p) p++;

    // Field 3: Sequential message ID (for multi-part)
    int seq_id = 0;
    if (*p && *p != ',') seq_id = atoi(p);
    while (*p && *p != ',') p++;
    if (*p) p++;

    // Field 4: Channel (A or B)
    while (*p && *p != ',') p++;
    if (*p) p++;

    // Field 5: Payload
    const char* payload_start = p;
    while (*p && *p != ',') p++;
    int payload_len = p - payload_start;

    // Field 6: Fill bits (before checksum)
    int fill_bits = 0;
    if (*p == ',') {
        p++;
        fill_bits = atoi(p);
    }

    // Handle multi-fragment messages
    if (frag_count > 1) {
        if (frag_num == 1) {
            // First fragment - start new assembly
            memset(fragment_buffer, 0, sizeof(fragment_buffer));
            strncpy(fragment_buffer, payload_start, payload_len);
            expected_fragments = frag_count;
            received_fragments = 1;
            fragment_msg_id = seq_id;
            return;
        } else if (seq_id == fragment_msg_id && frag_num == received_fragments + 1) {
            // Continuation fragment
            int current_len = strlen(fragment_buffer);
            if (current_len + payload_len < sizeof(fragment_buffer) - 1) {
                strncat(fragment_buffer, payload_start, payload_len);
                received_fragments++;
            }
            if (received_fragments < expected_fragments) return;
            // Complete - use assembled buffer
            payload_start = fragment_buffer;
            payload_len = strlen(fragment_buffer);
        } else {
            // Out of sequence - discard
            expected_fragments = 0;
            received_fragments = 0;
            return;
        }
    }

    // Decode 6-bit armored payload to binary
    uint8_t decoded[28];  // Max ~168 bits for type 1/2/3
    memset(decoded, 0, sizeof(decoded));

    int bit_idx = 0;
    for (int i = 0; i < payload_len; i++) {
        uint8_t val = decode_char(payload_start[i]);
        // Pack 6 bits into decoded array
        for (int b = 5; b >= 0; b--) {
            int byte_pos = bit_idx / 8;
            int bit_pos = 7 - (bit_idx % 8);
            if (byte_pos < sizeof(decoded)) {
                if (val & (1 << b)) {
                    decoded[byte_pos] |= (1 << bit_pos);
                }
            }
            bit_idx++;
        }
    }

    int total_bits = bit_idx - fill_bits;

    // Check message type
    uint8_t msg_type = extract_bits(decoded, 0, 6);
    if (msg_type >= 1 && msg_type <= 3) {
        parse_position_report(decoded, total_bits);
    }
    // Types 4, 5, 18, 19, etc. could be added here

    // Reset fragment buffer after processing
    expected_fragments = 0;
    received_fragments = 0;
}

const ais_target* AISManager::get_target(uint8_t index) const {
    if (index >= AIS_MAX_TARGETS) return nullptr;
    return &targets[index];
}

uint8_t AISManager::get_active_count() const {
    uint8_t count = 0;
    for (int i = 0; i < AIS_MAX_TARGETS; i++) {
        if (targets[i].valid) count++;
    }
    return count;
}

void AISManager::cleanup_stale() {
    uint32_t now = millis();
    for (int i = 0; i < AIS_MAX_TARGETS; i++) {
        if (targets[i].valid) {
            if (now - targets[i].last_update > AIS_TARGET_TIMEOUT_MS) {
                targets[i].valid = false;
                Serial.printf("AIS: Removed stale target MMSI=%lu\n", targets[i].mmsi);
            }
        }
    }
}

// Calculate bearing from own position to target (degrees true)
float calculate_bearing(int32_t own_lat, int32_t own_lon, int32_t tgt_lat, int32_t tgt_lon) {
    float lat1 = own_lat * 1e-6f * DEG_TO_RAD;
    float lon1 = own_lon * 1e-6f * DEG_TO_RAD;
    float lat2 = tgt_lat * 1e-6f * DEG_TO_RAD;
    float lon2 = tgt_lon * 1e-6f * DEG_TO_RAD;

    float dLon = lon2 - lon1;
    float y = sinf(dLon) * cosf(lat2);
    float x = cosf(lat1) * sinf(lat2) - sinf(lat1) * cosf(lat2) * cosf(dLon);
    float bearing = atan2f(y, x) * RAD_TO_DEG;

    return fmodf(bearing + 360.0f, 360.0f);
}

// Calculate distance in nautical miles (Haversine formula)
float calculate_distance_nm(int32_t own_lat, int32_t own_lon, int32_t tgt_lat, int32_t tgt_lon) {
    float lat1 = own_lat * 1e-6f * DEG_TO_RAD;
    float lon1 = own_lon * 1e-6f * DEG_TO_RAD;
    float lat2 = tgt_lat * 1e-6f * DEG_TO_RAD;
    float lon2 = tgt_lon * 1e-6f * DEG_TO_RAD;

    float dLat = lat2 - lat1;
    float dLon = lon2 - lon1;

    float a = sinf(dLat / 2) * sinf(dLat / 2) +
              cosf(lat1) * cosf(lat2) * sinf(dLon / 2) * sinf(dLon / 2);
    float c = 2 * atan2f(sqrtf(a), sqrtf(1 - a));

    // Earth radius in nautical miles
    return 3440.065f * c;
}
