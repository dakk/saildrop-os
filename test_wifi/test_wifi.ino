#include <WiFi.h>

const char* ssid = "Redmi Note 14";
const char* password = "diversasailing";

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("Setting up AP...");

  WiFi.mode(WIFI_AP); // force AP mode
  WiFi.softAP("ESP32-AP", "12345678");

  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());
}

// void setup() {
//   Serial.begin(115200);
//   WiFi.mode(WIFI_STA);
//   WiFi.begin(ssid, password);

//   Serial.print("Connecting");
//   while (WiFi.status() != WL_CONNECTED) {
//     delay(500);
//     Serial.print(".");
//   }

//   Serial.println("\nConnected!");
//   Serial.print("IP: ");
//   Serial.println(WiFi.localIP());
// }

void loop() {}