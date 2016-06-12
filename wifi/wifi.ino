#include <ESP8266WiFi.h>
#include <ArduinoJson.h>

const char* ssid = "ssid";
const char* pass = "pass";
const char* host = "192.168.1.203";
const int   port = 3000;

// Store icoming data from serial, then send via network
String data = String();
char ch;

void setup() {
  Serial.begin(9600);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
}

void loop() {
  while (Serial.available() > 0) {
    char ch = Serial.read();

    if (
      // Number
      (ch < 48 || ch > 57) &&

      // .
      ch != 46 &&

      // +
      ch != 43
    ) {
      continue;
    }

    // Ignore + character, only for debugging
    if (ch == '+') {
      Serial.print(ch);
      continue;
    }

    if (ch == '.') {
      send_data();
      data = "";
      continue;
    }

    data += ch;
  }

  delay(1000);
}

void send_data() {

  // Avoid sending empty data
  if (data.length() < 1) {
    return;
  }

  WiFiClient client;
  if (!client.connect(host, port)) {
    delay(1000);
    return;
  }

  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["device"] = "petr";
  root["value"] = data;

  client.println("POST /api/sleeps HTTP/1.1");
  client.print("Host: "); client.println(host);
  client.println("Cache-Control: no-cache");
  client.println("Content-Type: application/json");
  client.print("Content-Length: "); client.println(root.measureLength());
  client.println();
  root.printTo(client);
  client.println();

  delay(5000);
}

