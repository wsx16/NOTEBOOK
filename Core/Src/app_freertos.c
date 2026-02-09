/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : app_freertos.c
  * Description        : FreeRTOS applicative file
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
#include "app_freertos.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "modbus.h"
#include "mqtt.h"
#include "bsp_ili9341_4line.h"
#include "bsp_ft6336.h"
#include "lvgl.h"          // 它为整个LVGL提供了更完整的头文件引用
#include "lv_port_disp.h"  // LVGL的显示支持
#include "lv_port_indev.h" // LVGL的触屏支持
#include "stream_buffer.h"
#include "OTA.h"
#include "lvgls.h"
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
/* USER CODE BEGIN Variables */
int ret;
TaskHandle_t lvglhandle;
QueueHandle_t queuehandle;
StreamBufferHandle_t sb_f1_log = NULL;
TaskHandle_t f1loghandle;
/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 1024 * 4
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
void lvgl(void const * argument);
void F1_LogTask(void *argument);
 
/* USER CODE END FunctionPrototypes */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
	queuehandle = xQueueCreate(4, sizeof(modbus_pack_t));
	sb_f1_log = xStreamBufferCreate(2048, 1);
  /* USER CODE END RTOS_QUEUES */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(modbus_parse_task, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
	ret = xTaskCreate((TaskFunction_t)lvgl,
										"lvgl",
										2048 ,
										NULL,
										osPriorityLow,
										&lvglhandle);
	if (ret == pdPASS)
	{
		printf("lvgl create ok\n");
	}
	ret = xTaskCreate(F1_LogTask,
										"F1Log",
										1024,           
										NULL,
										osPriorityBelowNormal,  
										&f1loghandle);

	if (ret == pdPASS)
	{
		printf("F1Log create ok\n");
	}
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}
/* USER CODE BEGIN Header_modbus_parse_task */
/**
* @brief Function implementing the defaultTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_modbus_parse_task */
extern uint8_t f1_rx_buffer[512]; 
void modbus_parse_task(void *argument)
{
  /* USER CODE BEGIN defaultTask */
	modbus_pack_t pack;
	// uint8_t dev;
	// uint8_t func;
	int ret;
//  OTA_init();
  /* Infinite loop */
  for(;;)
  {
		if (xQueueReceive(queuehandle , &pack, pdMS_TO_TICKS(100)) == pdTRUE)
		{
				peripheral_ctrl(&pack);
		}
  }

  /* USER CODE END defaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

void lvgl(void const *argument)
{
#if LVGL
	setup_peripheral_control_ui();
#endif
  for(;;)
  {	
    lv_timer_handler(); 

    vTaskDelay(pdMS_TO_TICKS(5)); 
	}
}

void F1_LogTask(void *argument)
{
    uint8_t ch;
    int line_start = 1;

    for (;;)
    {
        if (xStreamBufferReceive(sb_f1_log, &ch, 1, portMAX_DELAY) == 1)
        {
            if (line_start)
            {
                printf("[F0 Reply]: ");
                line_start = 0;
            }

            putchar(ch);

            if (ch == '\n')  // 以换行作为一条日志结束
            {
                line_start = 1;
            }
        }
    }
}


/* USER CODE END Application */

