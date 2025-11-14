#include <Wire.h>
#include <Adafruit_Sensor.h>
// #include <Adafruit_ADS1X15.h>
#include <MPU9250_asukiaaa.h>
#include <DHT.h>
#include <GyverOLED.h>
#include "driver/i2s.h"

// ================== CONFIG ==================
#define I2C_SDA 21
#define I2C_SCL 22

#define DHTPIN 15
#define DHTTYPE DHT22

// #define ADS_LM35_CH 0
// #define ADS_ACS_CH  1

#define I2S_WS   25
#define I2S_SD   33
#define I2S_SCK  26
#define I2S_PORT I2S_NUM_0
#define SAMPLE_BUFFER 256
// ============================================

// --- Sensors and OLED ---
// Adafruit_ADS1115 ads;
MPU9250_asukiaaa mpu;
DHT dht(DHTPIN, DHTTYPE);
GyverOLED<SSH1106_128x64, OLED_BUFFER> oled;

// --- Flags ---
// bool adsOK = false, mpuOK = false;

// --- Helper Functions ---
// float adsToVoltage(int16_t raw) {
//   return (raw * 0.1875f) / 1000.0f; // ADS1115 default gain
// }

static const i2s_config_t i2s_config = {
  .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),
  .sample_rate = 16000,                             // 16 kHz
  .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,     // 32-bit per sample
  .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,      // Left channel only
  .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
  .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
  .dma_buf_count = 4,
  .dma_buf_len = SAMPLE_BUFFER,
  .use_apll = false
};

// I2S pin configuration
static const i2s_pin_config_t pin_config = {
  .bck_io_num = I2S_SCK,
  .ws_io_num = I2S_WS,
  .data_out_num = I2S_PIN_NO_CHANGE,
  .data_in_num = I2S_SD
};

// Function to compute RMS of mic signal
float readSoundRMS() {
  int32_t buffer[SAMPLE_BUFFER];
  size_t bytesRead = 0;
  esp_err_t result = i2s_read(I2S_PORT, (void*)buffer, sizeof(buffer), &bytesRead, 100);
  if (result != ESP_OK || bytesRead == 0) return 0.0f;

  int samples = bytesRead / sizeof(int32_t);
  double sumsq = 0.0;

  for (int i = 0; i < samples; ++i) {
    int32_t s = buffer[i] >> 8;                      // shift down to 24-bit
    double val = (double)s / (double)(1 << 23);      // normalize to -1..1
    sumsq += val * val;
  }

  double mean = sumsq / samples;
  return sqrt(mean);
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

  // // --- ADS1115 ---
  // adsOK = ads.begin();
  // if (!adsOK) Serial.println("⚠️ ADS1115 not detected!");

  // --- MPU9250 ---
  // mpuOK = mpu.setup(0x68);
  // if (!mpuOK) Serial.println("⚠️ MPU9250 not detected!");
   mpu.setWire(&Wire);
  mpu.beginAccel();
  mpu.beginGyro();
  mpu.beginMag();

  // --- I2S (INMP441 mic) ---
  
  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_PORT, &pin_config);
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
  float currentA = 4.2;

  // --- ADS1115 (LM35 + ACS712) ---
  // float tempLM35 = NAN, currentA = NAN;
  // if (adsOK) {
  //   int16_t rawLM = ads.readADC_SingleEnded(ADS_LM35_CH);
  //   int16_t rawAC = ads.readADC_SingleEnded(ADS_ACS_CH);
  //   float vLM = adsToVoltage(rawLM);
  //   float vAC = adsToVoltage(rawAC);
  //   tempLM35 = vLM * 100.0f;
  //   currentA = (vAC - 2.5f) / 0.185f;  // ACS712-5A formula
  // }

  // --- MPU9250 ---
 mpu.accelUpdate();
  mpu.gyroUpdate();
  mpu.magUpdate();

  // --- Read Acceleration (m/s²) ---
  float ax = mpu.accelX();
  float ay = mpu.accelY();
  float az = mpu.accelZ() + 1.0 ;

  // --- Read Gyroscope (°/s) ---
  float gx = mpu.gyroX();
  float gy = mpu.gyroY();
  float gz = mpu.gyroZ();

  // --- Read Magnetometer (µT) ---
  float mx = mpu.magX();
  float my = mpu.magY();
  float mz = mpu.magZ();

  float vibrationIntensity = sqrt(ax * ax + ay * ay + az * az );

  float magnetometer = sqrt(mx *mx + my * my + mz * mz );

  // --- INMP441 Mic RMS ---
  float rms = readSoundRMS();
  float dB = 20.0 * log10(rms);

  // --- Serial Output ---
  Serial.printf("T(DHT)=%.1f°C, H=%.1f%%, I=%.2fA, Mag=%.3f,Vib=%.3f, Sound=%.3f\n",
                tempDHT, hum, currentA, magnetometer,vibrationIntensity, rms);

  // --- OLED Display Update ---
  oled.clear();
  oled.setScale(1);
  oled.setCursor(0, 0);
  oled.print("Temp: ");
  oled.print(tempDHT, 1);
  oled.print("C  ");

  //

  oled.setCursor(0, 1);
  oled.print("Hum: ");
  oled.print(hum, 0);
  oled.print("%");

  // oled.setCursor(0, 2);
  // oled.print("LM35: ");
  // oled.print(tempLM35, 1);
  // oled.print("C");

  oled.setCursor(0, 2);
  oled.print("Curr: ");
  oled.print(currentA, 2);
  oled.print("A");

  oled.setCursor(0, 3);
  oled.print("Magn: ");
  oled.print(magnetometer, 5);

  oled.setCursor(0, 4);
  oled.print("Vibration: ");
  oled.print(vibrationIntensity, 5);

  oled.setCursor(0, 5);
  oled.print("Sound:");
  oled.print(rms, 5);

  oled.update();
  delay(1000);
}
