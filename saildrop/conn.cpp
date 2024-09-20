#include <WiFi.h>
#include <WiFiUdp.h>
#include "conn.h"
#include "conf.h"

String networks[32];
uint8_t n_networks = 0;
bool connected = false;

WiFiUDP udp;
char packetBuffer[255];

void initialize_connections()
{
    WiFi.mode(WIFI_STA);
    WiFi.onEvent(on_wifi_event);
}

void on_wifi_event(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.print("WiFi connected! IP address: ");
      Serial.println(WiFi.localIP());
      udp.begin(WIFI_DEFAULT_UDP_PORT);
      connected = true;
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.println("WiFi lost connection");
      connected = false;
      break;
    default: break;
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
    int packetSize = udp.parsePacket();
    Serial.print(" Received packet from : "); Serial.println(udp.remoteIP());
    Serial.print(" Size : "); Serial.println(packetSize);
    if (packetSize) {
        int len = udp.read(packetBuffer, 255);
        if (len > 0) packetBuffer[len - 1] = 0;
        Serial.printf("Data : %s\n", packetBuffer);
    }
}

void disconnect_wifi()
{
}

void is_network_present()
{
}

void connect()
{
}

void disconnect()
{
}
