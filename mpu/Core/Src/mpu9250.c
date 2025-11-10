/*
 * mpu9250.c
 *
 *  Created on: Nov 7, 2025
 *      Author: Arya
 */
#include "mpu9250.h"

void MPU9250_WriteReg(uint8_t reg, uint8_t data) {
    HAL_I2C_Mem_Write(&hi2c1, MPU9250_ADDR, reg, 1, &data, 1, HAL_MAX_DELAY);
}

uint8_t MPU9250_ReadReg(uint8_t reg) {
    uint8_t value;
    HAL_I2C_Mem_Read(&hi2c1, MPU9250_ADDR, reg, 1, &value, 1, HAL_MAX_DELAY);
    return value;
}

void MPU9250_Init(void) {
    uint8_t check = MPU9250_ReadReg(WHO_AM_I_MPU9250);

    if (check == 0x71)  // ID for MPU9250
    {
        // Wake up sensor
        MPU9250_WriteReg(PWR_MGMT_1, 0x00);
        HAL_Delay(100);

        // ✅ Blink LED once to confirm detection
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);   // LED ON
        HAL_Delay(300);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET); // LED OFF
    }
    else
    {
        // ❌ No response — blink continuously
        while (1)
        {
            HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
            HAL_Delay(200);
        }
    }
}


void MPU9250_Read_Accel(float *ax, float *ay, float *az) {
    uint8_t data[6];
    HAL_I2C_Mem_Read(&hi2c1, MPU9250_ADDR, ACCEL_XOUT_H, 1, data, 6, HAL_MAX_DELAY);

    int16_t raw_ax = (data[0] << 8) | data[1];
    int16_t raw_ay = (data[2] << 8) | data[3];
    int16_t raw_az = (data[4] << 8) | data[5];

    *ax = raw_ax / 16384.0f;
    *ay = raw_ay / 16384.0f;
    *az = raw_az / 16384.0f;
}

void MPU9250_Read_Gyro(float *gx, float *gy, float *gz) {
    uint8_t data[6];
    HAL_I2C_Mem_Read(&hi2c1, MPU9250_ADDR, GYRO_XOUT_H, 1, data, 6, HAL_MAX_DELAY);

    int16_t raw_gx = (data[0] << 8) | data[1];
    int16_t raw_gy = (data[2] << 8) | data[3];
    int16_t raw_gz = (data[4] << 8) | data[5];

    *gx = raw_gx / 131.0f;
    *gy = raw_gy / 131.0f;
    *gz = raw_gz / 131.0f;
}

void MPU9250_Calibrate(void) {
    float gx, gy, gz;
    float gx_offset = 0, gy_offset = 0, gz_offset = 0;

    for (int i = 0; i < 100; i++) {
        MPU9250_Read_Gyro(&gx, &gy, &gz);
        gx_offset += gx;
        gy_offset += gy;
        gz_offset += gz;
        HAL_Delay(10);
    }

    gx_offset /= 100;
    gy_offset /= 100;
    gz_offset /= 100;

    // Store these offsets globally if needed
}


