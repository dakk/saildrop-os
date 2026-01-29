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
#include "settings.h"
#include <string.h>

static SettingsManager settingsInstance;

SettingsManager* getSettings() {
    return &settingsInstance;
}

SettingsManager::SettingsManager() : initialized(false) {
    // Initialize with compile-time defaults - Station mode
    strncpy(settings.wifi_ssid, WIFI_DEFAULT_SSID, SETTINGS_SSID_MAX - 1);
    settings.wifi_ssid[SETTINGS_SSID_MAX - 1] = '\0';

    strncpy(settings.wifi_pass, WIFI_DEFAULT_PASSWORD, SETTINGS_PASS_MAX - 1);
    settings.wifi_pass[SETTINGS_PASS_MAX - 1] = '\0';

    strncpy(settings.nmea_ip, WIFI_DEFAULT_IP, SETTINGS_IP_MAX - 1);
    settings.nmea_ip[SETTINGS_IP_MAX - 1] = '\0';

    settings.nmea_port = WIFI_DEFAULT_TCP_PORT;
    settings.protocol = PROTO_TCP;

    // Initialize with compile-time defaults - AP mode
    settings.ap_mode = false;
    strncpy(settings.ap_ssid, AP_SSID, SETTINGS_SSID_MAX - 1);
    settings.ap_ssid[SETTINGS_SSID_MAX - 1] = '\0';

    strncpy(settings.ap_pass, AP_PASS, SETTINGS_PASS_MAX - 1);
    settings.ap_pass[SETTINGS_PASS_MAX - 1] = '\0';

    settings.listen_port = WIFI_DEFAULT_TCP_PORT;
    settings.listen_protocol = PROTO_TCP;

    settings.configured = false;
}

void SettingsManager::begin() {
    initialized = true;
}

void SettingsManager::load() {
    if (!initialized) {
        begin();
    }

    prefs.begin(SETTINGS_NAMESPACE, true);  // Read-only mode

    if (prefs.isKey(SETTINGS_KEY_CONFIGURED)) {
        settings.configured = prefs.getBool(SETTINGS_KEY_CONFIGURED, false);

        if (settings.configured) {
            // Load saved values - Station mode
            String ssid = prefs.getString(SETTINGS_KEY_SSID, WIFI_DEFAULT_SSID);
            String pass = prefs.getString(SETTINGS_KEY_PASS, WIFI_DEFAULT_PASSWORD);
            String ip = prefs.getString(SETTINGS_KEY_IP, WIFI_DEFAULT_IP);

            strncpy(settings.wifi_ssid, ssid.c_str(), SETTINGS_SSID_MAX - 1);
            settings.wifi_ssid[SETTINGS_SSID_MAX - 1] = '\0';

            strncpy(settings.wifi_pass, pass.c_str(), SETTINGS_PASS_MAX - 1);
            settings.wifi_pass[SETTINGS_PASS_MAX - 1] = '\0';

            strncpy(settings.nmea_ip, ip.c_str(), SETTINGS_IP_MAX - 1);
            settings.nmea_ip[SETTINGS_IP_MAX - 1] = '\0';

            settings.nmea_port = prefs.getUShort(SETTINGS_KEY_PORT, WIFI_DEFAULT_TCP_PORT);
            settings.protocol = (NmeaProtocol)prefs.getUChar(SETTINGS_KEY_PROTOCOL, PROTO_TCP);

            // Load saved values - AP mode
            settings.ap_mode = prefs.getBool(SETTINGS_KEY_AP_MODE, false);

            String apSsid = prefs.getString(SETTINGS_KEY_AP_SSID, AP_SSID);
            String apPass = prefs.getString(SETTINGS_KEY_AP_PASS, AP_PASS);

            strncpy(settings.ap_ssid, apSsid.c_str(), SETTINGS_SSID_MAX - 1);
            settings.ap_ssid[SETTINGS_SSID_MAX - 1] = '\0';

            strncpy(settings.ap_pass, apPass.c_str(), SETTINGS_PASS_MAX - 1);
            settings.ap_pass[SETTINGS_PASS_MAX - 1] = '\0';

            settings.listen_port = prefs.getUShort(SETTINGS_KEY_LISTEN_PORT, WIFI_DEFAULT_TCP_PORT);
            settings.listen_protocol = (NmeaProtocol)prefs.getUChar(SETTINGS_KEY_LISTEN_PROTOCOL, PROTO_TCP);

            Serial.println("Settings loaded from NVS:");
            Serial.printf("  AP Mode: %s\n", settings.ap_mode ? "enabled" : "disabled");
            if (settings.ap_mode) {
                Serial.printf("  AP SSID: %s\n", settings.ap_ssid);
                Serial.printf("  Listen Port: %d (%s)\n", settings.listen_port,
                             settings.listen_protocol == PROTO_TCP ? "TCP" : "UDP");
            } else {
                Serial.printf("  WiFi SSID: %s\n", settings.wifi_ssid);
                Serial.printf("  NMEA IP: %s\n", settings.nmea_ip);
                Serial.printf("  NMEA Port: %d (%s)\n", settings.nmea_port,
                             settings.protocol == PROTO_TCP ? "TCP" : "UDP");
            }
        }
    } else {
        Serial.println("No saved settings found, using defaults");
    }

    prefs.end();
}

void SettingsManager::save() {
    if (!initialized) {
        begin();
    }

    prefs.begin(SETTINGS_NAMESPACE, false);  // Read-write mode

    // Save Station mode settings
    prefs.putString(SETTINGS_KEY_SSID, settings.wifi_ssid);
    prefs.putString(SETTINGS_KEY_PASS, settings.wifi_pass);
    prefs.putString(SETTINGS_KEY_IP, settings.nmea_ip);
    prefs.putUShort(SETTINGS_KEY_PORT, settings.nmea_port);
    prefs.putUChar(SETTINGS_KEY_PROTOCOL, (uint8_t)settings.protocol);

    // Save AP mode settings
    prefs.putBool(SETTINGS_KEY_AP_MODE, settings.ap_mode);
    prefs.putString(SETTINGS_KEY_AP_SSID, settings.ap_ssid);
    prefs.putString(SETTINGS_KEY_AP_PASS, settings.ap_pass);
    prefs.putUShort(SETTINGS_KEY_LISTEN_PORT, settings.listen_port);
    prefs.putUChar(SETTINGS_KEY_LISTEN_PROTOCOL, (uint8_t)settings.listen_protocol);

    prefs.putBool(SETTINGS_KEY_CONFIGURED, true);

    settings.configured = true;

    prefs.end();

    Serial.println("Settings saved to NVS");
}

void SettingsManager::clear() {
    if (!initialized) {
        begin();
    }

    prefs.begin(SETTINGS_NAMESPACE, false);
    prefs.clear();
    prefs.end();

    // Reset to defaults - Station mode
    strncpy(settings.wifi_ssid, WIFI_DEFAULT_SSID, SETTINGS_SSID_MAX - 1);
    settings.wifi_ssid[SETTINGS_SSID_MAX - 1] = '\0';

    strncpy(settings.wifi_pass, WIFI_DEFAULT_PASSWORD, SETTINGS_PASS_MAX - 1);
    settings.wifi_pass[SETTINGS_PASS_MAX - 1] = '\0';

    strncpy(settings.nmea_ip, WIFI_DEFAULT_IP, SETTINGS_IP_MAX - 1);
    settings.nmea_ip[SETTINGS_IP_MAX - 1] = '\0';

    settings.nmea_port = WIFI_DEFAULT_TCP_PORT;
    settings.protocol = PROTO_TCP;

    // Reset to defaults - AP mode
    settings.ap_mode = false;
    strncpy(settings.ap_ssid, AP_SSID, SETTINGS_SSID_MAX - 1);
    settings.ap_ssid[SETTINGS_SSID_MAX - 1] = '\0';

    strncpy(settings.ap_pass, AP_PASS, SETTINGS_PASS_MAX - 1);
    settings.ap_pass[SETTINGS_PASS_MAX - 1] = '\0';

    settings.listen_port = WIFI_DEFAULT_TCP_PORT;
    settings.listen_protocol = PROTO_TCP;

    settings.configured = false;

    Serial.println("Settings cleared (factory reset)");
}

bool SettingsManager::isConfigured() {
    return settings.configured;
}

SaildropSettings* SettingsManager::get() {
    return &settings;
}

void SettingsManager::setWifiSsid(const char* ssid) {
    strncpy(settings.wifi_ssid, ssid, SETTINGS_SSID_MAX - 1);
    settings.wifi_ssid[SETTINGS_SSID_MAX - 1] = '\0';
}

void SettingsManager::setWifiPass(const char* pass) {
    strncpy(settings.wifi_pass, pass, SETTINGS_PASS_MAX - 1);
    settings.wifi_pass[SETTINGS_PASS_MAX - 1] = '\0';
}

void SettingsManager::setNmeaIp(const char* ip) {
    strncpy(settings.nmea_ip, ip, SETTINGS_IP_MAX - 1);
    settings.nmea_ip[SETTINGS_IP_MAX - 1] = '\0';
}

void SettingsManager::setNmeaPort(uint16_t port) {
    settings.nmea_port = port;
}

void SettingsManager::setApMode(bool enabled) {
    settings.ap_mode = enabled;
}

void SettingsManager::setApSsid(const char* ssid) {
    strncpy(settings.ap_ssid, ssid, SETTINGS_SSID_MAX - 1);
    settings.ap_ssid[SETTINGS_SSID_MAX - 1] = '\0';
}

void SettingsManager::setApPass(const char* pass) {
    strncpy(settings.ap_pass, pass, SETTINGS_PASS_MAX - 1);
    settings.ap_pass[SETTINGS_PASS_MAX - 1] = '\0';
}

void SettingsManager::setListenPort(uint16_t port) {
    settings.listen_port = port;
}

void SettingsManager::setProtocol(NmeaProtocol proto) {
    settings.protocol = proto;
}

void SettingsManager::setListenProtocol(NmeaProtocol proto) {
    settings.listen_protocol = proto;
}
