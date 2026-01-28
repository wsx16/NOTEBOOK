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

#include "lvgl.h" // 它为整个LVGL提供了更完整的头文件引用
#include "lv_port_disp.h" // LVGL的显示支持
#include "lv_port_indev.h" // LVGL的触屏支持

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
lv_obj_t * ui_main_screen;   // 主屏幕
lv_obj_t * ui_finger_screen; // 指纹功能屏幕
extern StreamBufferHandle_t sb_f1_log;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void MX_FREERTOS_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

#if LVGL
extern void create_wifi_settings_window(void);

// 1. LED 控制：STM32U575 的 LED 连接在 PC13 [3], [4]
static void led_event_cb(lv_event_t * e) {
		static int flag = 1;
    if (lv_event_get_code(e) == LV_EVENT_CLICKED)
    {
        modbus_pack_t pack = {0};

        pack.dev  = LED;
				if (flag)
				{
					pack.func = ON;
					flag = 0;
				}
				else
				{
					pack.func = OFF;
					flag = 1;
				}

        xQueueSend(queuehandle, &pack, 0);
    }
}

// 2. 蜂鸣器控制：BEEP 连接在 PA15 [6]
static void beep_event_cb(lv_event_t * e) {
	  static int flag = 1;
    if (lv_event_get_code(e) == LV_EVENT_CLICKED)
    {
        modbus_pack_t pack = {0};

        pack.dev  = BEEP;
				if (flag)
				{
					pack.func = ON;
					flag = 0;
				}
				else
				{
					pack.func = OFF;
					flag = 1;
				}

        xQueueSend(queuehandle, &pack, 0);
    }
}

// 3. mqtt物联网通信
static void mqtt_event_cb(lv_event_t * e) {
    if(lv_event_get_code(e) == LV_EVENT_CLICKED) {

			static int flag = 1;
			if (lv_event_get_code(e) == LV_EVENT_CLICKED)
			{
					modbus_pack_t pack = {0};

					pack.dev  = WIFI;
					if (flag)
					{
						pack.func = ON;
						flag = 0;
					}
					else
					{
						pack.func = OFF;
						flag = 1;
					}

					xQueueSend(queuehandle, &pack, 0);
			}
    }
}

// 点击“设置”按钮的回调函数
static void wifi_setting_btn_cb(lv_event_t * e) {
    if(lv_event_get_code(e) == LV_EVENT_CLICKED) {
        // 只有点击时，才创建弹窗
        create_wifi_settings_window();
    }
}
// --- 回调 1: 发送指纹指令 ---
static void finger_op_event_cb(lv_event_t * e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    uint8_t cmd = (uint8_t)(uintptr_t)lv_event_get_user_data(e);

    modbus_pack_t pack = {0};
    pack.dev  = FINGER;
    pack.func = cmd;

    xQueueSend(queuehandle, &pack, pdMS_TO_TICKS(10));

}

// --- 回调 2: 屏幕切换 ---
static void screen_switch_event_cb(lv_event_t * e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED)
    {
        // 获取要跳转的目标屏幕
        lv_obj_t * target_screen = (lv_obj_t *)lv_event_get_user_data(e);
        
        // 执行跳转动画 (无动画，立即跳转)
        lv_scr_load(target_screen);
    }
}

static lv_obj_t * ui_finger_del_screen;
static lv_obj_t * ta_del_id;
static lv_obj_t * kb_del;   // 键盘对象

static void ta_focus_cb(lv_event_t * e);
static void kb_event_cb(lv_event_t * e);

static void finger_del_confirm_cb(lv_event_t * e);
static void finger_del_cancel_cb(lv_event_t * e);


static lv_obj_t * btn_ok;
static lv_obj_t * btn_back;


static void setup_finger_delete_screen(void)
{
    ui_finger_del_screen = lv_obj_create(NULL);

    lv_obj_t * title = lv_label_create(ui_finger_del_screen);
    lv_label_set_text(title, "Delete Finger ID");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

    ta_del_id = lv_textarea_create(ui_finger_del_screen);
    lv_obj_set_size(ta_del_id, 120, 45);
    lv_obj_align(ta_del_id, LV_ALIGN_TOP_MID, 0, 70);
    lv_textarea_set_one_line(ta_del_id, true);
    lv_textarea_set_max_length(ta_del_id, 1);
    lv_textarea_set_accepted_chars(ta_del_id, "0123456789");
    lv_textarea_set_placeholder_text(ta_del_id, "0-9");

    /* 重点：监听 focus/defocus */
    lv_obj_add_event_cb(ta_del_id, ta_focus_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(ta_del_id, ta_focus_cb, LV_EVENT_DEFOCUSED, NULL);

    /* Confirm/Back 按钮（默认显示） */
    btn_ok = lv_btn_create(ui_finger_del_screen);
    lv_obj_set_size(btn_ok, 80, 45);
    lv_obj_align(btn_ok, LV_ALIGN_CENTER, -60, 60);
    lv_obj_t * lbl_ok = lv_label_create(btn_ok);
    lv_label_set_text(lbl_ok, "Confirm");
    lv_obj_center(lbl_ok);
    lv_obj_add_event_cb(btn_ok, finger_del_confirm_cb, LV_EVENT_CLICKED, NULL);

    btn_back = lv_btn_create(ui_finger_del_screen);
    lv_obj_set_size(btn_back, 80, 45);
    lv_obj_align(btn_back, LV_ALIGN_CENTER, 60, 60);
    lv_obj_t * lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, "Back");
    lv_obj_center(lbl_back);
    lv_obj_add_event_cb(btn_back, finger_del_cancel_cb, LV_EVENT_CLICKED, NULL);

    /* 键盘（默认隐藏） */
    kb_del = lv_keyboard_create(ui_finger_del_screen);
    lv_keyboard_set_mode(kb_del, LV_KEYBOARD_MODE_NUMBER);
    lv_keyboard_set_textarea(kb_del, ta_del_id);
    lv_obj_set_size(kb_del, LV_PCT(100), 140);
    lv_obj_align(kb_del, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(kb_del, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(kb_del, kb_event_cb, LV_EVENT_VALUE_CHANGED, NULL);


}

static void ta_focus_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_FOCUSED)
    {
        /* 键盘出现：覆盖按钮区域 */
        lv_obj_clear_flag(kb_del, LV_OBJ_FLAG_HIDDEN);

        /* 你要的效果：按钮消失 */
        lv_obj_add_flag(btn_ok,   LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(btn_back, LV_OBJ_FLAG_HIDDEN);

        lv_keyboard_set_textarea(kb_del, ta_del_id);
    }
    else if (code == LV_EVENT_DEFOCUSED)
    {
        /* 退出输入框：键盘收起、按钮恢复 */
        lv_obj_add_flag(kb_del, LV_OBJ_FLAG_HIDDEN);

        lv_obj_clear_flag(btn_ok,   LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(btn_back, LV_OBJ_FLAG_HIDDEN);
    }
}


static void kb_event_cb(lv_event_t * e)
{
    lv_obj_t * kb = lv_event_get_target(e);

    /* 先交给默认处理：把数字写入 textarea */
    lv_keyboard_def_event_cb(e);

    uint16_t btn_id = lv_keyboard_get_selected_btn(kb);
    if (btn_id == LV_BTNMATRIX_BTN_NONE) return;

    const char * txt = lv_btnmatrix_get_btn_text(kb, btn_id);
    if (!txt) return;

    if (strcmp(txt, "OK") == 0 || strcmp(txt, LV_SYMBOL_OK) == 0 ||
        strcmp(txt, "Close") == 0 || strcmp(txt, LV_SYMBOL_CLOSE) == 0)
    {
        /* 清焦点 -> 触发 ta_del_id 的 LV_EVENT_DEFOCUSED -> 自动恢复按钮 */
        lv_obj_clear_state(ta_del_id, LV_STATE_FOCUSED);
    }
}


static void finger_del_confirm_cb(lv_event_t * e)
{
    (void)e;
    const char *s = lv_textarea_get_text(ta_del_id);
    if (!s || s[0] < '0' || s[0] > '9' || s[1] != '\0') {
        return; // 当前F0删除实现只支持单字符0~9
    }

    modbus_pack_t pack = {0};
    pack.dev  = FINGER;
    pack.func = FINGER_CMD_DELETE;   // '3'
    pack.arg0 = (uint8_t)s[0];       // ASCII '0'..'9'

    xQueueSend(queuehandle, &pack, pdMS_TO_TICKS(10));

    // 发送后回到指纹主界面（可选）
    lv_scr_load(ui_finger_screen);
}

static void finger_del_cancel_cb(lv_event_t * e)
{
    (void)e;
    lv_scr_load(ui_finger_screen);
}


// --- 初始化指纹子屏幕 ---
void setup_finger_screen(void) {
    // 1. 创建屏幕对象

    ui_finger_screen = lv_obj_create(NULL);
    
    // 2. 添加标题
    lv_obj_t * label_title = lv_label_create(ui_finger_screen);
    lv_label_set_text(label_title, "Fingerprint Mode");
    lv_obj_align(label_title, LV_ALIGN_TOP_MID, 0, 20);

    // 3. 创建 [录入] 按钮
    lv_obj_t * btn_enroll = lv_btn_create(ui_finger_screen);
    lv_obj_set_size(btn_enroll, 120, 50);
    lv_obj_align(btn_enroll, LV_ALIGN_CENTER, 0, -60);
    lv_obj_t * lbl_enroll = lv_label_create(btn_enroll);
    lv_label_set_text(lbl_enroll, "Enroll (1)");
    lv_obj_center(lbl_enroll);
    // 绑定事件：发送 '1'
    lv_obj_add_event_cb(btn_enroll, finger_op_event_cb, LV_EVENT_CLICKED, (void*)FINGER_CMD_ENROLL);

    // 4. 创建 [识别] 按钮
    lv_obj_t * btn_unlock = lv_btn_create(ui_finger_screen);
    lv_obj_set_size(btn_unlock, 120, 50);
    lv_obj_align(btn_unlock, LV_ALIGN_CENTER, 0, 10);
    lv_obj_t * lbl_unlock = lv_label_create(btn_unlock);
    lv_label_set_text(lbl_unlock, "Unlock (2)");
    lv_obj_center(lbl_unlock);
    // 绑定事件：发送 '2'
    lv_obj_add_event_cb(btn_unlock, finger_op_event_cb, LV_EVENT_CLICKED, (void*)FINGER_CMD_UNLOCK);

    // 5. 创建 [删除] 按钮
    lv_obj_t * btn_delete = lv_btn_create(ui_finger_screen);
    lv_obj_set_size(btn_delete, 120, 50);
    lv_obj_align(btn_delete, LV_ALIGN_CENTER, 0, 80);
    lv_obj_t * lbl_delete = lv_label_create(btn_delete);
    lv_label_set_text(lbl_delete, "Delete (3)");
    lv_obj_center(lbl_delete);
    // 绑定事件：发送 '3'

    // 6. 创建 [返回] 按钮
    lv_obj_t * btn_back = lv_btn_create(ui_finger_screen);
    lv_obj_set_size(btn_back, 60, 30);
    lv_obj_align(btn_back, LV_ALIGN_CENTER, 0, 135);
    lv_obj_t * lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, "< Back");
    lv_obj_center(lbl_back);
    
    // 绑定事件：跳转回主屏幕 (此时 ui_main_screen 在 setup_peripheral_control_ui 开头已赋值)
    lv_obj_add_event_cb(btn_back, screen_switch_event_cb, LV_EVENT_CLICKED, ui_main_screen);
		
		setup_finger_delete_screen();
		lv_obj_add_event_cb(btn_delete, screen_switch_event_cb, LV_EVENT_CLICKED, ui_finger_del_screen);

}



void setup_peripheral_control_ui(void) {
	  ui_main_screen = lv_scr_act(); 
    // --- LED 按钮 ---
    lv_obj_t * btn_led = lv_btn_create(lv_scr_act());
    lv_obj_set_pos(btn_led, 10, 10);
    lv_obj_set_size(btn_led, 150, 50);
    lv_obj_t * label_led = lv_label_create(btn_led);
    lv_label_set_text(label_led, "LED ON/OFF");
    lv_obj_align(label_led, LV_ALIGN_CENTER, 0, 0);

    lv_obj_add_event_cb(btn_led, led_event_cb, LV_EVENT_CLICKED, NULL);

    // --- 蜂鸣器按钮 ---
    lv_obj_t * btn_beep = lv_btn_create(lv_scr_act());
    lv_obj_set_pos(btn_beep, 10, 70); // 纵向排布
    lv_obj_set_size(btn_beep, 150, 50);
    lv_obj_t * label_beep = lv_label_create(btn_beep);
    lv_label_set_text(label_beep, "BEEP ON/OFF");
    lv_obj_align(label_beep, LV_ALIGN_CENTER, 0, 0);
	
    lv_obj_add_event_cb(btn_beep, beep_event_cb, LV_EVENT_CLICKED, NULL);

	  // --- wifi按钮 ---
    lv_obj_t * btn_wifi = lv_btn_create(lv_scr_act());
    lv_obj_set_pos(btn_wifi, 10, 130); // 纵向排布
    lv_obj_set_size(btn_wifi, 150, 50);
    lv_obj_t * label_wifi = lv_label_create(btn_wifi);
    lv_label_set_text(label_wifi, "Retry / Connect");
    lv_obj_align(label_wifi, LV_ALIGN_CENTER, 0, 0);

    lv_obj_add_event_cb(btn_wifi, mqtt_event_cb, LV_EVENT_CLICKED, NULL);

    // --- WiFi 配置按钮 ---
    lv_obj_t * btn_set = lv_btn_create(lv_scr_act());
    lv_obj_set_pos(btn_set, 10, 190); // 纵向排布
    lv_obj_set_size(btn_set, 150, 50);
    
    lv_obj_t * label_set = lv_label_create(btn_set);
    lv_label_set_text(label_set, "WiFi Settings"); // 按钮文字
    lv_obj_align(label_set, LV_ALIGN_CENTER, 0, 0);

		lv_obj_add_event_cb(btn_set, wifi_setting_btn_cb, LV_EVENT_CLICKED, NULL);
		
    // --- 指纹界面 ---
    lv_obj_t * btn_finger_menu = lv_btn_create(ui_main_screen);
    lv_obj_set_pos(btn_finger_menu, 10, 250); // 
    lv_obj_set_size(btn_finger_menu, 150, 50);
    lv_obj_t * label_menu = lv_label_create(btn_finger_menu);
    lv_label_set_text(label_menu, "Fingerprint >");
    lv_obj_align(label_menu, LV_ALIGN_CENTER, 0, 0);

    // 2. 初始化指纹子屏幕
    setup_finger_screen();

    // 3. 绑定跳转事件
    // 主界面 -> 指纹界面
    lv_obj_add_event_cb(btn_finger_menu, screen_switch_event_cb, LV_EVENT_CLICKED, ui_finger_screen);

}
#endif
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
