#include <Arduino.h>

#define SENSOR_PIN 34             // ACS712 OUT → GPIO34 (ADC1)
const int mVperAmp = 66;          // 30A module: 66 mV per Amp
const float mainsVoltage = 240.0; // Adjust for your local AC voltage
const float calibrationFactor = 1.2; // Tune if readings differ

double Voltage = 0;
double VRMS = 0;
double AmpsRMS = 0;
int Watt = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("ACS712 30A Current Sensor (ESP32)");
  delay(1000);
}

void loop() {
  Voltage = getVPP();                         // Measure peak-to-peak voltage
  VRMS = (Voltage / 2.0) * 0.707;             // Convert to RMS voltage
  AmpsRMS = ((VRMS * 1000.0) / mVperAmp) - 0.3; // Convert to RMS current (A)
  if (AmpsRMS < 0) AmpsRMS = 0;               // Avoid negative current at no load

  Watt = (AmpsRMS * mainsVoltage) / calibrationFactor;

  // --- Serial Output ---
  Serial.print("Current: ");
  Serial.print(AmpsRMS, 3);
  Serial.print(" A  |  Power: ");
  Serial.print(Watt);
  Serial.println(" W");

  delay(1000);
}

// ================== Measure Peak-to-Peak Voltage ==================
float getVPP() {
  int readValue;
  int maxValue = 0;
  int minValue = 4096;  // ESP32 ADC resolution (12-bit)

  uint32_t start_time = millis();
  while ((millis() - start_time) < 1000) { // Sample for 1 second
    readValue = analogRead(SENSOR_PIN);
    if (readValue > maxValue) maxValue = readValue;
    if (readValue < minValue) minValue = readValue;
  }

  // Convert ADC difference to actual voltage
  float result = ((maxValue - minValue) * 3.3) / 4096.0;
  return result;
}
