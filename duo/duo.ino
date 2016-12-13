#if defined(ARDUINO) 
SYSTEM_MODE(MANUAL); 
#endif

#define SERIAL
// #define DBG
#define DISPLAY
#define ACCEL

#include "Adafruit_DHT.h"
#include "MyClock.h"

#ifdef DISPLAY
#include "Adafruit_SSD1306.h"
#endif

const char* ssid = "ssid";
const char* pass = "pass";
const char* host = "192.168.1.204";
const int   port = 8086;
const int   interval = 5 * 60 * 1000;
const float calib_t = -1.6;
const float calib_h = 11;

#ifdef ACCEL
const int   MPU_addr = 0x68;
#endif

// Connect pin 1 (on the left) of the sensor to +3.3V or +5V
// Connect pin 2 of the sensor to whatever your DHTPIN is
// Connect pin 4 (on the right) of the sensor to GROUND
// Connect a 10K resistor from data pin to pin power of the sensor
DHT dht(A0, DHT22); // Possiblee constants: DHT11, DHT22 (AM2302), DHT21 (AM2301)

#ifdef DISPLAY
// Connect pin VCC to +3.3V
// Connect pin GND to GROUND
// Connect pin SDA to D0
// Connect pin SCL to D1
Adafruit_SSD1306 oled(4); // Reset pin (unused)
#endif

IPAddress server(192, 168, 1, 204);
TCPClient client, timeClient;
MyClock mclock;
long tsend = 20 * 1000; // Wait 10 secs from start to send
long tclock = 0;

#ifdef ACCEL
// Actual and prev gyro values
int16_t gx, gy, gz;
int16_t lx = 0, ly = 0, lz = 0;
#endif

// Actual sleep movement, temperature and humidity
long sum = 0;
float temperature = 0, humidity = 0;

// ----------------------------------------------------------------------------

void print_wifi_status();
void show();
int accuracy(int diff);
void send_msg();

#ifdef ACCEL
void read_mpu();
#endif

// ----------------------------------------------------------------------------
// Setup
// ----------------------------------------------------------------------------

#ifdef ACCEL
void setup_mpu() {
  Wire.begin();
  Wire.beginTransmission(MPU_addr);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission(true);

#ifdef SERIAL
  Serial.println("mpu6050");
#endif
#ifdef DISPLAY
  oled.println("- MPU");
  oled.display();
#endif
  delay(100);
}
#endif

void setup_dht() {
  dht.begin();

#ifdef SERIAL
  Serial.println("dht");
#endif
#ifdef DISPLAY
  oled.println("- DHT");
  oled.display();
#endif
  delay(100);
}

void setup_wifi() {

#ifdef DISPLAY
  oled.print("- WiFi");
  oled.display();
#endif

  WiFi.on();
  WiFi.setCredentials(ssid, pass);
  WiFi.connect();

  while (WiFi.connecting()) {
#ifdef SERIAL
    Serial.print(".");
#endif
#ifdef DISPLAY
    oled.print(".");
    oled.display();
#endif
    delay(300);
  }
  
#ifdef SERIAL
  Serial.println("\nYou're connected to the network");
  Serial.println("Waiting for an IP address");
#endif
  
  IPAddress localIP = WiFi.localIP();
  while (localIP[0] == 0) {
    localIP = WiFi.localIP();
#ifdef SERIAL
    Serial.println("Waiting for an IP address");
#endif
#ifdef DISPLAY
    oled.print(".");
    oled.display();
#endif
    delay(1000);
  }

#ifdef DISPLAY
  oled.print(" OK");
  oled.display();
#endif
#ifdef SERIAL
  Serial.println("\nIP Address obtained");
#endif

  print_wifi_status();
}

void setup() {
#ifdef SERIAL
  Serial.begin(115200);
  Serial.println("setup");
#endif
#ifdef DISPLAY
  oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  oled.setTextSize(2);
  oled.setTextColor(WHITE);
  oled.clearDisplay();
  oled.println("Setup:");
  oled.display();
  oled.setTextSize(1);
#endif

#ifdef ACCEL
  setup_mpu();
#endif
  setup_dht();
  setup_wifi();

#ifdef DISPLAY
  oled.println("Done");
  oled.display();
#endif
}

// ----------------------------------------------------------------------------
// Main
// ----------------------------------------------------------------------------

void loop() {
  long now = millis();

#ifdef ACCEL
  read_mpu();

  // Add differences to sum
  int dx = accuracy(abs(gx - lx));
  int dy = accuracy(abs(gy - ly));
  int dz = accuracy(abs(gz - lz));
  lx = gx;
  ly = gy;
  lz = gz;
  sum += (dx + dy + dz);
#endif

  if (tsend < now) {
    tsend = now + interval;

    // Avoid big numbers
    if (sum > 500) sum = 500;

    float h = dht.getHumidity();
    float t = dht.getTempCelcius();
    float f = dht.getTempFarenheit();
    if (isnan(h) || isnan(t) || isnan(f)) {
#ifdef SERIAL
      Serial.println("failed to read from DHT sensor!");
#endif
    } else {
      float hi = dht.getHeatIndex();
      float dp = dht.getDewPoint();
      float k = dht.getTempKelvin();

      temperature = t + calib_t;
      humidity = h + calib_h;
    }

    send_msg();
    sum = 0;
  } else {
#ifdef SERIAL
#ifdef DBG
    Serial.print((tsend - now) / 1000);
    Serial.println(" s to send data");
#endif
#endif
  }

  mclock.proceed();
  if (tclock < now) {
    tclock = now + 5 * 1000;
    show();
  }

  // Receive response from server
  while (client.available()) {
    String line = client.readStringUntil('\n');
#ifdef SERIAL
    Serial.println(line);
#endif
    if (line.startsWith("Date: ")) {
#ifdef SERIAL
      Serial.print("Found time header '");
      Serial.print(line);
      Serial.println("'");
#endif
      mclock.h = 10 * (((int) line.charAt(23)) - 48) + (((int) line.charAt(24)) - 48);
      mclock.m = 10 * (((int) line.charAt(26)) - 48) + (((int) line.charAt(27)) - 48);
  
      // Timezone +1 (summer time needs +2)
      mclock.h += 1;
      mclock.h %= 24;
#ifdef SERIAL
      Serial.print("Time is now = ");
      Serial.print(mclock.h);
      Serial.print(":");
      Serial.println(mclock.m);
#endif
    }
  }

  delay(100);
}

void print_wifi_status() {
#ifdef SERIAL
  Serial.print("Network Name: ");
  Serial.println(WiFi.SSID());
#endif

  IPAddress ip = WiFi.localIP();
#ifdef SERIAL
  Serial.print("IP Address: ");
  Serial.println(ip);
#endif

  long rssi = WiFi.RSSI();
#ifdef SERIAL
  Serial.print("Signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
#endif
}

void show() {
#ifdef SERIAL
#ifdef DBG
  Serial.print("show ");
  Serial.print(mclock.h);
  Serial.print(":");
  Serial.print(mclock.m);
  Serial.println();
#endif
#endif
#ifdef DISPLAY
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
#endif

}

#ifdef ACCEL
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
  if (diff < 200) {
    return 0;
  } else {
    return diff / 100;
  }
}
#endif

// ----------------------------------------------------------------------------
// Communication
// ----------------------------------------------------------------------------

void send_msg() {
#ifdef SERIAL
  Serial.println("Attempting to connect to server");
#endif
  uint8_t tries = 0;
  while (client.connect(server, port) == false) {
#ifdef SERIAL
    Serial.print(".");
#endif
    if (tries++ > 100) {
      Serial.println("\nThe server isn't responding");
      return;
    }
    delay(100);
  }

#ifdef SERIAL
  Serial.println("\nConnected to the server!");
#endif

  String body;

#ifdef ACCEL
  // Sleep data
  body = "sleep person1=";
  body = body + sum;
  body = body + ",person2=0";
#ifdef SERIAL
  Serial.println("Sending sleep data...");
  Serial.println(body);
#endif

  client.println("POST /write?db=smarthome HTTP/1.1");
  client.print("Host: "); client.println(host);
  client.println("Cache-Control: no-cache");
  client.println("Content-Type: application/json");
  client.println("Authorization: Basic cGhvZW5peDpoZXNsbw==");
  client.print("Content-Length: ");
  client.println(body.length());
  client.println();
  client.println(body);
  client.println();
#endif

  // Temperature and humidity
  body = "bedroom t=";
  body = body + temperature;
  body = body + ",h=";
  body = body + humidity;
  body = body + ",v=0";
#ifdef SERIAL
  Serial.println("Sending temperature and humidity: ");
  Serial.println(body);
#endif
  client.println("POST /write?db=smarthome HTTP/1.1");
  client.print("Host: "); client.println(host);
  client.println("Cache-Control: no-cache");
  client.println("Content-Type: application/json");
  client.println("Authorization: Basic cGhvZW5peDpoZXNsbw==");
  client.print("Content-Length: ");
  client.println(body.length());
  client.println();
  client.println(body);
  client.println();

#ifdef SERIAL
  Serial.println("Done!");
#endif
}
