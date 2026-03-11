/**
 * @file    tsl2561.c
 * @brief   TSL2561 ambient light sensor driver for STM32 HAL + FreeRTOS
 *
 * Datasheet: TAOS059N (March 2009)
 *
 * Communication model
 * -------------------
 * HAL_I2C_Mem_Write / HAL_I2C_Mem_Read are used throughout.
 * The "memory address" parameter is the TSL2561 COMMAND byte
 * (CMD bit always set, WORD bit set for 16-bit reads).
 * HAL automatically generates the repeated-start required by the
 * I2C combined-format read protocol shown in Figure 17 of the datasheet.
 */

#include "tsl2561.h"
#include "cmsis_os.h"   /* osDelay */

/* -----------------------------------------------------------------------
 * Integer lux calculation constants (datasheet pp. 24-28)
 * Naming convention matches the original TAOS source code exactly.
 * ----------------------------------------------------------------------- */
#define LUX_SCALE       14
#define RATIO_SCALE     9
#define CH_SCALE        10
#define CHSCALE_TINT0   0x7517u   /* 322/11  * 2^CH_SCALE */
#define CHSCALE_TINT1   0x0FE7u   /* 322/81  * 2^CH_SCALE */

/* T, FN, CL package coefficients */
#define K1T  0x0040u
#define B1T  0x01F2u
#define M1T  0x01BEu
#define K2T  0x0080u
#define B2T  0x0214u
#define M2T  0x02D1u
#define K3T  0x00C0u
#define B3T  0x023Fu
#define M3T  0x037Bu
#define K4T  0x0100u
#define B4T  0x0270u
#define M4T  0x03FEu
#define K5T  0x0138u
#define B5T  0x016Fu
#define M5T  0x01FCu
#define K6T  0x019Au
#define B6T  0x00D2u
#define M6T  0x00FBu
#define K7T  0x029Au
#define B7T  0x0018u
#define M7T  0x0012u
#define K8T  0x029Au
#define B8T  0x0000u
#define M8T  0x0000u

/* CS package coefficients */
#define K1C  0x0043u
#define B1C  0x0204u
#define M1C  0x01ADu
#define K2C  0x0085u
#define B2C  0x0228u
#define M2C  0x02C1u
#define K3C  0x00C8u
#define B3C  0x0253u
#define M3C  0x0363u
#define K4C  0x010Au
#define B4C  0x0282u
#define M4C  0x03DFu
#define K5C  0x014Du
#define B5C  0x0177u
#define M5C  0x01DDu
#define K6C  0x019Au
#define B6C  0x0101u
#define M6C  0x0127u
#define K7C  0x029Au
#define B7C  0x0037u
#define M7C  0x002Bu
#define K8C  0x029Au
#define B8C  0x0000u
#define M8C  0x0000u

/* -----------------------------------------------------------------------
 * Integration-time wait in ms — add 10 % margin over nominal values.
 * (datasheet Table 6: 13.7 ms, 101 ms, 402 ms)
 * ----------------------------------------------------------------------- */
static const uint32_t s_integ_delay_ms[] = { 15u, 112u, 450u };

/* -----------------------------------------------------------------------
 * Private helper: write a single byte to a register.
 * The command byte = TSL2561_CMD | register_address.
 * ----------------------------------------------------------------------- */
static TSL2561_Status prv_write_reg(TSL2561_Handle *dev,
                                    uint8_t reg,
                                    uint8_t value)
{
    uint8_t cmd = TSL2561_CMD | (reg & 0x0Fu);
    HAL_StatusTypeDef ret =
        HAL_I2C_Mem_Write(dev->hi2c,
                          dev->addr,
                          cmd,
                          I2C_MEMADD_SIZE_8BIT,
                          &value,
                          1u,
                          TSL2561_I2C_TIMEOUT);
    return (ret == HAL_OK) ? TSL2561_OK : TSL2561_ERROR;
}

/* -----------------------------------------------------------------------
 * Private helper: read one byte from a register.
 * ----------------------------------------------------------------------- */
static TSL2561_Status prv_read_reg(TSL2561_Handle *dev,
                                   uint8_t reg,
                                   uint8_t *out)
{
    uint8_t cmd = TSL2561_CMD | (reg & 0x0Fu);
    HAL_StatusTypeDef ret =
        HAL_I2C_Mem_Read(dev->hi2c,
                         dev->addr,
                         cmd,
                         I2C_MEMADD_SIZE_8BIT,
                         out,
                         1u,
                         TSL2561_I2C_TIMEOUT);
    return (ret == HAL_OK) ? TSL2561_OK : TSL2561_ERROR;
}

/* -----------------------------------------------------------------------
 * Private helper: read two bytes (word) from a register pair.
 * WORD bit set in command byte — HAL reads both low and high bytes in
 * one combined-format transaction (see datasheet Figure 17).
 * ----------------------------------------------------------------------- */
static TSL2561_Status prv_read_word(TSL2561_Handle *dev,
                                    uint8_t reg_low,
                                    uint16_t *out)
{
    uint8_t buf[2];
    /* CMD | WORD | register address */
    uint8_t cmd = TSL2561_CMD | TSL2561_CMD_WORD | (reg_low & 0x0Fu);
    HAL_StatusTypeDef ret =
        HAL_I2C_Mem_Read(dev->hi2c,
                         dev->addr,
                         cmd,
                         I2C_MEMADD_SIZE_8BIT,
                         buf,
                         2u,
                         TSL2561_I2C_TIMEOUT);
    if (ret != HAL_OK)
        return TSL2561_ERROR;

    /* buf[0] = low byte, buf[1] = high byte (datasheet p. 19) */
    *out = (uint16_t)((buf[1] << 8u) | buf[0]);
    return TSL2561_OK;
}

/* =======================================================================
 * Public API
 * ======================================================================= */

TSL2561_Status TSL2561_Init(TSL2561_Handle *dev)
{
    /* 1. Power up (CONTROL register = 0x03) */
    if (prv_write_reg(dev, TSL2561_REG_CONTROL, TSL2561_POWER_UP) != TSL2561_OK)
        return TSL2561_ERROR;

    /* 2. Optional verification — read back CONTROL, expect 0x03 */
    uint8_t ctrl = 0;
    if (prv_read_reg(dev, TSL2561_REG_CONTROL, &ctrl) != TSL2561_OK)
        return TSL2561_ERROR;
    if (ctrl != TSL2561_POWER_UP)
        return TSL2561_ERROR;   /* Device not responding correctly */

    /* 3. Set gain and integration time (TIMING register) */
    uint8_t timing = (dev->gain & 0x10u) | (dev->integ & 0x03u);
    if (prv_write_reg(dev, TSL2561_REG_TIMING, timing) != TSL2561_OK)
        return TSL2561_ERROR;

    return TSL2561_OK;
}

TSL2561_Status TSL2561_PowerDown(TSL2561_Handle *dev)
{
    return prv_write_reg(dev, TSL2561_REG_CONTROL, TSL2561_POWER_DOWN);
}

TSL2561_Status TSL2561_ReadChannels(TSL2561_Handle *dev,
                                    uint16_t *ch0,
                                    uint16_t *ch1)
{
    if (prv_read_word(dev, TSL2561_REG_DATA0LOW, ch0) != TSL2561_OK)
        return TSL2561_ERROR;
    if (prv_read_word(dev, TSL2561_REG_DATA1LOW, ch1) != TSL2561_OK)
        return TSL2561_ERROR;
    return TSL2561_OK;
}

TSL2561_Status TSL2561_ReadID(TSL2561_Handle *dev, uint8_t *id)
{
    return prv_read_reg(dev, TSL2561_REG_ID, id);
}

/* -----------------------------------------------------------------------
 * Integer-only lux calculation.
 * Directly adapted from TAOS reference code (datasheet pp. 24-28).
 * All floating-point removed — safe on any Cortex-M.
 * ----------------------------------------------------------------------- */
uint32_t TSL2561_CalculateLux(TSL2561_Handle *dev,
                               uint16_t ch0,
                               uint16_t ch1)
{
    /* --- Step 1: scale channels for integration time --- */
    uint32_t chScale;
    switch (dev->integ)
    {
        case 0:  chScale = CHSCALE_TINT0; break;   /* 13.7 ms */
        case 1:  chScale = CHSCALE_TINT1; break;   /*  101 ms */
        default: chScale = (1u << CH_SCALE);        /*  402 ms */
    }

    /* Scale for 1x gain (nominal is 16x) */
    if (dev->gain == TSL2561_GAIN_1X)
        chScale <<= 4u;

    uint32_t channel0 = ((uint32_t)ch0 * chScale) >> CH_SCALE;
    uint32_t channel1 = ((uint32_t)ch1 * chScale) >> CH_SCALE;

    /* --- Step 2: compute CH1/CH0 ratio (scaled) --- */
    uint32_t ratio = 0u;
    if (channel0 != 0u)
    {
        uint32_t ratio1 = (channel1 << (RATIO_SCALE + 1u)) / channel0;
        ratio = (ratio1 + 1u) >> 1u;   /* round */
    }

    /* --- Step 3: pick piecewise coefficients --- */
    uint32_t b = 0u, m = 0u;

    if (dev->pkg_type == TSL2561_PKG_T)     /* T, FN, CL package */
    {
        if      (ratio <= K1T) { b = B1T; m = M1T; }
        else if (ratio <= K2T) { b = B2T; m = M2T; }
        else if (ratio <= K3T) { b = B3T; m = M3T; }
        else if (ratio <= K4T) { b = B4T; m = M4T; }
        else if (ratio <= K5T) { b = B5T; m = M5T; }
        else if (ratio <= K6T) { b = B6T; m = M6T; }
        else if (ratio <= K7T) { b = B7T; m = M7T; }
        else                   { b = B8T; m = M8T; }
    }
    else                                    /* CS package */
    {
        if      (ratio <= K1C) { b = B1C; m = M1C; }
        else if (ratio <= K2C) { b = B2C; m = M2C; }
        else if (ratio <= K3C) { b = B3C; m = M3C; }
        else if (ratio <= K4C) { b = B4C; m = M4C; }
        else if (ratio <= K5C) { b = B5C; m = M5C; }
        else if (ratio <= K6C) { b = B6C; m = M6C; }
        else if (ratio <= K7C) { b = B7C; m = M7C; }
        else                   { b = B8C; m = M8C; }
    }

    /* --- Step 4: compute lux --- */
    /* temp = (ch0 * b) - (ch1 * m)  — guard against underflow */
    uint32_t temp = 0u;
    uint32_t term0 = channel0 * b;
    uint32_t term1 = channel1 * m;
    if (term0 > term1)
        temp = term0 - term1;

    /* Round and strip fractional bits */
    temp += (1u << (LUX_SCALE - 1u));
    return temp >> LUX_SCALE;
}

/* -----------------------------------------------------------------------
 * Convenience function: powers up, waits one full integration period,
 * reads both channels, calculates lux, and returns.
 * Uses osDelay — must be called from a FreeRTOS task, not from main()
 * before osKernelStart().
 * ----------------------------------------------------------------------- */
TSL2561_Status TSL2561_ReadLux(TSL2561_Handle *dev, uint32_t *lux)
{
    uint16_t ch0, ch1;

    /* Guard against invalid integ index */
    uint8_t idx = (dev->integ < 3u) ? dev->integ : 2u;

    /* Wait for at least one full conversion (FreeRTOS-friendly delay) */
    osDelay(s_integ_delay_ms[idx]);

    if (TSL2561_ReadChannels(dev, &ch0, &ch1) != TSL2561_OK)
        return TSL2561_ERROR;

    *lux = TSL2561_CalculateLux(dev, ch0, ch1);
    return TSL2561_OK;
}
