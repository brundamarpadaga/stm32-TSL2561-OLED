# STM32F767ZI — Ambient Light Sensor Display with FreeRTOS

**Platform:** STM32F767ZI (Nucleo-144)
**Toolchain:** STM32CubeIDE · arm-none-eabi-gcc · SEGGER J-Link RTT Viewer
**RTOS:** FreeRTOS (native API)
**Sensor:** TSL2561 (I2C ambient light sensor)
**Display:** OLED SSD1306 (target — in progress)

---

## Project Objective

The primary goal of this project is STM32 platform bring-up and hands-on familiarity with the STM32CubeIDE ecosystem, while deepening practical knowledge of FreeRTOS. The TSL2561 light sensor and OLED display serve as the application layer to make the learning concrete and hardware-grounded.

**Learning targets:**
- STM32 HAL bring-up: clocks, GPIO, I2C peripheral configuration via CubeMX
- FreeRTOS task creation, scheduling, and inter-task communication using queues
- Writing and integrating a peripheral driver (TSL2561) on top of STM32 HAL
- Real-time debug workflow using SEGGER RTT and SystemView

---

## Hardware

| Component | Details |
|-----------|---------|
| MCU board | STM32F767ZI Nucleo-144 |
| Sensor | TSL2561 ambient light sensor, I2C address 0x39 (ADDR SEL floating) |
| Display | SSD1306 OLED (planned) |
| I2C pins | PB6 = SCL, PB9 = SDA |
| Debug | SEGGER J-Link RTT (SWD) |

---

## Architecture

Three FreeRTOS tasks communicate via a native FreeRTOS queue:

```
┌─────────────────────┐         ┌──────────────────────┐
│  StartReadLight     │──lux──▶ │   sensorReadingQ      │
│  SensorTask         │  queue  │   (QueueHandle_t)     │
│  Priority: 2        │         └──────────┬───────────┘
└─────────────────────┘                    │
                                           ▼
                                ┌──────────────────────┐
                                │   StartUpdateOLED    │
                                │   Task               │
                                │   Priority: 1        │
                                └──────────────────────┘

┌─────────────────────┐
│  StartDefaultTask   │  Heartbeat LED blink (PB7), idle priority
└─────────────────────┘
```

**Queue:** holds up to 16 `uint16_t` lux readings. Producer posts without blocking (timeout = 0). Consumer blocks indefinitely with `portMAX_DELAY`.

---

## TSL2561 Driver

A custom HAL-based driver (`tsl2561.c / tsl2561.h`) wraps all sensor communication. Key design decisions:

- Uses `HAL_I2C_Mem_Write` / `HAL_I2C_Mem_Read` — HAL handles the I2C combined-format repeated-start automatically
- COMMAND byte format: `0x80 | register_address` (CMD bit always set)
- WORD reads: `0x80 | 0x20 | register_address` for 16-bit channel reads
- Integer-only lux calculation — no floating point, safe on Cortex-M without FPU license
- Integration delay handled via `vTaskDelay` (FreeRTOS-friendly, must be called from a task)

**Public API:**
```c
TSL2561_Status TSL2561_Init(TSL2561_Handle *dev);
TSL2561_Status TSL2561_ReadChannels(TSL2561_Handle *dev, uint16_t *ch0, uint16_t *ch1);
TSL2561_Status TSL2561_ReadLux(TSL2561_Handle *dev, uint32_t *lux);
uint32_t       TSL2561_CalculateLux(TSL2561_Handle *dev, uint16_t ch0, uint16_t ch1);
```

---

## Current Status

### Done
- [x] Baremetal I2C driver for TSL2561 — register reads, word reads, lux calculation verified
- [x] I2C bus scan and device detection confirmed (address 0x72, 8-bit)
- [x] HAL I2C bring-up on STM32F767ZI — pins PB6/PB9 configured
- [x] TSL2561 HAL driver integrated and initializing correctly
- [x] FreeRTOS scheduler running — three tasks created and scheduling correctly
- [x] Lux readings flowing from sensor task into queue
- [x] RTT logging of lux values confirmed working in J-Link RTT Viewer
- [x] NVIC priority grouping configured correctly for FreeRTOS (`NVIC_SetPriorityGrouping(0)`)

### In Progress
- [ ] Stack size tuning — HardFault under investigation, likely stack overflow in sensor task from `snprintf` usage. Increasing `STACK_SIZE` from 256 to 512 words.
- [ ] OLED SSD1306 bring-up — I2C driver integration pending stack issue resolution
- [ ] `StartUpdateOLED` task — currently receives from queue and blinks LED as placeholder; OLED rendering to be added

### Planned
- [ ] Display lux value on SSD1306 OLED
- [ ] SystemView trace integration for task timing analysis
- [ ] Investigate interrupt-driven I2C vs current polling/HAL approach

---

## Key Bugs Encountered During Bring-Up

A log of real issues hit during development — useful reference and interview material.

**1. I2C bus stuck (SDA held low)**
Switching from baremetal to HAL firmware without power cycling the sensor left the TSL2561 mid-transaction, pulling SDA low. Diagnosed by reading `GPIOB->IDR` bit 9 directly. Fixed by implementing a 9-clock SCL recovery sequence before `MX_I2C1_Init()`, and power cycling the sensor.

**2. Control register readback mismatch**
`TSL2561_Init` was failing because the control register read back `0x33` instead of `0x03`. The upper bits are undefined per the datasheet. Fixed by masking: `(ctrl & 0x03) != TSL2561_POWER_UP`.

**3. NVIC priority grouping assertion**
FreeRTOS `configASSERT` fired in `port.c` because `HAL_Init()` sets priority grouping to 4 (split preemption/sub-priority bits). FreeRTOS requires all bits to be preemption bits. Fixed by adding `NVIC_SetPriorityGrouping(0)` immediately after `HAL_Init()`.

**4. Missing pull-ups on I2C lines**
HAL MSP init had `GPIO_NOPULL` on PB6/PB9. I2C is open-drain and requires pull-ups. Baremetal code had internal pull-ups enabled. Fixed by changing to `GPIO_PULLUP` in `HAL_I2C_MspInit`.

**5. osDelay in FreeRTOS native context**
`TSL2561_ReadLux` used `osDelay` (CMSIS-RTOS wrapper). After removing CMSIS-RTOS in favour of native FreeRTOS API, replaced with `vTaskDelay(pdMS_TO_TICKS(...))`.

**6. Stack overflow / HardFault**
Adding `snprintf` to the sensor task caused a HardFault — `snprintf` draws significant stack from the newlib-nano runtime. Stack size of 256 words (1KB) is too small. Fix: increase to 512 words.

---

## FreeRTOS Concepts Practiced

| Concept | Where used |
|---------|-----------|
| Task creation (`xTaskCreate`) | Three tasks: default, sensor, OLED |
| Task priorities | Sensor task at priority 2, OLED and default at priority 1 |
| Queue (`xQueueCreate`, `xQueueSend`, `xQueueReceive`) | Lux readings producer/consumer |
| `portMAX_DELAY` | OLED task blocks indefinitely waiting for sensor data |
| `vTaskDelay` / `pdMS_TO_TICKS` | Sensor integration wait, LED blink timing |
| Stack overflow detection | `configCHECK_FOR_STACK_OVERFLOW`, `vApplicationStackOverflowHook` |
| NVIC priority configuration | Correct grouping for FreeRTOS syscall priority |

---

## Build Configuration

| Setting | Value |
|---------|-------|
| MCU | STM32F767ZITx |
| FPU | fpv5-d16, hard ABI |
| C standard | gnu11 |
| Optimization | -O0 (debug) |
| Specs | nano.specs (newlib-nano) |
| I2C clock | ~100kHz, TIMINGR = 0x20303E5D @ 48MHz PCLK1 |
| FreeRTOS port | GCC/ARM\_CM7/r0p1 |
