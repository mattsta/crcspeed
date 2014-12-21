#ifndef CRC64SPEED_H
#define CRC64SPEED_H
#include "crcspeed.h"
#include "stdbool.h"

uint64_t crc64(uint64_t crc, void *data, uint64_t len);

bool crc64speed_init(void);
uint64_t crc64speed(uint64_t crc, const unsigned char *s, uint64_t l);
uint64_t crc64_lookup(uint64_t crc, void *in_data, uint64_t len);

bool crc64speed_init_big(void);
uint64_t crc64speed_big(uint64_t crc, const unsigned char *s, uint64_t l);

bool crc64speed_init_native(void);
uint64_t crc64speed_native(uint64_t crc, const unsigned char *s, uint64_t l);
#endif
