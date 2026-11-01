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
#include "ais.h"

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
            if (strncmp(buf, "!AIVDM", 6) == 0 || strncmp(buf, "!AIVDO", 6) == 0)
                get_ais_manager()->process_sentence(buf);
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

    // --- Periodic stale-target cleanup ---
    if (millis() - conn_last_cleanup() > 60000) {
        conn_last_cleanup() = millis();
        get_ais_manager()->cleanup_stale();
    }
}

#endif // CONN_H
