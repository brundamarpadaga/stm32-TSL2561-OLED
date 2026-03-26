#ifndef SEGGER_SYSVIEW_CONF_H
#define SEGGER_SYSVIEW_CONF_H

#include "stm32f7xx.h"   /* gives you DWT, SCB, CoreDebug */

/*********************************************************************
* Core clock and RTT settings
*********************************************************************/
#define SEGGER_SYSVIEW_RTT_BUFFER_SIZE      4096
#define SEGGER_SYSVIEW_RTT_CHANNEL          1
#define SEGGER_SYSVIEW_MAX_NOF_TASKS        8
#define SEGGER_SYSVIEW_CPU_FREQ             96000000
#define SEGGER_SYSVIEW_SYSDESC0             "I#15=SysTick"

/*********************************************************************
* SEGGER_SYSVIEW_GET_TIMESTAMP
* Uses the DWT cycle counter — highest resolution timestamp on Cortex-M7.
* DWT->CYCCNT must be enabled before SystemView starts (done below).
*********************************************************************/
#define SEGGER_SYSVIEW_GET_TIMESTAMP()      (DWT->CYCCNT)

/*********************************************************************
* SEGGER_SYSVIEW_GET_INTERRUPT_ID
* Returns the currently active interrupt number from IPSR register.
* Bits [8:0] of IPSR hold the exception number.
*********************************************************************/
#define SEGGER_SYSVIEW_GET_INTERRUPT_ID()   ((SCB->ICSR & 0x1FFUL))

#endif /* SEGGER_SYSVIEW_CONF_H */
