#include <WiFi.h>
#include <MicroNMEA.h>
// #include <WiFiUdp.h>
#include "conn.h"
#include "conf.h"
#include "data.h"

String networks[32];
uint8_t n_networks = 0;
bool connected = false;

// WiFiUDP udp;
WiFiClient tcp;
uint8_t packetBuffer[255];

char nmeaBuffer[100];
MicroNMEA nmea(nmeaBuffer, sizeof(nmeaBuffer));


void initialize_connections()
{
    #ifdef AP_MODE
        WiFi.mode(WIFI_AP);
        WiFi.onEvent(on_wifi_event);
        
        bool result = WiFi.softAP(AP_SSID, AP_PASS);
        if (!result) {
            Serial.println("Failed to start AP");
        } else {
            Serial.println("AP started successfully");
        }
    #else
        WiFi.mode(WIFI_STA);
    #endif
    WiFi.onEvent(on_wifi_event);
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
    if (!connected) {
        Serial.println("Trying to connect...");
        if (tcp.connect(WIFI_DEFAULT_IP, WIFI_DEFAULT_TCP_PORT)) {
            Serial.println("TCP connected!");
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

    // int packetSize = udp.parsePacket();
    // Serial.print("Received packet from: "); 
    // Serial.println(udp.remoteIP());
    // Serial.print("Size: "); 
    // Serial.println(packetSize);

    // if (packetSize) {
    //     int len = udp.read(packetBuffer, 255);
    //     if (len > 0) packetBuffer[len - 1] = 0;
    //     Serial.printf("Data : %s\n", packetBuffer);
    // }
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
