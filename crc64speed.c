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

#include "crcspeed.h"
#include "crc64.h"

static uint64_t crc64_table[8][256] = {{0}};

void crc64speed_init(void) {
    crcfn64 forced = (crcfn64)crc64;
    crcspeed_init64(forced, crc64_table);
}

uint64_t crc64speed(uint64_t crc, const unsigned char *s, uint64_t l) {
    return crcspeed64(crc64_table, crc, (void *)s, l);
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
        (unsigned long long) crc64speed(0,(unsigned char*)"123456789",9));
    return 0;
}
#endif

#ifdef REDIS_TEST_MAIN
int main(int argc, char *argv[]) {
    return crc64Test(argc, argv);
}
#endif
