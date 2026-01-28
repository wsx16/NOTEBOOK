#ifndef __LVGLs__
#define __LVGLs__


#include "lvgl.h"           // 它为整个LVGL提供了更完整的头文件引用
#include "lv_port_disp.h"   // LVGL的显示支持
#include "lv_port_indev.h"  // LVGL的触屏支持
#include "modbus.h"
#include "FreeRTOS.h"
#include "queue.h"

extern lv_obj_t * ui_main_screen;   // 主屏幕
extern lv_obj_t * ui_finger_screen; // 指纹功能屏幕

/* --- 外部变量引用 --- */
extern QueueHandle_t queuehandle; // FreeRTOS 消息队列句柄
extern char g_wifi_ssid[32];      // 用于存储 SSID 的全局变量
extern char g_wifi_pwd[64];       // 用于存储 密码 的全局变量

extern void setup_peripheral_control_ui(void);
extern void setup_finger_screen(void);
extern void create_wifi_settings_window(void);


#endif 