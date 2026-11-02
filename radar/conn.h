/*
 * conn.h - WiFi station + TCP client for a raw NMEA AIS feed. Header-only
 * (shared via ../common/). Run conn_loop() on a dedicated core.
 *
 * Expected config defines (in the sketch's conf.h):
 *   WIFI_SSID, WIFI_PASSWORD        home WiFi
 *   AIS_HOST, AIS_PORT              endpoint streaming raw !AIVDM/!AIVDO lines
 *                                   (e.g. a LAN bridge filtering AISHub to the
 *                                   gulf bbox); wrap in TLS on the bridge if remote
 *   AIS_HELLO (optional)            line sent right after connect (API key etc.)
 */
#ifndef CONN_H
#define CONN_H

#include <WiFi.h>
#include "common/ais.h"

#ifndef AIS_RECONNECT_MS
#define AIS_RECONNECT_MS 5000
#endif

// Feed status for the UI status line (state lives in function-local statics
// below to stay header-only and C++11-safe)
inline WiFiClient& conn_tcp() { static WiFiClient c; return c; }
inline uint32_t& conn_last_rx() { static uint32_t t = 0; return t; }
inline uint32_t& conn_next_try() { static uint32_t t = 0; return t; }
inline uint32_t& conn_last_cleanup() { static uint32_t t = 0; return t; }

inline bool conn_feed_connected() {
    return conn_tcp().connected() && (millis() - conn_last_rx() < 30000);
}

inline uint32_t conn_feed_last_rx() { return conn_last_rx(); }

// AIS message rate: one ring slot per second, last 60 s
inline uint32_t& conn_msg_sec_count() { static uint32_t c = 0; return c; }  // msgs in current second
inline uint32_t* conn_msg_ring() { static uint32_t r[60] = {0}; return r; }
inline uint8_t& conn_msg_ring_pos() { static uint8_t p = 0; return p; }
inline uint32_t& conn_msg_last_sec() { static uint32_t t = 0; return t; }

inline uint32_t conn_msgs_per_min() {
    uint32_t sum = 0;
    uint32_t* r = conn_msg_ring();
    for (int i = 0; i < 60; i++) sum += r[i];
    return sum;
}

// call from conn_loop (runs every ~100 ms): bucket per-second message counts
inline void conn_msg_tick() {
    uint32_t now = millis();
    if (now - conn_msg_last_sec() >= 1000) {
        conn_msg_last_sec() = now;
        conn_msg_ring()[conn_msg_ring_pos()] = conn_msg_sec_count();
        conn_msg_ring_pos() = (conn_msg_ring_pos() + 1) % 60;
        conn_msg_sec_count() = 0;
    }
}

inline void conn_setup() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.printf("WiFi: connecting to %s ...\n", WIFI_SSID);
}

// Feed one received char into the AIS sentence assembler
inline void conn_ais_char(char c, char* buf, uint8_t &idx) {
    if (c == '!') idx = 0;
    if (idx < 99) {
        buf[idx++] = c;
        if (c == '\n' || c == '\r') {
            buf[idx] = '\0';
            if (strncmp(buf, "!AIVDM", 6) == 0 || strncmp(buf, "!AIVDO", 6) == 0) {
                get_ais_manager()->process_sentence(buf);
                conn_msg_sec_count()++;
            }
            idx = 0;
        }
    } else {
        idx = 0;
    }
}

inline void conn_loop() {
    // --- WiFi reconnect ---
    if (WiFi.status() != WL_CONNECTED) {
        static uint32_t wifi_retry = 0;
        if (millis() - wifi_retry > 10000) {
            wifi_retry = millis();
            Serial.println("WiFi: reconnecting...");
            WiFi.disconnect();
            WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        }
        return;
    }

    // --- TCP feed reconnect ---
    if (!conn_tcp().connected()) {
        if (millis() >= conn_next_try()) {
            conn_next_try() = millis() + AIS_RECONNECT_MS;
            Serial.printf("AIS: connecting to %s:%d ...\n", AIS_HOST, AIS_PORT);
            if (conn_tcp().connect(AIS_HOST, AIS_PORT)) {
                Serial.println("AIS: feed connected");
                conn_last_rx() = millis();
#ifdef AIS_HELLO
                conn_tcp().println(AIS_HELLO);
#endif
            } else {
                Serial.println("AIS: connect failed");
            }
        }
        return;
    }

    // --- Read feed ---
    static char ais_buf[100];
    static uint8_t ais_idx = 0;

    uint8_t packet[256];
    int n = conn_tcp().read(packet, sizeof(packet) - 1);
    if (n > 0) {
        conn_last_rx() = millis();
        for (int i = 0; i < n; i++)
            conn_ais_char((char)packet[i], ais_buf, ais_idx);
    }

    // Watchdog: connection alive but feed silent for 60 s -> force reconnect
    if (millis() - conn_last_rx() > 60000) {
        Serial.println("AIS: feed silent, reconnecting");
        conn_tcp().stop();
        conn_next_try() = 0;
    }

    // --- Periodic stale-target cleanup ---
    if (millis() - conn_last_cleanup() > 60000) {
        conn_last_cleanup() = millis();
        get_ais_manager()->cleanup_stale();
    }

    conn_msg_tick();
}

#endif // CONN_H
