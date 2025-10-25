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

#ifndef CONN_H
#define CONN_H

#include <WiFi.h>

void initialize_connections();
void list_networks();
void connect_wifi(const char *ssid, const char *password);
void disconnect_wifi();
void is_network_present();
void connect();
void disconnect();
void conn_loop();

void on_wifi_event(WiFiEvent_t event);

#endif
