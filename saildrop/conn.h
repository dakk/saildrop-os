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
