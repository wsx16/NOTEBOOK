# STM32U575 Multi-Protocol IoT Terminal

A multi-protocol IoT embedded system built on **STM32U575RITx** (Cortex-M33, 80 MHz) featuring Modbus RTU, MQTT over WiFi, fingerprint biometric authentication, touchscreen GUI, and OTA firmware updates with A/B partition rollback.

## Features

- **Modbus RTU Slave** — RS-485 bus communication with CRC16 validation, supporting LED/Buzzer/WiFi/Fingerprint device control
- **MQTT over WiFi** — ESP8266 AT command-based MQTT pub/sub with runtime WiFi configuration via touchscreen
- **Fingerprint Authentication** — Enroll, unlock, and delete operations via dedicated UART to fingerprint module
- **Touchscreen GUI** — LVGL 8.3.11 on ILI9341 240x320 TFT with FT6336 capacitive touch (SPI + I2C)
- **OTA Firmware Update** — A/B dual-partition scheme with MD5 verification, breakpoint resume, and auto-rollback on 3 consecutive boot failures
- **FreeRTOS 11.2.0** — Event-driven architecture with ISR-safe message queues and stream buffers

## Architecture

```
+------------------+       +------------------+       +------------------+
|   Modbus Master  |       |   MQTT Broker    |       |  OneNET Cloud    |
|   (RS-485 Bus)   |       |  (broker.emqx.io)|       |  (OTA Server)    |
+--------+---------+       +--------+---------+       +--------+---------+
         |                          |                          |
     USART1 9600                 UART5                      UART5
         |                          |                          |
+--------+--------+-----------------+--------------------------+---------+
|                          STM32U575RITx                                 |
|                                                                        |
|  +-------------+  +-------------+  +-------------+  +---------------+  |
|  | Modbus Task |  | LVGL Task   |  | Finger Task |  | OTA Module    |  |
|  | (Queue RX)  |  | (5ms tick)  |  | (StreamBuf) |  | (A/B Update)  |  |
|  +------+------+  +------+------+  +------+------+  +-------+-------+  |
|         |                |                |                  |          |
|  +------+----------------+----------------+------------------+-------+  |
|  |                    FreeRTOS 11.2.0 Kernel                         |  |
|  +-------------------------------------------------------------------+  |
|                                                                        |
|  +-------------------------------------------------------------------+  |
|  |                    STM32 HAL / BSP Drivers                        |  |
|  |  USART1 | UART5 | USART3 | SPI1(LCD) | I2C1(Touch) | TIM6(Tick) |  |
|  +-------------------------------------------------------------------+  |
+------------------------------------------------------------------------+
         |                          |
   ILI9341 TFT 240x320      FT6336 Touch Panel
   (SPI1, RGB565)            (I2C1)
```

### UART Peripheral Mapping

| UART   | Baud  | Device            | Protocol                        |
|--------|-------|-------------------|---------------------------------|
| USART1 | 9600  | RS-485 Transceiver| Modbus RTU (Slave)             |
| UART5  | 115200| ESP8266 WiFi      | AT Commands, MQTT, OTA Download|
| USART3 | 57600 | Fingerprint Module| Enroll / Unlock / Delete       |

### Modbus Device Table

| Device ID | Device       | Supported Functions            |
|-----------|-------------|--------------------------------|
| `0x01`    | LED         | ON (`0x01`) / OFF (`0x02`)     |
| `0x02`    | Buzzer      | ON / OFF                       |
| `0x03`    | WiFi/MQTT   | Connect / Disconnect           |
| `0x04`    | Fingerprint | Enroll / Unlock / Delete       |
| `0xFF`    | System      | WiFi Config (`0xAA`) / OTA (`0xBB`) |

### OTA A/B Partition Layout

```
Flash 2MB (0x08000000 - 0x080FFFFF)
+------------------+  0x08000000
|   Bootloader     |  40 KB  (Pages 0-4)
+------------------+  0x0800A000
|   Slot A (App)   |  344 KB (Pages 5-47)
+------------------+  0x08060000
|   Slot B (Backup)|  344 KB (Pages 48-90)
+------------------+  0x080FC000
|   OTA Progress   |  8 KB   (Page 126) — breakpoint resume
+------------------+  0x080FE000
|   Boot Config    |  8 KB   (Page 127) — active slot + fail count
+------------------+
```

## Project Structure

```
STM32U5/
├── Core/
│   ├── Inc/              # Global headers (main.h, config.h, FreeRTOSConfig.h)
│   ├── Src/              # HAL init, UART callbacks, FreeRTOS tasks
│   ├── modbus/           # Modbus RTU protocol (CRC16, frame parsing, ACK)
│   ├── mqtt/             # ESP8266 AT driver, MQTT pub/sub, WiFi management
│   ├── OTA/              # A/B firmware update, MD5, boot config, version
│   ├── LCD/              # ILI9341 SPI driver, FT6336 I2C touch driver
│   └── lvgl/
│       ├── src/core/     # Custom UI logic (WiFi settings popup, etc.)
│       ├── guider/       # NXP GUI Guider generated screens
│       └── examples/     # LVGL display & input porting layer
├── Drivers/              # STM32 HAL & CMSIS
├── Middlewares/           # FreeRTOS kernel, LVGL library
└── MDK-ARM/              # Keil uVision project files
```

## Build

**Prerequisites:** Keil MDK-ARM v5/v6 with ARM Compiler 6 (ARMCLANG)

1. Open `MDK-ARM/STM32U5.uvprojx` in Keil uVision
2. Copy `Core/Inc/config.h` to `Core/Inc/config_private.h` and fill in your WiFi/MQTT/OTA credentials
3. Build target "STM32U5" (Ctrl+F7)
4. Flash via ST-Link or generate HEX/BIN for OTA

> **Note:** The scatter file links the application at `0x0800A000` (Slot A). The bootloader project is maintained separately.

## Configuration

Credentials are defined in `Core/Inc/config.h` with safe defaults. For actual deployment, create `Core/Inc/config_private.h` (git-ignored) to override:

```c
#define WIFI_SSID           "YourSSID"
#define WIFI_PWD            "YourPassword"
#define ONENET_AUTHORIZATION "your_auth_token"
// ... see config.h for all available options
```

## License

This project is provided for educational and portfolio demonstration purposes.
