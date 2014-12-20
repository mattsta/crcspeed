#ifndef CRC64SPEED_H
#define CRC64SPEED_H
#include "crcspeed.h"
#include "crc64.h"
#include "stdbool.h"

bool crc64speed_init(void);
uint64_t crc64speed(uint64_t crc, const unsigned char *s, uint64_t l);

bool crc64speed_init_big(void);
uint64_t crc64speed_big(uint64_t crc, const unsigned char *s, uint64_t l);

bool crc64speed_init_native(void);
uint64_t crc64speed_native(uint64_t crc, const unsigned char *s, uint64_t l);
#endif
