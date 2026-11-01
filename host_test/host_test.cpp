/*
 * host_test.cpp - host-side unit test for common/geo.h and common/ais.h.
 * Build: make test   (g++ -std=c++17)
 *
 * Builds synthetic but spec-correct AIVDM sentences with a 6-bit encoder and
 * checks the decoder round-trip, the bbox filter, names (type 5 multi-part)
 * and the geo/projection math.
 */
#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>
#include <chrono>

unsigned long millis() {
    static auto t0 = std::chrono::steady_clock::now();
    return (unsigned long)std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - t0).count();
}

#include "../common/geo.h"
#include "../common/ais.h"

static int failures = 0;
#define CHECK(cond, ...) do { \
    if (!(cond)) { failures++; printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); } \
    else { printf("ok:   "); printf(__VA_ARGS__); printf("\n"); } \
} while (0)

// ---- 6-bit AIS payload encoder (inverse of the decoder) ----
struct BitWriter {
    uint8_t buf[84] = {0};
    int bits = 0;
    void put(uint32_t v, int len) {
        for (int i = len - 1; i >= 0; i--) {
            int bp = bits / 8, bi = 7 - (bits % 8);
            if ((v >> i) & 1) buf[bp] |= (1 << bi);
            bits++;
        }
    }
    void put_text(const char* s, int chars) {
        for (int i = 0; i < chars; i++) {
            char c = s[i];
            uint8_t v = (c >= 64) ? (uint8_t)(c - 64) : (uint8_t)c;
            put(v, 6);
        }
    }
    std::string armor() {
        while (bits % 6) put(0, 1);  // pad to multiple of 6
        std::string s;
        for (int i = 0; i + 6 <= bits; i += 6) {
            uint8_t v = 0;
            for (int b = 0; b < 6; b++) {
                int bp = (i + b) / 8, bi = 7 - ((i + b) % 8);
                if (buf[bp] & (1 << bi)) v |= (1 << (5 - b));
            }
            char c = v + 48;
            if (v > 39) c += 8;
            s += c;
        }
        return s;
    }
};

static std::string sentence1(const std::string& payload) {
    return "!AIVDM,1,1,,A," + payload + ",0*00";
}

// Type 1/2/3 Class A position report
static std::string make_type1(uint32_t mmsi, double lat_deg, double lon_deg,
                              uint16_t sog, uint16_t cog, uint16_t hdg, uint8_t nav) {
    BitWriter w;
    w.put(1, 6);            // type
    w.put(0, 2);            // repeat
    w.put(mmsi, 30);
    w.put(nav, 4);          // nav status
    w.put(0, 8);            // rot
    w.put(sog, 10);
    w.put(0, 1);            // pos accuracy
    w.put((uint32_t)(lon_deg * 600000), 28);  // signed in reality; positive here
    w.put((uint32_t)(lat_deg * 600000), 27);
    w.put(cog, 12);
    w.put(hdg, 9);
    w.put(0, 6);            // timestamp
    while (w.bits < 168) w.put(0, 1);
    return sentence1(w.armor());
}

// Type 18 Class B position report
static std::string make_type18(uint32_t mmsi, double lat_deg, double lon_deg,
                               uint16_t sog, uint16_t cog, uint16_t hdg) {
    BitWriter w;
    w.put(18, 6);
    w.put(0, 2);
    w.put(mmsi, 30);
    w.put(0, 8);            // reserved
    w.put(sog, 10);
    w.put(0, 1);
    w.put((uint32_t)(lon_deg * 600000), 28);
    w.put((uint32_t)(lat_deg * 600000), 27);
    w.put(cog, 12);
    w.put(hdg, 9);
    w.put(0, 6);
    while (w.bits < 168) w.put(0, 1);
    return sentence1(w.armor());
}

// Type 5 static (name), split into 2 fragments
static void make_type5(std::string& frag1, std::string& frag2, uint32_t mmsi, const char* name) {
    BitWriter w;
    w.put(5, 6);
    w.put(0, 2);
    w.put(mmsi, 30);
    w.put(0, 2);            // ais version
    w.put(0, 30);           // imo
    w.put_text("ABC1234", 7);   // callsign (42 bits)
    char n[21];
    memset(n, '@', 20); n[20] = 0;
    strncpy(n, name, strlen(name));
    w.put_text(n, 20);      // name (120 bits)
    w.put(70, 8);           // ship type
    while (w.bits < 424) w.put(0, 1);
    std::string p = w.armor();
    frag1 = "!AIVDM,2,1,7,A," + p.substr(0, 36) + ",0*00";
    frag2 = "!AIVDM,2,2,7,A," + p.substr(36) + ",2*00";
}

int main() {
    AISManager* ais = get_ais_manager();

    // --- Type 1 decode ---
    ais->process_sentence(make_type1(123456789, 39.216, 9.267, 125, 900, 90, 5).c_str());
    const ais_target* t = ais->get_target(0);
    CHECK(t && t->valid && t->mmsi == 123456789, "type1 mmsi stored");
    CHECK(t && t->lat == 39216000 && t->lon == 9267000, "type1 lat/lon microdeg (%ld, %ld)",
          (long)(t ? t->lat : 0), (long)(t ? t->lon : 0));
    CHECK(t && t->sog == 125 && t->cog == 900 && t->heading == 90 && t->nav_status == 5,
          "type1 sog/cog/hdg/nav");
    CHECK(t && t->class_b == 0, "type1 class A flag");

    // --- bbox rejection: 40.5N is outside the gulf box ---
    ais->process_sentence(make_type1(987654321, 40.5, 9.0, 100, 0, 0, 0).c_str());
    CHECK(ais->get_active_count() == 1, "out-of-bbox target rejected (count=%d)", ais->get_active_count());

    // --- Type 18 Class B decode ---
    ais->process_sentence(make_type18(999000111, 39.10, 9.30, 80, 2700, 511).c_str());
    bool found18 = false;
    for (int i = 0; i < AIS_MAX_TARGETS; i++) {
        const ais_target* x = ais->get_target(i);
        if (x->valid && x->mmsi == 999000111) {
            found18 = true;
            CHECK(x->lat == 39100000 && x->lon == 9300000 && x->sog == 80 && x->cog == 2700,
                  "type18 fields (%ld %ld %d %d)", (long)x->lat, (long)x->lon, x->sog, x->cog);
            CHECK(x->class_b == 1, "type18 class B flag");
        }
    }
    CHECK(found18, "type18 target stored");

    // --- Type 5 multi-fragment name attaches to existing MMSI ---
    std::string f1, f2;
    make_type5(f1, f2, 123456789, "TEST SHIP");
    ais->process_sentence(f1.c_str());
    ais->process_sentence(f2.c_str());
    t = nullptr;
    for (int i = 0; i < AIS_MAX_TARGETS; i++) {
        const ais_target* x = ais->get_target(i);
        if (x->valid && x->mmsi == 123456789) t = x;
    }
    CHECK(t && strcmp(t->name, "TEST SHIP") == 0, "type5 name decoded: '%s'", t ? t->name : "(none)");

    // --- geo math ---
    const int32_t HOME_LAT = 39215977, HOME_LON = 9267019;
    float b = geo_bearing_deg(HOME_LAT, HOME_LON, 39000000, 9200000);  // mid-gulf
    float d = geo_distance_nm(HOME_LAT, HOME_LON, 39000000, 9200000);
    CHECK(b > 190 && b < 198, "bearing home->mid-gulf ~195 (got %.1f)", b);
    CHECK(d > 12.5f && d < 14.5f, "distance home->mid-gulf ~13.5nm (got %.2f)", d);

    int32_t dlat, dlon;
    geo_destination(HOME_LAT, HOME_LON, 100.0f, 10.0f, dlat, dlon);
    CHECK(fabsf(geo_distance_nm(HOME_LAT, HOME_LON, dlat, dlon) - 10.0f) < 0.05f, "destination distance roundtrip");
    CHECK(fabsf(geo_bearing_deg(HOME_LAT, HOME_LON, dlat, dlon) - 100.0f) < 0.5f, "destination bearing roundtrip");

    // projection: up bearing 195, mid-gulf should land near top-center
    geo_projection p = { HOME_LAT, HOME_LON, 195.0f, 20.0f, 240, 440 };
    float x, y;
    geo_project(39000000, 9200000, p, x, y);
    CHECK(fabsf(x - 240) < 15 && y < 440, "mid-gulf projects above origin (x=%.1f y=%.1f)", x, y);
    float ub, ud;
    geo_unproject(x, y, p, ub, ud);
    CHECK(fabsf(ub - b) < 1.0f && fabsf(ud - d) < 0.2f, "unproject roundtrip (b=%.1f d=%.2f)", ub, ud);

    printf("\n%s (%d failures)\n", failures ? "FAILED" : "ALL PASSED", failures);
    return failures ? 1 : 0;
}
