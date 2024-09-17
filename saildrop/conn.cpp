#include <WiFi.h>
#include "conn.h"

String networks[32];
uint8_t n_networks = 0;

void initialize_connections()
{
    WiFi.mode(WIFI_STA);
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
