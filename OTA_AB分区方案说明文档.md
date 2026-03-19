# STM32U575 OTA A/B 分区方案说明文档

## 一、方案概述

本项目采用 **A/B 双分区 OTA** 方案，由两个独立的 Keil 工程协作完成：

| 工程 | 路径 | Flash 范围 | 职责 |
|------|------|-----------|------|
| Bootloader | `E:\WSX\stm32\bootloader` | `0x08000000 - 0x0800A000` (40KB) | 读取启动配置，决定跳转 A 或 B 分区 |
| App (STM32U5) | `E:\WSX\stm32\STM32U5` | `0x0800A000` 起 (344KB/Slot) | 运行业务逻辑，执行 OTA 下载与升级 |

---

## 二、Flash 分区布局

```
STM32U575RITx  2MB Flash (Bank 1: 0x08000000 - 0x080FFFFF)
每页 8KB，共 128 页

地址              页号        用途
─────────────────────────────────────────────
0x08000000       Page 0-4    Bootloader (40KB, 固定不更新)
0x0800A000       Page 5-47   Slot A - App 分区 A (344KB)
0x08060000       Page 48-90  Slot B - App 分区 B (344KB)
0x080B6000       Page 91-126 预留空间
0x080FE000       Page 127    Boot Config (启动配置, 16字节)
0x08100000       ---         Bank 1 结束
```

### Boot Config 结构 (16 字节 = 1 个 QuadWord)

```c
typedef struct {
    uint32_t active_slot;       // 当前活动分区: 0=Slot A, 1=Slot B
    uint32_t update_pending;    // 升级待确认:   0=已确认, 1=待确认
    uint32_t boot_fail_count;   // 连续启动失败次数 (用于自动回滚)
    uint32_t reserved;          // 保留
} BootConfig_t;
```

存储在 Flash Page 127 (`0x080FE000`)，Bootloader 和 App **共享读写**此区域。

---

## 三、完整工作流程

### 3.1 正常启动流程 (无 OTA)

```
上电 / 复位
    │
    ▼
┌── Bootloader ──────────────────────────┐
│  1. HAL_Init, 时钟配置, UART 初始化     │
│  2. 读取 Boot Config                    │
│     → active_slot=0, pending=0          │
│  3. 跳转到 Slot A (0x0800A000)          │
└────────────┬───────────────────────────┘
             │
             ▼
┌── App ─────────────────────────────────┐
│  1. OTA_ConfirmBoot()                   │
│     → pending=0, 无需操作               │
│  2. 正常运行 (Modbus, LVGL, MQTT...)    │
└─────────────────────────────────────────┘
```

### 3.2 OTA 升级流程

```
App 运行中触发 OTA_Init()
    │
    ▼
┌── App: OTA_Init() ─────────────────────────────────┐
│  1. 连接 WiFi → TCP 连接 OneNET OTA 服务器           │
│  2. POST 上报当前版本号                               │
│  3. GET 查询新版本 → 解析 tid, size, md5             │
│  4. 判断当前 active_slot=0(A)                        │
│     → 目标写入: Slot B (0x08060000)   ← 非活动分区    │
│  5. 分块下载固件 (1024字节/块, HTTP Range)            │
│     → 逐块写入 Slot B 的 Flash                       │
│     → 当前运行的 Slot A 完全不受影响                   │
│  6. 下载完成 → MD5 校验                               │
│     → 失败: 打印错误, 直接返回, 不切换                 │
│     → 成功: 继续下一步                                │
│  7. BootConfig_SwitchSlot():                         │
│     → active_slot = 1 (切到 B)                       │
│     → update_pending = 1 (标记待确认)                 │
│  8. NVIC_SystemReset() → 重启                        │
└──────────────────────┬─────────────────────────────┘
                       │
                       ▼
┌── Bootloader ──────────────────────────────────────┐
│  1. 读取 Boot Config                                │
│     → active_slot=1, pending=1, fail_count=0        │
│  2. pending=1 且 fail_count < 3                     │
│     → fail_count++ (变为1), 写回 Flash               │
│  3. 跳转到 Slot B (0x08060000) ← 新固件              │
└──────────────────────┬─────────────────────────────┘
                       │
                       ▼
┌── App (新固件) ────────────────────────────────────┐
│  1. OTA_ConfirmBoot()                               │
│     → pending=1, 说明是新固件首次启动                 │
│     → 清除 pending=0, 归零 fail_count=0              │
│     → 写回 Flash                                    │
│     → 至此, 新固件正式"转正"                          │
│  2. 正常运行                                        │
└─────────────────────────────────────────────────────┘
```

### 3.3 自动回滚流程 (新固件有 Bug)

```
Bootloader 跳转到新固件 (Slot B)
    │
    ▼
新固件启动失败 (崩溃 / HardFault / 死循环)
    │
    ▼  看门狗超时
系统复位
    │
    ▼
┌── Bootloader (第2次) ──────────────────────────────┐
│  读取 Boot Config                                   │
│  → active_slot=1, pending=1, fail_count=1           │
│  → fail_count < 3, 继续尝试                         │
│  → fail_count++ (变为2), 写回                       │
│  → 跳转 Slot B → 再次崩溃 → 再次复位               │
└─────────────────────────────────────────────────────┘
    │
    ▼  (重复到第3次)
┌── Bootloader (第4次) ──────────────────────────────┐
│  读取 Boot Config                                   │
│  → active_slot=1, pending=1, fail_count=3           │
│  → fail_count >= BOOT_FAIL_MAX (3)                  │
│  → ★ 自动回滚! ★                                    │
│  → active_slot 翻转回 0, pending=0, fail_count=0    │
│  → 跳转到 Slot A (旧固件) → 正常运行                │
└─────────────────────────────────────────────────────┘
```

### 3.4 掉电安全分析

| 断电时刻 | 后果 | 恢复方式 |
|---------|------|---------|
| 下载固件到 Slot B 过程中 | Slot B 写了一半，Slot A 完好 | 重启后仍从 Slot A 正常运行 |
| MD5 校验阶段 | 校验未完成，标志未切换 | 重启后仍从 Slot A 正常运行 |
| `BootConfig_SwitchSlot()` 写 Flash 时 | 标志写入是原子操作(1个QuadWord)，要么写成功要么没写 | 没写成功→旧分区；写成功→新分区 |
| Bootloader 递增 `fail_count` 时 | 同上，原子写入 | 最坏情况少计一次，多尝试一次而已 |

---

## 四、两个工程共享的关键定义

以下宏和结构体在两个工程中**必须完全一致**：

```c
// -------- 地址定义 --------
#define SLOT_A_ADDR        0x0800A000
#define SLOT_B_ADDR        0x08060000
#define SLOT_MAX_SIZE      0x00056000   // 344KB
#define BOOT_CONFIG_ADDR   0x080FE000
#define BOOT_CONFIG_PAGE   127
#define BOOT_FAIL_MAX      3

// -------- 启动配置结构体 --------
typedef struct {
    uint32_t active_slot;       // 0=A, 1=B
    uint32_t update_pending;    // 0=已确认, 1=待确认
    uint32_t boot_fail_count;   // 启动失败计数
    uint32_t reserved;          // 保留
} BootConfig_t;
```

**定义位置**：
- Bootloader 工程: `Core/hardware/iap_flash.h`
- App 工程: `Core/OTA/version.h`

---

## 五、涉及修改的文件清单

### Bootloader 工程

| 文件 | 改动内容 |
|------|---------|
| `Core/hardware/iap_flash.h` | 新增 BootConfig_t、A/B 地址宏、函数声明 |
| `Core/hardware/iap_flash.c` | 新增 BootConfig_Read/Write/Rollback 函数 |
| `Core/Src/main.c` | iap_load_app 增加 MSP 检查和 HAL_DeInit；main 改为读 BootConfig 决定跳转 |

### App 工程

| 文件 | 改动内容 |
|------|---------|
| `Core/OTA/version.h` | 重写，A/B 分区定义 + BootConfig_t + 接口声明 |
| `Core/OTA/version.c` | 重写，BootConfig 读写/切换/确认/回滚 |
| `Core/OTA/OTA.h` | 重写，OTA_Status_t 状态码 + 简化接口 |
| `Core/OTA/OTA.c` | 重写，下载到非活动分区 → MD5 → 切标志 → 复位 |
| `Core/Src/app_freertos.c` | 启动时调用 OTA_ConfirmBoot() |
| `Core/OTA/md5.c / md5.h` | 无改动 |

---

## 六、Keil 工程配置注意事项

### 6.1 Bootloader 工程

打开 `MDK-ARM/bootloader.uvprojx`，在 Options → Target 中：

```
IROM1:
  Start: 0x08000000
  Size:  0x0000A000  (40KB)
```

### 6.2 App 工程

打开 `MDK-ARM/STM32U5.uvprojx`，在 Options → Target 中：

```
IROM1:
  Start: 0x0800A000
  Size:  0x00056000  (344KB, 即 Slot A 的大小)
```

### 6.3 App 中断向量表偏移 (VTOR)

App 不在默认的 0x08000000 启动，必须设置 VTOR 偏移。在 App 工程的 `system_stm32u5xx.c` 中确认：

```c
#define VECT_TAB_OFFSET  0x0000A000U
```

或者在 `main.c` 的 `SystemInit()` 之后手动设置：

```c
SCB->VTOR = 0x0800A000;
```

> **如果不设置 VTOR，App 的中断会跳到 Bootloader 的中断向量表，导致 HardFault。**

### 6.4 烧录顺序

1. **先烧 Bootloader** (0x08000000)
2. **再烧 App** 到 Slot A (0x0800A000)
3. 首次烧录时 Boot Config 区 (Page 127) 全为 0xFF
4. Bootloader 会将 0xFF 识别为默认值 (active_slot=0 即 Slot A)，正常跳转

---

## 七、重要注意事项

### 7.1 两个工程的结构体必须一致

`BootConfig_t` 在 Bootloader (`iap_flash.h`) 和 App (`version.h`) 中各定义了一份。**修改任何一边时，另一边必须同步修改**，否则两边读写同一块 Flash 会产生数据错乱。

### 7.2 App 固件大小不能超过 344KB

单个 Slot 的容量为 `0x56000` = 344KB。OTA 下载前会检查 `firmware_size > SLOT_MAX_SIZE`，但编译时也要注意 Keil 的 IROM 大小设置不要超过此值。

### 7.3 看门狗配合自动回滚

自动回滚机制依赖 **看门狗 (IWDG/WWDG)**：
- 新固件如果崩溃，必须靠看门狗超时来复位
- 建议在 Bootloader 中开启 IWDG（如 8 秒超时）
- App 正常运行时定期喂狗
- App 启动失败 → 不喂狗 → 看门狗复位 → Bootloader 累加 fail_count → 3 次后回滚

如果**不使用看门狗**，新固件崩溃后只能靠手动复位来触发回滚。

### 7.4 OTA_ConfirmBoot() 的调用时机

`OTA_ConfirmBoot()` 应在 App **所有关键初始化完成后** 调用。当前放在 `modbus_parse_task` 的开头，即 FreeRTOS 任务启动后。这意味着：

- RTOS 调度器正常
- UART / SPI / I2C 外设已初始化
- 基本功能可用

只有走到这一步，才能说明新固件"活了"。如果初始化过程中就崩溃了，`OTA_ConfirmBoot()` 不会被执行，pending 标志不会清除，满足回滚条件。

### 7.5 不要在 Bootloader 中使用 FreeRTOS

Bootloader 是裸机程序，没有 RTOS。其中调用的 `send_AT_Cmd` 等带 `vTaskDelay` 的函数**不能在 Bootloader 中使用**。Bootloader 只做三件事：读配置、判断、跳转。

### 7.6 Flash 写入的原子性

STM32U5 的 Flash 最小编程单位是 16 字节 (QuadWord)。`BootConfig_t` 恰好是 16 字节，一次 `HAL_FLASH_Program(FLASH_TYPEPROGRAM_QUADWORD, ...)` 写入。这保证了配置写入的原子性——要么 16 字节全部写成功，要么没写入。

### 7.7 调试技巧

通过串口 (USART1) 可以观察完整的启动日志：

```
======== A/B Bootloader ========
active_slot=0 (A), pending=0, fail_count=0
Try Slot A (0x0800A000)...
Jump to 0x0800A000 (MSP=0x200xxxxx, Reset=0x0800xxxx)
```

OTA 升级后首次启动：

```
======== A/B Bootloader ========
active_slot=1 (B), pending=1, fail_count=0
Pending update, fail_count -> 1
Try Slot B (0x08060000)...
Jump to 0x08060000 (MSP=0x200xxxxx, Reset=0x0806xxxx)

[App] Boot check: Slot B, pending=1, fail_count=1
[App] New firmware boot success, confirming...
[App] Boot confirmed on Slot B
```

### 7.8 后续可优化方向

| 方向 | 说明 |
|------|------|
| 固件签名 | 加入 ECDSA/RSA 签名验证，防止固件被篡改 |
| 防降级 | 在 BootConfig 中增加版本号字段，拒绝刷入更低版本 |
| 差分更新 | 只传输新旧固件的差异 (bsdiff)，减少下载时间 |
| 加密传输 | 使用 TLS 加密 OTA 下载通道 |
| 压缩 | 下载压缩固件，设备端解压后写入，节省传输流量 |
