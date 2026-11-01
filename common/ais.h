/*
 * ais.h - AIS (AIVDM/AIVDO) parser and target table. Header-only so any sketch
 * can include it via ../common/ais.h (arduino-cli only compiles sources inside
 * the sketch dir, so shared code must be header-only).
 *
 * Extends the saildrop parser with:
 *  - more target slots (AIS_MAX_TARGETS)
 *  - Class B position reports (types 18, 19)
 *  - ship names (type 5 and type 24 part A)
 *  - geographic bounding-box filter (AIS_BBOX_* in conf.h or defaults below)
 *  - debug_inject() for SHOWCASE demo mode
 *
 * Host-testable: when ARDUINO is not defined, millis() must be provided by
 * the test harness and logging goes to stdout.
 */
#ifndef AIS_H
#define AIS_H

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#ifdef ARDUINO
    #include <Arduino.h>
    #define AIS_LOG(...) Serial.printf(__VA_ARGS__)
#else
    #include <stdio.h>
    unsigned long millis();  // provided by the host test harness
    #define AIS_LOG(...) do { printf(__VA_ARGS__); } while (0)
#endif

#ifndef AIS_MAX_TARGETS
#define AIS_MAX_TARGETS 32
#endif

#define AIS_TARGET_TIMEOUT_MS 180000  // 3 minutes stale timeout

// Gulf of Cagliari bounding box (microdegrees); override in conf.h
#ifndef AIS_BBOX_LAT_MIN
#define AIS_BBOX_LAT_MIN 38850000   // 38.85 N
#define AIS_BBOX_LAT_MAX 39300000   // 39.30 N
#define AIS_BBOX_LON_MIN 8800000    //  8.80 E
#define AIS_BBOX_LON_MAX 9600000    //  9.60 E
#endif

struct ais_target {
    uint32_t mmsi;
    int32_t lat;            // microdegrees
    int32_t lon;            // microdegrees
    uint16_t cog;           // 0.1 deg units (0-3599), 3600 = unavailable
    uint16_t sog;           // 0.1 knot units
    uint16_t heading;       // degrees, 511 = unavailable
    uint8_t nav_status;     // 0-15 (Class A)
    uint8_t class_b;        // 1 for Class B (types 18/19)
    char name[21];          // from type 5 / 24, may be empty
    uint32_t last_update;   // millis()
    bool valid;
};

class AISManager {
public:
    AISManager() {
        memset(targets, 0, sizeof(targets));
        memset(fragment_buffer, 0, sizeof(fragment_buffer));
        expected_fragments = 0;
        received_fragments = 0;
        fragment_msg_id = 0;
    }

    // Process one raw AIVDM/AIVDO sentence (with or without trailing \r\n)
    void process_sentence(const char* sentence) {
        if (sentence == nullptr) return;
        if (strncmp(sentence, "!AIVDM", 6) != 0 && strncmp(sentence, "!AIVDO", 6) != 0) return;

        const char* p = sentence + 7;  // skip "!AIVDM,"

        int frag_count = atoi(p);
        while (*p && *p != ',') p++;
        if (*p) p++;

        int frag_num = atoi(p);
        while (*p && *p != ',') p++;
        if (*p) p++;

        int seq_id = 0;
        if (*p && *p != ',') seq_id = atoi(p);
        while (*p && *p != ',') p++;
        if (*p) p++;

        while (*p && *p != ',') p++;  // channel
        if (*p) p++;

        const char* payload_start = p;
        while (*p && *p != ',') p++;
        int payload_len = (int)(p - payload_start);

        int fill_bits = 0;
        if (*p == ',') {
            p++;
            fill_bits = atoi(p);
        }

        if (frag_count > 1) {
            if (frag_num == 1) {
                memset(fragment_buffer, 0, sizeof(fragment_buffer));
                strncpy(fragment_buffer, payload_start, payload_len);
                expected_fragments = frag_count;
                received_fragments = 1;
                fragment_msg_id = seq_id;
                return;
            } else if (seq_id == fragment_msg_id && frag_num == received_fragments + 1) {
                int current_len = (int)strlen(fragment_buffer);
                if (current_len + payload_len < (int)sizeof(fragment_buffer) - 1) {
                    strncat(fragment_buffer, payload_start, payload_len);
                    received_fragments++;
                }
                if (received_fragments < expected_fragments) return;
                payload_start = fragment_buffer;
                payload_len = (int)strlen(fragment_buffer);
            } else {
                expected_fragments = 0;
                received_fragments = 0;
                return;
            }
        }

        // Decode 6-bit armored payload to binary (type 5 needs up to 424 bits)
        uint8_t decoded[84];
        memset(decoded, 0, sizeof(decoded));

        int bit_idx = 0;
        for (int i = 0; i < payload_len; i++) {
            uint8_t val = decode_char(payload_start[i]);
            for (int b = 5; b >= 0; b--) {
                int byte_pos = bit_idx / 8;
                int bit_pos = 7 - (bit_idx % 8);
                if (byte_pos < (int)sizeof(decoded)) {
                    if (val & (1 << b)) decoded[byte_pos] |= (1 << bit_pos);
                }
                bit_idx++;
            }
        }
        int total_bits = bit_idx - fill_bits;

        uint8_t msg_type = (uint8_t)extract_bits(decoded, 0, 6);
        switch (msg_type) {
            case 1: case 2: case 3: parse_position_report(decoded, total_bits, 0); break;
            case 18: case 19:       parse_position_report(decoded, total_bits, 1); break;
            case 5:                 parse_static_name(decoded, total_bits); break;
            case 24:                parse_static_name_24(decoded, total_bits); break;
            default: break;
        }

        expected_fragments = 0;
        received_fragments = 0;
    }

    const ais_target* get_target(uint8_t index) const {
        if (index >= AIS_MAX_TARGETS) return nullptr;
        return &targets[index];
    }

    uint8_t get_active_count() const {
        uint8_t count = 0;
        for (int i = 0; i < AIS_MAX_TARGETS; i++)
            if (targets[i].valid) count++;
        return count;
    }

    void cleanup_stale() {
        uint32_t now = millis();
        for (int i = 0; i < AIS_MAX_TARGETS; i++) {
            if (targets[i].valid && now - targets[i].last_update > AIS_TARGET_TIMEOUT_MS) {
                AIS_LOG("AIS: stale target MMSI=%lu removed\n", (unsigned long)targets[i].mmsi);
                targets[i].valid = false;
            }
        }
    }

    // Demo mode (SHOWCASE): inject/move a synthetic target
    void debug_inject(uint32_t mmsi, int32_t lat, int32_t lon, uint16_t cog,
                      uint16_t sog, uint16_t heading, const char* name) {
        int slot = find_or_create_slot(mmsi);
        if (slot < 0) return;
        ais_target &t = targets[slot];
        t.mmsi = mmsi;
        t.lat = lat;
        t.lon = lon;
        t.cog = cog;
        t.sog = sog;
        t.heading = heading;
        t.last_update = millis();
        t.valid = true;
        if (name) strncpy(t.name, name, sizeof(t.name) - 1);
    }

private:
    ais_target targets[AIS_MAX_TARGETS];
    char fragment_buffer[168];
    uint8_t expected_fragments;
    uint8_t received_fragments;
    uint8_t fragment_msg_id;

    static uint8_t decode_char(char c) {
        uint8_t val = c - 48;
        if (val > 40) val -= 8;
        return val & 0x3F;
    }

    static uint32_t extract_bits(const uint8_t* payload, int start, int len) {
        uint32_t result = 0;
        for (int i = 0; i < len; i++) {
            int bit_pos = start + i;
            int byte_idx = bit_pos / 8;
            int bit_idx = 7 - (bit_pos % 8);
            if (payload[byte_idx] & (1 << bit_idx))
                result |= (1u << (len - 1 - i));
        }
        return result;
    }

    static int32_t extract_signed_bits(const uint8_t* payload, int start, int len) {
        uint32_t raw = extract_bits(payload, start, len);
        if (raw & (1u << (len - 1)))
            return (int32_t)(raw | (~0u << len));
        return (int32_t)raw;
    }

    // AIS 6-bit text: 0-31 -> '@'..'_' (ASCII 64+), 32-63 -> ' '..'?'
    static void extract_text(const uint8_t* payload, int start, int len_bits, char* out, int out_len) {
        int n = len_bits / 6;
        if (n > out_len - 1) n = out_len - 1;
        for (int i = 0; i < n; i++) {
            uint8_t v = (uint8_t)extract_bits(payload, start + i * 6, 6);
            out[i] = v < 32 ? (char)(v + 64) : (char)v;
        }
        while (n > 0 && (out[n - 1] == '@' || out[n - 1] == ' ')) n--;
        out[n] = '\0';
    }

    static bool in_bbox(int32_t lat, int32_t lon) {
        return lat >= AIS_BBOX_LAT_MIN && lat <= AIS_BBOX_LAT_MAX &&
               lon >= AIS_BBOX_LON_MIN && lon <= AIS_BBOX_LON_MAX;
    }

    int find_slot(uint32_t mmsi) const {
        for (int i = 0; i < AIS_MAX_TARGETS; i++)
            if (targets[i].valid && targets[i].mmsi == mmsi) return i;
        return -1;
    }

    int find_or_create_slot(uint32_t mmsi) {
        int oldest_slot = -1;
        uint32_t oldest_time = UINT32_MAX;
        for (int i = 0; i < AIS_MAX_TARGETS; i++) {
            if (targets[i].valid && targets[i].mmsi == mmsi) return i;
            if (!targets[i].valid) return i;
            if (targets[i].last_update < oldest_time) {
                oldest_time = targets[i].last_update;
                oldest_slot = i;
            }
        }
        return oldest_slot;
    }

    // class_a: bit offsets differ between Class A (1/2/3) and Class B (18/19)
    void parse_position_report(const uint8_t* payload, int payload_bits, bool class_b) {
        int min_bits = class_b ? 168 : 168;
        if (payload_bits < min_bits) return;

        uint32_t mmsi = extract_bits(payload, 8, 30);
        int sog_off, lon_off, lat_off, cog_off, hdg_off;
        uint8_t nav_status = 0;
        if (class_b) {
            sog_off = 46; lon_off = 57; lat_off = 85; cog_off = 112; hdg_off = 124;
        } else {
            nav_status = (uint8_t)extract_bits(payload, 38, 4);
            sog_off = 50; lon_off = 61; lat_off = 89; cog_off = 116; hdg_off = 128;
        }

        uint16_t sog = (uint16_t)extract_bits(payload, sog_off, 10);
        int32_t lon_raw = extract_signed_bits(payload, lon_off, 28);  // 1/10000 min
        int32_t lat_raw = extract_signed_bits(payload, lat_off, 27);
        uint16_t cog = (uint16_t)extract_bits(payload, cog_off, 12);
        uint16_t heading = (uint16_t)extract_bits(payload, hdg_off, 9);

        // "not available" sentinels
        if (lon_raw == 0x6791AC0 || lat_raw == 0x3412140) return;

        // 1/10000 min -> microdegrees: raw * 5 / 3
        int32_t lat = (int32_t)((int64_t)lat_raw * 5 / 3);
        int32_t lon = (int32_t)((int64_t)lon_raw * 5 / 3);

        if (!in_bbox(lat, lon)) return;  // outside the gulf: ignore

        int slot = find_or_create_slot(mmsi);
        if (slot < 0) return;

        ais_target &t = targets[slot];
        t.mmsi = mmsi;
        t.lat = lat;
        t.lon = lon;
        t.cog = cog;
        t.sog = sog;
        t.heading = heading;
        t.nav_status = nav_status;
        t.class_b = class_b ? 1 : 0;
        t.last_update = millis();
        t.valid = true;

        AIS_LOG("AIS: MMSI=%lu lat=%.5f lon=%.5f COG=%.1f SOG=%.1f%s%s\n",
                (unsigned long)mmsi, lat * 1e-6f, lon * 1e-6f, cog / 10.0f, sog / 10.0f,
                t.name[0] ? " name=" : "", t.name);
    }

    // Type 5: static voyage data, vessel name at bit 112, 120 bits (20 chars)
    void parse_static_name(const uint8_t* payload, int payload_bits) {
        if (payload_bits < 232) return;
        uint32_t mmsi = extract_bits(payload, 8, 30);
        int slot = find_slot(mmsi);
        if (slot < 0) return;  // no position yet: skip (position report will come)
        extract_text(payload, 112, 120, targets[slot].name, sizeof(targets[slot].name));
    }

    // Type 24: Class B static; part A (partno 0) carries the name at bit 40
    void parse_static_name_24(const uint8_t* payload, int payload_bits) {
        if (payload_bits < 160) return;
        uint32_t mmsi = extract_bits(payload, 8, 30);
        uint8_t partno = (uint8_t)extract_bits(payload, 38, 2);
        if (partno != 0) return;
        int slot = find_slot(mmsi);
        if (slot < 0) return;
        extract_text(payload, 40, 120, targets[slot].name, sizeof(targets[slot].name));
    }
};

inline AISManager* get_ais_manager() {
    static AISManager manager;
    return &manager;
}

#endif // AIS_H
