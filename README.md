# STM32U5 物联网智能终端

[![STM32](https://img.shields.io/badge/STM32U575-Cortex--M33-blue)](https://www.st.com/en/microcontrollers-microprocessors/stm32u575-585.html)
[![FreeRTOS](https://img.shields.io/badge/FreeRTOS-10.4.6-green)](https://www.freertos.org/)
[![LVGL](https://img.shields.io/badge/LVGL-8.3.11-orange)](https://lvgl.io/)

一个基于 **STM32U575RITx** 的多协议物联网终端系统，集成了 Modbus RTU 工业通信、MQTT 云端通信、指纹识别和触摸屏 GUI。采用 FreeRTOS 实时操作系统，实现了本地设备控制与远程物联网通信的统一管理。

## 🚀 核心功能

### 🔌 多协议通信
- **Modbus RTU**：通过 RS-485 串口实现工业设备控制
- **MQTT over WiFi**：基于 ESP8266 的云端物联网通信
- **指纹识别**：集成指纹录入、比对、删除功能

### 📱 触摸屏界面
- 基于 LVGL 图形库的 2.8 寸 TFT 显示屏
- 电容触摸交互，支持多屏切换
- 动态 WiFi 配置弹窗（带虚拟键盘）

### ⚡ 实时控制
- LED 指示灯控制
- 蜂鸣器开关
- 外设状态监控

### 🔄 OTA 升级
- 支持远程固件升级
- MD5 校验保证固件完整性
- 双分区升级方案

## 🏗️ 系统架构

```
┌─────────────────┐
│   应用层 (LVGL) │ ← 触摸屏 GUI
├─────────────────┤
│ 通信层 (UART)   │ ← Modbus/MQTT/指纹
├─────────────────┤
│ 系统层 (RTOS)   │ ← FreeRTOS 任务调度
└─────────────────┘
```

### 任务设计
- **modbus_parse_task** (Normal)：统一指令解析与外设控制
- **F0_LogTask** (BelowNormal)：指纹模块日志处理
- **lvgl_task** (Low)：GUI 界面刷新

### 核心设计理念
**事件驱动 + 消息队列解耦**：所有控制指令统一抽象为 `modbus_pack_t` 结构体，通过 FreeRTOS 消息队列实现 UI、通信、控制三层解耦。

## 🛠️ 技术栈

### 硬件平台
- **MCU**: STM32U575RITx (Cortex-M33, 160MHz)
- **显示**: ILI9341 SPI TFT (240x320)
- **触摸**: FT6336 I2C 电容触摸
- **WiFi**: ESP8266 (AT 指令控制)
- **指纹**: 外挂 STM32F0 指纹模块

### 软件框架
- **RTOS**: FreeRTOS v10.4.6
- **GUI**: LVGL v8.3.11
- **通信协议**: Modbus RTU, MQTT, 自定义指纹协议
- **开发工具**: STM32CubeMX, Keil MDK

## 📁 项目结构

```
STM32/
├── Core/                    # STM32 HAL 驱动
├── Middlewares/            # FreeRTOS & LVGL 中间件
├── Drivers/                # 外设驱动 (LCD, TP, 指纹)
├── Application/            # 应用层代码
│   ├── modbus/            # Modbus 协议实现
│   ├── mqtt/              # MQTT 通信
│   ├── lvgl_ui/           # GUI 界面
│   └── fingerprint/       # 指纹识别
├── assets/                # 文档资源
└── README.md              # 项目文档
```

## 🚀 快速开始

### 环境搭建
1. **开发环境**: Keil MDK v5.36 + STM32CubeMX v6.8.0
2. **固件包**: STM32U5 最新 HAL 库
3. **工具链**: ARM Compiler 6

### 编译运行
```bash
# 克隆项目
git clone <repository-url>
cd STM32

# 使用 Keil 打开项目文件
# STM32_U5_Project.uvprojx

# 编译并下载到开发板
```

### 硬件连接
- **USART1**: RS-485 (Modbus RTU)
- **UART5**: ESP8266 (MQTT)
- **USART3**: 指纹模块
- **SPI1**: ILI9341 显示屏
- **I2C1**: FT6336 触摸屏

## 💡 技术亮点

### 1. 统一指令模型
```c
typedef struct {
    uint8_t device_id;      // 设备号
    uint8_t function_code;  // 功能码
    uint8_t param;          // 参数
    uint16_t crc;           // CRC校验
} modbus_pack_t;
```
所有控制来源（串口/GUI/云端）统一为 Modbus 指令包，通过消息队列解耦。

### 2. 中断 + RTOS 协作
- UART 空闲中断完成帧接收
- ISR 中仅做数据入队操作
- 业务逻辑在任务上下文执行

### 3. 低功耗设计
- STM32U5 超低功耗特性
- FreeRTOS Tickless 模式
- 智能休眠唤醒机制

### 4. 云端集成
- MQTT Broker: `broker.emqx.io:1883`
- 支持设备远程控制和状态上报
- WiFi 配置持久化存储

## 📊 性能指标

- **响应时间**: Modbus 指令 < 10ms
- **GUI 刷新率**: 200Hz (5ms/帧)
- **内存占用**: RAM 64KB, Flash 512KB
- **功耗**: 运行状态 < 100mA, 休眠 < 10uA

## 🔧 开发与调试

### 测试工具
- **串口调试**: Modbus Poll (RTU 模式)
- **MQTT 测试**: MQTT.fx
- **示波器**: 验证 UART/SPI/I2C 时序

### 调试接口
- SWD 接口支持实时调试
- UART1 输出系统日志
- LED 指示运行状态

## 📚 相关文档

- [STM32U5 项目说明文档](STM32U5_项目说明文档.md)
- [驱动开发笔记](驱动开发.md)
- [OTA 升级指南](OTA升级演示操作指南.md)
- [面试问答准备](STM32U5_面试问答准备.md)

## 🤝 贡献

欢迎提交 Issue 和 Pull Request！

## 📄 许可证

本项目仅用于学习和面试展示。

## 📞 联系方式

如有问题请通过 GitHub Issue 联系。

---

**⭐ 如果这个项目对你有帮助，请给个 Star！**
