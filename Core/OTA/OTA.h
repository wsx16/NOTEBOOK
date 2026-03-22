#ifndef __OTA_H
#define __OTA_H

#include "mqtt.h"
#include "version.h"
#include "md5.h"

#define OTA_MAX_RETRIES  10   /* Max retry count for network operations */

/* OTA 状态码 */
typedef enum {
    OTA_OK = 0,
    OTA_ERR_NETWORK,
    OTA_ERR_NO_UPDATE,
    OTA_ERR_DOWNLOAD,
    OTA_ERR_MD5,
    OTA_ERR_FLASH,
} OTA_Status_t;

/* 对外接口 */
extern OTA_Status_t OTA_Init(void);
extern int          OTA_ConfirmBoot(void);

#endif
