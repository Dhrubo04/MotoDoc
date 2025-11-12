#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADS1X15.h>
#include <MPU9250.h>
#include <DHT.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "driver/i2s.h"

#define I2C_SDA 21
#define I2C_SCL 22
#define DHTPIN 15
#define DHTTYPE DHT22
#define ADS_LM35_CH 0
#define ADS_ACS_CH 1

#define I2S_WS   25
#define I2S_SD   22
#define I2S_SCK  26
#define I2S_PORT I2S_NUM_0

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
Adafruit_ADS1115 ads;
MPU9250 mpu;
DHT dht(DHTPIN, DHTTYPE);

bool adsOK = false, mpuOK = false;

float adsToVoltage(int16_t raw) { return (raw * 0.1875f) / 1000.0f; }

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

void setup() {
  Serial.begin(115200);
  delay(300);
  Wire.begin(I2C_SDA, I2C_SCL);

  // DHT
  dht.begin();

  // ADS1115
  adsOK = ads.begin();
  if (!adsOK) Serial.println("⚠️ ADS1115 not detected!");

  // MPU9250
  mpuOK = mpu.setup(0x68);
  if (!mpuOK) Serial.println("⚠️ MPU9250 not detected!");

  // OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("⚠️ OLED not found!");
  } else {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.print("System starting...");
    display.display();
  }

  // I2S
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
}

void loop() {
  float tempDHT = dht.readTemperature();
  float hum = dht.readHumidity();

  float tempLM35 = NAN, currentA = NAN;
  if (adsOK) {
    int16_t rawLM = ads.readADC_SingleEnded(ADS_LM35_CH);
    int16_t rawAC = ads.readADC_SingleEnded(ADS_ACS_CH);
    float vLM = adsToVoltage(rawLM);
    float vAC = adsToVoltage(rawAC);
    tempLM35 = vLM * 100.0f;
    currentA = (vAC - 2.5f) / 0.185f;
  }

  float ax = 0, ay = 0, az = 0;
  if (mpuOK) {
    mpu.update();
    ax = mpu.getAccX();
    ay = mpu.getAccY();
    az = mpu.getAccZ();
  }

  float soundRMS = readSoundRMS();

  // --- Always print something ---
  Serial.printf("T(DHT)=%.1f°C, H=%.1f%%, T(LM35)=%.1f°C, I=%.2fA, Acc=[%.2f,%.2f,%.2f], Sound=%.3f\n",
                tempDHT, hum, tempLM35, currentA, ax, ay, az, soundRMS);

  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.printf("DHT22: %.1fC  %.0f%%\n", tempDHT, hum);
  display.printf("LM35:  %.1fC\n", tempLM35);
  display.printf("Curr:  %.2fA\n", currentA);
  display.printf("Sound: %.3f\n", soundRMS);
  display.display();

  delay(1000);
}
