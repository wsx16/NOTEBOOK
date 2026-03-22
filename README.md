# 📚 STM32 开发学习笔记

[![STM32](https://img.shields.io/badge/STM32U575-Cortex--M33-blue)](https://www.st.com/en/microcontrollers-microprocessors/stm32u575-585.html)
[![FreeRTOS](https://img.shields.io/badge/FreeRTOS-10.4.6-green)](https://www.freertos.org/)
[![LVGL](https://img.shields.io/badge/LVGL-8.3.11-orange)](https://lvgl.io/)

STM32 系列 MCU 开发学习笔记，记录开发经验和项目实践。

## 📖 笔记目录

### 🔧 环境搭建
- Keil + STM32CubeMX 配置
- 开发工具链安装
- SWD/JTAG 调试接口

### 🚀 FreeRTOS 应用
- FreeRTOS 在 STM32U5 上的移植
- 多任务调度设计
- 消息队列与信号量
- 中断与任务协作

### 📡 通信协议
- **Modbus RTU**: RS-485 工业通信
- **MQTT over WiFi**: ESP8266 AT 指令
- **指纹模块**: 自定义串口协议
- UART 空闲中断优化

### 🖥️ 图形界面
- LVGL 图形库移植
- ILI9341 SPI 显示屏驱动
- FT6336 I2C 触摸屏驱动
- 多屏界面设计

### 🔄 OTA 升级
- IAP 固件升级机制
- Flash 分区管理
- MD5 校验实现

## 📋 学习进度

### 已完成 ✅
- [x] FreeRTOS 移植到 U5 板
- [x] Modbus RTU 协议实现
- [x] ESP8266 MQTT 通信
- [x] 指纹模块集成
- [x] LCD/触摸屏驱动
- [x] LVGL 图形界面
- [x] WiFi 配置界面
- [x] OTA 升级框架

### 进行中 🔄
- [ ] PCB 设计学习
- [ ] 万用表功能扩展

## 🛠️ 开发工具

### 硬件平台
- **MCU**: STM32U575RITx (160MHz Cortex-M33)
- **显示**: ILI9341 2.8寸 SPI TFT (240x320)
- **触摸**: FT6336 I2C 电容触摸
- **WiFi**: ESP8266 模块
- **指纹**: STM32F0 指纹控制板

### 软件工具
- **IDE**: Keil MDK v5.36
- **配置**: STM32CubeMX v6.8.0
- **调试**: ST-Link V2/V3
- **串口**: PuTTY, Modbus Poll

## 📚 核心文档

| 文档 | 描述 |
|------|------|
| [项目说明文档](STM32U5_项目说明文档.md) | 架构设计与实现思路 |
| [驱动开发笔记](驱动开发.md) | 外设驱动开发经验 |
| [OTA 升级指南](OTA升级演示操作指南.md) | 固件升级实现 |
| [Linux 系统移植](嵌入式Linux系统移植实战指南.md) | 系统移植指南 |
| [面试问答](STM32U5_面试问答准备.md) | 技术要点总结 |

## 💡 技术要点

### 架构设计
- **统一指令模型**: 所有控制抽象为 Modbus 指令包
- **消息队列解耦**: UI/通信/控制三层分离
- **事件驱动**: 中断 + RTOS 协作处理

### 开发经验
- UART 空闲中断优化接收
- FreeRTOS 任务优先级设计
- LVGL 界面开发最佳实践

## 🔍 调试技巧

### 常用工具
- **示波器**: UART/SPI/I2C 时序分析
- **串口调试**: 实时日志输出
- **LED 指示**: 运行状态监控

### 问题排查
- 通信协议调试方法
- 中断服务函数优化
- 内存管理问题解决

## 📝 代码示例

### UART 空闲中断接收
```c
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size) {
    if (huart->Instance == USART1) {
        modbus_frame_received = 1;
        HAL_UARTEx_ReceiveToIdle_IT(&huart1, rx_buffer, RX_BUFFER_SIZE);
    }
}
```

### FreeRTOS 消息队列
```c
xQueue = xQueueCreate(QUEUE_LENGTH, sizeof(modbus_pack_t));
xQueueSend(xQueue, &pack, portMAX_DELAY);
xQueueReceive(xQueue, &pack, portMAX_DELAY);
```

## 🚀 运行环境

### 硬件连接
- **USART1**: RS-485 (Modbus RTU)
- **UART5**: ESP8266 (MQTT)
- **USART3**: 指纹模块
- **SPI1**: ILI9341 显示屏
- **I2C1**: FT6336 触摸屏

### 编译配置
- 优化等级: -O2
- 堆栈大小: 4KB (任务), 1KB (ISR)

## 📊 性能指标

- **响应时间**: Modbus 指令 < 10ms
- **GUI 刷新**: 200Hz (5ms/帧)
- **内存占用**: RAM 64KB, Flash 512KB
- **功耗**: 运行 < 100mA, 休眠 < 10uA

## 🔗 相关资源

- [STM32 官方文档](https://www.st.com/en/microcontrollers-microprocessors/stm32u575-585.html)
- [FreeRTOS 文档](https://www.freertos.org/Documentation/RTOS_book.html)
- [LVGL 文档](https://docs.lvgl.io/)

---

**📝 记录学习过程，积累开发经验**
