#if defined(ARDUINO) 
SYSTEM_MODE(MANUAL); 
#endif

#include "Adafruit_DHT.h"
#include "Adafruit_SSD1306.h"
#include "MyClock.h"

const char ssid[] = "ssid";
const char pass[] = "password";
const int interval = 5 * 60 * 10;
const int MPU_addr = 0x68;

// Connect pin 1 (on the left) of the sensor to +3.3V or +5V
// Connect pin 2 of the sensor to whatever your DHTPIN is
// Connect pin 4 (on the right) of the sensor to GROUND
// Connect a 10K resistor from data pin to pin power of the sensor
DHT dht(A0, DHT22); // Possiblee constants: DHT11, DHT22 (AM2302), DHT21 (AM2301)

// Connect pin VCC to +3.3V
// Connect pin GND to GROUND
// Connect pin SDA to D0
// Connect pin SCL to D1
Adafruit_SSD1306 oled(4); // Reset pin (unused)

uint16_t port = 3000;
IPAddress server(192, 168, 1, 203);
TCPClient client;
MyClock mclock;
long tclock = 0;

// Actual and prev gyro values
int16_t gx, gy, gz;
int16_t lx = 0, ly = 0, lz = 0;

// Actual sleep movement, temperature and humidity
long sum = 0;
float temperature = 0, humidity = 0;

// Count of cycles
int count = interval - 5 * 10;

long last = 0;

// ----------------------------------------------------------------------------

void print_wifi_status();
void show();
void read_mpu();
int accuracy(int diff);
void send_msg();
void recv_msg();

// ----------------------------------------------------------------------------
// Setup
// ----------------------------------------------------------------------------

void setup_mpu() {
  Wire.begin();
  Wire.beginTransmission(MPU_addr);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission(true);

  Serial.println("mpu6050");
  oled.println("- MPU");
  oled.display();
  delay(100);
}

void setup_dht() {
  dht.begin();

  Serial.println("dht");
  oled.println("- DHT");
  oled.display();
  delay(100);
}

void setup_wifi() {
  oled.print("- WiFi");
  oled.display();

  WiFi.on();
  WiFi.setCredentials(ssid, pass);
  WiFi.connect();

  while (WiFi.connecting()) {
    Serial.print(".");
    oled.print(".");
    oled.display();
    delay(300);
  }
  
  Serial.println("\nYou're connected to the network");
  Serial.println("Waiting for an IP address");
  
  IPAddress localIP = WiFi.localIP();
  while (localIP[0] == 0) {
    localIP = WiFi.localIP();
    Serial.println("Waiting for an IP address");
    oled.print(".");
    oled.display();
    delay(1000);
  }

  oled.print(" OK");
  oled.display();

  Serial.println("\nIP Address obtained");
  print_wifi_status();
}

void setup() {
  Serial.begin(115200);
  Serial.println("setup");

  oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  oled.setTextSize(2);
  oled.setTextColor(WHITE);
  oled.clearDisplay();
  oled.println("Setup:");
  oled.display();
  oled.setTextSize(1);

  setup_mpu();
  setup_dht();
  setup_wifi();

  oled.println("Done");
  oled.display();
}

// ----------------------------------------------------------------------------
// Main
// ----------------------------------------------------------------------------

void loop() {
  read_mpu();

  // Add differences to sum
  int dx = accuracy(abs(gx - lx));
  int dy = accuracy(abs(gy - ly));
  int dz = accuracy(abs(gz - lz));
  lx = gx;
  ly = gy;
  lz = gz;
  sum += (dx + dy + dz);

  count++;
  if (count >= interval) {

    // Avoid big numbers
    if (sum > 500) sum = 500;

    float h = dht.getHumidity();
    float t = dht.getTempCelcius();
    float f = dht.getTempFarenheit();
    if (isnan(h) || isnan(t) || isnan(f)) {
      Serial.println("failed to read from DHT sensor!");
      temperature = 0;
      humidity = 0;
    } else {
      float hi = dht.getHeatIndex();
      float dp = dht.getDewPoint();
      float k = dht.getTempKelvin();

      temperature = t;
      humidity = h;
    }

    send_msg();
    sum = 0;
    count = 0;
  }

  mclock.proceed();
  long now = millis();
  if (tclock < now) {
    tclock = now + 5000;
    show();
  }

  recv_msg();
  delay(100);
}

void print_wifi_status() {
  Serial.print("Network Name: ");
  Serial.println(WiFi.SSID());

  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}

void show() {
  // Serial.println("show");

  oled.clearDisplay();
  oled.setCursor(0, 0);
  oled.setTextSize(1);

  // Show temperature and humidity
  oled.setCursor(0, 0);
  oled.print(temperature);
  oled.print(" *C");
  oled.setCursor(88, 0);
  oled.print(humidity);
  oled.print("%");

  // Show sleep data
  oled.setCursor(0, 56);
  oled.print(sum);

  // Show clock
  oled.setCursor(2, 18);
  oled.setTextSize(4);

  if (mclock.h < 10) oled.print("0"); oled.print(mclock.h);
  oled.print(":");
  if (mclock.m < 10) oled.print("0"); oled.print(mclock.m);

  oled.display();
}

void read_mpu() {
  int16_t ax, ay, az, tmp;
  Wire.beginTransmission(MPU_addr);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_addr, 14, true);
  ax  = Wire.read() <<8 | Wire.read();
  ay  = Wire.read() <<8 | Wire.read();
  az  = Wire.read() <<8 | Wire.read();
  tmp = Wire.read() <<8 | Wire.read();
  gx  = Wire.read() <<8 | Wire.read();
  gy  = Wire.read() <<8 | Wire.read();
  gz  = Wire.read() <<8 | Wire.read();
}

// Ignore low values
int accuracy(int diff) {
  if (diff < 100) {
    return 0;
  } else {
    return diff / 100;
  }
}

// ----------------------------------------------------------------------------
// Communication
// ----------------------------------------------------------------------------

void send_msg() {
  Serial.println("Attempting to connect to server");
  uint8_t tries = 0;
  while (client.connect(server, port) == false) {
    Serial.print(".");
    if (tries++ > 100) {
      Serial.println("\nThe server isn't responding");
      return;
    }
    delay(100);
  }
  Serial.println("\nConnected to the server!");

  String body;

  // Sleep data
  Serial.println("Sending sleep data...");
  body = "{\"device\":\"petr\",\"value\":\"";
  body = body + String(sum);;
  body = body + "\"}";
  client.println("POST /api/sleeps HTTP/1.1");
  client.println("Host: 192.168.1.203");
  client.println("Cache-Control: no-cache");
  client.println("Content-Type: application/json");
  client.print("Content-Length: ");
  client.println(body.length());
  client.println();
  client.println(body);
  client.println();

  // Temperature and humidity
  Serial.println("Sending temperature and humidity...");
  body = "{\"dev\":\"bedroom\",\"t\":\"";
  body = body + String(temperature);
  body = body + "\",\"h\":\"";
  body = body + String(humidity);
  body = body + "\"}";
  client.println("POST /api/measurements HTTP/1.1");
  client.println("Host: 192.168.1.203");
  client.println("Cache-Control: no-cache");
  client.println("Content-Type: application/json");
  client.print("Content-Length: ");
  client.println(body.length());
  client.println();
  client.println(body);
  client.println();

  Serial.println("Done!");
}

void recv_msg() {
  if (!client.available()) {
    // Serial.println("no received data");
    return;
  }

  char buffer[255] = {0};
  client.read((uint8_t*) buffer, client.available());
  // Serial.print("Received: ");
  // Serial.println(buffer);

  // Read response from server and parse clock
  int i = -1;
  while (true) {
    i++;
    if (i >= 255 - 30) {
      break;
    }

    // Expected: "created_at":"2016-08-09T18:42:06.309Z"
    // Message should containt at least: "created_at":"xxxxxxxxxxx18:42:
    if (buffer[i +  0] != '"') continue;
    if (buffer[i +  1] != 'c') continue;
    if (buffer[i +  2] != 'r') continue;
    if (buffer[i +  3] != 'e') continue;
    if (buffer[i +  4] != 'a') continue;
    if (buffer[i +  5] != 't') continue;
    if (buffer[i +  6] != 'e') continue;
    if (buffer[i +  7] != 'd') continue;
    if (buffer[i +  8] != '_') continue;
    if (buffer[i +  9] != 'a') continue;
    if (buffer[i + 10] != 't') continue;
    if (buffer[i + 11] != '"') continue;
    if (buffer[i + 12] != ':') continue;
    if (buffer[i + 13] != '"') continue;
    if (buffer[i + 30] != ':') continue;

    mclock.h = 10 * (((int) buffer[i + 25]) - 48) + (((int) buffer[i + 26]) - 48);
    mclock.m = 10 * (((int) buffer[i + 28]) - 48) + (((int) buffer[i + 29]) - 48);
    // Serial.println("----- clock -----");

    // Timezone +2
    mclock.h += 2;
    mclock.h %= 24;
    break;
  }
}

