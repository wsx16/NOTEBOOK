#ifndef __VERSION_H
#define __VERSION_H

#include "main.h"
#include "stm32u5xx_hal.h"
#include "stm32u5xx_hal_flash.h"
#include "stm32u5xx_hal_flash_ex.h"
#include <stdio.h>

/*============================================================
 *  A/B 分区 Flash 布局
 *============================================================
 *  Bootloader:   0x08000000 - 0x0800A000  (40KB,  Pages 0-4)
 *  Slot A:       0x0800A000 - 0x08060000  (344KB, Pages 5-47)
 *  Slot B:       0x08060000 - 0x080B6000  (344KB, Pages 48-90)
 *  OTA Progress: 0x080FC000               (8KB,   Page 126)
 *  Boot Config:  0x080FE000               (8KB,   Page 127)
 *============================================================*/

#define SLOT_A_ADDR        0x0800A000
#define SLOT_B_ADDR        0x08060000
#define SLOT_MAX_SIZE      0x00056000   /* 344KB */

#define SLOT_A_PAGE_START  5
#define SLOT_B_PAGE_START  48
#define SLOT_PAGES         43           /* 每个 Slot 占 43 页 (43*8KB=344KB) */

#define OTA_PROGRESS_ADDR  0x080FC000
#define OTA_PROGRESS_PAGE  126
#define OTA_PROGRESS_MAGIC 0x4F544150   /* "OTAP" — 标识有效进度记录 */

#define BOOT_CONFIG_ADDR   0x080FE000
#define BOOT_CONFIG_PAGE   127

/*------------------------------------------------------------
 *  Boot Config 结构体 (16 字节, 恰好一个 QuadWord)
 *
 *  active_slot:     当前活动分区 (0=A, 1=B)
 *  update_pending:  升级待确认标志 (0=已确认, 1=待确认)
 *  boot_fail_count: 启动失败计数 (用于自动回滚)
 *  reserved:        保留
 *------------------------------------------------------------*/
typedef struct {
    uint32_t active_slot;
    uint32_t update_pending;
    uint32_t boot_fail_count;
    uint32_t reserved;
} BootConfig_t;

#define BOOT_FAIL_MAX  3   /* 连续启动失败 3 次则自动回滚 */

/* 函数声明 */
void     BootConfig_Read(BootConfig_t *cfg);
int      BootConfig_Write(const BootConfig_t *cfg);
uint32_t BootConfig_GetActiveSlotAddr(void);
uint32_t BootConfig_GetInactiveSlotAddr(void);
uint32_t BootConfig_GetInactivePageStart(void);
uint8_t  BootConfig_GetActiveSlot(void);
uint8_t  BootConfig_GetInactiveSlot(void);
int      BootConfig_SwitchSlot(void);
int      BootConfig_ConfirmBoot(void);
int      BootConfig_Rollback(void);

/*------------------------------------------------------------
 *  OTA 下载进度结构体 (80 字节 = 5 个 QuadWord)
 *  存储在 Page 126 (0x080FC000), 用于断点续传
 *------------------------------------------------------------*/
typedef struct {
    /* QuadWord 0 (16 bytes) */
    uint32_t magic;              /* OTA_PROGRESS_MAGIC 表示有效 */
    uint32_t firmware_size;      /* 固件总大小 */
    uint32_t write_offset;       /* 已写入字节数 (8KB 对齐) */
    uint32_t target_addr;        /* 目标分区起始地址 */
    /* QuadWord 1 (16 bytes) */
    uint32_t target_page_start;  /* 目标分区起始页号 */
    uint32_t reserved[3];
    /* QuadWord 2-4 (48 bytes) */
    char     tid[8];             /* 任务 ID */
    char     server_md5[33];     /* 服务器 MD5 (32 hex + '\0') */
    char     _pad[7];            /* 补齐到 48 字节 (3 个 QuadWord) */
} OTA_Progress_t;  /* 总计 80 bytes = 5 QuadWords */

void OTA_Progress_Read(OTA_Progress_t *prog);
int  OTA_Progress_Write(const OTA_Progress_t *prog);
int  OTA_Progress_Clear(void);
int  OTA_Progress_IsValid(const OTA_Progress_t *prog);

/* 兼容旧接口 */
#define version_add  BOOT_CONFIG_ADDR
extern void version_set(uint32_t *buf);

#endif
