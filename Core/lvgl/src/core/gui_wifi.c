/**
  ******************************************************************************
  * @file    gui_wifi.c
  * @brief   LVGL Wi-Fi Configuration Window for 240x320 Screen
  ******************************************************************************
  */

#include "lvgls.h"


/* --- 静态变量 (控件指针) --- */
static lv_obj_t * wifi_win = NULL; // 窗口容器
static lv_obj_t * ta_ssid = NULL;  // SSID 输入框
static lv_obj_t * ta_pwd = NULL;   // 密码 输入框
static lv_obj_t * kb = NULL;       // 虚拟键盘

/* --- 回调函数声明 --- */
static void ta_event_cb(lv_event_t * e);
static void btn_save_event_cb(lv_event_t * e);
static void btn_close_event_cb(lv_event_t * e);

/**
 * @brief  创建 WiFi 设置窗口的入口函数
 * @note   在 "WiFi Settings" 按钮的回调中调用此函数
 */
void create_wifi_settings_window(void) 
{
    // 1. 如果窗口已经存在，不再创建，直接前置
    if (wifi_win != NULL) {
        lv_obj_move_foreground(wifi_win);
        return;
    }

    // 2. 创建窗口容器
    // 屏幕宽240，窗口设为220，留出左右边距
    wifi_win = lv_obj_create(lv_scr_act());
    lv_obj_set_size(wifi_win, 220, 260); 
    lv_obj_center(wifi_win); // 初始居中

    // 使用垂直 Flex 布局，控件自动从上往下排列
    lv_obj_set_flex_flow(wifi_win, LV_FLEX_FLOW_COLUMN); 
    lv_obj_set_flex_align(wifi_win, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  
    // 3. 创建标题栏区域 (用于放置标题和关闭按钮)
    lv_obj_t * title_cont = lv_obj_create(wifi_win);
    lv_obj_set_size(title_cont, 190, 40);
    lv_obj_set_style_pad_all(title_cont, 0, 0); // 去除内边距
    lv_obj_set_style_border_width(title_cont, 0, 0); // 去除边框
    lv_obj_set_flex_flow(title_cont, LV_FLEX_FLOW_ROW); // 水平排列
    lv_obj_set_flex_align(title_cont, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    // 标题
    lv_obj_t * lbl_title = lv_label_create(title_cont);
    lv_label_set_text(lbl_title, "Config Wi-Fi");
    
    // 关闭按钮 (右上角的小 X)
    lv_obj_t * btn_close = lv_btn_create(title_cont);
    lv_obj_set_size(btn_close, 30, 30);
    lv_obj_set_style_bg_color(btn_close, lv_palette_main(LV_PALETTE_RED), 0);
    lv_label_set_text(lv_label_create(btn_close), LV_SYMBOL_CLOSE);
    lv_obj_add_event_cb(btn_close, btn_close_event_cb, LV_EVENT_CLICKED, NULL);

    // 4. SSID 输入区域
    lv_obj_t * label_ssid = lv_label_create(wifi_win);
    lv_label_set_text(label_ssid, "SSID:");
    
    ta_ssid = lv_textarea_create(wifi_win);
    lv_textarea_set_one_line(ta_ssid, true);
    lv_obj_set_width(ta_ssid, 180); // 宽度适配
    lv_textarea_set_max_length(ta_ssid, 31);
    lv_obj_add_event_cb(ta_ssid, ta_event_cb, LV_EVENT_ALL, NULL);

    // 5. Password 输入区域
    lv_obj_t * label_pwd = lv_label_create(wifi_win);
    lv_label_set_text(label_pwd, "Password:");
    
    ta_pwd = lv_textarea_create(wifi_win);
    lv_textarea_set_one_line(ta_pwd, true);
    lv_textarea_set_password_mode(ta_pwd, true); 
    lv_obj_set_width(ta_pwd, 180);
    lv_textarea_set_max_length(ta_pwd, 63);
    lv_obj_add_event_cb(ta_pwd, ta_event_cb, LV_EVENT_ALL, NULL);

    // 6. 连接保存按钮
    lv_obj_t * btn_save = lv_btn_create(wifi_win);
    lv_obj_set_width(btn_save, 120);
	lv_obj_set_style_pad_row(wifi_win, 10, 0); 
    
    lv_obj_t * lbl_save = lv_label_create(btn_save);
    lv_label_set_text(lbl_save, "Save & Connect");
    lv_obj_center(lbl_save);
    
    lv_obj_add_event_cb(btn_save, btn_save_event_cb, LV_EVENT_CLICKED, NULL);

    // 7. 创建虚拟键盘 (初始隐藏)
    kb = lv_keyboard_create(lv_scr_act());
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
}

/**
 * @brief  输入框事件回调 (处理键盘弹出和窗口避让)
 */
static void ta_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * ta = lv_event_get_target(e);

    if (code == LV_EVENT_FOCUSED || code == LV_EVENT_CLICKED) 
    {
        // 显示键盘
        if (kb != NULL) {
            lv_keyboard_set_textarea(kb, ta);
            lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
            
            // 键盘弹出时，把窗口往上移，防止被遮挡
            if(wifi_win != NULL) {
                lv_obj_align(wifi_win, LV_ALIGN_TOP_MID, 0, 5);
            }
        }
    }
    else if (code == LV_EVENT_DEFOCUSED || code == LV_EVENT_READY || code == LV_EVENT_CANCEL) 
    {
        // 失焦/点击回车/取消时：隐藏键盘
        if (kb != NULL) {
            lv_keyboard_set_textarea(kb, NULL);
            lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
            
            // 键盘收起后，窗口回到屏幕中间
            if(wifi_win != NULL) {
                lv_obj_center(wifi_win);
            }
        }
    }
}

/**
 * @brief  保存按钮事件回调
 */
static void btn_save_event_cb(lv_event_t * e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) 
    {
        // 1. 获取输入内容
        const char * txt_ssid = lv_textarea_get_text(ta_ssid);
        const char * txt_pwd = lv_textarea_get_text(ta_pwd);

        // 2. 存入全局变量
        memset(g_wifi_ssid, 0, 32);
        memset(g_wifi_pwd, 0, 64);
        strncpy(g_wifi_ssid, txt_ssid, 31);
        strncpy(g_wifi_pwd, txt_pwd, 63);

        // 3. 发送指令到 Modbus 任务
        modbus_pack_t pack;
        pack.dev = DEV_SYSTEM;      
        pack.func = FUNC_SAVE_WIFI; 
        pack.crc = 0;               
        
        // 发送队列
        if(xQueueSend(queuehandle, &pack, 0) == pdPASS) {
             lv_label_set_text(lv_obj_get_child(lv_event_get_target(e), 0), "Sent!");
        } else {
             lv_label_set_text(lv_obj_get_child(lv_event_get_target(e), 0), "Busy!");
        }

        // 收起键盘
        if(kb != NULL) {
            lv_keyboard_set_textarea(kb, NULL);
            lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
            lv_obj_center(wifi_win);
        }
    }
}

/**
 * @brief  关闭按钮事件回调
 */
static void btn_close_event_cb(lv_event_t * e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) 
    {
        // 1. 删除键盘对象
        if (kb != NULL) {
            lv_obj_del(kb);
            kb = NULL;
        }
        
        // 2. 删除窗口对象
        if (wifi_win != NULL) {
            lv_obj_del(wifi_win);
            wifi_win = NULL; // 只有置空，下次才能重新创建
            ta_ssid = NULL;
            ta_pwd = NULL;
        }
    }
}