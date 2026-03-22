#include "version.h"
#include <string.h>

/*============================================================
 *  Boot Config 读写 (存储在 Flash Page 127, 地址 0x080FE000)
 *============================================================*/

/* 从 Flash 读取 Boot Config */
void BootConfig_Read(BootConfig_t *cfg)
{
    uint32_t *src = (uint32_t *)BOOT_CONFIG_ADDR;

    cfg->active_slot     = src[0];
    cfg->update_pending  = src[1];
    cfg->boot_fail_count = src[2];
    cfg->reserved        = src[3];

    /* Flash 擦除后全 0xFF, 视为默认值: Slot A 活动, 无待更新 */
    if (cfg->active_slot == 0xFFFFFFFF) {
        cfg->active_slot     = 0;
        cfg->update_pending  = 0;
        cfg->boot_fail_count = 0;
        cfg->reserved        = 0;
    }
}
 
/* 擦除 Page 127 并写入新的 Boot Config (16 字节 = 1 个 QuadWord) */
int BootConfig_Write(const BootConfig_t *cfg)
{
    HAL_FLASH_Unlock();

    /* 擦除 Page 127 */
    FLASH_EraseInitTypeDef erase = {
        .TypeErase = FLASH_TYPEERASE_PAGES,
        .Banks     = FLASH_BANK_1,
        .Page      = BOOT_CONFIG_PAGE,
        .NbPages   = 1,
    };
    uint32_t err;
    if (HAL_FLASHEx_Erase(&erase, &err) != HAL_OK) {
        printf("BootConfig erase err\r\n");
        HAL_FLASH_Lock();
        return 1;
    }

    /* 写入 16 字节 (QuadWord) */
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_QUADWORD,
                          BOOT_CONFIG_ADDR,
                          (uint32_t)cfg) != HAL_OK) {
        printf("BootConfig write err\r\n");
        HAL_FLASH_Lock();
        return 1;
    }

    HAL_FLASH_Lock();
    return 0;
}

/*============================================================
 *  便捷查询函数
 *============================================================*/

uint8_t BootConfig_GetActiveSlot(void)
{
    BootConfig_t cfg;
    BootConfig_Read(&cfg);
    return (cfg.active_slot == 0) ? 0 : 1;
}

uint8_t BootConfig_GetInactiveSlot(void)
{
    return BootConfig_GetActiveSlot() ? 0 : 1;
}

uint32_t BootConfig_GetActiveSlotAddr(void)
{
    return BootConfig_GetActiveSlot() == 0 ? SLOT_A_ADDR : SLOT_B_ADDR;
}

uint32_t BootConfig_GetInactiveSlotAddr(void)
{
    return BootConfig_GetActiveSlot() == 0 ? SLOT_B_ADDR : SLOT_A_ADDR;
}

uint32_t BootConfig_GetInactivePageStart(void)
{
    return BootConfig_GetActiveSlot() == 0 ? SLOT_B_PAGE_START : SLOT_A_PAGE_START;
}

/*============================================================
 *  A/B 切换 & 回滚 & 确认
 *============================================================*/

/* OTA 下载校验完成后调用: 切换活动分区, 设置 update_pending */
int BootConfig_SwitchSlot(void)
{
    BootConfig_t cfg;
    BootConfig_Read(&cfg);

    cfg.active_slot     = cfg.active_slot ? 0 : 1;  /* 翻转 */
    cfg.update_pending  = 1;                         /* 标记待确认 */
    cfg.boot_fail_count = 0;

    printf("Switch to Slot %c, pending confirm\r\n", cfg.active_slot ? 'B' : 'A');
    return BootConfig_Write(&cfg);
}

/* App 启动成功后调用: 清除 pending 标志, 表示新固件运行正常 */
int BootConfig_ConfirmBoot(void)
{
    BootConfig_t cfg;
    BootConfig_Read(&cfg);

    if (cfg.update_pending == 0) {
        return 0;  /* 无需确认 */
    }

    cfg.update_pending  = 0;
    cfg.boot_fail_count = 0;

    printf("Boot confirmed on Slot %c\r\n", cfg.active_slot ? 'B' : 'A');
    return BootConfig_Write(&cfg);
}

/* 回滚到上一个分区 (Bootloader 或 App 检测失败时调用) */
int BootConfig_Rollback(void)
{
    BootConfig_t cfg;
    BootConfig_Read(&cfg);

    cfg.active_slot     = cfg.active_slot ? 0 : 1;  /* 翻转回去 */
    cfg.update_pending  = 0;
    cfg.boot_fail_count = 0;

    printf("Rollback to Slot %c\r\n", cfg.active_slot ? 'B' : 'A');
    return BootConfig_Write(&cfg);
}

/*============================================================
 *  OTA 下载进度 读写 (存储在 Flash Page 126, 地址 0x080FC000)
 *  用于断点续传: 掉电后恢复下载进度
 *============================================================*/

void OTA_Progress_Read(OTA_Progress_t *prog)
{
    memcpy(prog, (void *)OTA_PROGRESS_ADDR, sizeof(OTA_Progress_t));
}

int OTA_Progress_Write(const OTA_Progress_t *prog)
{
    HAL_FLASH_Unlock();

    /* 擦除 Page 126 */
    FLASH_EraseInitTypeDef erase = {
        .TypeErase = FLASH_TYPEERASE_PAGES,
        .Banks     = FLASH_BANK_1,
        .Page      = OTA_PROGRESS_PAGE,
        .NbPages   = 1,
    };
    uint32_t err;
    if (HAL_FLASHEx_Erase(&erase, &err) != HAL_OK) {
        printf("OTA_Progress erase err\r\n");
        HAL_FLASH_Lock();
        return 1;
    }

    /* 写入 80 字节 = 5 个 QuadWord */
    uint32_t quad_count = sizeof(OTA_Progress_t) / 16;
    const uint8_t *src = (const uint8_t *)prog;

    for (uint32_t i = 0; i < quad_count; i++) {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_QUADWORD,
                              OTA_PROGRESS_ADDR + 16 * i,
                              (uint32_t)(src + 16 * i)) != HAL_OK) {
            printf("OTA_Progress write err at QW %d\r\n", i);
            HAL_FLASH_Lock();
            return 1;
        }
    }

    HAL_FLASH_Lock();
    return 0;
}

int OTA_Progress_Clear(void)
{
    HAL_FLASH_Unlock();

    FLASH_EraseInitTypeDef erase = {
        .TypeErase = FLASH_TYPEERASE_PAGES,
        .Banks     = FLASH_BANK_1,
        .Page      = OTA_PROGRESS_PAGE,
        .NbPages   = 1,
    };
    uint32_t err;
    if (HAL_FLASHEx_Erase(&erase, &err) != HAL_OK) {
        printf("OTA_Progress clear err\r\n");
        HAL_FLASH_Lock();
        return 1;
    }

    HAL_FLASH_Lock();
    return 0;
}

int OTA_Progress_IsValid(const OTA_Progress_t *prog)
{
    return (prog->magic == OTA_PROGRESS_MAGIC) ? 1 : 0;
}

/*============================================================
 *  兼容旧接口
 *============================================================*/
void version_set(uint32_t *buf)
{
    BootConfig_Write((const BootConfig_t *)buf);
}
