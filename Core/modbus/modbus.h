#ifndef __MODBUS_H__
#define __MODBUS_H__

#include "usart.h"
#include "app_freertos.h"
#include "queue.h"
#include <string.h>
#include "mqtt.h"

#define MODBUS_CRC_LEN 2
#define MODBUS_MAX_PACK_LEN 128
#define MODBUS_DEV 0
#define MODBUS_FUNC 1
#define MODBUS_CRC 2

// --- 设备号定义 ---
#define LED    0x01
#define BEEP   0x02
#define WIFI   0x03
#define FINGER 0x04
#define DEV_SYSTEM      0xFF  



// --- 功能码定义 ---
#define ON 0x01
#define OFF 0x02
#define FINGER_CMD_ENROLL  '1' // ASCII '1'
#define FINGER_CMD_UNLOCK  '2' // ASCII '2'
#define FINGER_CMD_DELETE  '3' // ASCII '3'
#define FUNC_SAVE_WIFI  0xAA  

typedef struct
{
	uint8_t dev;
	uint8_t func;
	uint16_t crc;
	uint8_t  arg0;
}__attribute__((packed))modbus_pack_t;


extern uint8_t buffer[MODBUS_MAX_PACK_LEN];
extern uint16_t uart_rx_len;
extern QueueHandle_t queuehandle;

extern void Modbus_Rx_Handler(uint16_t Size);
extern void peripheral_ctrl(const modbus_pack_t *p);
extern void modbus_ack_send(uint8_t dev, uint8_t func, uint8_t ERR);


void close_mqtt();

typedef enum
{
    MODBUS_OK,   
    MODBUS_UNKNOW_DEV,
    MODBUS_FUNC_ERR,
    MODBUS_CRC_ERR
}MODBUS_ERR;


#endif