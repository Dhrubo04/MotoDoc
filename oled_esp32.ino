#include <Wire.h>
#include <GyverOLED.h>
#include "DHT.h"

// ========== CONFIG ==========
#define DHTPIN 15        // ESP32 GPIO15 (D15)
#define DHTTYPE DHT22
// ============================

// OLED setup (for SH1106 128x64 I2C)
GyverOLED<SSH1106_128x64, OLED_BUFFER> oled;
DHT dht(DHTPIN, DHTTYPE);

void setup() {
  Serial.begin(9600);
  Wire.begin();
  Wire.setClock(800000L);
  oled.init();
  dht.begin();

  oled.clear();
  oled.setScale(2);         // Larger text
  oled.setCursor(0, 0);
  oled.print("DHT22 READY");
  oled.update();
  delay(1500);
}

void loop() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  // If sensor reading failed
  if (isnan(h) || isnan(t)) {
    oled.clear();
    oled.setScale(1);
    oled.setCursor(0, 0);
    oled.print("Sensor Error!");
    oled.update();
    delay(2000);
    return;
  }

  oled.clear();
  oled.setScale(2);
  oled.setCursor(0, 0);
  oled.print("Temp: ");
  oled.print(t, 1);
  oled.print("C");

  oled.setCursor(0, 3);
  oled.print("Hum:  ");
  oled.print(h, 1);
  oled.print("%");

  oled.update();
  delay(1000); // update every second
}
