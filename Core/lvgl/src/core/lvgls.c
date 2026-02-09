#include "lvgls.h"


lv_obj_t * ui_main_screen;   // 主屏幕
lv_obj_t * ui_finger_screen; // 指纹功能屏幕


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
    static int flag = 1;
    if(lv_event_get_code(e) == LV_EVENT_CLICKED) {
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

// 4. 点击“设置”按钮的回调函数
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
static lv_obj_t * kb_del;   

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

    /* 监听 focus/defocus */
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
        /* 按钮消失 */
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

    // 发送后回到指纹主界面
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
    lv_obj_set_pos(btn_finger_menu, 10, 250); 
    lv_obj_set_size(btn_finger_menu, 150, 50);
    lv_obj_t * label_menu = lv_label_create(btn_finger_menu);
    lv_label_set_text(label_menu, "Fingerprint");
    lv_obj_align(label_menu, LV_ALIGN_CENTER, 0, 0);

    // 初始化指纹子屏幕
    setup_finger_screen();

    // 绑定跳转事件
    lv_obj_add_event_cb(btn_finger_menu, screen_switch_event_cb, LV_EVENT_CLICKED, ui_finger_screen);

}
