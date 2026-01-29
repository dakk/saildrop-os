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
#include <WiFi.h>
#include <MicroNMEA.h>
#include <WiFiUdp.h>
#include "conn.h"
#include "conf.h"
#include "data.h"
#include "settings.h"

String networks[32];
uint8_t n_networks = 0;
bool connected = false;
bool ap_mode_active = false;
NmeaProtocol active_protocol = PROTO_TCP;

WiFiUDP udp;
WiFiClient tcp;
WiFiServer* tcpServer = nullptr;
WiFiClient serverClient;
uint8_t packetBuffer[255];

char nmeaBuffer[100];
MicroNMEA nmea(nmeaBuffer, sizeof(nmeaBuffer));

// Forward declarations
void conn_loop_server();
void conn_loop_client();


void initialize_connections()
{
    ap_mode_active = false;
    WiFi.onEvent(on_wifi_event);
}

void initialize_connections_ap(uint16_t listen_port, NmeaProtocol protocol)
{
    ap_mode_active = true;
    active_protocol = protocol;

    if (protocol == PROTO_UDP) {
        // Start UDP server
        udp.begin(listen_port);
        Serial.printf("UDP Server started on port %d\n", listen_port);
    } else {
        // Create and start TCP server
        if (tcpServer) {
            tcpServer->stop();
            delete tcpServer;
        }
        tcpServer = new WiFiServer(listen_port);
        tcpServer->begin();
        Serial.printf("TCP Server started on port %d\n", listen_port);
    }

    connected = true;  // In AP mode, we're always "connected" (waiting for clients)
}

void on_wifi_event(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.print("WiFi connected! IP address: ");
      Serial.println(WiFi.localIP());
    //   udp.begin(WIFI_DEFAULT_UDP_PORT);
      connected = true;
      break;
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      Serial.println("WiFi connected, waiting IP...");
      connected = true;
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.println("WiFi lost connection");
      connected = false;
      break;
    default: 
      Serial.println("WiFi unknown event");
      break;
  }
}

void list_networks()
{
    n_networks = 0;
    Serial.println("Wifi scan start");

    // WiFi.scanNetworks will return the number of networks found
    int n = WiFi.scanNetworks();
    Serial.println("Wifi scan done");
    if (n == 0)
    {
        Serial.println("No networks found");
    }
    else
    {
        Serial.print(n);
        Serial.println(" networks found");
        for (int i = 0; i < n; ++i)
        {
            // Print SSID and RSSI for each network found
            Serial.print(i + 1);
            Serial.print(": ");
            Serial.print(WiFi.SSID(i));
            Serial.print(" (");
            Serial.print(WiFi.RSSI(i));
            Serial.print(")");
            Serial.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? " " : "*");

            if (WiFi.encryptionType(i) == WIFI_AUTH_OPEN)
            {
                networks[n_networks] = WiFi.SSID(i);
                n_networks++;
            }
        }
    }
    Serial.println("");
}

void connect_wifi(const char *ssid, const char *password)
{
    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi ..");
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print('.');
        delay(1000);
    }
    Serial.println(WiFi.localIP());
}


void conn_loop() {
    if (ap_mode_active) {
        // AP Mode: Listen for incoming connections
        conn_loop_server();
    } else {
        // Station Mode: Connect to remote NMEA server
        conn_loop_client();
    }
}

// Server mode loop (AP mode - listening for connections)
void conn_loop_server() {
    if (active_protocol == PROTO_UDP) {
        // UDP server mode
        int packetSize = udp.parsePacket();
        if (packetSize > 0) {
            int len = udp.read(packetBuffer, sizeof(packetBuffer) - 1);
            if (len > 0) {
                packetBuffer[len] = '\0';
                Serial.printf("UDP Received from %s:%d: %s\n",
                             udp.remoteIP().toString().c_str(),
                             udp.remotePort(), packetBuffer);

                // Process NMEA data
                for (int i = 0; i < len; i++)
                    nmea.process(packetBuffer[i]);

                get_data()->sog = nmea.getSpeed() / 100.;
                get_data()->hdg = nmea.getCourse() / 1000.;
            }
        }
    } else {
        // TCP server mode
        if (!tcpServer) return;

        // Check for new client connections
        if (tcpServer->hasClient()) {
            if (serverClient && serverClient.connected()) {
                // Already have a client, reject new one
                WiFiClient rejectedClient = tcpServer->available();
                rejectedClient.stop();
                Serial.println("Rejected new client (already connected)");
            } else {
                // Accept new client
                serverClient = tcpServer->available();
                Serial.printf("New client connected from %s\n",
                             serverClient.remoteIP().toString().c_str());
            }
        }

        // Read data from connected client
        if (serverClient && serverClient.connected() && serverClient.available()) {
            int len = serverClient.read(packetBuffer, sizeof(packetBuffer) - 1);
            if (len > 0) {
                packetBuffer[len] = '\0';
                Serial.printf("Received: %s\n", packetBuffer);

                // Process NMEA data
                for (int i = 0; i < len; i++)
                    nmea.process(packetBuffer[i]);

                get_data()->sog = nmea.getSpeed() / 100.;
                get_data()->hdg = nmea.getCourse() / 1000.;
            }
        }

        // Check for client disconnect
        if (serverClient && !serverClient.connected()) {
            Serial.println("Client disconnected");
            serverClient.stop();
        }
    }
}

// Client mode loop (Station mode - connecting to remote server)
void conn_loop_client() {
    SaildropSettings* s = getSettings()->get();

    if (s->protocol == PROTO_UDP) {
        // UDP client mode
        static bool udpStarted = false;
        if (!udpStarted) {
            udp.begin(s->nmea_port);  // Listen on the same port
            Serial.printf("UDP client listening on port %d\n", s->nmea_port);
            udpStarted = true;
            connected = true;
        }

        int packetSize = udp.parsePacket();
        if (packetSize > 0) {
            int len = udp.read(packetBuffer, sizeof(packetBuffer) - 1);
            if (len > 0) {
                packetBuffer[len] = '\0';
                Serial.printf("UDP Received: %s\n", packetBuffer);

                for (int i = 0; i < len; i++)
                    nmea.process(packetBuffer[i]);

                get_data()->sog = nmea.getSpeed() / 100.;
                get_data()->hdg = nmea.getCourse() / 1000.;
            }
        }
    } else {
        // TCP client mode
        if (!connected) {
            Serial.println("Trying to connect...");
            if (tcp.connect(s->nmea_ip, s->nmea_port)) {
                Serial.printf("TCP connected to %s:%d\n", s->nmea_ip, s->nmea_port);
                connected = true;
                tcp.println("Hello from ESP32");
            } else {
                Serial.println("TCP connection failed");
                delay(1000);
                return;
            }
        }

        // Check if data is available
        if (tcp.connected() && tcp.available()) {
            int len = tcp.read(packetBuffer, sizeof(packetBuffer) - 1);
            if (len > 0) {
                packetBuffer[len] = '\0';  // null-terminate properly
                Serial.printf("Received: %s\n", packetBuffer);

                for (int i = 0; i < len; i++)
                    nmea.process(packetBuffer[i]);

                Serial.print("Speed: ");
                get_data()->sog = nmea.getSpeed() / 100.;
                Serial.println(nmea.getSpeed() / 1000., 3);
                Serial.print("Course: ");
                get_data()->hdg = nmea.getCourse() / 1000.;
                Serial.println(nmea.getCourse() / 1000., 3);
            }
        }

        // Detect disconnect
        if (!tcp.connected()) {
            Serial.println("TCP disconnected");
            tcp.stop();
            connected = false;
        }
    }
}

void disconnect_wifi()
{
    connected = false;
}

void is_network_present()
{
}

void connect()
{
}

void disconnect()
{
    connected = false;
}
