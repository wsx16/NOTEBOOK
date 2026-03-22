#include "OTA.h"

/*============================================================
 *  私有变量
 *============================================================*/
static char     tid_str[8]    = {0};
static uint32_t firmware_size = 0;
static char     server_md5[33] = {0};
static char     local_md5[33]  = {0};

static uint32_t write_offset = 0;        /* 已写入字节偏移 */
static uint32_t target_addr  = 0;        /* 写入目标分区起始地址 */
static uint32_t target_page_start = 0;   /* 写入目标分区起始页号 */

static uint32_t iapbuf[256];             /* 1024 字节写缓冲 */

/*============================================================
 *  Flash 底层操作
 *============================================================*/
static int eraseFlash(uint32_t page_num)
{
    HAL_FLASH_Unlock();
    FLASH_EraseInitTypeDef erase = {
        .TypeErase = FLASH_TYPEERASE_PAGES,
        .Banks     = FLASH_BANK_1,
        .Page      = page_num,
        .NbPages   = 1,
    };
    uint32_t err;
    if (HAL_FLASHEx_Erase(&erase, &err) != HAL_OK) {
        printf("Flash erase err, page: %d\r\n", page_num);
        HAL_FLASH_Lock();
        return 1;
    }
    HAL_FLASH_Lock();
    return 0;
}

static int writeFlash(uint32_t addr, uint32_t len)
{
    HAL_FLASH_Unlock();
    uint32_t quad_count = (len + 15) / 16;

    for (uint32_t i = 0; i < quad_count; i++) {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_QUADWORD,
                              addr + 16 * i,
                              (uint32_t)(iapbuf + 4 * i)) != HAL_OK) {
            printf("Flash write err, addr: 0x%08X\r\n", addr + 16 * i);
            HAL_FLASH_Lock();
            return 1;
        }
    }

    HAL_FLASH_Lock();
    return 0;
}

/* 写入一块固件数据到目标分区 */
static int writeAppBin(uint8_t *pBuffer, uint16_t NumToWrite)
{
    __disable_irq();

    memset(iapbuf, 0, sizeof(iapbuf));
    memcpy(iapbuf, pBuffer, NumToWrite);

    /* 每到 8KB 页边界, 擦除该页 */
    if ((write_offset % 0x2000) == 0) {
        uint32_t page = target_page_start + (write_offset / 0x2000);
        if (eraseFlash(page) != 0) {
            __enable_irq();
            return 1;
        }
    }

    /* 写入 */
    if (writeFlash(target_addr + write_offset, NumToWrite) != 0) {
        __enable_irq();
        return 1;
    }

    write_offset += NumToWrite;
    __enable_irq();
    return 0;
}

/*============================================================
 *  网络通信 (与 OneNET OTA 服务器交互)
 *============================================================*/

/* 发送请求并检查响应关键字 */
static int OTA_SendAndCheck(const char *cmd, const char *response)
{
    memset((char *)esp_buffer, 0, sizeof(esp_buffer));
    HAL_UARTEx_ReceiveToIdle_IT(&huart5, (uint8_t *)esp_buffer, sizeof(esp_buffer));
    HAL_UART_Transmit(&huart5, (uint8_t *)cmd, strlen(cmd), 500);

    HAL_Delay(1000);

    if (strstr((char *)esp_buffer, response) != NULL) {
        printf("OTA resp OK: %s\r\n", response);
        memset((char *)esp_buffer, 0, sizeof(esp_buffer));
        return 0;
    }

    printf("OTA resp err: %s\r\n", esp_buffer);
    memset((char *)esp_buffer, 0, sizeof(esp_buffer));
    return 1;
}

/* 上报版本号 */
static void OTA_ReportVersion(void)
{
    char request_body[64] = {0};
    snprintf(request_body, sizeof(request_body),
             "{\"s_version\":\"V%d.%d\",\"f_version\":\"V%d.%d\"}", 1, 0, 1, 0);
    uint16_t body_len = strlen(request_body);

    uint8_t buf[512] = {0};
    snprintf((char *)buf, sizeof(buf) - 1,
             "POST " ONENET_OTA_PATH "/version HTTP/1.1\r\n"
             "Content-Type: application/json\r\n"
             "Authorization:" ONENET_AUTHORIZATION "\r\n"
             "Host: " ONENET_HOST "\r\n"
             "Content-Length: %d\r\n\r\n%s",
             body_len, request_body);

    printf("Report version...\r\n");
    for (int r = 0; r < OTA_MAX_RETRIES; r++) {
        if (OTA_SendAndCheck((char *)buf, "succ") == 0) break;
        HAL_Delay(1000);
    }
}

/* 查询新版本, 解析 tid / size / md5 */
static int OTA_CheckUpdate(void)
{
    memset((char *)esp_buffer, 0, sizeof(esp_buffer));

    uint8_t req[512] = {0};
    snprintf((char *)req, sizeof(req) - 1,
             "GET " ONENET_OTA_PATH "/check?type=2&version=1.0 HTTP/1.1\r\n"
             "Content-Type: application/json\r\n"
             "Authorization:" ONENET_AUTHORIZATION "\r\n"
             "Host: " ONENET_HOST "\r\n\r\n");

    HAL_UARTEx_ReceiveToIdle_IT(&huart5, (uint8_t *)esp_buffer, sizeof(esp_buffer));
    HAL_UART_Transmit(&huart5, req, strlen((char *)req), 500);

    HAL_Delay(1000);
    printf("OTA check resp: %s\r\n", esp_buffer);

    /* 解析 tid */
    char *tmp = strstr((char *)esp_buffer, "\"tid\":\"");
    if (tmp) {
        sscanf(tmp, "\"tid\":\"%7[^\"]\"", tid_str);
    } else {
        tmp = strstr((char *)esp_buffer, "\"tid\":");
        if (tmp) sscanf(tmp, "\"tid\":%7s", tid_str);
    }

    /* 解析 size */
    tmp = strstr((char *)esp_buffer, "\"size\":");
    if (tmp) sscanf(tmp, "\"size\":%d", &firmware_size);

    /* 解析 md5 */
    tmp = strstr((char *)esp_buffer, "\"md5\":\"");
    if (tmp) sscanf(tmp, "\"md5\":\"%32[^\"]\"", server_md5);

    printf("Parsed: tid=%s, size=%d, md5=%s\r\n", tid_str, firmware_size, server_md5);
    memset((char *)esp_buffer, 0, sizeof(esp_buffer));

    if (firmware_size > 0 && strlen(tid_str) > 0) {
        return 0;  /* 有新版本 */
    }
    return 1;  /* 无新版本 */
}

/* 下载一块固件数据并写入 Flash */
static void OTA_DownloadBlock(uint8_t *cmd)
{
    memset((char *)esp_buffer, 0, sizeof(esp_buffer));
    HAL_UART_Transmit(&huart5, cmd, strlen((char *)cmd), 500);
    HAL_Delay(1000);

    char *tmp = strstr((char *)esp_buffer, "\r\n\r\n");
    if (tmp != NULL) {
        uint16_t valid_len = 1024;
        if (write_offset + 1024 > firmware_size) {
            valid_len = firmware_size - write_offset;
        }
        writeAppBin((uint8_t *)tmp + 4, valid_len);
    } else {
        printf("No firmware data in block\r\n");
    }

    memset((char *)esp_buffer, 0, sizeof(esp_buffer));
}

/* 保存当前下载进度到 Flash (断点续传) */
static void OTA_SaveProgress(void)
{
    OTA_Progress_t prog;
    memset(&prog, 0, sizeof(prog));

    prog.magic            = OTA_PROGRESS_MAGIC;
    prog.firmware_size    = firmware_size;
    prog.write_offset     = write_offset;
    prog.target_addr      = target_addr;
    prog.target_page_start = target_page_start;
    memcpy(prog.tid, tid_str, sizeof(prog.tid));
    memcpy(prog.server_md5, server_md5, sizeof(prog.server_md5));

    if (OTA_Progress_Write(&prog) == 0) {
        printf("Progress saved: offset=%d/%d\r\n", write_offset, firmware_size);
    } else {
        printf("Progress save FAILED\r\n");
    }
}

/* 分块下载整个固件到目标分区 (支持断点续传) */
static int OTA_DownloadFirmware(void)
{
    int blocks      = (firmware_size + 1023) / 1024;
    int start_block = write_offset / 1024;

    printf("Download: %d blocks, %d bytes -> Slot %c (0x%08X)\r\n",
           blocks, firmware_size,
           (target_addr == SLOT_A_ADDR) ? 'A' : 'B',
           target_addr);

    if (start_block > 0) {
        printf("Resuming from block %d (offset=%d)\r\n", start_block, write_offset);
    }

    for (int i = start_block; i < blocks; i++) {
        uint32_t start = i * 1024;
        uint32_t end   = (i == blocks - 1) ? (firmware_size - 1) : (start + 1023);

        uint8_t req[512] = {0};
        snprintf((char *)req, sizeof(req) - 1,
                 "GET " ONENET_OTA_PATH "/%s/download HTTP/1.1\r\n"
                 "Authorization:" ONENET_AUTHORIZATION "\r\n"
                 "Host: " ONENET_HOST "\r\n"
                 "Range:bytes=%d-%d\r\n\r\n",
                 tid_str, start, end);

        printf("Block %d/%d: %d-%d\r\n", i + 1, blocks, start, end);
        OTA_DownloadBlock(req);
        HAL_Delay(500);

        /* 每写满一个 8KB 页 (8 个 block), 保存进度 */
        if ((write_offset % 0x2000) == 0 && write_offset > 0) {
            OTA_SaveProgress();
        }
    }

    printf("Download done, written: %d bytes\r\n", write_offset);

    /* 下载完成, 清除进度记录 */
    OTA_Progress_Clear();

    if (write_offset != firmware_size) {
        printf("Size mismatch! expected=%d, actual=%d\r\n", firmware_size, write_offset);
        return 1;
    }
    return 0;
}

/*============================================================
 *  MD5 校验
 *============================================================*/
static void OTA_MD5SelfTest(void)
{
    const char test_data[] = "123456";
    char md5_str[33] = {0};
    MD5_Flash_String((uint32_t)test_data, strlen(test_data), md5_str);
    printf("MD5 self-test: %s (expect: e10adc3949ba59abbe56e057f20f883e)\r\n", md5_str);
}

static int OTA_VerifyMD5(void)
{
    OTA_MD5SelfTest();

    MD5_Flash_String(target_addr, write_offset, local_md5);
    printf("Local  MD5: %s\r\n", local_md5);
    printf("Server MD5: %s\r\n", server_md5);

    if (strcasecmp(local_md5, server_md5) == 0) {
        printf("MD5 verify OK\r\n");
        return 0;
    }

    printf("MD5 verify FAILED\r\n");
    return 1;
}

/*============================================================
 *  OTA 主流程 (A/B 方案, 支持断点续传)
 *============================================================*/
OTA_Status_t OTA_Init(void)
{
    int resuming = 0;

    /* 0. 打印当前 A/B 状态 */
    BootConfig_t cfg;
    BootConfig_Read(&cfg);
    printf("\r\n========== OTA A/B Update ==========\r\n");
    printf("Current active: Slot %c\r\n", cfg.active_slot ? 'B' : 'A');
    printf("Update pending: %d\r\n", cfg.update_pending);
    printf("====================================\r\n");

    /* 1. 网络初始化 */
    send_AT_Cmd("AT+RST", "ready", 3000);
    CONNECT_WIFI();

    char at_buf[128];
    snprintf(at_buf, sizeof(at_buf),
             "AT+CIPSTART=\"TCP\",\"%s\",%d", ONENET_OTA_SERVER, ONENET_OTA_PORT);

    int retries;
    for (retries = 0; retries < OTA_MAX_RETRIES; retries++) {
        if (send_AT_Cmd(at_buf, "OK", 2000)) break;
        HAL_Delay(1000);
    }
    if (retries >= OTA_MAX_RETRIES) {
        printf("TCP connect failed after %d retries\r\n", OTA_MAX_RETRIES);
        return OTA_ERR_NETWORK;
    }

    for (retries = 0; retries < OTA_MAX_RETRIES; retries++) {
        if (send_AT_Cmd("AT+CIPMODE=1", "OK", 1000)) break;
        HAL_Delay(500);
    }
    if (retries >= OTA_MAX_RETRIES) return OTA_ERR_NETWORK;

    for (retries = 0; retries < OTA_MAX_RETRIES; retries++) {
        if (send_AT_Cmd("AT+CIPSEND", ">", 1000)) break;
        HAL_Delay(500);
    }
    if (retries >= OTA_MAX_RETRIES) return OTA_ERR_NETWORK;
    printf("Network ready\r\n");

    /* 2. 检查是否有未完成的下载进度 (断点续传) */
    OTA_Progress_t prog;
    OTA_Progress_Read(&prog);

    if (OTA_Progress_IsValid(&prog)) {
        /* 验证目标分区是否仍然是非活动分区 */
        uint32_t inactive_addr = BootConfig_GetInactiveSlotAddr();

        if (prog.target_addr == inactive_addr &&
            prog.firmware_size > 0 &&
            prog.write_offset < prog.firmware_size &&
            prog.write_offset > 0) {

            /* 恢复进度 */
            memcpy(tid_str, prog.tid, sizeof(tid_str));
            firmware_size    = prog.firmware_size;
            memcpy(server_md5, prog.server_md5, sizeof(server_md5));
            write_offset     = prog.write_offset;
            target_addr      = prog.target_addr;
            target_page_start = prog.target_page_start;
            resuming = 1;

            printf("*** Resuming download ***\r\n");
            printf("tid=%s, size=%d, offset=%d, md5=%s\r\n",
                   tid_str, firmware_size, write_offset, server_md5);
            printf("Target: Slot %c, addr=0x%08X\r\n",
                   (target_addr == SLOT_A_ADDR) ? 'A' : 'B', target_addr);
        } else {
            printf("Saved progress invalid (slot changed?), clearing\r\n");
            OTA_Progress_Clear();
        }
    }

    if (!resuming) {
        /* 3. 正常流程: 上报版本, 查询新版本 */
        OTA_ReportVersion();

        if (OTA_CheckUpdate() != 0) {
            printf("No update available\r\n");
            return OTA_ERR_NO_UPDATE;
        }

        /* 4. 检查固件大小是否超过分区容量 */
        if (firmware_size > SLOT_MAX_SIZE) {
            printf("Firmware too large: %d > %d\r\n", firmware_size, SLOT_MAX_SIZE);
            return OTA_ERR_DOWNLOAD;
        }

        /* 5. 确定写入目标: 非活动分区 */
        target_addr       = BootConfig_GetInactiveSlotAddr();
        target_page_start = BootConfig_GetInactivePageStart();
        write_offset      = 0;

        printf("Target: Slot %c, addr=0x%08X, page_start=%d\r\n",
               (target_addr == SLOT_A_ADDR) ? 'A' : 'B',
               target_addr, target_page_start);

        /* 保存初始进度 */
        OTA_SaveProgress();
    }

    /* 6. 分块下载固件到非活动分区 (自动从 write_offset 处续传) */
    if (OTA_DownloadFirmware() != 0) {
        printf("Download failed\r\n");
        return OTA_ERR_DOWNLOAD;
    }

    /* 7. MD5 校验 */
    if (OTA_VerifyMD5() != 0) {
        printf("MD5 failed, abort. Active slot unchanged.\r\n");
        OTA_Progress_Clear();
        return OTA_ERR_MD5;
    }

    /* 8. 校验通过 -> 切换活动分区 -> 复位 */
    printf("MD5 OK! Switching active slot...\r\n");

    __disable_irq();
    if (BootConfig_SwitchSlot() != 0) {
        __enable_irq();
        printf("Switch slot failed\r\n");
        return OTA_ERR_FLASH;
    }

    printf("Rebooting...\r\n");
    NVIC_SystemReset();

    /* 不会执行到这里 */
    return OTA_OK;
}

/*============================================================
 *  启动确认 (App 启动后调用)
 *============================================================*/
int OTA_ConfirmBoot(void)
{
    BootConfig_t cfg;
    BootConfig_Read(&cfg);

    printf("Boot check: Slot %c, pending=%d, fail_count=%d\r\n",
           cfg.active_slot ? 'B' : 'A',
           cfg.update_pending,
           cfg.boot_fail_count);

    if (cfg.update_pending == 1) {
        /*
         * 新固件首次启动, 如果能执行到这里说明启动成功
         * 清除 pending 标志, 确认新固件可用
         */
        printf("New firmware boot success, confirming...\r\n");
        return BootConfig_ConfirmBoot();
    }

    return 0;
}
