# OTA 升级演示操作指南

> 适用工程：STM32U575RITx + ESP8266 + OneNET OTA 平台
> 方案：A/B 双分区，支持断点续传，MD5 校验，自动回滚

---

## 一、Flash 分区布局

| 区域 | 起始地址 | 大小 | 说明 |
|------|----------|------|------|
| Bootloader | `0x08000000` | 40 KB | 读取 BootConfig，跳转到活动分区 |
| Slot A（App） | `0x0800A000` | 344 KB | 默认活动分区 |
| Slot B（备用） | `0x08060000` | 344 KB | OTA 下载目标分区 |
| OTA 进度 | `0x080FC000` | 8 KB | 断点续传进度（Page 126） |
| Boot Config | `0x080FE000` | 8 KB | A/B 切换标志（Page 127） |

---

## 二、本次为使 OTA 可用所做的修改

### 1. App 链接地址修正（`MDK-ARM/STM32U5/STM32U5.sct`）

```diff
- LR_IROM1 0x08000000 0x00050000
- ER_IROM1 0x08000000 0x00050000
+ LR_IROM1 0x0800A000 0x00056000  ; Slot A: 344KB
+ ER_IROM1 0x0800A000 0x00056000
```

**原因：** App 必须链接到 Slot A 起始地址，Bootloader 跳转时读取 `0x0800A000` 处的 MSP 和复位向量才能正确执行。

---

### 2. 删除 App 自设 VTOR（`Core/Src/main.c`）

```diff
- __set_MSP(*((volatile unsigned long int *)0x0800A000));
- SCB->VTOR = FLASH_BASE | 0xA000;
- __enable_irq();
```

**原因：** Bootloader 跳转时已正确设置 MSP 和 PC，App 无需也不应再修改 VTOR。

---

### 3. 新增 OTA Modbus 触发功能码（`Core/modbus/modbus.h`）

```c
#define FUNC_OTA_START  0xBB  /* 触发 OTA 升级: FF BB [CRC16] */
```

---

### 4. Modbus 处理 OTA 触发（`Core/modbus/modbus.c`）

在 `DEV_SYSTEM (0xFF)` 分支新增：

```c
if (func == FUNC_OTA_START)
{
    printf("[OTA] Triggered by Modbus FF BB\r\n");
    modbus_ack_send(dev, func, MODBUS_OK);
    HAL_Delay(100);
    OTA_Init();   /* 阻塞执行，成功后 NVIC_SystemReset() */
}
```

---

### 5. 修复 AT 命令双重换行（`Core/OTA/OTA.c`）

`send_AT_Cmd()` 内部已自动追加 `\r\n`，原代码字符串末尾重复携带导致 ESP8266 命令识别失败：

```diff
- while (!send_AT_Cmd("AT+CIPSTART=\"TCP\",\"183.230.40.33\",80\r\n", "OK", 1000))
+ while (!send_AT_Cmd("AT+CIPSTART=\"TCP\",\"183.230.40.33\",80", "OK", 1000))

- while (!send_AT_Cmd("AT+CIPMODE=1\r\n", "OK", 1000))
+ while (!send_AT_Cmd("AT+CIPMODE=1", "OK", 1000))

- while (!send_AT_Cmd("AT+CIPSEND\r\n", "OK", 1000))
+ while (!send_AT_Cmd("AT+CIPSEND", ">", 1000))
```

> `AT+CIPSEND` 进入透传模式后响应为 `>`，不是 `OK`。

---

## 三、烧录步骤

### 第一步：烧录 Bootloader

1. 用 Keil 打开 `E:\WSX\stm32\bootloader\MDK-ARM\bootloader.uvprojx`
2. 点击 **Rebuild** 全量编译
3. 点击 **Download** 下载到 `0x08000000`

### 第二步：烧录 App

1. 打开当前工程 `MDK-ARM\STM32U5.uvprojx`
2. 点击 **Rebuild** 重新编译（Scatter 已修改，必须重编）
3. 点击 **Download** — Keil 按 Scatter 文件自动下载到 `0x0800A000`，**不会覆盖 Bootloader**

---

## 四、验证 Bootloader 正常工作

复位后，串口（USART1，9600bps）应打印：

```
======== A/B Bootloader ========
active_slot=0 (A), pending=0, fail_count=0
Try Slot A (0x0800A000)...
Jump to 0x0800A000 (MSP=0x2xxxx, Reset=0x0800xxxx)
```

随后 App 正常启动。若看到此输出，说明 Bootloader + App 配合正常。

---

## 五、触发 OTA 升级

### Modbus 触发帧

向 **USART1 RS-485** 发送：

| 字节 | 值 | 说明 |
|------|-----|------|
| 0 | `0xFF` | 设备地址：系统 |
| 1 | `0xBB` | 功能码：触发 OTA |
| 2 | `0xC8` | CRC16 低字节 |
| 3 | `0x7A` | CRC16 高字节 |

完整帧（HEX）：**`FF BB C8 7A`**

### OTA 串口打印流程

```
[OTA] Triggered by Modbus FF BB
======== OTA A/B Update ==========
Current active: Slot A
Update pending: 0
====================================
Report version...
OTA check resp: ...
Parsed: tid=xxxxxx, size=xxxxx, md5=xxxxxxxx...
Target: Slot B, addr=0x08060000, page_start=48
Download: N blocks, xxxxx bytes -> Slot B (0x08060000)
Block 1/N: 0-1023
Block 2/N: 1024-2047
...
Download done, written: xxxxx bytes
MD5 self-test: e10adc3949ba59abbe56e057f20f883e ✓
Local  MD5: xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
Server MD5: xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
MD5 verify OK
Switch to Slot B, pending confirm
Rebooting...
```

复位后 Bootloader 跳转到 Slot B，App 启动后调用 `OTA_ConfirmBoot()` 确认新固件，完成整个升级流程。

---

## 六、自动回滚机制

若新固件启动失败，Bootloader 每次上电递增 `boot_fail_count`，连续失败 **3 次**后自动回滚到上一个分区：

```
Boot failed 3 times! Auto rollback.
Rollback to Slot A
```

---

## 七、注意事项

| 事项 | 说明 |
|------|------|
| WiFi 热点 | 代码硬编码为 `Redmi K70` / `200212137`（`mqtt.c:62`）。演示时确保此热点开启，或提前用 Modbus `FF AA` 命令更换 |
| OneNET 固件版本 | 服务器上需上传版本号 > `1.0` 的固件包，否则 `OTA_CheckUpdate` 返回"无新版本" |
| 编译后固件大小 | 不得超过 344 KB（`0x00056000`），否则 OTA 报 `Firmware too large` |
| 烧录顺序 | 先烧 Bootloader，再烧 App。若顺序颠倒或 App 全片擦除，Bootloader 会丢失 |
