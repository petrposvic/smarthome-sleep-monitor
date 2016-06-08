#include <ESP8266WiFi.h>
#include <ArduinoJson.h>

const char* ssid     = "ssid";
const char* password = "pass";
const char* host     = "192.168.1.203";
const int   port     = 3000;

StaticJsonBuffer<200> jsonBuffer;
JsonObject& root = jsonBuffer.createObject();
String data = "";

class ByteCounter : public Print {
  public:
    ByteCounter(): len(0) {}

    virtual size_t write(uint8_t c) {
      len++;
    }        

    int length() const {
      return len;
    }

  private:
    int len;
};

void setup() {
  Serial.begin(9600);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  root["device"] = "petr";
}

void loop() {
  while (Serial.available() > 0) {
    char ch = Serial.read();

    // Ignore + character, only for debugging
    if (ch == '+') {
      Serial.print(ch);
      continue;
    }

    if (ch == '.') {
      root["value"] = data;
      sendData();

      data = "";
      break;
    }

    data += ch;
  }
}

void sendData() {
  WiFiClient client;
  if (client.connect(host, port)) {
    ByteCounter counter;
    root.printTo(counter);
    int contentLength = counter.length();

    client.println("POST /api/sleeps HTTP/1.1");
    client.print("Host: "); client.println(host);
    client.println("Cache-Control: no-cache");
    client.println("Content-Type: application/json");
    client.print("Content-Length: ");
    client.println(contentLength);
    client.println();
    root.printTo(client);
    client.println();
    delay(500);
  }
}

