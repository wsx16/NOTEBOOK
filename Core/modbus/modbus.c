#include "modbus.h"
#include "OTA.h"

// 定义串口接收缓冲区，大小由 MODBUS_MAX_PACK_LEN 宏决定
uint8_t buffer[MODBUS_MAX_PACK_LEN];
// 记录当前接收到的数据总长度
uint16_t uart_rx_len;
extern char wifi_ssid[32];
extern char wifi_pwd[64];
extern char cmd_buffer[128];
extern uint8_t f0_rx_buffer[512]; 
/**
 * @brief  Modbus CRC16 校验计算函数
 * @param  data:   指向要计算的数据缓冲区的指针
 * @param  length: 数据长度
 * @return 计算出的 16 位 CRC 校验码
 */
uint16_t CRC16_Modbus(uint8_t *data, uint16_t length)
{
    uint16_t crc = 0xFFFF; // 1. CRC 寄存器初始值通常是全 1
    
    for (int i = 0; i < length; i++) {
        crc ^= data[i];     // 2. 把数据字节放入 CRC 寄存器低位进行异或
        
        for (int j = 0; j < 8; j++) { // 3. 循环处理这 1 个字节的 8 个 bit
            if (crc & 0x0001) { // 如果最低位是 1 (相当于移出的位是 1)
                crc = (crc >> 1) ^ 0xA001; // 右移并在高位异或 Modbus 多项式 0xA001
            } else {
                crc = (crc >> 1);          // 如果最低位是 0，仅仅右移
            }
        }
    }
    return crc; // 4. 返回算出来的 2 字节校验码
}

#if 0


void Modbus_Rx_Handler(uint16_t Size)
{

    // 累加接收数据长度
    if ((uart_rx_len + Size) > MODBUS_MAX_PACK_LEN) {
        // 缓冲区将溢出，重新开始
        uart_rx_len = 0;
        goto rx_restart;
    }
    uart_rx_len += Size;

    // 检查最小帧头：至少5字节(HEAD+DEV+FUNC+LEN_L+LEN_H)
    if (uart_rx_len < 5) {
        goto rx_restart;
    }
    
    // 获取帧头和长度字段
    uint8_t head = buffer[0];
    uint16_t data_len = (uint16_t)buffer[3] | ((uint16_t)buffer[4] << 8);
    
    // 检查帧头
    if (head != FIX_TX_HEAD) {
        // 帧头错误，重新开始
        uart_rx_len = 0;
        goto rx_restart;
    }

    // 计算整帧长度：1(头) + 1(设备) + 1(功能码) + 2(长度) + len(数据) + 2(CRC)
    uint16_t expected_len = 5 + data_len + MODBUS_CRC_LEN;

    // 检查长度是否合理
    if (expected_len > MODBUS_MAX_PACK_LEN) {
        uart_rx_len = 0;
        goto rx_restart;
    }

    // 检查是否收到完整帧
    if (uart_rx_len < expected_len) {
        // 尚未收完，继续等待
        goto rx_restart;
    }

    // 接收到完整帧
    if (uart_rx_len == expected_len) {
        // 创建接收缓冲结构，发送到队列
        modbus_rx_buffer_t rx_data = {0};
        rx_data.len = uart_rx_len;
        memcpy(rx_data.buffer, buffer, uart_rx_len);
        
        // 通过xQueueSendFromISR将数据发送到队列
        if (modbus_queue != NULL) {
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            xQueueSendFromISR(modbus_queue, &rx_data, &xHigherPriorityTaskWoken);
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }

        // 重置接收缓冲区
        uart_rx_len = 0;
    } else {
        // 多收了数据（粘包）
        uart_rx_len = 0;
    }

rx_restart:
    // 重新启动UART接收
    HAL_UARTEx_ReceiveToIdle_IT(&huart1, buffer + uart_rx_len,
                               MODBUS_MAX_PACK_LEN - uart_rx_len);
}

#endif


void Modbus_Rx_Handler(uint16_t Size)
{
    BaseType_t hpw = pdFALSE;
    modbus_pack_t pack = {0};

    /* 1) 累加长度 */
    if ((uart_rx_len + Size) > MODBUS_MAX_PACK_LEN) {
        uart_rx_len = 0;
        goto rx_restart;
    }
    uart_rx_len += Size;

    /* 2) 只接受 4 或 5 字节 */
    if (uart_rx_len < 4) {
        goto rx_restart;
    }
    if (uart_rx_len != 4 && uart_rx_len != 5) {
        /* 长度不合法：丢弃并重启 */
        uart_rx_len = 0;
        goto rx_restart;
    }

    /* 3) CRC 校验：最后两字节为 CRC Lo/Hi */
    uint16_t calc_crc = CRC16_Modbus(buffer, uart_rx_len - 2);
    uint16_t recv_crc = ((uint16_t)buffer[uart_rx_len - 1] << 8) | buffer[uart_rx_len - 2];
    if (calc_crc != recv_crc) {
        uart_rx_len = 0;
        goto rx_restart;
    }

    /* 4) 组包 */
    pack.dev  = buffer[0];
    pack.func = buffer[1];
    pack.crc  = recv_crc;
    pack.arg0 = (uart_rx_len == 5) ? buffer[2] : 0;

    xQueueSendFromISR(queuehandle, &pack, &hpw);
    portYIELD_FROM_ISR(hpw);

    /* 5) 一帧处理完，准备收下一帧 */
    uart_rx_len = 0;

rx_restart:
    HAL_UARTEx_ReceiveToIdle_IT(&huart1, buffer + uart_rx_len,
                               MODBUS_MAX_PACK_LEN - uart_rx_len);
}

/**
 * @brief  外设控制逻辑处理函数
 *         根据解析出的设备号和功能码执行具体动作
 * @param  dev:  设备地址/ID (如 LED, BEEP)
 * @param  func: 功能指令 (如 ON, OFF)
 */
void peripheral_ctrl(const modbus_pack_t *p)
{
	uint8_t dev  = p->dev;
  uint8_t func = p->func;
	switch(dev)
	{
		case LED: // 处理 LED 设备
			if (func == ON) {
					HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);   // 点亮LED
					modbus_ack_send(dev, func, MODBUS_OK);                 // 发送成功应答
			} else if (func == OFF) {
					HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET); // 熄灭LED
					modbus_ack_send(dev, func, MODBUS_OK);                 // 发送成功应答
			} else {
					modbus_ack_send(dev, func, MODBUS_FUNC_ERR);           // 功能码错误应答
			}
			break;   
		case BEEP: // 处理 蜂鸣器 设备
			if (func == ON) {
					HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_SET);   // 蜂鸣器响
					modbus_ack_send(dev, func, MODBUS_OK);                 // 发送成功应答
			} else if (func == OFF) {
					HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_RESET); // 蜂鸣器停
					modbus_ack_send(dev, func, MODBUS_OK);                 // 发送成功应答
			} else {
					modbus_ack_send(dev, func, MODBUS_FUNC_ERR);           // 功能码错误应答
			}
			break;
		case WIFI: // 处理 wifi 设备
			if (func == ON) {
					MQTT_init();                                              // 连接wifi
					modbus_ack_send(dev, func, MODBUS_OK);                 // 发送成功应答
			} else if (func == OFF) {
					close_mqtt();                                             // 关闭wifi
					modbus_ack_send(dev, func, MODBUS_OK);                 // 发送成功应答
			} else {
					modbus_ack_send(dev, func, MODBUS_FUNC_ERR);           // 功能码错误应答
			}
			break;
		case FINGER: // 处理指纹模块
			{
				// U5 -> F0：USART3
				if (func == FINGER_CMD_DELETE) {
						uint8_t id = p->arg0;

						if (id < '0' || id > '9') {
								printf("[U5] Delete ignored: invalid id=0x%02X\r\n", id);
								break;
						}
						uint8_t cmd = FINGER_CMD_DELETE; // '3'
						HAL_UART_Transmit(&huart3, &cmd, 1, 100);
						HAL_UART_Transmit(&huart3, &id,  1, 100);
				} else {
						// enroll/unlock 仍是一字节命令
						uint8_t cmd = func;              // '1' / '2'
						HAL_UART_Transmit(&huart3, &cmd, 1, 100);
				}
				break;
			}
		case DEV_SYSTEM:
			if (func == FUNC_OTA_START)
			{
				printf("[OTA] Triggered by Modbus FF BB\r\n");
				modbus_ack_send(dev, func, MODBUS_OK);
				HAL_Delay(100);  /* 等待应答发出 */
				OTA_Init();      /* 阻塞执行, 成功后 NVIC_SystemReset() */
			}
			else if (func == FUNC_SAVE_WIFI)
			{
				printf("[System] Start configuring WiFi...\r\n");
				printf("SSID: %s, PWD: %s\r\n", wifi_ssid, wifi_pwd);
				// 1. 断开现有连接
				close_mqtt(); 

				// 2. AT+CWJAP 指令 (连接并保存到 Flash)
				sprintf(cmd_buffer, "AT+CWJAP=\"%s\",\"%s\"", wifi_ssid, wifi_pwd);

				// 3. 发送指令 (给予 15秒 超时，因为连接路由很慢)
				if(send_AT_Cmd(cmd_buffer, "WIFI GOT IP", 15000)) 
				{
						printf("[System] WiFi Connected & Saved!\r\n");
						
						// 4. 连接成功，重新初始化 MQTT
						MQTT_init(); 
						
						// 5. 发送成功应答
						modbus_ack_send(dev, func, MODBUS_OK);
				}
				else 
				{
						printf("[System] WiFi Connection Failed!\r\n");
						// 发送失败应答
						modbus_ack_send(dev, func, MODBUS_FUNC_ERR);
				}
			}
		return; // 处理完系统指令直接返回
	}
}

/**
 * @brief  Modbus 应答发送函数
 *         构建并发送包含 CRC 的响应帧
 * @param  dev:  设备地址
 * @param  func: 功能码
 * @param  ERR:  错误码或状态码 (MODBUS_OK 或 MODBUS_FUNC_ERR)
 */
void modbus_ack_send(uint8_t dev, uint8_t func, uint8_t ERR)
{
	uint8_t ack_buffer[5]; // 响应帧长度固定为 5 字节
	uint16_t crc;
	
    // 填充数据部分
	ack_buffer[0] = dev;
	ack_buffer[1] = func;
	ack_buffer[2] = ERR;
    
    // 计算前 3 个字节的 CRC
	crc = CRC16_Modbus(ack_buffer, 3);
    
    // 填充 CRC (小端模式：低位在前) 
	ack_buffer[3] = crc & 0xff;      // CRC 低 8 位
	ack_buffer[4] = crc >> 8 & 0xff; // CRC 高 8 位
	printf("ack_buffer:");
	for (int i = 0; i < 5; i++)
	{
		printf("%#x ",ack_buffer[i]);
	}
	putchar(10);

    // 发送响应帧到 RS-485 总线
	HAL_UART_Transmit(&huart1, ack_buffer, 5, 10);
}

