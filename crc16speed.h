#ifndef CRC16SPEED_H
#define CRC16SPEED_H
#include "crcspeed.h"
#include "crc16.h"
#include "stdbool.h"

bool crc16speed_init(void);
uint16_t crc16speed(uint16_t crc, const unsigned char *s, uint64_t l);

bool crc16speed_init_big(void);
uint16_t crc16speed_big(uint16_t crc, const unsigned char *s, uint64_t l);

bool crc16speed_init_native(void);
uint16_t crc16speed_native(uint16_t crc, const unsigned char *s, uint64_t l);
#endif
