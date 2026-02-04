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
#ifndef TILEMGR_H
#define TILEMGR_H

#include "conf.h"

#ifdef BOARD_LCD_4

#include <stdint.h>
#include <math.h>
#include <SD_MMC.h>
#include <esp_heap_caps.h>

#ifndef CHART_TILE_SIZE
#define CHART_TILE_SIZE 128
#endif

#ifndef CHART_CACHE_TILES
#define CHART_CACHE_TILES 9
#endif

#define TILE_BYTES (CHART_TILE_SIZE * CHART_TILE_SIZE * 2)  // RGB565

// Tile cache entry
struct CachedTile {
    uint8_t zoom;
    uint32_t tile_x;
    uint32_t tile_y;
    uint8_t *data;          // Pointer to PSRAM buffer
    uint32_t last_used;     // millis() timestamp for LRU
    bool valid;
};

class TileManager {
private:
    CachedTile cache[CHART_CACHE_TILES];
    uint8_t *cache_memory;      // Contiguous PSRAM allocation
    char tile_path[64];         // Reusable path buffer

    uint32_t cache_hits;
    uint32_t cache_misses;

    // Find tile in cache, returns index or -1
    int find_in_cache(uint8_t zoom, uint32_t tile_x, uint32_t tile_y) {
        for (int i = 0; i < CHART_CACHE_TILES; i++) {
            if (cache[i].valid &&
                cache[i].zoom == zoom &&
                cache[i].tile_x == tile_x &&
                cache[i].tile_y == tile_y) {
                return i;
            }
        }
        return -1;
    }

    // Find LRU slot or invalid slot
    int find_evict_slot() {
        int oldest_idx = 0;
        uint32_t oldest_time = UINT32_MAX;

        for (int i = 0; i < CHART_CACHE_TILES; i++) {
            if (!cache[i].valid) {
                return i;  // Use invalid slot first
            }
            if (cache[i].last_used < oldest_time) {
                oldest_time = cache[i].last_used;
                oldest_idx = i;
            }
        }
        return oldest_idx;
    }

    // Build file path for tile
    void build_tile_path(uint8_t zoom, uint32_t tile_x, uint32_t tile_y) {
        snprintf(tile_path, sizeof(tile_path),
                 "/tiles/%d/%lu/%lu.bin",
                 zoom, (unsigned long)tile_x, (unsigned long)tile_y);
    }

    // Load tile from SD card into buffer
    bool load_tile_from_sd(uint8_t zoom, uint32_t tile_x, uint32_t tile_y, uint8_t *buffer) {
        build_tile_path(zoom, tile_x, tile_y);

        File file = SD_MMC.open(tile_path, FILE_READ);
        if (!file) {
            #ifdef DEBUG
            Serial.printf("Tile not found: %s\n", tile_path);
            #endif
            return false;
        }

        size_t bytes_read = file.read(buffer, TILE_BYTES);
        file.close();

        if (bytes_read != TILE_BYTES) {
            #ifdef DEBUG
            Serial.printf("Tile size mismatch: %s (got %d, expected %d)\n",
                         tile_path, bytes_read, TILE_BYTES);
            #endif
            return false;
        }

        return true;
    }

public:
    TileManager() : cache_memory(nullptr), cache_hits(0), cache_misses(0) {
        for (int i = 0; i < CHART_CACHE_TILES; i++) {
            cache[i].valid = false;
            cache[i].data = nullptr;
        }
    }

    ~TileManager() {
        if (cache_memory) {
            heap_caps_free(cache_memory);
        }
    }

    // Initialize tile manager, allocate PSRAM
    bool begin() {
        // Allocate contiguous PSRAM for all cache tiles
        size_t total_size = CHART_CACHE_TILES * TILE_BYTES;
        cache_memory = (uint8_t*)heap_caps_malloc(total_size, MALLOC_CAP_SPIRAM);

        if (!cache_memory) {
            #ifdef DEBUG
            Serial.printf("Failed to allocate %d bytes PSRAM for tile cache\n", total_size);
            #endif
            return false;
        }

        // Assign buffer pointers to cache entries
        for (int i = 0; i < CHART_CACHE_TILES; i++) {
            cache[i].data = cache_memory + (i * TILE_BYTES);
            cache[i].valid = false;
        }

        #ifdef DEBUG
        Serial.printf("TileManager: allocated %d KB for %d tiles\n",
                     total_size / 1024, CHART_CACHE_TILES);
        #endif

        return true;
    }

    // Convert lat/lon (microdegrees) to tile coordinates
    static void lat_lon_to_tile_xy(int32_t lat_micro, int32_t lon_micro,
                                   uint8_t zoom, uint32_t *tile_x, uint32_t *tile_y) {
        double lat = lat_micro / 1000000.0;
        double lon = lon_micro / 1000000.0;
        double n = pow(2.0, zoom);

        *tile_x = (uint32_t)((lon + 180.0) / 360.0 * n);

        double lat_rad = lat * M_PI / 180.0;
        *tile_y = (uint32_t)((1.0 - asinh(tan(lat_rad)) / M_PI) / 2.0 * n);
    }

    // Convert tile coordinates to lat/lon of tile's NW corner (microdegrees)
    static void tile_xy_to_lat_lon(uint32_t tile_x, uint32_t tile_y, uint8_t zoom,
                                   int32_t *lat_micro, int32_t *lon_micro) {
        double n = pow(2.0, zoom);
        double lon = tile_x / n * 360.0 - 180.0;
        double lat_rad = atan(sinh(M_PI * (1.0 - 2.0 * tile_y / n)));
        double lat = lat_rad * 180.0 / M_PI;

        *lon_micro = (int32_t)(lon * 1000000.0);
        *lat_micro = (int32_t)(lat * 1000000.0);
    }

    // Get pixel offset within tile for a coordinate
    void get_pixel_offset(int32_t lat_micro, int32_t lon_micro, uint8_t zoom,
                          uint32_t tile_x, uint32_t tile_y,
                          int16_t *pixel_x, int16_t *pixel_y) {
        // Get tile corner coordinates
        int32_t tile_lat, tile_lon;
        int32_t next_tile_lat, next_tile_lon;
        tile_xy_to_lat_lon(tile_x, tile_y, zoom, &tile_lat, &tile_lon);
        tile_xy_to_lat_lon(tile_x + 1, tile_y + 1, zoom, &next_tile_lat, &next_tile_lon);

        // Calculate fractional position within tile
        double frac_x = (double)(lon_micro - tile_lon) / (double)(next_tile_lon - tile_lon);
        double frac_y = (double)(tile_lat - lat_micro) / (double)(tile_lat - next_tile_lat);

        *pixel_x = (int16_t)(frac_x * CHART_TILE_SIZE);
        *pixel_y = (int16_t)(frac_y * CHART_TILE_SIZE);
    }

    // Get tile data (loads from SD if not cached)
    // Returns pointer to tile data or nullptr if not found
    const uint8_t* get_tile(uint8_t zoom, uint32_t tile_x, uint32_t tile_y) {
        // Check cache first
        int cache_idx = find_in_cache(zoom, tile_x, tile_y);
        if (cache_idx >= 0) {
            cache[cache_idx].last_used = millis();
            cache_hits++;
            return cache[cache_idx].data;
        }

        // Cache miss - find slot to use
        cache_misses++;
        int slot = find_evict_slot();

        // Load tile from SD
        if (!load_tile_from_sd(zoom, tile_x, tile_y, cache[slot].data)) {
            return nullptr;
        }

        // Update cache entry
        cache[slot].zoom = zoom;
        cache[slot].tile_x = tile_x;
        cache[slot].tile_y = tile_y;
        cache[slot].last_used = millis();
        cache[slot].valid = true;

        return cache[slot].data;
    }

    // Preload tiles around a position (3x3 grid)
    void preload_area(int32_t center_lat, int32_t center_lon, uint8_t zoom) {
        uint32_t center_tile_x, center_tile_y;
        lat_lon_to_tile_xy(center_lat, center_lon, zoom, &center_tile_x, &center_tile_y);

        // Preload 3x3 grid around center
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                uint32_t tx = center_tile_x + dx;
                uint32_t ty = center_tile_y + dy;
                get_tile(zoom, tx, ty);  // This will cache the tile
            }
        }
    }

    // Check if tiles directory exists for a zoom level
    bool has_zoom_level(uint8_t zoom) {
        char path[32];
        snprintf(path, sizeof(path), "/tiles/%d", zoom);
        return SD_MMC.exists(path);
    }

    // Statistics
    uint32_t get_cache_hits() { return cache_hits; }
    uint32_t get_cache_misses() { return cache_misses; }

    void reset_stats() {
        cache_hits = 0;
        cache_misses = 0;
    }
};

#endif // BOARD_LCD_4

#endif // TILEMGR_H
