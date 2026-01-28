/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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
#include "main.h"
#include "cmsis_os2.h"
#include "i2c.h"
#include "icache.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "modbus.h"
#include "mqtt.h"
#include "bsp_ili9341_4line.h"
#include "bsp_ft6336.h"

#include "lvgls.h"

#include "gui_guider.h"
#include "events_init.h"
#include "stream_buffer.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
uint8_t f1_rx_buffer[512]; 
lv_ui guider_ui;
char g_wifi_ssid[32];
char g_wifi_pwd[64];
char test[128];

extern StreamBufferHandle_t sb_f1_log;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void MX_FREERTOS_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
	__set_MSP(*((volatile unsigned long int *)0x0800A000));  
	SCB->VTOR = FLASH_BASE | 0xA000;
	__enable_irq();
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */
  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART1_UART_Init();
  MX_ICACHE_Init();
  MX_SPI1_Init();
  MX_UART5_Init();
  MX_I2C1_Init();
  MX_TIM6_Init();
  MX_USART3_UART_Init();
  /* USER CODE BEGIN 2 */
	FT6336_init();

	lv_init();              // LVGL 初始化
  lv_port_disp_init();    // 注册LVGL的显示任务
  lv_port_indev_init();   // 注册LVGL的触屏检测任务
	
	HAL_TIM_Base_Start_IT(&htim6);
#if !LVGL 
	setup_ui(&guider_ui);
	events_init(&guider_ui);
#endif
	// 1. 启动 Modbus 接收
	HAL_UARTEx_ReceiveToIdle_IT(&huart1, (uint8_t *)buffer, MODBUS_MAX_PACK_LEN);
  // 启动 MQTT (ESP8266) 接收
	HAL_UARTEx_ReceiveToIdle_IT(&huart5, (uint8_t *)esp_rx_buffer, RX_BUF_SIZE);
	HAL_UARTEx_ReceiveToIdle_IT(&huart3, f1_rx_buffer, sizeof(f1_rx_buffer));
  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();
  /* Call init function for freertos objects (in app_freertos.c) */
  MX_FREERTOS_Init();

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_0;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
  RCC_OscInitStruct.PLL.PLLMBOOST = RCC_PLLMBOOST_DIV4;
  RCC_OscInitStruct.PLL.PLLM = 3;
  RCC_OscInitStruct.PLL.PLLN = 10;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 1;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLLVCIRANGE_1;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_PCLK3;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    // --- 情况 1: Modbus ---
    if (huart->Instance == USART1) 
    {
        Modbus_Rx_Handler(Size);
    }
    
    // --- 情况 2: MQTT/ESP8266 ---
    else if (huart->Instance == UART5) 
    {
        MQTT_Rx_Handler(Size);
    }
		// --- 情况 3: 指纹模块 F0 (USART3) ---
    else if (huart->Instance == USART3)
    {
				BaseType_t hpw = pdFALSE;

				// 只搬运数据到StreamBuffer，不做printf
				if (sb_f1_log != NULL && Size > 0)
				{
						xStreamBufferSendFromISR(sb_f1_log, f1_rx_buffer, Size, &hpw);
				}

				// 立刻重启接收
				HAL_UARTEx_ReceiveToIdle_IT(&huart3, f1_rx_buffer, sizeof(f1_rx_buffer));

				portYIELD_FROM_ISR(hpw);
    }

}
/**
  * @brief  串口错误回调函数 (防止 ESP8266 重启乱码导致串口锁死)
  */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    // 判断是否是连接 ESP8266 的串口 (UART5)
    if (huart->Instance == UART5) 
    {

        
        // 重新开启空闲中断接收
        HAL_UARTEx_ReceiveToIdle_IT(&huart5, (uint8_t *)esp_rx_buffer, RX_BUF_SIZE);
    }
    // 如果是 Modbus 串口出错，也要重启
    else if (huart->Instance == USART1)
    {
        HAL_UARTEx_ReceiveToIdle_IT(&huart1, (uint8_t *)buffer, MODBUS_MAX_PACK_LEN);
    }
		// 指纹模块 F0 串口防锁死重启
    else if (huart->Instance == USART3) {
        HAL_UARTEx_ReceiveToIdle_IT(&huart3, f1_rx_buffer, sizeof(f1_rx_buffer));
    }
}
/* USER CODE END 4 */

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM1 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

	if (htim->Instance == TIM6)
  {
    lv_tick_inc(1);//提供对应的心跳
  }
  /* USER CODE END Callback 0 */

  /* USER CODE BEGIN Callback 1 */
  if (htim->Instance == TIM1)
  {
    HAL_IncTick();
  }
  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
