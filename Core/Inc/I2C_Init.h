#ifndef BSP_I2C_INIT_H_
#define BSP_I2C_INIT_H_

#include "stm32f7xx_hal.h"

/* Global handle - accessible by any file that includes this header */
extern I2C_HandleTypeDef hi2c1;

/* Call once before RTOS starts */
void MX_I2C1_Init(void);

#endif /* BSP_I2C_INIT_H_ */
