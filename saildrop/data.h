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
#ifndef DATA_H
#define DATA_H

#include <lvgl.h>

struct nmea_data {
    uint32_t sog;
    uint32_t hdg;

    uint32_t tws;
    uint32_t twa;
    uint32_t aws;
    uint32_t awa;

    uint32_t depth;  // Depth in 0.1m units

    // Own boat position for AIS display
    int32_t lat;          // Latitude in microdegrees (-90,000,000 to +90,000,000)
    int32_t lon;          // Longitude in microdegrees (-180,000,000 to +180,000,000)
    bool position_valid;  // True when GPS fix is available
};

nmea_data *get_data();

// Get TWA with fallback calculation from apparent wind if true wind is unavailable
// Returns TWA in 0.1 degree units (0-3600)
uint32_t get_twa();

// Get TWS with fallback calculation from apparent wind if true wind is unavailable
// Returns TWS in 0.1 knot units
uint32_t get_tws();

// TODO: use a singleton class instead?
// TODO: add setters
// TODO: add an handler list for new data (or stick with timers?)

#endif