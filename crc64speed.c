/* Copyright (c) 2014, Matt Stancliff <matt@genges.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE. */

#include "crc64speed.h"

/* If CRCSPEED64_DUAL is defined, we allow calls to
 * both _little and _big CRC.
 * By default, we only allow one endianness to be used
 * and the first call to either _init function will set the
 * lookup table endianness for the life of this module.
 * We don't enable dual lookups by default because
 * each 8x256 lookup table is 16k. */
#ifndef CRC64SPEED_DUAL
static uint64_t crc64_table[8][256] = { { 0 } };
static void *crc64_table_little = NULL, *crc64_table_big = NULL;
static const bool dual = false;
#else
static uint64_t crc64_table_little[8][256] = { { 0 } };
static uint64_t crc64_table_big[8][256] = { { 0 } };
static void *crc64_table = NULL;
static const bool dual = true;
#endif

/* Returns false if table already initialized. */
bool crc64speed_init(void) {
#ifndef CRC64SPEED_DUAL
    if (crc64_table[0][1] != 0)
        return false;
#endif
    crcfn64 forced = (crcfn64)crc64;
    crcspeed64little_init(forced, dual ? crc64_table_little : crc64_table);
    return true;
}

/* Returns false if table already initialized. */
bool crc64speed_init_big(void) {
#ifndef CRC64SPEED_DUAL
    if (crc64_table[0][1] != 0)
        return false;
#endif
    crcfn64 forced = (crcfn64)crc64;
    crcspeed64big_init(forced, dual ? crc64_table_big : crc64_table);
    return true;
}

uint64_t crc64speed(uint64_t crc, const unsigned char *s, uint64_t l) {
    /* Quickly check if CRC table is initialized to little endian correctly. */
    if (crc64_table[0][1] != UINT64_C(0x7ad870c830358979))
        return -1;

    return crcspeed64little(dual ? crc64_table_little : crc64_table, crc,
                            (void *)s, l);
}

uint64_t crc64speed_big(uint64_t crc, const unsigned char *s, uint64_t l) {
    /* Quickly check if CRC table is initialized to big endian correctly. */
    if (crc64_table[0][1] != UINT64_C(0x79893530c870d87a))
        return -1;

    return crcspeed64big(dual ? crc64_table_big : crc64_table, crc, (void *)s,
                         l);
}

bool crc64speed_init_native(void) {
    const uint64_t n = 1;
    return *(char *)&n ? crc64speed_init() : crc64speed_init_big();
}

/* If you are on a platform where endianness can change at runtime, this
 * will break unless you compile with CRC64SPEED_DUAL and manually run
 * _init() and _init_big() instead of using _init_native() */
uint64_t crc64speed_native(uint64_t crc, const unsigned char *s, uint64_t l) {
    const uint64_t n = 1;
    return *(char *)&n ? crc64speed(crc, s, l) : crc64speed_big(crc, s, l);
}


/* Test main */
#if defined(REDIS_TEST) || defined(REDIS_TEST_MAIN)
#include <stdio.h>

#define UNUSED(x) (void)(x)
int crc64Test(int argc, char *argv[]) {
    UNUSED(argc);
    UNUSED(argv);
    crc64speed_init();
    printf("e9c6d914c4b8d9ca == %016llx\n",
           (unsigned long long)crc64speed(0, (unsigned char *)"123456789", 9));
    return 0;
}
#endif

#ifdef REDIS_TEST_MAIN
int main(int argc, char *argv[]) { return crc64Test(argc, argv); }
#endif
