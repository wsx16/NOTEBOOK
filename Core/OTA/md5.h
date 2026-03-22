#ifndef MD5_H
#define MD5_H

#include <stdint.h>
#include <string.h>

#define MD5_DIGEST_LENGTH 16

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t state[4];
    uint64_t count;
    uint8_t buffer[64];
} MD5_CTX;

void MD5_Init(MD5_CTX *ctx);
void MD5_Update(MD5_CTX *ctx, const uint8_t *data, uint32_t len);
void MD5_Final(MD5_CTX *ctx, uint8_t digest[MD5_DIGEST_LENGTH]);

void MD5_Flash(uint32_t start_addr, uint32_t total_bytes, uint8_t digest[16]);
void MD5_ToString(const uint8_t digest[MD5_DIGEST_LENGTH], char str[33]);
void MD5_Flash_String(uint32_t start_addr, uint32_t total_bytes, char md5_str[33]);

#ifdef __cplusplus
}
#endif

#endif