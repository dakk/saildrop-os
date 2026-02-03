/*
 * GT911 Touch Controller Driver
 * Based on common GT911 implementations
 *
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
#ifndef GT911_H
#define GT911_H

#include <Arduino.h>
#include <Wire.h>

// GT911 I2C addresses (depends on initialization sequence)
#define GT911_ADDR_1    0x5D
#define GT911_ADDR_2    0x14

// GT911 Registers
#define GT911_REG_COMMAND       0x8040
#define GT911_REG_CONFIG        0x8047
#define GT911_REG_STATUS        0x814E
#define GT911_REG_POINT1        0x814F  // First point data (8 bytes per point)
#define GT911_REG_PRODUCT_ID    0x8140

// Gesture IDs (compatible with CST816S)
enum GT911_GESTURE {
    GT911_NONE = 0x00,
    GT911_SWIPE_UP = 0x01,
    GT911_SWIPE_DOWN = 0x02,
    GT911_SWIPE_LEFT = 0x03,
    GT911_SWIPE_RIGHT = 0x04,
    GT911_SINGLE_CLICK = 0x05,
    GT911_DOUBLE_CLICK = 0x0B,
    GT911_LONG_PRESS = 0x0C
};

struct GT911_TouchData {
    uint8_t gestureID;
    uint8_t points;
    uint8_t event;
    int x;
    int y;
    int x2, y2;  // Second touch point
    int x3, y3;  // Third touch point
    int x4, y4;  // Fourth touch point
    int x5, y5;  // Fifth touch point
};

class GT911 {
public:
    GT911(int sda, int scl, int rst, int irq);

    // skipWireBegin: set to true if Wire.begin() was already called
    bool begin(TwoWire *wire = &Wire, bool skipWireBegin = false);
    bool available();
    void sleep();

    GT911_TouchData data;
    String gesture();

    // Get product ID
    void getProductID(char* id, int len);

    // Debug: scan I2C bus
    void scanI2C();

private:
    int _sda;
    int _scl;
    int _rst;
    int _irq;
    uint8_t _addr;
    TwoWire *_wire;

    bool _initialized;
    bool _touchAvailable;

    // Gesture detection state
    unsigned long _touchStartTime;
    int _touchStartX;
    int _touchStartY;
    bool _tracking;
    unsigned long _lastTouchTime;

    bool findAddress();
    void reset();
    uint8_t readRegister(uint16_t reg);
    void writeRegister(uint16_t reg, uint8_t value);
    void readRegisters(uint16_t reg, uint8_t *buf, uint8_t len);
    void readTouchPoints();
    GT911_GESTURE detectGesture();
};

// Implementation
GT911::GT911(int sda, int scl, int rst, int irq)
    : _sda(sda), _scl(scl), _rst(rst), _irq(irq),
      _addr(GT911_ADDR_1), _wire(nullptr), _initialized(false),
      _touchAvailable(false), _touchStartTime(0), _touchStartX(0),
      _touchStartY(0), _tracking(false), _lastTouchTime(0) {
    memset(&data, 0, sizeof(data));
}

bool GT911::begin(TwoWire *wire, bool skipWireBegin) {
    _wire = wire;

    // Initialize I2C only if not skipped (Wire may already be initialized)
    if (!skipWireBegin) {
        _wire->begin(_sda, _scl);
    }
    _wire->setClock(400000);  // 400kHz

    // Scan I2C bus to help diagnose issues
    Serial.println("GT911: Scanning I2C bus...");
    scanI2C();

    // Reset sequence - only if we have direct reset pin access
    if (_rst >= 0) {
        reset();
    }

    // Set up interrupt pin
    if (_irq >= 0) {
        pinMode(_irq, INPUT);
    }

    // Find the correct I2C address
    if (!findAddress()) {
        Serial.println("GT911: Device not found on I2C bus!");
        Serial.println("GT911: Make sure the IO expander is properly configured");
        return false;
    }

    _initialized = true;

    // Read product ID
    char productId[5] = {0};
    getProductID(productId, 4);
    Serial.printf("GT911: Initialized at address 0x%02X, Product ID: %s\n", _addr, productId);

    return true;
}

void GT911::scanI2C() {
    Serial.println("I2C scan results:");
    int nDevices = 0;
    for (uint8_t address = 1; address < 127; address++) {
        _wire->beginTransmission(address);
        uint8_t error = _wire->endTransmission();
        if (error == 0) {
            Serial.printf("  Found device at 0x%02X", address);
            if (address == GT911_ADDR_1 || address == GT911_ADDR_2) {
                Serial.print(" (GT911 candidate)");
            }
            if (address == 0x24) {
                Serial.print(" (IO Expander PCA9557)");
            }
            Serial.println();
            nDevices++;
        }
    }
    if (nDevices == 0) {
        Serial.println("  No I2C devices found!");
    } else {
        Serial.printf("  Total: %d device(s)\n", nDevices);
    }
}

void GT911::reset() {
    // GT911 reset sequence
    // The address is determined by the INT pin state during reset
    if (_irq >= 0) {
        pinMode(_irq, OUTPUT);
        digitalWrite(_irq, LOW);  // For address 0x5D
    }

    pinMode(_rst, OUTPUT);
    digitalWrite(_rst, LOW);
    delay(10);
    digitalWrite(_rst, HIGH);
    delay(10);

    if (_irq >= 0) {
        pinMode(_irq, INPUT);
    }
    delay(50);
}

bool GT911::findAddress() {
    // Try both possible addresses
    _wire->beginTransmission(GT911_ADDR_1);
    if (_wire->endTransmission() == 0) {
        _addr = GT911_ADDR_1;
        return true;
    }

    _wire->beginTransmission(GT911_ADDR_2);
    if (_wire->endTransmission() == 0) {
        _addr = GT911_ADDR_2;
        return true;
    }

    return false;
}

uint8_t GT911::readRegister(uint16_t reg) {
    uint8_t value = 0;
    readRegisters(reg, &value, 1);
    return value;
}

void GT911::writeRegister(uint16_t reg, uint8_t value) {
    _wire->beginTransmission(_addr);
    _wire->write((reg >> 8) & 0xFF);
    _wire->write(reg & 0xFF);
    _wire->write(value);
    _wire->endTransmission();
}

void GT911::readRegisters(uint16_t reg, uint8_t *buf, uint8_t len) {
    _wire->beginTransmission(_addr);
    _wire->write((reg >> 8) & 0xFF);
    _wire->write(reg & 0xFF);
    _wire->endTransmission(false);

    _wire->requestFrom(_addr, len);
    for (int i = 0; i < len && _wire->available(); i++) {
        buf[i] = _wire->read();
    }
}

void GT911::getProductID(char* id, int len) {
    uint8_t buf[4] = {0};
    readRegisters(GT911_REG_PRODUCT_ID, buf, 4);
    for (int i = 0; i < len && i < 4; i++) {
        id[i] = buf[i];
    }
}

bool GT911::available() {
    if (!_initialized) return false;

    // Always poll via I2C status register
    // The interrupt pin on some boards (like ESP32-S3-Touch-LCD-4) may not be reliable
    // or may require different handling when controlled via IO expander

    // Read status register
    uint8_t status = readRegister(GT911_REG_STATUS);

    // Check buffer status (bit 7) and number of touch points (bits 0-3)
    bool bufferReady = (status & 0x80) != 0;
    uint8_t touchPoints = status & 0x0F;

    if (!bufferReady) {
        // No touch data ready, but check for gesture completion
        if (_tracking && (millis() - _lastTouchTime) > 50) {
            data.gestureID = detectGesture();
            _tracking = false;
            if (data.gestureID != GT911_NONE) {
                return true;
            }
        }
        return false;
    }

    data.points = touchPoints;

    if (data.points > 0 && data.points <= 5) {
        readTouchPoints();

        // Clear status (MUST clear after reading)
        writeRegister(GT911_REG_STATUS, 0);

        // Track for gesture detection
        unsigned long now = millis();
        if (!_tracking) {
            _tracking = true;
            _touchStartTime = now;
            _touchStartX = data.x;
            _touchStartY = data.y;
        }
        _lastTouchTime = now;

        // For now, set gesture to NONE (will be detected on release)
        data.gestureID = GT911_NONE;
        data.event = 2;  // Contact

        return true;
    }

    // Clear status even if no valid points
    writeRegister(GT911_REG_STATUS, 0);
    return false;
}

void GT911::readTouchPoints() {
    // Read all point data at once (8 bytes per point, up to 5 points)
    uint8_t buf[40];
    uint8_t bytesToRead = data.points * 8;
    if (bytesToRead > 40) bytesToRead = 40;

    readRegisters(GT911_REG_POINT1, buf, bytesToRead);

    // Each point is 8 bytes:
    // [0]: trackID
    // [1-2]: x (little endian)
    // [3-4]: y (little endian)
    // [5-6]: size (little endian)
    // [7]: reserved

    // Parse first touch point
    data.x = buf[1] | (buf[2] << 8);
    data.y = buf[3] | (buf[4] << 8);

    // Parse additional points if available
    if (data.points >= 2) {
        data.x2 = buf[8 + 1] | (buf[8 + 2] << 8);
        data.y2 = buf[8 + 3] | (buf[8 + 4] << 8);
    }
    if (data.points >= 3) {
        data.x3 = buf[16 + 1] | (buf[16 + 2] << 8);
        data.y3 = buf[16 + 3] | (buf[16 + 4] << 8);
    }
    if (data.points >= 4) {
        data.x4 = buf[24 + 1] | (buf[24 + 2] << 8);
        data.y4 = buf[24 + 3] | (buf[24 + 4] << 8);
    }
    if (data.points >= 5) {
        data.x5 = buf[32 + 1] | (buf[32 + 2] << 8);
        data.y5 = buf[32 + 3] | (buf[32 + 4] << 8);
    }
}

GT911_GESTURE GT911::detectGesture() {
    // Calculate movement
    int dx = data.x - _touchStartX;
    int dy = data.y - _touchStartY;
    unsigned long duration = _lastTouchTime - _touchStartTime;

    // Thresholds
    const int SWIPE_THRESHOLD = 50;
    const unsigned long LONG_PRESS_TIME = 800;
    const unsigned long CLICK_TIME = 300;

    // Detect long press
    if (duration > LONG_PRESS_TIME && abs(dx) < SWIPE_THRESHOLD && abs(dy) < SWIPE_THRESHOLD) {
        return GT911_LONG_PRESS;
    }

    // Detect swipes
    if (abs(dx) > SWIPE_THRESHOLD || abs(dy) > SWIPE_THRESHOLD) {
        if (abs(dx) > abs(dy)) {
            // Horizontal swipe
            return (dx > 0) ? GT911_SWIPE_RIGHT : GT911_SWIPE_LEFT;
        } else {
            // Vertical swipe
            return (dy > 0) ? GT911_SWIPE_DOWN : GT911_SWIPE_UP;
        }
    }

    // Detect click
    if (duration < CLICK_TIME) {
        return GT911_SINGLE_CLICK;
    }

    return GT911_NONE;
}

void GT911::sleep() {
    writeRegister(GT911_REG_COMMAND, 0x05);  // Enter sleep mode
}

String GT911::gesture() {
    switch (data.gestureID) {
        case GT911_SWIPE_UP:    return "SWIPE UP";
        case GT911_SWIPE_DOWN:  return "SWIPE DOWN";
        case GT911_SWIPE_LEFT:  return "SWIPE LEFT";
        case GT911_SWIPE_RIGHT: return "SWIPE RIGHT";
        case GT911_SINGLE_CLICK: return "SINGLE CLICK";
        case GT911_DOUBLE_CLICK: return "DOUBLE CLICK";
        case GT911_LONG_PRESS:  return "LONG PRESS";
        default:                return "NONE";
    }
}

#endif // GT911_H
