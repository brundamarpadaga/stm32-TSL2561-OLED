#ifndef TSL2561_H
#define TSL2561_H

#include "stm32f7xx_hal.h"  // Change to match your STM32 series
#include <stdint.h>

/* -----------------------------------------------------------------------
 * I2C Slave Address (7-bit)
 * ADDR SEL pin:  GND   -> 0x29
 *                Float -> 0x39  (most common breakout default)
 *                VDD   -> 0x49
 * HAL expects the address left-shifted by 1 (8-bit format)
 * ----------------------------------------------------------------------- */
#define TSL2561_ADDR_GND    (0x29 << 1)
#define TSL2561_ADDR_FLOAT  (0x39 << 1)
#define TSL2561_ADDR_VDD    (0x49 << 1)

/* Default address — change if your ADDR SEL pin is wired differently */
#define TSL2561_ADDR        TSL2561_ADDR_FLOAT

/* -----------------------------------------------------------------------
 * Command Register bits (datasheet Table 3)
 * Bit 7 (CMD) must always be 1 when writing the command byte.
 * ----------------------------------------------------------------------- */
#define TSL2561_CMD         0x80   /* CMD bit — must be set */
#define TSL2561_CMD_WORD    0x20   /* WORD bit — read/write two bytes */
#define TSL2561_CMD_CLEAR   0x40   /* CLEAR bit — clears pending interrupt */

/* -----------------------------------------------------------------------
 * Register addresses (datasheet Table 2)
 * These are OR'd with TSL2561_CMD when used as the memory address in HAL
 * ----------------------------------------------------------------------- */
#define TSL2561_REG_CONTROL     0x00
#define TSL2561_REG_TIMING      0x01
#define TSL2561_REG_INTERRUPT   0x06
#define TSL2561_REG_ID          0x0A
#define TSL2561_REG_DATA0LOW    0x0C   /* Channel 0 low byte  */
#define TSL2561_REG_DATA1LOW    0x0E   /* Channel 1 low byte  */

/* -----------------------------------------------------------------------
 * Control register values (datasheet Table 4)
 * ----------------------------------------------------------------------- */
#define TSL2561_POWER_UP        0x03
#define TSL2561_POWER_DOWN      0x00

/* -----------------------------------------------------------------------
 * Timing register values (datasheet Table 5 & 6)
 * Gain:  0 = 1x (low), 1 = 16x (high)
 * INTEG: 00 = 13.7ms, 01 = 101ms, 10 = 402ms (default)
 * ----------------------------------------------------------------------- */
#define TSL2561_GAIN_1X         0x00
#define TSL2561_GAIN_16X        0x10

#define TSL2561_INTEG_13MS      0x00   /* scale = 0.034 */
#define TSL2561_INTEG_101MS     0x01   /* scale = 0.252 */
#define TSL2561_INTEG_402MS     0x02   /* scale = 1.0   (default) */

/* -----------------------------------------------------------------------
 * Package type — affects lux formula coefficients (datasheet p.22)
 * TSL2561T / FN / CL  -> TSL2561_PKG_T
 * TSL2561CS           -> TSL2561_PKG_CS
 * ----------------------------------------------------------------------- */
#define TSL2561_PKG_T           0
#define TSL2561_PKG_CS          1

/* -----------------------------------------------------------------------
 * HAL timeout for I2C operations (ms)
 * Increase if you see HAL_TIMEOUT errors
 * ----------------------------------------------------------------------- */
#define TSL2561_I2C_TIMEOUT     100

/* -----------------------------------------------------------------------
 * Return status
 * ----------------------------------------------------------------------- */
typedef enum {
    TSL2561_OK    = 0,
    TSL2561_ERROR = 1
} TSL2561_Status;

/* -----------------------------------------------------------------------
 * Device handle — pass this around instead of raw globals
 * ----------------------------------------------------------------------- */
typedef struct {
    I2C_HandleTypeDef *hi2c;
    uint8_t  addr;       /* 8-bit HAL address (7-bit << 1)  */
    uint8_t  gain;       /* TSL2561_GAIN_1X or TSL2561_GAIN_16X */
    uint8_t  integ;      /* TSL2561_INTEG_13MS/101MS/402MS       */
    uint8_t  pkg_type;   /* TSL2561_PKG_T or TSL2561_PKG_CS      */
} TSL2561_Handle;

/* -----------------------------------------------------------------------
 * Public API
 * ----------------------------------------------------------------------- */

/**
 * @brief  Initialise the sensor (power up + apply gain/integration time).
 * @param  dev   Pointer to a TSL2561_Handle you have already filled in.
 * @retval TSL2561_OK on success, TSL2561_ERROR otherwise.
 */
TSL2561_Status TSL2561_Init(TSL2561_Handle *dev);

/**
 * @brief  Power down the sensor to save ~0.24 mA.
 */
TSL2561_Status TSL2561_PowerDown(TSL2561_Handle *dev);

/**
 * @brief  Read raw ADC counts from both channels.
 * @param  ch0  Output: channel 0 (visible + IR)
 * @param  ch1  Output: channel 1 (IR only)
 */
TSL2561_Status TSL2561_ReadChannels(TSL2561_Handle *dev,
                                    uint16_t *ch0,
                                    uint16_t *ch1);

/**
 * @brief  Calculate lux from raw channel counts.
 *         Integer-only arithmetic — safe for Cortex-M without FPU licence.
 * @param  dev  Device handle (used for gain, integ, pkg_type).
 * @param  ch0  Raw channel 0 count.
 * @param  ch1  Raw channel 1 count.
 * @return Lux value (unsigned 32-bit).
 */
uint32_t TSL2561_CalculateLux(TSL2561_Handle *dev,
                               uint16_t ch0,
                               uint16_t ch1);

/**
 * @brief  One-shot: read channels and return lux directly.
 *         Blocks for one integration period (osDelay inside — FreeRTOS safe).
 * @param  lux  Output lux value.
 */
TSL2561_Status TSL2561_ReadLux(TSL2561_Handle *dev, uint32_t *lux);

/**
 * @brief  Read the ID register to verify communication.
 *         Upper nibble: 0x0 = TSL2560, 0x1 = TSL2561.
 * @param  id   Output byte from ID register.
 */
TSL2561_Status TSL2561_ReadID(TSL2561_Handle *dev, uint8_t *id);

#endif /* TSL2561_H */
