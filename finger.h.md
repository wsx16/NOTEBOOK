#ifndef _FINGER_PRINT_HEAD_H
#define _FINGER_PRINT_HEAD_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define FINGER_DEBUG

#define FINGER_MAX_TIMEOUT  HAL_MAX_DELAY

typedef struct{
	uint8_t  head[2];
	uint8_t  addrCode[4];
	uint8_t  id;
	uint8_t  len[2];
	uint8_t  data[1];
}__attribute__((packed))package_t;

#define PACKAGE_ID_SIZE     sizeof(uint8_t)
#define PACKAGE_LEN_SIZE    sizeof(uint16_t)
#define PACKAGE_DATA_CELL_SIZE  sizeof(uint8_t)
#define PACKAGE_CHECKSUM_SIZE  sizeof(uint16_t)

#define PACKAGE_HEAD_TYPE   0xEF01
#define PACKAGE_ADDR_CODE   0xFFFFFFFF
#define PACKAGE_COMMAND_ID  0x01
#define PACKAGE_DATA_ID     0X02
#define PACKAGE_ACK_ID      0x07
#define PACKAGE_DATA_END_ID 0x08

#define FIRST_BUFFER_ID           0x1
#define SECOND_BUFFER_ID          0x2          


#define GENIMG                0x01  
#define UPIMAGE               0x0A  
#define DOWMIMAGE             0x0B  
#define IMG2TZ                0x02  
#define REGMODEL              0x05  
#define UPCHAR                0x08  
#define DOWNCHAR              0x09  
#define STORE                 0x06  
#define LOADCHAR              0x07  
#define DELETCHAR             0x0C  
#define EMPTY                 0x0D  
#define Match                 0x03  
extern int8_t collectFingerPrintImage(void);
extern int8_t makeFingerPrintModel(void);
extern int8_t requestEnterFingerPrint(int pageId);
extern int8_t verifyFingerPrint(uint16_t startPage,uint16_t pageNum,uint16_t *matchPage);
extern int8_t searchFingerPrintLib(uint16_t startPage,uint16_t pageNum,uint16_t *matchPage);
extern int8_t clearFingerPrintLib(void);

#endif
