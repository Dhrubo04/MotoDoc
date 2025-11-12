/*
  ESP32 INMP441 Microphone Test
  -----------------------------
  Measures sound RMS using I2S and prints to Serial Monitor.
  Wire as follows:

  INMP441  →  ESP32
  -------------------
  VCC       →  3.3V
  GND       →  GND
  L/R       →  GND   (Left channel)
  SD (DOUT) →  GPIO22
  SCK (BCLK)→  GPIO26
  WS (LRCLK)→  GPIO25
*/

#include "driver/i2s.h"

#define I2S_WS   25   // LRCLK
#define I2S_SD   22   // DOUT
#define I2S_SCK  26   // BCLK
#define I2S_PORT I2S_NUM_0

#define SAMPLE_BUFFER 256

// I2S configuration
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

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n🎤 INMP441 Microphone Test Starting...");

  // Initialize I2S
  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_PORT, &pin_config);
  i2s_start(I2S_PORT);

  Serial.println("✅ I2S initialized successfully.");
  delay(500);
}

void loop() {
  float rms = readSoundRMS();
  Serial.print("Sound RMS: ");
  Serial.println(rms, 5);
  delay(500);
}
