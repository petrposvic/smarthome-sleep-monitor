#include <Wire.h>
#include <SoftwareSerial.h>

const int MPU_addr = 0x68;
const int interval = 5 * 60 * 10;

SoftwareSerial wifi(10, 11); // RX, TX

int16_t ax, ay, az;
int16_t gx, gy, gz;
int16_t tmp;
int16_t lx = 0, ly = 0, lz = 0; // Last values
int dx, dy, dz;                 // Differences

long sum = 0;
int count = interval - 5 * 10;

void setup() {

  // MPU6050
  Wire.begin();
  Wire.beginTransmission(MPU_addr);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission(true);

  // Debug serial
  Serial.begin(38400);
  Serial.println("ok");

  // Wifi serial
  wifi.begin(9600);
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
    if (sum > 2500) sum = 2500;

    Serial.println(sum);
    wifi.print(sum);
    wifi.print('.');

    sum = 0;
    count = 0;
  }

  delay(100);
}

void read_mpu() {
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

