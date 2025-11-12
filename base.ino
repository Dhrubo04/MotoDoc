#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADS1X15.h>
#include <MPU9250.h>
#include <DHT.h>
#include <GyverOLED.h>
#include "driver/i2s.h"

// ================== CONFIG ==================
#define I2C_SDA 21
#define I2C_SCL 22

#define DHTPIN 15
#define DHTTYPE DHT22

#define ADS_LM35_CH 0
#define ADS_ACS_CH  1

#define I2S_WS   25
#define I2S_SD   22
#define I2S_SCK  26
#define I2S_PORT I2S_NUM_0
// ============================================

// --- Sensors and OLED ---
Adafruit_ADS1115 ads;
MPU9250 mpu;
DHT dht(DHTPIN, DHTTYPE);
GyverOLED<SSH1106_128x64, OLED_BUFFER> oled;

// --- Flags ---
bool adsOK = false, mpuOK = false;

// --- Helper Functions ---
float adsToVoltage(int16_t raw) {
  return (raw * 0.1875f) / 1000.0f; // ADS1115 default gain
}

float readSoundRMS() {
  const int READ_SAMPLES = 128;
  int32_t buffer[READ_SAMPLES];
  size_t bytesRead = 0;
  if (i2s_read(I2S_PORT, (void*)buffer, sizeof(buffer), &bytesRead, 50) != ESP_OK) return 0.0f;
  if (bytesRead == 0) return 0.0f;

  int samples = bytesRead / sizeof(int32_t);
  double sumsq = 0;
  for (int i = 0; i < samples; i++) {
    int32_t s = buffer[i] >> 8;
    double val = (double)s / (1 << 23);
    sumsq += val * val;
  }
  return sqrt(sumsq / samples);
}

// ================== SETUP ==================
void setup() {
  Serial.begin(115200);
  delay(300);

  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(800000L);

  // --- Initialize OLED ---
  oled.init();
  oled.clear();
  oled.setScale(2);
  oled.setCursor(0, 0);
  oled.print("System Boot");
  oled.update();

  // --- DHT22 ---
  dht.begin();

  // --- ADS1115 ---
  adsOK = ads.begin();
  if (!adsOK) Serial.println("⚠️ ADS1115 not detected!");

  // --- MPU9250 ---
  mpuOK = mpu.setup(0x68);
  if (!mpuOK) Serial.println("⚠️ MPU9250 not detected!");

  // --- I2S (INMP441 mic) ---
  const i2s_config_t cfg = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 16000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = 128,
    .use_apll = false
  };
  const i2s_pin_config_t pins = {I2S_SCK, I2S_WS, I2S_PIN_NO_CHANGE, I2S_SD};
  i2s_driver_install(I2S_PORT, &cfg, 0, NULL);
  i2s_set_pin(I2S_PORT, &pins);
  i2s_start(I2S_PORT);

  Serial.println("✅ Setup complete");

  oled.clear();
  oled.setCursor(0, 2);
  oled.print("READY");
  oled.update();
  delay(1000);
}

// ================== LOOP ==================
void loop() {
  // --- DHT22 ---
  float tempDHT = dht.readTemperature();
  float hum = dht.readHumidity();

  // --- ADS1115 (LM35 + ACS712) ---
  float tempLM35 = NAN, currentA = NAN;
  if (adsOK) {
    int16_t rawLM = ads.readADC_SingleEnded(ADS_LM35_CH);
    int16_t rawAC = ads.readADC_SingleEnded(ADS_ACS_CH);
    float vLM = adsToVoltage(rawLM);
    float vAC = adsToVoltage(rawAC);
    tempLM35 = vLM * 100.0f;
    currentA = (vAC - 2.5f) / 0.185f;  // ACS712-5A formula
  }

  // --- MPU9250 ---
  float ax = 0, ay = 0, az = 0;
  if (mpuOK) {
    mpu.update();
    ax = mpu.getAccX();
    ay = mpu.getAccY();
    az = mpu.getAccZ();
  }

  // --- INMP441 Mic RMS ---
  float soundRMS = readSoundRMS();

  // --- Serial Output ---
  Serial.printf("T(DHT)=%.1f°C, H=%.1f%%, T(LM35)=%.1f°C, I=%.2fA, Acc=[%.2f,%.2f,%.2f], Sound=%.3f\n",
                tempDHT, hum, tempLM35, currentA, ax, ay, az, soundRMS);

  // --- OLED Display Update ---
  oled.clear();
  oled.setScale(1);
  oled.setCursor(0, 0);
  oled.print("DHT: ");
  oled.print(tempDHT, 1);
  oled.print("C  ");
  oled.print(hum, 0);
  oled.print("%");

  oled.setCursor(0, 2);
  oled.print("LM35: ");
  oled.print(tempLM35, 1);
  oled.print("C");

  oled.setCursor(0, 3);
  oled.print("Curr: ");
  oled.print(currentA, 2);
  oled.print("A");

  oled.setCursor(0, 4);
  oled.print("AccZ: ");
  oled.print(az, 2);

  oled.setCursor(0, 5);
  oled.print("Sound:");
  oled.print(soundRMS, 3);

  oled.update();
  delay(1000);
}
