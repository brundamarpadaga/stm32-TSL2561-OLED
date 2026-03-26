/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include <ETH_Init.h>
#include <I2C_Init.h>
#include <UART_Init.h>
#include <USB_Init.h>
#include "main.h"
#include "cmsis_os.h"
#include "tsl2561.h"
#include<FreeRTOS.h>
#include "queue.h"      /* QueueHandle_t, xQueueCreate, xQueueSend, xQueueReceive */
#include "task.h"       /* portMAX_DELAY, pdPASS */
#include<SEGGER_SYSVIEW.h>

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define SENSOR_QUEUE_LENGTH     16
#define SENSOR_QUEUE_ITEM_SIZE  sizeof(uint16_t)
#define STACK_SIZE              256
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* Task handles */
TaskHandle_t defaultTaskHandle;
TaskHandle_t readLightSensorHandle;
TaskHandle_t updateOLEDHandle;

/* USER CODE BEGIN PV */
/* Native FreeRTOS queue - replaces osMessageQueue */
static QueueHandle_t sensorReadingQHandle;

static TSL2561_Handle g_light_sensor;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
void StartDefaultTask(void *argument);
void StartReadLightSensor(void *argument);
void StartUpdateOLED(void *argument);

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* MCU Configuration -------------------------------------------------------*/
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ETH_Init();
  MX_USART3_UART_Init();
  MX_USB_OTG_FS_PCD_Init();
  MX_I2C1_Init();

  /* USER CODE BEGIN 2 */

  /* Configure TSL2561 light sensor over I2C */
  g_light_sensor.hi2c     = &hi2c1;
  g_light_sensor.addr     = TSL2561_ADDR_FLOAT;   /* ADDR SEL pin floating  */
  g_light_sensor.gain     = TSL2561_GAIN_1X;      /* 1x for bright rooms    */
  g_light_sensor.integ    = TSL2561_INTEG_101MS;  /* 101 ms, good balance   */
  g_light_sensor.pkg_type = TSL2561_PKG_T;        /* T/FN/CL package        */

  /* Sanity check before RTOS starts — HAL_Delay is safe here (no scheduler) */
  uint8_t sensor_id = 0;
  if (TSL2561_Init(&g_light_sensor) != TSL2561_OK)
  {
      /* If this fires: check wiring, pull-ups, and ADDR SEL pin voltage */
      Error_Handler();
  }
  TSL2561_ReadID(&g_light_sensor, &sensor_id);
  /* sensor_id upper nibble: 0x0 = TSL2560, 0x1 = TSL2561 */

  /* Enable DWT cycle counter — required for SystemView timestamps */
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0;
  DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;

  /* Start SystemView */
  SEGGER_SYSVIEW_Conf();
  SEGGER_SYSVIEW_Start();

  /* USER CODE END 2 */

  /* Init scheduler */
  //osKernelInitialize();

  /* USER CODE BEGIN RTOS_MUTEX */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* Native FreeRTOS queue — holds up to 16 uint16_t lux readings */
  sensorReadingQHandle = xQueueCreate(SENSOR_QUEUE_LENGTH, SENSOR_QUEUE_ITEM_SIZE);
  configASSERT(sensorReadingQHandle != NULL);
  /* USER CODE END RTOS_QUEUES */

  /* Create the threads */
  /* USER CODE BEGIN RTOS_THREADS */
  xTaskCreate(StartDefaultTask,      "defaultTask",      STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, &defaultTaskHandle);
  xTaskCreate(StartReadLightSensor,  "readLightSensor",  STACK_SIZE, NULL, tskIDLE_PRIORITY + 2, &readLightSensorHandle);
  xTaskCreate(StartUpdateOLED,       "updateOLED",       STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, &updateOLEDHandle);
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* USER CODE END RTOS_EVENTS */

  /* Start scheduler — should never return */
  vTaskStartScheduler();

  /* If we reach here, something went wrong with the scheduler */
  while (1) {}
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief Default idle task — reserved for future use
  */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */
  for (;;)
  {
    vTaskDelay(pdMS_TO_TICKS(1));
  }
  /* USER CODE END StartDefaultTask */
}

/**
  * @brief Reads lux value from TSL2561 and posts to sensor queue
  */
void StartReadLightSensor(void *argument)
{
  /* USER CODE BEGIN StartReadLightSensor */
  uint32_t lux = 0;

  for (;;)
  {
      /* TSL2561_ReadLux internally delays one integration period (~101 ms) */
      if (TSL2561_ReadLux(&g_light_sensor, &lux) == TSL2561_OK)
      {
          /* Queue holds uint16_t — cap lux at 65535 for safety */
          uint16_t lux16 = (lux > 65535u) ? 65535u : (uint16_t)lux;

          /* Post to queue, don't block if full (timeout = 0) */
          xQueueSend(sensorReadingQHandle, &lux16, 0);
      }

      vTaskDelay(pdMS_TO_TICKS(200));
  }
  /* USER CODE END StartReadLightSensor */
}

/**
  * @brief Receives lux readings from queue and updates OLED display
  */
void StartUpdateOLED(void *argument)
{
  /* USER CODE BEGIN StartUpdateOLED */
  uint16_t lux = 0;

  for (;;)
  {
      /* Block until a lux reading is available (portMAX_DELAY = wait forever) */
      if (xQueueReceive(sensorReadingQHandle, &lux, portMAX_DELAY) == pdPASS)
      {
          HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
          vTaskDelay(25/portTICK_PERIOD_MS);
          HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
          vTaskDelay(25/portTICK_PERIOD_MS);
      }
  }
  /* USER CODE END StartUpdateOLED */
}

/**
  * @brief System Clock Configuration
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  HAL_PWR_EnableBkUpAccess();
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState       = RCC_HSE_BYPASS;
  RCC_OscInitStruct.PLL.PLLState   = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM       = 4;
  RCC_OscInitStruct.PLL.PLLN       = 96;
  RCC_OscInitStruct.PLL.PLLP       = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ       = 4;
  RCC_OscInitStruct.PLL.PLLR       = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) { Error_Handler(); }

  if (HAL_PWREx_EnableOverDrive() != HAL_OK) { Error_Handler(); }

  RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                   | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK) { Error_Handler(); }
}

/**
  * @brief GPIO Initialization — LEDs and USB power pins
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();

  HAL_GPIO_WritePin(GPIOB, LD1_Pin | LD3_Pin | LD2_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(USB_PowerSwitchOn_GPIO_Port, USB_PowerSwitchOn_Pin, GPIO_PIN_RESET);

  /* User button — rising edge interrupt */
  GPIO_InitStruct.Pin  = USER_Btn_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(USER_Btn_GPIO_Port, &GPIO_InitStruct);

  /* LED pins — push-pull output */
  GPIO_InitStruct.Pin   = LD1_Pin | LD3_Pin | LD2_Pin;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USB power switch */
  GPIO_InitStruct.Pin   = USB_PowerSwitchOn_Pin;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(USB_PowerSwitchOn_GPIO_Port, &GPIO_InitStruct);

  /* USB overcurrent sense — input only */
  GPIO_InitStruct.Pin  = USB_OverCurrent_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(USB_OverCurrent_GPIO_Port, &GPIO_InitStruct);
}

/**
  * @brief Error handler — disables interrupts and halts
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  __disable_irq();
  while (1) {}
  /* USER CODE END Error_Handler_Debug */
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
