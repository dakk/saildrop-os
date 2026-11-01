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
#ifndef SDCARD_H
#define SDCARD_H

#include "conf.h"

#ifdef BOARD_LCD_4

#include <SD_MMC.h>
#include "boards/boards.h"

static bool sd_initialized = false;

// Initialize SD card using MMC 1-bit mode
// Based on Waveshare ESP32-S3-Touch-LCD-4 example
static bool sdcard_init() {
    if (sd_initialized) {
        return true;
    }

    // IO expander is already configured by HAL::begin()
    // Set SD_MMC pins
    SD_MMC.setPins(SD_MMC_CLK_PIN, SD_MMC_CMD_PIN, SD_MMC_DATA_PIN);

    // Small delay for SD card stability
    delay(100);

    // Mount SD card in 1-bit mode
    if (!SD_MMC.begin("/sdcard", true)) {
        #ifdef DEBUG
        Serial.println("SD Card mount failed");
        #endif
        return false;
    }

    uint8_t cardType = SD_MMC.cardType();
    if (cardType == CARD_NONE) {
        #ifdef DEBUG
        Serial.println("No SD card attached");
        #endif
        return false;
    }

    #ifdef DEBUG
    Serial.print("SD Card Type: ");
    if (cardType == CARD_MMC) {
        Serial.println("MMC");
    } else if (cardType == CARD_SD) {
        Serial.println("SDSC");
    } else if (cardType == CARD_SDHC) {
        Serial.println("SDHC");
    } else {
        Serial.println("UNKNOWN");
    }

    uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
    Serial.printf("SD Card Size: %lluMB\n", cardSize);
    #endif

    sd_initialized = true;
    return true;
}

// Check if SD card is mounted
static bool sdcard_is_mounted() {
    return sd_initialized;
}

// Get SD card size in MB
static uint64_t sdcard_size_mb() {
    if (!sd_initialized) return 0;
    return SD_MMC.cardSize() / (1024 * 1024);
}

// Check if a file exists
static bool sdcard_file_exists(const char* path) {
    if (!sd_initialized) return false;
    return SD_MMC.exists(path);
}

// Read file into buffer, returns bytes read or -1 on error
static int32_t sdcard_read_file(const char* path, uint8_t* buffer, size_t max_size) {
    if (!sd_initialized) return -1;

    File file = SD_MMC.open(path, FILE_READ);
    if (!file) {
        return -1;
    }

    size_t bytes_read = file.read(buffer, max_size);
    file.close();
    return bytes_read;
}

#endif // BOARD_LCD_4

#endif // SDCARD_H
