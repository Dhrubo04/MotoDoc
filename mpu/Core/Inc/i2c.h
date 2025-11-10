#ifndef __I2C_H__
#define __I2C_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

// 👇 THIS LINE IS CRUCIAL
extern I2C_HandleTypeDef hi2c1;

// Function prototypes
void MX_I2C1_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* __I2C_H__ */
