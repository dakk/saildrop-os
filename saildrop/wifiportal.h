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
#ifndef WIFIPORTAL_H
#define WIFIPORTAL_H

#include <WiFiManager.h>
#include "conf.h"

// Portal configuration
#ifndef PORTAL_SSID
#define PORTAL_SSID "SAILDROP_SETUP"
#endif

#ifndef PORTAL_PASS
#define PORTAL_PASS "saildrop"
#endif

#ifndef PORTAL_TIMEOUT_SEC
#define PORTAL_TIMEOUT_SEC 180  // 3 minutes
#endif

enum PortalStatus {
    PORTAL_IDLE,
    PORTAL_RUNNING,
    PORTAL_SUCCESS,
    PORTAL_TIMEDOUT,
    PORTAL_FAILED
};

class WifiPortal {
    // Allow save callback to access private members
    friend void saveAllParameters();

private:
    WiFiManager wm;
    // Station mode parameters
    WiFiManagerParameter* paramNmeaIp;
    WiFiManagerParameter* paramNmeaPort;
    WiFiManagerParameter* paramProtocol;
    // AP mode parameters
    WiFiManagerParameter* paramApMode;
    WiFiManagerParameter* paramApSsid;
    WiFiManagerParameter* paramApPass;
    WiFiManagerParameter* paramListenPort;
    WiFiManagerParameter* paramListenProtocol;

    PortalStatus status;
    bool paramsCreated;

    void createParameters();
    void cleanupParameters();

public:
    WifiPortal();
    ~WifiPortal();

    // Initialize WiFiManager with custom parameters
    void begin();

    // Start the captive portal (blocking)
    // Returns true if configuration was successful
    bool startPortal();

    // Start auto-connect (tries saved credentials first, then portal if fail)
    // Returns true if connected successfully
    bool autoConnect();

    // Get current portal status
    PortalStatus getStatus();

    // Reset WiFiManager saved credentials
    void resetSettings();
};

// Global singleton accessor
WifiPortal* getPortal();

#endif
