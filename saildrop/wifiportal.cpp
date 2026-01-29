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
#include "wifiportal.h"
#include "settings.h"
#include <stdlib.h>

static WifiPortal portalInstance;

// Callback for when config is saved
static void saveConfigCallback() {
    Serial.println("WiFiManager: Config saved callback triggered");
}

WifiPortal* getPortal() {
    return &portalInstance;
}

WifiPortal::WifiPortal()
    : paramNmeaIp(nullptr)
    , paramNmeaPort(nullptr)
    , paramProtocol(nullptr)
    , paramApMode(nullptr)
    , paramApSsid(nullptr)
    , paramApPass(nullptr)
    , paramListenPort(nullptr)
    , paramListenProtocol(nullptr)
    , status(PORTAL_IDLE)
    , paramsCreated(false) {
}

WifiPortal::~WifiPortal() {
    cleanupParameters();
}

void WifiPortal::createParameters() {
    if (paramsCreated) return;

    SaildropSettings* s = getSettings()->get();

    // Create AP Mode checkbox (value "1" = enabled, "" = disabled)
    // Using a custom HTML input for checkbox
    const char* apModeHtml = "<br/><label for='ap_mode'>AP Mode (create own WiFi)</label>"
                             "<input type='checkbox' id='ap_mode' name='ap_mode' value='1' %s>"
                             "<br/><small>Enable to run as access point instead of connecting to WiFi</small><br/>";
    static char apModeBuffer[256];
    snprintf(apModeBuffer, sizeof(apModeBuffer), apModeHtml, s->ap_mode ? "checked" : "");
    paramApMode = new WiFiManagerParameter(apModeBuffer);

    // Create AP SSID parameter
    paramApSsid = new WiFiManagerParameter(
        "ap_ssid",
        "AP SSID (when AP mode enabled)",
        s->ap_ssid,
        32
    );

    // Create AP Password parameter
    paramApPass = new WiFiManagerParameter(
        "ap_pass",
        "AP Password",
        s->ap_pass,
        64
    );

    // Create Listen Port parameter (for AP mode)
    char listenPortStr[6];
    snprintf(listenPortStr, sizeof(listenPortStr), "%d", s->listen_port);
    paramListenPort = new WiFiManagerParameter(
        "listen_port",
        "Listen Port (AP mode)",
        listenPortStr,
        5
    );

    // Create Listen Protocol dropdown (for AP mode)
    const char* listenProtoHtml = "<br/><label for='listen_proto'>Protocol (AP mode)</label>"
                                  "<select id='listen_proto' name='listen_proto'>"
                                  "<option value='0' %s>TCP</option>"
                                  "<option value='1' %s>UDP</option>"
                                  "</select><br/>";
    static char listenProtoBuffer[256];
    snprintf(listenProtoBuffer, sizeof(listenProtoBuffer), listenProtoHtml,
             s->listen_protocol == PROTO_TCP ? "selected" : "",
             s->listen_protocol == PROTO_UDP ? "selected" : "");
    paramListenProtocol = new WiFiManagerParameter(listenProtoBuffer);

    // Create NMEA IP parameter (for station mode)
    paramNmeaIp = new WiFiManagerParameter(
        "nmea_ip",
        "NMEA Server IP (Station mode)",
        s->nmea_ip,
        15
    );

    // Create NMEA Port parameter (for station mode)
    char portStr[6];
    snprintf(portStr, sizeof(portStr), "%d", s->nmea_port);
    paramNmeaPort = new WiFiManagerParameter(
        "nmea_port",
        "NMEA Port (Station mode)",
        portStr,
        5
    );

    // Create Protocol dropdown (for station mode)
    const char* protoHtml = "<br/><label for='protocol'>Protocol (Station mode)</label>"
                            "<select id='protocol' name='protocol'>"
                            "<option value='0' %s>TCP</option>"
                            "<option value='1' %s>UDP</option>"
                            "</select><br/>";
    static char protoBuffer[256];
    snprintf(protoBuffer, sizeof(protoBuffer), protoHtml,
             s->protocol == PROTO_TCP ? "selected" : "",
             s->protocol == PROTO_UDP ? "selected" : "");
    paramProtocol = new WiFiManagerParameter(protoBuffer);

    paramsCreated = true;
}

void WifiPortal::cleanupParameters() {
    if (paramNmeaIp) {
        delete paramNmeaIp;
        paramNmeaIp = nullptr;
    }
    if (paramNmeaPort) {
        delete paramNmeaPort;
        paramNmeaPort = nullptr;
    }
    if (paramApMode) {
        delete paramApMode;
        paramApMode = nullptr;
    }
    if (paramApSsid) {
        delete paramApSsid;
        paramApSsid = nullptr;
    }
    if (paramApPass) {
        delete paramApPass;
        paramApPass = nullptr;
    }
    if (paramListenPort) {
        delete paramListenPort;
        paramListenPort = nullptr;
    }
    if (paramProtocol) {
        delete paramProtocol;
        paramProtocol = nullptr;
    }
    if (paramListenProtocol) {
        delete paramListenProtocol;
        paramListenProtocol = nullptr;
    }
    paramsCreated = false;
}

void WifiPortal::begin() {
    // Create custom parameters
    createParameters();

    // Add AP mode parameters first (checkbox at top)
    wm.addParameter(paramApMode);
    wm.addParameter(paramApSsid);
    wm.addParameter(paramApPass);
    wm.addParameter(paramListenPort);
    wm.addParameter(paramListenProtocol);

    // Add station mode parameters
    wm.addParameter(paramNmeaIp);
    wm.addParameter(paramNmeaPort);
    wm.addParameter(paramProtocol);

    // Set save config callback
    wm.setSaveConfigCallback(saveConfigCallback);

    // Configure portal timeout
    wm.setConfigPortalTimeout(PORTAL_TIMEOUT_SEC);

    // Set AP callback for when portal starts
    wm.setAPCallback([](WiFiManager* wm) {
        Serial.println("WiFiManager: Portal started");
        Serial.printf("  AP SSID: %s\n", PORTAL_SSID);
        Serial.printf("  AP IP: %s\n", WiFi.softAPIP().toString().c_str());
    });

    // Set minimum signal quality
    wm.setMinimumSignalQuality(20);

    // Don't show the saved networks in the scan
    wm.setRemoveDuplicateAPs(true);

    Serial.println("WiFiManager initialized");
}

bool WifiPortal::startPortal() {
    status = PORTAL_RUNNING;

    Serial.println("Starting WiFi configuration portal...");

    // Start the configuration portal (blocking)
    bool success = wm.startConfigPortal(PORTAL_SSID, PORTAL_PASS);

    if (success) {
        status = PORTAL_SUCCESS;
        Serial.println("WiFiManager: Configuration successful");

        // Check if AP mode is enabled (checkbox returns "1" if checked)
        bool apModeEnabled = wm.server->hasArg("ap_mode") && wm.server->arg("ap_mode") == "1";
        getSettings()->setApMode(apModeEnabled);
        Serial.printf("  AP Mode: %s\n", apModeEnabled ? "enabled" : "disabled");

        // Save AP mode parameters
        if (paramApSsid && strlen(paramApSsid->getValue()) > 0) {
            getSettings()->setApSsid(paramApSsid->getValue());
            Serial.printf("  AP SSID: %s\n", paramApSsid->getValue());
        }

        if (paramApPass && strlen(paramApPass->getValue()) > 0) {
            getSettings()->setApPass(paramApPass->getValue());
        }

        if (paramListenPort && strlen(paramListenPort->getValue()) > 0) {
            uint16_t port = atoi(paramListenPort->getValue());
            if (port > 0 && port <= 65535) {
                getSettings()->setListenPort(port);
                Serial.printf("  Listen Port: %d\n", port);
            }
        }

        // Save listen protocol (AP mode)
        if (wm.server->hasArg("listen_proto")) {
            int proto = atoi(wm.server->arg("listen_proto").c_str());
            getSettings()->setListenProtocol(proto == 1 ? PROTO_UDP : PROTO_TCP);
            Serial.printf("  Listen Protocol: %s\n", proto == 1 ? "UDP" : "TCP");
        }

        // Save station mode parameters
        if (paramNmeaIp && strlen(paramNmeaIp->getValue()) > 0) {
            getSettings()->setNmeaIp(paramNmeaIp->getValue());
            Serial.printf("  NMEA IP: %s\n", paramNmeaIp->getValue());
        }

        if (paramNmeaPort && strlen(paramNmeaPort->getValue()) > 0) {
            uint16_t port = atoi(paramNmeaPort->getValue());
            if (port > 0 && port <= 65535) {
                getSettings()->setNmeaPort(port);
                Serial.printf("  NMEA Port: %d\n", port);
            }
        }

        // Save station mode protocol
        if (wm.server->hasArg("protocol")) {
            int proto = atoi(wm.server->arg("protocol").c_str());
            getSettings()->setProtocol(proto == 1 ? PROTO_UDP : PROTO_TCP);
            Serial.printf("  Protocol: %s\n", proto == 1 ? "UDP" : "TCP");
        }

        // Save WiFi credentials to our settings (even if AP mode, keep them for later)
        getSettings()->setWifiSsid(WiFi.SSID().c_str());
        getSettings()->setWifiPass(WiFi.psk().c_str());

        // Persist to NVS
        getSettings()->save();

        return true;
    } else {
        status = PORTAL_TIMEDOUT;
        Serial.println("WiFiManager: Portal timeout or cancelled");
        return false;
    }
}

bool WifiPortal::autoConnect() {
    status = PORTAL_RUNNING;

    Serial.println("WiFiManager: Attempting auto-connect...");

    // autoConnect tries saved credentials first, then starts portal if fail
    bool success = wm.autoConnect(PORTAL_SSID, PORTAL_PASS);

    if (success) {
        status = PORTAL_SUCCESS;
        Serial.println("WiFiManager: Connected successfully");

        // If portal was used (not just saved creds), save the new settings
        if (paramNmeaIp && strlen(paramNmeaIp->getValue()) > 0) {
            getSettings()->setNmeaIp(paramNmeaIp->getValue());
        }

        if (paramNmeaPort && strlen(paramNmeaPort->getValue()) > 0) {
            uint16_t port = atoi(paramNmeaPort->getValue());
            if (port > 0 && port <= 65535) {
                getSettings()->setNmeaPort(port);
            }
        }

        // Update WiFi credentials in settings
        getSettings()->setWifiSsid(WiFi.SSID().c_str());
        getSettings()->setWifiPass(WiFi.psk().c_str());
        getSettings()->save();

        return true;
    } else {
        status = PORTAL_FAILED;
        Serial.println("WiFiManager: Auto-connect failed");
        return false;
    }
}

PortalStatus WifiPortal::getStatus() {
    return status;
}

void WifiPortal::resetSettings() {
    wm.resetSettings();
    Serial.println("WiFiManager: Settings reset");
}
