#include <Wire.h>
#include <MPU9250_asukiaaa.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// I2C pins for ESP32
#define SDA_PIN 21
#define SCL_PIN 22

// OLED configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C

// Objects
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
MPU9250_asukiaaa mpu;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Wire.begin(SDA_PIN, SCL_PIN);

  // OLED setup
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("❌ OLED not found!");
  } else {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("System Starting...");
    display.display();
  }

  // MPU9250 setup
  mpu.setWire(&Wire);
  mpu.beginAccel();
  mpu.beginGyro();
  mpu.beginMag();

  Serial.println("✅ MPU9250_asukiaaa initialized!");
  delay(1000);
}

void loop() {
  // Update all sensors
  mpu.accelUpdate();
  mpu.gyroUpdate();
  mpu.magUpdate();

  // --- Read Acceleration (m/s²) ---
  float ax = mpu.accelX();
  float ay = mpu.accelY();
  float az = mpu.accelZ();

  // --- Read Gyroscope (°/s) ---
  float gx = mpu.gyroX();
  float gy = mpu.gyroY();
  float gz = mpu.gyroZ();

  // --- Read Magnetometer (µT) ---
  float mx = mpu.magX();
  float my = mpu.magY();
  float mz = mpu.magZ();

  // --- Serial Output ---
  Serial.println("---- MPU9250 9-DOF Data ----");
  Serial.printf("Accel [m/s²]: X=%.2f  Y=%.2f  Z=%.2f\n", ax, ay, az);
  Serial.printf("Gyro  [°/s]:  X=%.2f  Y=%.2f  Z=%.2f\n", gx, gy, gz);
  Serial.printf("Mag   [µT]:   X=%.2f  Y=%.2f  Z=%.2f\n", mx, my, mz);
  Serial.println("-----------------------------\n");

  // --- OLED Output ---
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("MPU9250 9-Axis Data");
  display.printf("A[X,Y,Z]: %.1f %.1f %.1f\n", ax, ay, az);
  display.printf("G[X,Y,Z]: %.1f %.1f %.1f\n", gx, gy, gz);
  display.printf("M[X,Y,Z]: %.1f %.1f %.1f\n", mx, my, mz);
  display.display();

  delay(500);
}
