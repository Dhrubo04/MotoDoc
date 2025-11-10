#ifndef MPU9250_H
#define MPU9250_H

#include "main.h"
#include "i2c.h"
#include <math.h>

#define MPU9250_ADDR 0x68 << 1

// Register definitions
#define WHO_AM_I_MPU9250 0x75
#define PWR_MGMT_1 0x6B
#define ACCEL_XOUT_H 0x3B
#define GYRO_XOUT_H  0x43

// Function prototypes
void MPU9250_Init(void);
void MPU9250_Read_Accel(float *ax, float *ay, float *az);
void MPU9250_Read_Gyro(float *gx, float *gy, float *gz);
void MPU9250_Calibrate(void);

#endif
