#include "mqtt.h"

char cmd_buffer[128];
char esp_buffer[RX_BUF_SIZE]; // 存放模组回复的数据

/**
 * @brief  带响应检查的AT指令发送函数
 * @param  cmd: 指令内容; ack: 期待的应答字符串; timeout: 等待延时(ms)
 * @return true: 收到预期应答; false: 超时或应答错误
 */
bool send_AT_Cmd(char *cmd, char *ack, uint32_t timeout) {
    // 1. 清空缓冲区
    memset(esp_buffer, 0, RX_BUF_SIZE);
    
    // 2. 发送指令
    HAL_UART_Transmit(&huart5, (uint8_t *)cmd, strlen(cmd), 100);
    HAL_UART_Transmit(&huart5, (uint8_t *)"\r\n", 2, 100);
    
    printf("CMD: %s\r\n", cmd); // 打印发送的指令

    // 3. 记录开始时间
    uint32_t tickStart = HAL_GetTick();

    // 4. 循环等待，直到超时
    while((HAL_GetTick() - tickStart) < timeout) {
        
        // 检查缓冲区是否包含关键词
        if (ack != NULL && strstr(esp_buffer, ack) != NULL) {
            printf("ACK: %s\r\n", esp_buffer); // 打印收到的正确回复
            return true; // 成功找到
        }
        
        // 延时一小会儿，给中断一点时间处理数据，防止死循环占用总线
		vTaskDelay(pdMS_TO_TICKS(50));
    }
    
    printf("Timeout or Error: %s\r\n", esp_buffer);
    return false; // 超时未找到
}


// 解析当前 SSID 的辅助函数
void parse_current_ssid(void) {
    if(send_AT_Cmd("AT+CWJAP?", "+CWJAP:", 2000)) {
        char *start = strchr(esp_buffer, '"');
        if (start) {
            start++;
            char *end = strchr(start, '"');
            if (end) {
                int len = end - start;
                if (len > 31) len = 31;
                memset(wifi_ssid, 0, 32);
                strncpy(wifi_ssid, start, len);
            }
        }
    }
}

void CONNECT_WIFI()
{
    // 2. 确保开启自动连接
    send_AT_Cmd("AT+CWJAP=\"Redmi K70\",\"200212137\"", "WIFI GOT IP", 10000);
    printf("等待 WiFi 自动连接...\r\n");

    bool connected = false;
    for(int i = 0; i < 10; i++) 
    {
        // 这里的延时用于给路由器分配 IP 的时间
        vTaskDelay(pdMS_TO_TICKS(1000)); 
        
        // 发送查询 IP 指令
        if(send_AT_Cmd("AT+CIFSR", "STAIP", 1000)) 
        {
            // 【关键修改】检查是否是无效 IP (0.0.0.0)
            if(strstr(esp_buffer, "0.0.0.0") != NULL) {
                printf("正在获取 IP (当前 0.0.0.0) - %d/10\r\n", i+1);
                continue; // 跳过本次循环，继续等待
            }

            // 获取到了有效 IP
            connected = true;
            printf("检测到有效 IP，WiFi 连接成功！\r\n");
            
            // 同步当前连接的 SSID 到全局变量，以便 UI 显示
            parse_current_ssid();
            
            break; // 跳出循环，进行下一步
        }
        else 
        {
            printf("正在连接 WiFi (%d/10)...\r\n", i+1);
        }
    }
}

void MQTT_init(void) {
    printf("--- ESP8266 初始化 (自动连接模式) ---\r\n");

    // 1. 复位 & 基础配置
    send_AT_Cmd("AT+RST", "ready", 3000);
    send_AT_Cmd("ATE0", "OK", 500);
    send_AT_Cmd("AT+CWMODE=1", "OK", 1000); 
    
    // 2. 确保开启自动连接
    send_AT_Cmd("AT+CWAUTOCONN=1", "OK", 1000);

    printf("等待 WiFi 自动连接...\r\n");

    bool connected = false;
    for(int i = 0; i < 10; i++) 
    {
        // 这里的延时用于给路由器分配 IP 的时间
        vTaskDelay(pdMS_TO_TICKS(1000)); 
        
        // 发送查询 IP 指令
        if(send_AT_Cmd("AT+CIFSR", "STAIP", 1000)) 
        {
            // 【关键修改】检查是否是无效 IP (0.0.0.0)
            if(strstr(esp_buffer, "0.0.0.0") != NULL) {
                printf("正在获取 IP (当前 0.0.0.0) - %d/10\r\n", i+1);
                continue; // 跳过本次循环，继续等待
            }

            // 获取到了有效 IP
            connected = true;
            printf("检测到有效 IP，WiFi 连接成功！\r\n");
            
            // 同步当前连接的 SSID 到全局变量
            parse_current_ssid();
            
            break; // 跳出循环，进行下一步
        }
        else 
        {
            printf("正在连接 WiFi (%d/10)...\r\n", i+1);
        }
    }
    
    // 3. 根据连接结果进行后续操作
    if (connected) 
    {
        printf("开始连接 MQTT...\r\n");
        
        // 4. 配置 MQTT 用户信息
        sprintf(cmd_buffer, "AT+MQTTUSERCFG=0,1,\"%s\",\"%s\",\"%s\",0,0,\"\"", 
                MQTTSERVER_CLIENT_ID, MQTTSERVER_USER, MQTTSERVER_PASSWORD);
        send_AT_Cmd(cmd_buffer, "OK", 1000);

        // 5. 连接 Broker
        sprintf(cmd_buffer, "AT+MQTTCONN=0,\"%s\",%d,1", MQTTSERVER_IP, MQTTSERVER_PORT);
        if(send_AT_Cmd(cmd_buffer, "OK", 5000)) { // 连接服务器可能慢，给 5秒
            printf("MQTT 连接成功！\r\n");
            
            // 6. 订阅主题
            sprintf(cmd_buffer, "AT+MQTTSUB=0,\"%s\",1", MQTTSERVER_TOPIC);
            send_AT_Cmd(cmd_buffer, "OK", 2000);
        } else {
            printf("MQTT 连接失败 (可能服务器不可达)\r\n");
        } 
    } else {
        // --- 没连上网的情况 ---
        printf("WiFi 未连接 (可能是新环境或密码错误)。\r\n");
        printf("请点击屏幕上的 [Wi-Fi Settings] 进行配置。\r\n");
        // 此时不执行 MQTT 连接，直接结束函数
    }
}


void MQTT_Rx_Handler(uint16_t Size)
{
    // 确保字符串以 0 结尾，方便 strstr 函数查找
    if (Size < RX_BUF_SIZE) {
        esp_buffer[Size] = '\0'; 
    } else {
        esp_buffer[RX_BUF_SIZE - 1] = '\0';
    }

    if (strstr(esp_buffer, "+MQTTSUBRECV") != NULL) {
        printf("MQTT RX: %s\r\n", esp_buffer);
    }

    // 重新开启接收，指向缓冲区头部（依然是覆盖模式，但配合上面的轮询函数已经够用了）
    HAL_UARTEx_ReceiveToIdle_IT(&huart5, (uint8_t *)esp_buffer, RX_BUF_SIZE);
}

/**
 * @brief  发布消息到指定主题
 */
void publish_message(char *msg) {
    sprintf(cmd_buffer, "AT+MQTTPUB=0,\"%s\",\"%s\",1,0", MQTTSERVER_TOPIC, msg);
    send_AT_Cmd(cmd_buffer, "OK", 500);
}

void close_mqtt(){
		sprintf(cmd_buffer, "AT+MQTTCLEAN=0");
		send_AT_Cmd(cmd_buffer, "OK", 1000);
}

