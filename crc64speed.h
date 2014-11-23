#ifndef CRC64SPEED_H
#define CRC64SPEED_H

void crc64speed_init(void);
uint64_t crc64speed(uint64_t crc, const unsigned char *s, uint64_t l);

#endif
