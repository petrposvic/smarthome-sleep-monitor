#include <Wire.h>
#include <SoftwareSerial.h>
#include "DHT.h"         // https://github.com/adafruit/DHT-sensor-library

#define DHTPIN 2
#define DHTTYPE DHT22

const int MPU_addr = 0x68;
const int interval = 5 * 60 * 10;

SoftwareSerial wifi(10, 11); // RX, TX
DHT dht(DHTPIN, DHTTYPE);

int16_t gx, gy, gz;
int16_t lx = 0, ly = 0, lz = 0; // Last values

long sum = 0;
int count = interval - 5 * 10;

void setup() {

  // Debug serial
  Serial.begin(9600);
  Serial.println("debug serial");
  delay(100);

  // Wifi serial
  wifi.begin(9600);
  Serial.println("wifi serial");
  delay(100);

  // MPU6050
  Wire.begin();
  Wire.beginTransmission(MPU_addr);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission(true);
  Serial.println("mpu6050");
  delay(100);

  // DHT22
  dht.begin();
  Serial.println("dht22");
}

void loop() {
  read_mpu();
  /*Serial.print(gx);
  Serial.print("x");
  Serial.print(gy);
  Serial.print("x");
  Serial.println(gz);*/

  int dx = accuracy(abs(gx - lx));
  int dy = accuracy(abs(gy - ly));
  int dz = accuracy(abs(gz - lz));
  /*Serial.print(dx);
  Serial.print(",");
  Serial.print(dy);
  Serial.print(",");
  Serial.println(dz);*/
  lx = gx;
  ly = gy;
  lz = gz;

  sum += (dx + dy + dz);
  count++;
  if (count >= interval) {

    // Avoid big numbers
    if (sum > 500) sum = 500;

    Serial.println(sum);

    send_at_cmd("AT+CIPSTART=\"TCP\",\"192.168.1.203\",3000\r\n");
    send_sleep();
    send_dht();
    send_at_cmd("AT+CIPCLOSE\r\n");

    sum = 0;
    count = 0;
  }

  // Print all from wifi module
  char ch;
  while (wifi.available()) {
    delay(10);
    ch = wifi.read();
    Serial.write(ch);
  }

  // Debug AT commands
  while (Serial.available()) {
    wifi.write(Serial.read());
  }

  delay(100);
}

void send_at_cmd(String cmd) {
  wifi.print(cmd);
  delay(30);
  while (wifi.available()){
    String res = wifi.readStringUntil('\n');
    Serial.println(res);
  }
}

void send_post(String path, String payload) {
  String msg = "";
  msg = msg + "POST " + path + " HTTP/1.1\r\n";
  msg = msg + "Host:192.168.1.203\r\n";
  msg = msg + "Cache-Control:no-cache\r\n";
  msg = msg + "Content-Type:application/json\r\n";
  msg = msg + "Content-Length:" + payload.length() + "\r\n";
  msg = msg + "\r\n";
  msg = msg + payload + "\r\n";
  msg = msg + "\r\n";

  /*Serial.print("msg = ");
  Serial.println(msg);
  Serial.println(payload);
  Serial.print("payload size = ");
  Serial.println(payload.length());
  Serial.print("msg size = ");
  Serial.println(msg.length());*/

  String cmd = "";
  cmd = cmd + "AT+CIPSEND=" + msg.length() + "\r\n";
  send_at_cmd(cmd);
  send_at_cmd(msg);
}

void send_sleep() {
  if (sum < 0) {
    Serial.println("ignore negative values!");
    return;
  }

  String payload = "{\"device\":\"petr\",\"value\":\"";
  payload = payload + sum + "\"}";
  send_post("/api/sleeps", payload);
}

void send_dht() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  if (isnan(h) || isnan(t)) {
    Serial.println("failed to read from DHT sensor!");
    return;
  }

  float hic = dht.computeHeatIndex(t, h, false);
  Serial.println(t);
  Serial.println(h);

  String payload = "{\"dev\":\"bedroom\",\"t\":\"";
  payload = payload + t + "\",\"h\":\"";
  payload = payload + h + "\"}";
  send_post("/api/measurements", payload);
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

int accuracy(int diff) {
  if (diff < 100) {
    return 0;
  } else {
    return diff / 100;
  }
}

