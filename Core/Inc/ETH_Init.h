#ifndef BSP_ETH_INIT_H_
#define BSP_ETH_INIT_H_

#include "stm32f7xx_hal.h"

/* Global handles - accessible by any file that includes this header */
extern ETH_HandleTypeDef heth;
extern ETH_TxPacketConfig TxConfig;

void MX_ETH_Init(void);

#endif /* BSP_ETH_INIT_H_ */
