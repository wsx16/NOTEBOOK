#include "md5.h"

#define LEFTROTATE(x, c) (((x) << (c)) | ((x) >> (32 - (c))))

static const uint32_t k[] = {
    0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
    0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
    0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
    0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
    0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
    0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
    0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
    0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
    0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
    0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
    0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
    0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
    0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
    0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
    0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
    0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
};

static const uint8_t r[] = {
    7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,
    5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,
    4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,
    6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21
};

static void md5_transform(MD5_CTX *ctx, const uint8_t block[64])
{
    uint32_t a = ctx->state[0];
    uint32_t b = ctx->state[1];
    uint32_t c = ctx->state[2];
    uint32_t d = ctx->state[3];
    uint32_t m[16];
    int i;

    for (i = 0; i < 16; i++) {
        m[i] = ((uint32_t)block[i*4+3] << 24) |
               ((uint32_t)block[i*4+2] << 16) |
               ((uint32_t)block[i*4+1] <<  8) |
               ((uint32_t)block[i*4+0] <<  0);
    }

    for (i = 0; i < 64; i++) {
        uint32_t f, g;

        if (i < 16) {
            f = (b & c) | (~b & d);
            g = i;
        } else if (i < 32) {
            f = (d & b) | (~d & c);
            g = (5*i + 1) % 16;
        } else if (i < 48) {
            f = b ^ c ^ d;
            g = (3*i + 5) % 16;
        } else {
            f = c ^ (b | ~d);
            g = (7*i) % 16;
        }

        uint32_t temp = d;
        d = c;
        c = b;
        b = b + LEFTROTATE((a + f + k[i] + m[g]), r[i]);
        a = temp;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
}

void MD5_Init(MD5_CTX *ctx)
{
    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xefcdab89;
    ctx->state[2] = 0x98badcfe;
    ctx->state[3] = 0x10325476;
    ctx->count = 0;
    memset(ctx->buffer, 0, 64);
}

void MD5_Update(MD5_CTX *ctx, const uint8_t *data, uint32_t len)
{
    uint32_t i, index, part_len;

    index = (ctx->count >> 3) & 0x3F;
    ctx->count += (uint64_t)len << 3;
    part_len = 64 - index;

    if (len >= part_len) {
        memcpy(&ctx->buffer[index], data, part_len);
        md5_transform(ctx, ctx->buffer);

        for (i = part_len; i + 63 < len; i += 64) {
            md5_transform(ctx, &data[i]);
        }

        index = 0;
    } else {
        i = 0;
    }

    memcpy(&ctx->buffer[index], &data[i], len - i);
}

void MD5_Final(MD5_CTX *ctx, uint8_t digest[16])
{
    uint8_t padding[64] = {0x80};
    uint32_t index, pad_len;
    uint64_t bits = ctx->count;

    index = (ctx->count >> 3) & 0x3F;
    pad_len = (index < 56) ? (56 - index) : (120 - index);
    MD5_Update(ctx, padding, pad_len);

    padding[0] = (uint8_t)(bits & 0xFF);
    padding[1] = (uint8_t)((bits >> 8) & 0xFF);
    padding[2] = (uint8_t)((bits >> 16) & 0xFF);
    padding[3] = (uint8_t)((bits >> 24) & 0xFF);
    padding[4] = (uint8_t)((bits >> 32) & 0xFF);
    padding[5] = (uint8_t)((bits >> 40) & 0xFF);
    padding[6] = (uint8_t)((bits >> 48) & 0xFF);
    padding[7] = (uint8_t)((bits >> 56) & 0xFF);
    MD5_Update(ctx, padding, 8);

    for (int i = 0; i < 4; i++) {
        digest[i*4+0] = (uint8_t)(ctx->state[i] & 0xFF);
        digest[i*4+1] = (uint8_t)((ctx->state[i] >> 8) & 0xFF);
        digest[i*4+2] = (uint8_t)((ctx->state[i] >> 16) & 0xFF);
        digest[i*4+3] = (uint8_t)((ctx->state[i] >> 24) & 0xFF);
    }
}

void MD5_Flash(uint32_t start_addr, uint32_t total_bytes, uint8_t digest[16])
{
    MD5_CTX ctx;
    MD5_Init(&ctx);

    const uint32_t BUF_SIZE = 1024;
    uint8_t buf[BUF_SIZE];
    uint32_t addr = start_addr;
    uint32_t rem = total_bytes;

    while (rem > 0) {
        uint32_t read = (rem > BUF_SIZE) ? BUF_SIZE : rem;
        for (uint32_t i = 0; i < read; i++) {
            buf[i] = *(volatile uint8_t *)(addr + i);
        }
        MD5_Update(&ctx, buf, read);
        addr += read;
        rem -= read;
    }

    MD5_Final(&ctx, digest);
}

void MD5_ToString(const uint8_t digest[MD5_DIGEST_LENGTH], char str[33])
{
    const char hex[] = "0123456789abcdef";
    for (int i = 0; i < 16; i++) {
        str[i*2]   = hex[(digest[i] >> 4) & 0x0F];
        str[i*2+1] = hex[digest[i] & 0x0F];
    }
    str[32] = '\0';
}

void MD5_Flash_String(uint32_t start_addr, uint32_t total_bytes, char md5_str[33])
{
    uint8_t digest[16];
    MD5_Flash(start_addr, total_bytes, digest);
    MD5_ToString(digest, md5_str);
}