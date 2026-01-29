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
#include <math.h>
#include "data.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

nmea_data data;


nmea_data *get_data() {
    return &data;
}

// Calculate TWA from apparent wind when true wind is not available
// All values in 0.1 units (0.1 degrees, 0.1 knots)
uint32_t get_twa() {
    // If we have true wind data, return it directly
    if (data.twa != 0) {
        return data.twa;
    }

    // Fallback: calculate from apparent wind
    if (data.aws == 0) return 0;

    // Convert to real units
    double awa_rad = (data.awa / 10.0) * M_PI / 180.0;
    double aws_kts = data.aws / 10.0;
    double sog_kts = data.sog / 10.0;

    // Calculate true wind angle using vector math
    // TWA = atan2(AWS * sin(AWA), AWS * cos(AWA) - SOG)
    double twa_rad = atan2(aws_kts * sin(awa_rad), aws_kts * cos(awa_rad) - sog_kts);

    // Convert to degrees
    double twa_deg = twa_rad * 180.0 / M_PI;

    // Normalize to 0-360
    if (twa_deg < 0) twa_deg += 360.0;

    return (uint32_t)(twa_deg * 10);
}

// Calculate TWS from apparent wind when true wind is not available
uint32_t get_tws() {
    // If we have true wind data, return it directly
    if (data.tws != 0) {
        return data.tws;
    }

    // Fallback: calculate from apparent wind
    if (data.aws == 0) return 0;

    // Convert to real units
    double awa_rad = (data.awa / 10.0) * M_PI / 180.0;
    double aws_kts = data.aws / 10.0;
    double sog_kts = data.sog / 10.0;

    // Calculate true wind speed using vector math
    // TWS = sqrt(AWS² + SOG² - 2*AWS*SOG*cos(AWA))
    double tws_kts = sqrt(aws_kts * aws_kts + sog_kts * sog_kts
                         - 2 * aws_kts * sog_kts * cos(awa_rad));

    return (uint32_t)(tws_kts * 10);
}