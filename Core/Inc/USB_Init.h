#ifndef BSP_USB_INIT_H_
#define BSP_USB_INIT_H_

#include "stm32f7xx_hal.h"

extern PCD_HandleTypeDef hpcd_USB_OTG_FS;

void MX_USB_OTG_FS_PCD_Init(void);

#endif /* BSP_USB_INIT_H_ */
