# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

STM32U575RITx (Cortex-M33) multi-protocol IoT embedded system featuring Modbus RTU, MQTT over WiFi (ESP8266), fingerprint biometric authentication, and a touchscreen GUI. Runs FreeRTOS 11.2.0 with LVGL 8.3.11 for the UI.

## Build System

- **IDE:** Keil MDK-ARM (uVision) v6.24, ARM Compiler 6 (ARMCLANG)
- **Project file:** `MDK-ARM/STM32U5.uvprojx`
- **Target MCU:** STM32U575RITx, 80 MHz system clock
- **Flash:** 2MB (0x08000000), **RAM:** 768KB (0x20000000)
- **Output:** HEX and BIN via post-build `fromelf.exe`
- No Makefile or CMake — build exclusively through Keil uVision

## Architecture

### UART Peripherals & Protocols
| UART   | Baud  | Connected To        | Purpose                        |
|--------|-------|---------------------|--------------------------------|
| USART1 | 9600  | RS-485 bus          | Modbus RTU slave               |
| UART5  | —     | ESP8266 WiFi module | AT commands, MQTT pub/sub      |
| USART3 | —     | Fingerprint module  | Enroll/unlock/delete commands  |

### FreeRTOS Tasks
- `modbus_parse_task` (defaultTask) — dequeues and processes Modbus packets
- `lvgl` task — runs `lv_timer_handler()` every 5ms for UI rendering
- `F0_LogTask` — reads fingerprint module output from stream buffer

### Interrupt-Driven Data Flow
UART RX callbacks in `main.c` (`HAL_UARTEx_RxEventCallback`) route data:
- USART1 → `Modbus_Rx_Handler()` → FreeRTOS queue → `modbus_parse_task`
- UART5 → `MQTT_Rx_Handler()` → processes ESP8266 AT responses
- USART3 → FreeRTOS stream buffer → `F0_LogTask`

### Modbus Device IDs
- 0x01: LED, 0x02: Buzzer, 0x03: WiFi/MQTT, 0x04: Fingerprint, 0xFF: System config

### Display Stack
- ILI9341 240x320 TFT (SPI1) with FT6336 capacitive touch (I2C1)
- LVGL display buffer: 240x10 pixels, 16-bit RGB565
- TIM6 provides 1ms tick via `lv_tick_inc(1)`
- UI generated with NXP GUI Guider (`Core/lvgl/guider/`)

### OTA Firmware Update (A/B 双分区)

**Flash 布局：**
| 区域 | 地址 | 大小 |
|------|------|------|
| Bootloader | `0x08000000` | 40 KB (Pages 0-4) |
| Slot A (App) | `0x0800A000` | 344 KB (Pages 5-47) |
| Slot B (备用) | `0x08060000` | 344 KB (Pages 48-90) |
| OTA 进度 | `0x080FC000` | 8 KB (Page 126) |
| Boot Config | `0x080FE000` | 8 KB (Page 127) |

**Bootloader 工程位置：** `E:\WSX\stm32\bootloader\MDK-ARM\bootloader.uvprojx`
- 链接地址 `0x08000000`，读取 BootConfig 后跳转到活动 Slot
- 连续启动失败 3 次自动回滚

**App 工程关键配置（已修正）：**
- Scatter 文件 `MDK-ARM/STM32U5/STM32U5.sct` 链接起始地址为 `0x0800A000`（344 KB）
- `main.c` 不设置 VTOR，由 Bootloader 跳转时负责
- OTA 触发：向 USART1 发送 Modbus 帧 `FF BB C8 7A`（设备 `0xFF`，功能码 `0xBB`）

**OTA 流程：**
1. Modbus `FF BB` → `OTA_Init()` 阻塞执行
2. ESP8266 TCP 连接 OneNET（`183.230.40.33:80`）
3. 上报版本、查询新版本、分块下载（每块 1024 字节）到 Slot B
4. MD5 校验通过 → 切换 BootConfig → `NVIC_SystemReset()`
5. Bootloader 跳转到 Slot B，App 启动后 `OTA_ConfirmBoot()` 确认

**关键实现细节：**
- `send_AT_Cmd()` 内部自动追加 `\r\n`，AT 命令字符串末尾不能再带 `\r\n`
- `AT+CIPSEND` 透传模式响应为 `>`，不是 `OK`
- `esp_buffer` 大小 2048 字节（`RX_BUF_SIZE`），由 `MQTT_Rx_Handler` 中断驱动填充，OTA 下载时复用
- WiFi 热点硬编码在 `mqtt.c:62`：SSID `Redmi K70`，密码 `200212137`
- OTA 相关文件（`OTA.c`, `version.c`, `md5.c`）已加入 Keil 工程

**Modbus 功能码（`0xFF` 系统设备）：**
- `0xAA`：配置 WiFi（`FUNC_SAVE_WIFI`）
- `0xBB`：触发 OTA 升级（`FUNC_OTA_START`）

## Key File Locations

| Area | Files |
|------|-------|
| Entry point & UART callbacks | `Core/Src/main.c` |
| FreeRTOS tasks & IPC setup | `Core/Src/app_freertos.c` |
| Modbus protocol | `Core/modbus/modbus.c`, `modbus.h` |
| MQTT / WiFi | `Core/mqtt/mqtt.c`, `mqtt.h` |
| LCD driver | `Core/LCD/bsp_ili9341_4line.c` |
| Touch driver | `Core/LCD/bsp_ft6336.c` |
| LVGL display port | `Core/lvgl/examples/porting/lv_port_disp.c` |
| LVGL input port | `Core/lvgl/examples/porting/lv_port_indev.c` |
| LVGL config | `Core/lvgl/lv_conf.h` |
| GUI screens (generated) | `Core/lvgl/guider/src/generated/` |
| Custom GUI code | `Core/lvgl/src/core/gui_wifi.c`, `lvgls.c` |
| FreeRTOS config | `Core/Inc/FreeRTOSConfig.h` |
| OTA / version / MD5 | `Core/OTA/OTA.c`, `version.c`, `md5.c` |
| Bootloader (独立工程) | `E:\WSX\stm32\bootloader\MDK-ARM\bootloader.uvprojx` |
| OTA 操作指南 | `OTA升级演示操作指南.md` |
| Keil project | `MDK-ARM/STM32U5.uvprojx` |

## Conventions

- Commit messages may be in Chinese or English
- HAL peripheral handles are global externs (e.g., `huart1`, `hspi1`, `hi2c1`) declared in `main.h`
- LVGL UI screens are generated code in `guider/src/generated/` — custom logic goes in `Core/lvgl/src/core/`
- Modbus frames use CRC16 (polynomial 0xA001) with little-endian byte order
- MQTT uses ESP8266 AT command set (`AT+MQTTCONN`, `AT+MQTTPUB`, `AT+MQTTSUB`)
