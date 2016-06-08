#include <I2Cdev.h>
#include <Wire.h>
#include <MPU6050.h>
#include <SoftwareSerial.h>

MPU6050 accelgyro;
SoftwareSerial wifi(10, 11); // RX, TX

int16_t ax, ay, az;
int16_t gx, gy, gz;
int16_t lx = 0, ly = 0, lz = 0; // Last values
int dx, dy, dz;                 // Differences

long sum = 0;
int count = 0;

void setup() {
  Serial.begin(38400);
  wifi.begin(9600);

  Wire.begin();
  accelgyro.initialize();
  if (!accelgyro.testConnection()) {
    Serial.println("accel gyro failed");
    while (1);
  }
}

void loop() {
  accelgyro.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
  /*Serial.print(gx);
  Serial.print("x");
  Serial.print(ay);
  Serial.print("x");
  Serial.println(az);*/

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
  if (count == 60 * 10) {

    // Avoid big numbers
    if (sum > 5000) sum = 5000;

    Serial.println(sum);
    wifi.print(sum);
    wifi.print('.');

    sum = 0;
    count = 0;
  }

  delay(100);
}

int accuracy(int diff) {
  if (diff < 100) {
    return 0;
  } else {
    return diff / 100;
  }
}

