/*
 * geo.h - geographic math for the radar (pure, host-testable, no Arduino/LVGL)
 *
 * All positions are int32 microdegrees (degrees * 1e6).
 * Screen convention: angle 0 = up (north on screen), clockwise positive,
 * y axis pointing down (LVGL convention).
 */
#ifndef GEO_H
#define GEO_H

#include <stdint.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define GEO_DEG_TO_RAD (M_PI / 180.0f)
#define GEO_RAD_TO_DEG (180.0f / (float)M_PI)

// Bearing from point 1 to point 2, degrees true (0-360)
inline float geo_bearing_deg(int32_t lat1, int32_t lon1, int32_t lat2, int32_t lon2) {
    float a1 = lat1 * 1e-6f * GEO_DEG_TO_RAD;
    float o1 = lon1 * 1e-6f * GEO_DEG_TO_RAD;
    float a2 = lat2 * 1e-6f * GEO_DEG_TO_RAD;
    float o2 = lon2 * 1e-6f * GEO_DEG_TO_RAD;
    float dO = o2 - o1;
    float y = sinf(dO) * cosf(a2);
    float x = cosf(a1) * sinf(a2) - sinf(a1) * cosf(a2) * cosf(dO);
    float b = atan2f(y, x) * GEO_RAD_TO_DEG;
    return fmodf(b + 360.0f, 360.0f);
}

// Distance in nautical miles (Haversine, earth radius 3440.065 nm)
inline float geo_distance_nm(int32_t lat1, int32_t lon1, int32_t lat2, int32_t lon2) {
    float a1 = lat1 * 1e-6f * GEO_DEG_TO_RAD;
    float o1 = lon1 * 1e-6f * GEO_DEG_TO_RAD;
    float a2 = lat2 * 1e-6f * GEO_DEG_TO_RAD;
    float o2 = lon2 * 1e-6f * GEO_DEG_TO_RAD;
    float dA = a2 - a1;
    float dO = o2 - o1;
    float h = sinf(dA / 2) * sinf(dA / 2) +
              cosf(a1) * cosf(a2) * sinf(dO / 2) * sinf(dO / 2);
    return 3440.065f * 2.0f * atan2f(sqrtf(h), sqrtf(1.0f - h));
}

// Destination point given start, bearing (deg true) and distance (nm)
inline void geo_destination(int32_t lat1, int32_t lon1, float bearing_deg, float dist_nm,
                            int32_t &out_lat, int32_t &out_lon) {
    float a1 = lat1 * 1e-6f * GEO_DEG_TO_RAD;
    float o1 = lon1 * 1e-6f * GEO_DEG_TO_RAD;
    float br = bearing_deg * GEO_DEG_TO_RAD;
    float d = dist_nm / 3440.065f;  // angular distance
    float a2 = asinf(sinf(a1) * cosf(d) + cosf(a1) * sinf(d) * cosf(br));
    float o2 = o1 + atan2f(sinf(br) * sinf(d) * cosf(a1),
                           cosf(d) - sinf(a1) * sinf(a2));
    out_lat = (int32_t)(a2 * GEO_RAD_TO_DEG * 1e6f);
    out_lon = (int32_t)(o2 * GEO_RAD_TO_DEG * 1e6f);
}

// Projection of lat/lon to radar screen coordinates.
// up_bearing_deg = true bearing that should point to screen-up.
struct geo_projection {
    int32_t home_lat, home_lon;  // microdeg, screen position of origin
    float up_bearing_deg;
    float px_per_nm;             // scale
    int32_t origin_x, origin_y;  // screen px of the origin (house)
};

inline void geo_project(int32_t lat, int32_t lon, const geo_projection &p, float &x, float &y) {
    float b = geo_bearing_deg(p.home_lat, p.home_lon, lat, lon);
    float d = geo_distance_nm(p.home_lat, p.home_lon, lat, lon);
    float rel = (b - p.up_bearing_deg) * GEO_DEG_TO_RAD;
    x = (float)p.origin_x + d * p.px_per_nm * sinf(rel);
    y = (float)p.origin_y - d * p.px_per_nm * cosf(rel);
}

// Inverse: screen px -> true bearing/distance from home (for tap-select info)
inline void geo_unproject(float x, float y, const geo_projection &p, float &bearing_deg, float &dist_nm) {
    float dx = x - (float)p.origin_x;
    float dy = (float)p.origin_y - y;  // up is +y in geo terms
    dist_nm = sqrtf(dx * dx + dy * dy) / p.px_per_nm;
    float rel = atan2f(dx, dy) * GEO_RAD_TO_DEG;  // clockwise from up
    bearing_deg = fmodf(rel + p.up_bearing_deg + 360.0f, 360.0f);
}

// Closest point of approach of a moving target to a fixed point (the house).
// cog_deg: true course, sog_kn: speed over ground in knots.
// Outputs: cpa_nm (>= 0), tcpa_h (hours until CPA; <= 0 = receding / already at CPA).
inline void geo_cpa(int32_t home_lat, int32_t home_lon, int32_t tgt_lat, int32_t tgt_lon,
                    float cog_deg, float sog_kn, float &cpa_nm, float &tcpa_h) {
    float d = geo_distance_nm(home_lat, home_lon, tgt_lat, tgt_lon);
    if (sog_kn < 0.3f) {  // moored/drifting: CPA is current distance, no approach
        cpa_nm = d;
        tcpa_h = -1.0f;
        return;
    }
    float brg = geo_bearing_deg(home_lat, home_lon, tgt_lat, tgt_lon) * GEO_DEG_TO_RAD;
    float pe = d * sinf(brg);  // target position rel. home, nm east
    float pn = d * cosf(brg);  // nm north
    float cr = cog_deg * GEO_DEG_TO_RAD;
    float ve = sog_kn * sinf(cr);  // velocity, nm/h
    float vn = sog_kn * cosf(cr);
    float v2 = ve * ve + vn * vn;
    cpa_nm = fabsf(pe * vn - pn * ve) / sqrtf(v2);
    tcpa_h = -(pe * ve + pn * vn) / v2;
}

#endif // GEO_H
