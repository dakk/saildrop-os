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
#ifndef SETTINGS_H
#define SETTINGS_H

#include <Preferences.h>
#include "conf.h"

// NVS namespace and keys
#define SETTINGS_NAMESPACE "saildrop"
#define SETTINGS_KEY_SSID "wifi_ssid"
#define SETTINGS_KEY_PASS "wifi_pass"
#define SETTINGS_KEY_IP "nmea_ip"
#define SETTINGS_KEY_PORT "nmea_port"
#define SETTINGS_KEY_PROTOCOL "protocol"
#define SETTINGS_KEY_AP_MODE "ap_mode"
#define SETTINGS_KEY_AP_SSID "ap_ssid"
#define SETTINGS_KEY_AP_PASS "ap_pass"
#define SETTINGS_KEY_LISTEN_PORT "listen_port"
#define SETTINGS_KEY_LISTEN_PROTOCOL "listen_proto"
#define SETTINGS_KEY_CONFIGURED "configured"

// Max lengths
#define SETTINGS_SSID_MAX 33
#define SETTINGS_PASS_MAX 65
#define SETTINGS_IP_MAX 16

// Protocol types
enum NmeaProtocol {
    PROTO_TCP = 0,
    PROTO_UDP = 1
};

struct SaildropSettings {
    // Station mode settings (connect to existing WiFi)
    char wifi_ssid[SETTINGS_SSID_MAX];
    char wifi_pass[SETTINGS_PASS_MAX];
    char nmea_ip[SETTINGS_IP_MAX];
    uint16_t nmea_port;
    NmeaProtocol protocol;  // TCP or UDP for client mode

    // AP mode settings (create own WiFi, listen for connections)
    bool ap_mode;
    char ap_ssid[SETTINGS_SSID_MAX];
    char ap_pass[SETTINGS_PASS_MAX];
    uint16_t listen_port;
    NmeaProtocol listen_protocol;  // TCP or UDP for server mode

    bool configured;
};

class SettingsManager {
private:
    Preferences prefs;
    SaildropSettings settings;
    bool initialized;

public:
    SettingsManager();

    // Initialize the settings manager
    void begin();

    // Load settings from NVS (uses defaults if not configured)
    void load();

    // Save current settings to NVS
    void save();

    // Clear all settings from NVS (factory reset)
    void clear();

    // Check if settings have been configured
    bool isConfigured();

    // Get pointer to settings struct
    SaildropSettings* get();

    // Individual setters - Station mode
    void setWifiSsid(const char* ssid);
    void setWifiPass(const char* pass);
    void setNmeaIp(const char* ip);
    void setNmeaPort(uint16_t port);
    void setProtocol(NmeaProtocol proto);

    // Individual setters - AP mode
    void setApMode(bool enabled);
    void setApSsid(const char* ssid);
    void setApPass(const char* pass);
    void setListenPort(uint16_t port);
    void setListenProtocol(NmeaProtocol proto);
};

// Global singleton accessor
SettingsManager* getSettings();

#endif
