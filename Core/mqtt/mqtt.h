#ifndef __MQTT_H__
#define __MQTT_H__

#include "main.h"
#include "string.h"
#include "usart.h"
#include <stdio.h>
#include <stdbool.h>
#include "app_freertos.h"

// WiFi连接信息
#define WIFI_SSID     "Hqyj"
#define WIFI_PWD      "12345678"

// MQTT服务器信息 

#define MQTTSERVER_IP        "broker.emqx.io"
#define MQTTSERVER_PORT      1883
#define MQTTSERVER_CLIENT_ID "STM32U575_Client"
#define MQTTSERVER_USER      "admin"
#define MQTTSERVER_PASSWORD  "public"
#define MQTTSERVER_TOPIC     "topic"

// 接收缓冲区大小
#define RX_BUF_SIZE 512

// 函数声明
bool send_AT_Cmd(char *cmd, char *ack, uint32_t timeout);
void MQTT_init(void);
void publish_message(char *msg);
extern void MQTT_Rx_Handler(uint16_t Size);
// 外部引用串口5接收缓冲区
extern char esp_rx_buffer[RX_BUF_SIZE]; 
extern char g_wifi_ssid[32];      // 用于存储 SSID 的全局变量
extern char g_wifi_pwd[64];       // 用于存储 密码 的全局变量
#endif