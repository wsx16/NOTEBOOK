#ifndef __MQTT_H__
#define __MQTT_H__

#include "main.h"
#include "string.h"
#include "usart.h"
#include <stdio.h>
#include <stdbool.h>
#include "app_freertos.h"

/* Include private config if available, otherwise use defaults from config.h */
#if __has_include("config_private.h")
#include "config_private.h"
#endif
#include "config.h"

// 接收缓冲区大小
#define RX_BUF_SIZE 2048

// 函数声明
bool send_AT_Cmd(char *cmd, char *ack, uint32_t timeout);
void CONNECT_WIFI();
void MQTT_init(void);
void publish_message(char *msg);
extern void MQTT_Rx_Handler(uint16_t Size);
// 外部引用串口5接收缓冲区
extern char esp_buffer[RX_BUF_SIZE]; 
extern char wifi_ssid[32];      // 用于存储 SSID 的全局变量
extern char wifi_pwd[64];       // 用于存储 密码 的全局变量
#endif