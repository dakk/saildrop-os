/*
 * coastline.h - stylized silhouette of the Gulf of Cagliari (no map tiles).
 *
 * Hand-simplified coastline polygons, positions in microdegrees (deg * 1e6).
 * The gulf opens to the south; the mainland coast below is its northern
 * shore, running from Capo Spartivento (W) to Capo Carbonara (E), plus the
 * islands of Sant'Antioco and San Pietro (SW). Coarse on purpose: this is a
 * silhouette, not a chart. Refine points as you like.
 */
#ifndef COASTLINE_H
#define COASTLINE_H

#include <stdint.h>

struct coast_point {
    int32_t lat;  // microdegrees
    int32_t lon;  // microdegrees
};

// Mainland coast, west -> east (sea is to the SOUTH of these points)
static const coast_point COAST_MAIN[] = {
    { 38873000,  8877000 },  // Capo Spartivento
    { 38905000,  8888000 },
    { 38935000,  8862000 },  // Chia
    { 38970000,  8885000 },
    { 38990000,  8925000 },  // Pula
    { 38995000,  8975000 },  // Nora
    { 39020000,  9025000 },
    { 39080000,  9050000 },
    { 39140000,  9075000 },
    { 39180000,  9095000 },
    { 39200000,  9110000 },  // Cagliari west (La Maddalena beach)
    { 39207000,  9125000 },  // port / Capo Sant'Elia
    { 39202000,  9145000 },  // Poetto
    { 39195000,  9172000 },  // Poetto end
    { 39208000,  9200000 },
    { 39214000,  9250000 },
    { 39210000,  9267000 },  // coast near the house (house: 39.215977, 9.267019, just inland)
    { 39216000,  9290000 },
    { 39224000,  9320000 },
    { 39230000,  9360000 },
    { 39236000,  9410000 },
    { 39214000,  9460000 },
    { 39185000,  9500000 },
    { 39150000,  9520000 },  // Villasimius
    { 39120000,  9555000 },
    { 39100000,  9577000 },  // Capo Carbonara
};
static const int COAST_MAIN_COUNT = sizeof(COAST_MAIN) / sizeof(COAST_MAIN[0]);

// Isola di Sant'Antioco
static const coast_point COAST_SANTANTIOCO[] = {
    { 39070000, 8460000 },
    { 39040000, 8385000 },
    { 39000000, 8390000 },
    { 38960000, 8450000 },
    { 38965000, 8520000 },
    { 39010000, 8545000 },
    { 39050000, 8520000 },
};
static const int COAST_SANTANTIOCO_COUNT = sizeof(COAST_SANTANTIOCO) / sizeof(COAST_SANTANTIOCO[0]);

// Isola di San Pietro
static const coast_point COAST_SANPIETRO[] = {
    { 39200000, 8360000 },
    { 39160000, 8350000 },
    { 39110000, 8370000 },
    { 39100000, 8410000 },
    { 39140000, 8420000 },
    { 39185000, 8420000 },
};
static const int COAST_SANPIETRO_COUNT = sizeof(COAST_SANPIETRO) / sizeof(COAST_SANPIETRO[0]);

#endif // COASTLINE_H
