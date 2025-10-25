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