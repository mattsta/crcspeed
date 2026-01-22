/* CRC SIMD Acceleration Implementation
 * Copyright (c) 2024
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * SIMD-accelerated CRC computation using PCLMULQDQ (x86) or PMULL (ARM64)
 * Based on Intel's "Fast CRC Computation for Generic Polynomials Using
 * PCLMULQDQ Instruction" whitepaper and Intel ISA-L implementation.
 *
 * Supports:
 * - CRC64-Jones: polynomial 0xad93d23594c935a9 (reflected)
 * - CRC16-CCITT: polynomial 0x1021 (non-reflected)
 *
 * References:
 * - Intel whitepaper: Fast CRC Computation for Generic Polynomials
 * - Intel ISA-L: https://github.com/intel/isa-l
 */

#include "crc_simd.h"
#include <string.h>

/* ============================================================================
 * Architecture Detection and Feature Testing
 * ============================================================================
 */

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) ||             \
    defined(_M_IX86)
#define ARCH_X86 1
#if defined(__GNUC__) || defined(__clang__)
#include <cpuid.h>
#endif
#include <immintrin.h>
#include <wmmintrin.h>
#elif defined(__aarch64__) || defined(_M_ARM64)
#define ARCH_ARM64 1
#include <arm_neon.h>
#if defined(__ARM_FEATURE_CRYPTO) || defined(__APPLE__)
#define HAS_PMULL 1
#include <arm_acle.h>
#endif
#endif

#if defined(_MSC_VER)
#define UNUSED 
#else
#define UNUSED __attribute__((unused))
#endif

static bool g_simd_checked = false;
static bool g_simd_available = false;

bool crc_simd_available(void) {
    if (g_simd_checked) {
        return g_simd_available;
    }
    g_simd_checked = true;

#if defined(ARCH_X86)
#if defined(__GNUC__) || defined(__clang__)
    unsigned int eax, ebx, ecx, edx;
    if (__get_cpuid(1, &eax, &ebx, &ecx, &edx)) {
        /* Check for PCLMULQDQ (bit 1 of ECX) and SSE4.1 (bit 19 of ECX) */
        g_simd_available = (ecx & (1 << 1)) && (ecx & (1 << 19));
    }
#endif
#elif defined(ARCH_ARM64)
#if defined(__APPLE__)
    /* Apple Silicon always has PMULL */
    g_simd_available = true;
#elif defined(HAS_PMULL)
    g_simd_available = true;
#else
    g_simd_available = false;
#endif
#else
    g_simd_available = false;
#endif

    return g_simd_available;
}

/* ============================================================================
 * CRC64 Jones Polynomial: 0xad93d23594c935a9 (reflected)
 *
 * Constants from Intel ISA-L crc64_jones_refl_by8.asm
 * ============================================================================
 */

/* 16-byte folding constants */
#define CRC64_RK1 UINT64_C(0x381d0015c96f4444)
#define CRC64_RK2 UINT64_C(0xd9d7be7d505da32c)

/* 128-byte folding constants */
#define CRC64_RK3 UINT64_C(0x768361524d29ed0b)
#define CRC64_RK4 UINT64_C(0xcc26fa7c57f8054c)

/* 8-way reduction constants */
#define CRC64_RK9 UINT64_C(0x5bc94ba8e2087636)
#define CRC64_RK10 UINT64_C(0x6cf09c8f37710b75)
#define CRC64_RK11 UINT64_C(0x3885fd59e440d95a)
#define CRC64_RK12 UINT64_C(0xbccba3936411fb7e)
#define CRC64_RK13 UINT64_C(0xe4dd0d81cbfce585)
#define CRC64_RK14 UINT64_C(0xb715e37b96ed8633)
#define CRC64_RK15 UINT64_C(0xf49784a634f014e4)
#define CRC64_RK16 UINT64_C(0xaf86efb16d9ab4fb)
#define CRC64_RK17 UINT64_C(0x7b3211a760160db8)
#define CRC64_RK18 UINT64_C(0xa062b2319d66692f)
#define CRC64_RK19 UINT64_C(0xef3d1d18ed889ed2)
#define CRC64_RK20 UINT64_C(0x6ba4d760ab38201e)

/* ============================================================================
 * CRC16-CCITT Polynomial: 0x1021 (non-reflected)
 *
 * Constants computed by gen_constants.c
 * ============================================================================
 */

/* 16-byte folding constants */
#define CRC16_K1 UINT64_C(0x10e2) /* x^144 mod P */
#define CRC16_K2 UINT64_C(0xaefc) /* x^128 mod P */

/* 64-byte folding constants */
#define CRC16_K3 UINT64_C(0x78b3) /* x^528 mod P */
#define CRC16_K4 UINT64_C(0x13fc) /* x^512 mod P */

/* 8-way reduction constants */
#define CRC16_RK9 UINT64_C(0x4347)  /* x^912 mod P */
#define CRC16_RK10 UINT64_C(0xcbc5) /* x^896 mod P */
#define CRC16_RK11 UINT64_C(0x9e3a) /* x^784 mod P */
#define CRC16_RK12 UINT64_C(0x106f) /* x^768 mod P */
#define CRC16_RK13 UINT64_C(0x9c1a) /* x^656 mod P */
#define CRC16_RK14 UINT64_C(0xda35) /* x^640 mod P */
#define CRC16_RK15 UINT64_C(0x78b3) /* x^528 mod P */
#define CRC16_RK16 UINT64_C(0x13fc) /* x^512 mod P */
#define CRC16_RK17 UINT64_C(0xbd64) /* x^400 mod P */
#define CRC16_RK18 UINT64_C(0xcde2) /* x^384 mod P */
#define CRC16_RK19 UINT64_C(0x8ddc) /* x^272 mod P */
#define CRC16_RK20 UINT64_C(0x8e29) /* x^256 mod P */

/* Barrett reduction constants */
#define CRC16_MU UINT64_C(0x0000f0d3) /* floor(x^32 / P) */
#define CRC16_POLY UINT64_C(0x1021)

/* ============================================================================
 * X86/X64 PCLMULQDQ Implementation
 * ============================================================================
 */

#if defined(ARCH_X86)

/* Helper: fold one 128-bit block for CRC64 (reflected) */
static inline __m128i fold_128(__m128i acc, __m128i data, __m128i k1k2) {
    __m128i t1 = _mm_clmulepi64_si128(acc, k1k2, 0x00);
    __m128i t2 = _mm_clmulepi64_si128(acc, k1k2, 0x11);
    return _mm_xor_si128(_mm_xor_si128(t1, t2), data);
}

/* Helper: byte-swap a 128-bit register (used for CRC16 non-reflected) */
UNUSED static inline __m128i bswap_128(__m128i x) {
    const __m128i mask =
        _mm_set_epi8(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
    return _mm_shuffle_epi8(x, mask);
}

/* Helper: fold one 128-bit block for CRC16 (non-reflected)
 * For non-reflected CRC, we use different PCLMULQDQ immediates
 * and the constants are positioned in the high 16 bits.
 * Note: Currently unused as we fall back to crc16speed for CRC16 SIMD on x86.
 */
UNUSED static inline __m128i
fold_128_16(__m128i acc, __m128i data, __m128i k1k2) {
    /* For non-reflected CRC16:
     * - k1 is in high 64 bits, k2 in low 64 bits
     * - We use 0x10 and 0x01 immediates (cross-multiply)
     */
    __m128i t1 = _mm_clmulepi64_si128(acc, k1k2, 0x10); /* acc_lo * k1 */
    __m128i t2 = _mm_clmulepi64_si128(acc, k1k2, 0x01); /* acc_hi * k2 */
    return _mm_xor_si128(_mm_xor_si128(t1, t2), data);
}

uint64_t crc64_simd(uint64_t crc, const void *data, uint64_t len) {
    const uint8_t *buf = (const uint8_t *)data;
    extern uint64_t crc64speed(uint64_t crc, const void *s, const uint64_t l);

    if (!crc_simd_available() || len < 32) {
        return crc64speed(crc, data, len);
    }

    const __m128i rk1_rk2 = _mm_set_epi64x(CRC64_RK1, CRC64_RK2);

    /* Large buffer: 8-way parallel folding */
    if (len >= 256) {
        const __m128i rk3_rk4 = _mm_set_epi64x(CRC64_RK3, CRC64_RK4);

        __m128i acc0 = _mm_loadu_si128((const __m128i *)(buf + 0));
        __m128i acc1 = _mm_loadu_si128((const __m128i *)(buf + 16));
        __m128i acc2 = _mm_loadu_si128((const __m128i *)(buf + 32));
        __m128i acc3 = _mm_loadu_si128((const __m128i *)(buf + 48));
        __m128i acc4 = _mm_loadu_si128((const __m128i *)(buf + 64));
        __m128i acc5 = _mm_loadu_si128((const __m128i *)(buf + 80));
        __m128i acc6 = _mm_loadu_si128((const __m128i *)(buf + 96));
        __m128i acc7 = _mm_loadu_si128((const __m128i *)(buf + 112));

        __m128i crc_vec = _mm_set_epi64x(0, crc);
        acc0 = _mm_xor_si128(acc0, crc_vec);

        buf += 128;
        len -= 128;

        while (len >= 128) {
            __m128i d0 = _mm_loadu_si128((const __m128i *)(buf + 0));
            __m128i d1 = _mm_loadu_si128((const __m128i *)(buf + 16));
            __m128i d2 = _mm_loadu_si128((const __m128i *)(buf + 32));
            __m128i d3 = _mm_loadu_si128((const __m128i *)(buf + 48));
            __m128i d4 = _mm_loadu_si128((const __m128i *)(buf + 64));
            __m128i d5 = _mm_loadu_si128((const __m128i *)(buf + 80));
            __m128i d6 = _mm_loadu_si128((const __m128i *)(buf + 96));
            __m128i d7 = _mm_loadu_si128((const __m128i *)(buf + 112));

            acc0 = fold_128(acc0, d0, rk3_rk4);
            acc1 = fold_128(acc1, d1, rk3_rk4);
            acc2 = fold_128(acc2, d2, rk3_rk4);
            acc3 = fold_128(acc3, d3, rk3_rk4);
            acc4 = fold_128(acc4, d4, rk3_rk4);
            acc5 = fold_128(acc5, d5, rk3_rk4);
            acc6 = fold_128(acc6, d6, rk3_rk4);
            acc7 = fold_128(acc7, d7, rk3_rk4);

            buf += 128;
            len -= 128;
        }

        const __m128i rk9_10 = _mm_set_epi64x(CRC64_RK9, CRC64_RK10);
        const __m128i rk11_12 = _mm_set_epi64x(CRC64_RK11, CRC64_RK12);
        const __m128i rk13_14 = _mm_set_epi64x(CRC64_RK13, CRC64_RK14);
        const __m128i rk15_16 = _mm_set_epi64x(CRC64_RK15, CRC64_RK16);
        const __m128i rk17_18 = _mm_set_epi64x(CRC64_RK17, CRC64_RK18);
        const __m128i rk19_20 = _mm_set_epi64x(CRC64_RK19, CRC64_RK20);

        acc7 = _mm_xor_si128(acc7, fold_128(acc0, _mm_setzero_si128(), rk9_10));
        acc7 =
            _mm_xor_si128(acc7, fold_128(acc1, _mm_setzero_si128(), rk11_12));
        acc7 =
            _mm_xor_si128(acc7, fold_128(acc2, _mm_setzero_si128(), rk13_14));
        acc7 =
            _mm_xor_si128(acc7, fold_128(acc3, _mm_setzero_si128(), rk15_16));
        acc7 =
            _mm_xor_si128(acc7, fold_128(acc4, _mm_setzero_si128(), rk17_18));
        acc7 =
            _mm_xor_si128(acc7, fold_128(acc5, _mm_setzero_si128(), rk19_20));
        acc7 =
            _mm_xor_si128(acc7, fold_128(acc6, _mm_setzero_si128(), rk1_rk2));

        __m128i acc = acc7;

        while (len >= 16) {
            __m128i data_vec = _mm_loadu_si128((const __m128i *)buf);
            acc = fold_128(acc, data_vec, rk1_rk2);
            buf += 16;
            len -= 16;
        }

        uint8_t final_buf[32];
        _mm_storeu_si128((__m128i *)final_buf, acc);

        if (len > 0) {
            memcpy(final_buf + 16, buf, len);
            return crc64speed(0, final_buf, 16 + len);
        } else {
            return crc64speed(0, final_buf, 16);
        }
    }

    /* Medium buffer: simple 16-byte folding */
    uint64_t blocks = len / 16;

    if (blocks < 2) {
        return crc64speed(crc, data, len);
    }

    __m128i acc = _mm_loadu_si128((const __m128i *)buf);
    __m128i crc_vec = _mm_set_epi64x(0, crc);
    acc = _mm_xor_si128(acc, crc_vec);
    buf += 16;
    blocks--;

    while (blocks > 0) {
        __m128i data_vec = _mm_loadu_si128((const __m128i *)buf);
        acc = fold_128(acc, data_vec, rk1_rk2);
        buf += 16;
        blocks--;
    }

    uint64_t remaining = len % 16;

    uint8_t final_buf[32];
    _mm_storeu_si128((__m128i *)final_buf, acc);

    if (remaining > 0) {
        memcpy(final_buf + 16, buf, remaining);
        return crc64speed(0, final_buf, 16 + remaining);
    } else {
        return crc64speed(0, final_buf, 16);
    }
}

void crc64_simd_init(void) {
    crc_simd_available();
}

/* ============================================================================
 * CRC16 SIMD Implementation (x86/x64 with PCLMULQDQ)
 *
 * CRC16-CCITT (polynomial 0x1021) is a non-reflected CRC.
 *
 * Non-reflected CRCs process data MSB-first, which requires:
 * 1. Byte-swapping data to get correct byte order for PCLMULQDQ
 * 2. Positioning CRC in high bits
 * 3. Different folding constant interpretation
 *
 * This implementation uses the marzooqy/crc-clmul approach:
 * - Data is byte-swapped (reversed within 128-bit blocks)
 * - CRC goes in high 64 bits
 * - For non-reflected: PCLMULQDQ multiplies acc_hi*k_hi and acc_lo*k_lo
 * - Final reduction via table lookup
 *
 * Folding constants for CRC16-CCITT polynomial 0x1021:
 * - x^192 mod P = 0x650b (for high 64-bit lane)
 * - x^128 mod P = 0xaefc (for low 64-bit lane)
 * ============================================================================
 */

/* CRC16-CCITT folding constants for 128-bit blocks */
/* x^192 mod P (for high 64-bit lane) */
#define CRC16_K_HI_X86 UINT64_C(0x650b)

/* x^128 mod P (for low 64-bit lane) */
#define CRC16_K_LO_X86 UINT64_C(0xaefc)

/* Byte-swap 128-bit register: reverse all 16 bytes */
static inline __m128i crc16_bswap_128_x86(__m128i x) {
    const __m128i mask =
        _mm_setr_epi8(15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0);
    return _mm_shuffle_epi8(x, mask);
}

/* Fold helper for non-reflected CRC16 on x86
 * For non-reflected: we multiply acc_hi by k_hi, acc_lo by k_lo
 * Constants packed as: k_hi in high 64 bits, k_lo in low 64 bits
 */
static inline __m128i crc16_fold_x86(__m128i acc, __m128i data, __m128i k) {
    /* Non-reflected folding:
     * - acc_hi (high 64) gets multiplied by k_hi (high 64) -> use 0x11
     * - acc_lo (low 64) gets multiplied by k_lo (low 64) -> use 0x00
     */
    __m128i t1 = _mm_clmulepi64_si128(acc, k, 0x11); /* acc_hi * k_hi */
    __m128i t2 = _mm_clmulepi64_si128(acc, k, 0x00); /* acc_lo * k_lo */
    return _mm_xor_si128(_mm_xor_si128(t1, t2), data);
}

uint16_t crc16_simd(uint16_t crc, const void *data, uint64_t len) {
    const uint8_t *buf = (const uint8_t *)data;
    extern uint16_t crc16speed(uint16_t crc, const void *s, const uint64_t l);

    /* Use table-based for small buffers - SIMD overhead not worth it */
    if (!crc_simd_available() || len < 64) {
        return crc16speed(crc, data, len);
    }

    /* Process 16-byte blocks using SIMD folding */
    uint64_t blocks = len / 16;
    if (blocks < 2) {
        return crc16speed(crc, data, len);
    }

    /* Constants: k_hi in high 64 bits, k_lo in low 64 bits */
    const __m128i k = _mm_set_epi64x(CRC16_K_HI_X86, CRC16_K_LO_X86);

    /* Load and byte-swap first 16 bytes */
    __m128i acc = crc16_bswap_128_x86(_mm_loadu_si128((const __m128i *)buf));

    /* XOR initial CRC into high 16 bits of high 64-bit lane
     * For non-reflected CRC, the CRC value is conceptually at the MSB end
     */
    uint64_t crc_shifted = (uint64_t)crc << 48;
    __m128i crc_vec = _mm_set_epi64x(crc_shifted, 0);
    acc = _mm_xor_si128(acc, crc_vec);

    buf += 16;
    blocks--;

    /* Main folding loop */
    while (blocks > 0) {
        /* Load and byte-swap next 16 bytes */
        __m128i data_vec =
            crc16_bswap_128_x86(_mm_loadu_si128((const __m128i *)buf));

        /* Fold accumulator with new data */
        acc = crc16_fold_x86(acc, data_vec, k);

        buf += 16;
        blocks--;
    }

    /* Store folded result (byte-swap back to original order) */
    __m128i acc_swapped = crc16_bswap_128_x86(acc);

    uint8_t final_buf[32];
    _mm_storeu_si128((__m128i *)final_buf, acc_swapped);

    /* Process remaining bytes through table lookup */
    uint64_t remaining = len % 16;
    if (remaining > 0) {
        memcpy(final_buf + 16, buf, remaining);
        return crc16speed(0, final_buf, 16 + remaining);
    } else {
        return crc16speed(0, final_buf, 16);
    }
}

void crc16_simd_init(void) {
    crc_simd_available();
}

#elif defined(ARCH_ARM64) && defined(HAS_PMULL)

/* ============================================================================
 * ARM64 NEON/PMULL Implementation
 * ============================================================================
 */

static inline uint64x2_t fold_128_neon(uint64x2_t acc, uint64x2_t data,
                                       uint64_t k1, uint64_t k2) {
    poly128_t t1 = vmull_p64(vgetq_lane_u64(acc, 0), k2);
    poly128_t t2 = vmull_p64(vgetq_lane_u64(acc, 1), k1);
    uint64x2_t t1_vec = vreinterpretq_u64_p128(t1);
    uint64x2_t t2_vec = vreinterpretq_u64_p128(t2);
    return veorq_u64(veorq_u64(t1_vec, t2_vec), data);
}

/* Byte-swap for NEON - used for non-reflected CRCs (currently unused, kept for
 * reference) */
#if 0
static inline uint8x16_t bswap_128_neon(uint8x16_t x) {
    return vrev64q_u8(x);
}
#endif

uint64_t crc64_simd(uint64_t crc, const void *data, uint64_t len) {
    const uint8_t *buf = (const uint8_t *)data;
    extern uint64_t crc64speed(uint64_t crc, const void *s, const uint64_t l);

    if (!crc_simd_available() || len < 32) {
        return crc64speed(crc, data, len);
    }

    if (len >= 256) {
        uint64x2_t acc0 = vld1q_u64((const uint64_t *)(buf + 0));
        uint64x2_t acc1 = vld1q_u64((const uint64_t *)(buf + 16));
        uint64x2_t acc2 = vld1q_u64((const uint64_t *)(buf + 32));
        uint64x2_t acc3 = vld1q_u64((const uint64_t *)(buf + 48));
        uint64x2_t acc4 = vld1q_u64((const uint64_t *)(buf + 64));
        uint64x2_t acc5 = vld1q_u64((const uint64_t *)(buf + 80));
        uint64x2_t acc6 = vld1q_u64((const uint64_t *)(buf + 96));
        uint64x2_t acc7 = vld1q_u64((const uint64_t *)(buf + 112));

        uint64x2_t crc_vec = vsetq_lane_u64(crc, vdupq_n_u64(0), 0);
        acc0 = veorq_u64(acc0, crc_vec);

        buf += 128;
        len -= 128;

        while (len >= 128) {
            uint64x2_t d0 = vld1q_u64((const uint64_t *)(buf + 0));
            uint64x2_t d1 = vld1q_u64((const uint64_t *)(buf + 16));
            uint64x2_t d2 = vld1q_u64((const uint64_t *)(buf + 32));
            uint64x2_t d3 = vld1q_u64((const uint64_t *)(buf + 48));
            uint64x2_t d4 = vld1q_u64((const uint64_t *)(buf + 64));
            uint64x2_t d5 = vld1q_u64((const uint64_t *)(buf + 80));
            uint64x2_t d6 = vld1q_u64((const uint64_t *)(buf + 96));
            uint64x2_t d7 = vld1q_u64((const uint64_t *)(buf + 112));

            acc0 = fold_128_neon(acc0, d0, CRC64_RK3, CRC64_RK4);
            acc1 = fold_128_neon(acc1, d1, CRC64_RK3, CRC64_RK4);
            acc2 = fold_128_neon(acc2, d2, CRC64_RK3, CRC64_RK4);
            acc3 = fold_128_neon(acc3, d3, CRC64_RK3, CRC64_RK4);
            acc4 = fold_128_neon(acc4, d4, CRC64_RK3, CRC64_RK4);
            acc5 = fold_128_neon(acc5, d5, CRC64_RK3, CRC64_RK4);
            acc6 = fold_128_neon(acc6, d6, CRC64_RK3, CRC64_RK4);
            acc7 = fold_128_neon(acc7, d7, CRC64_RK3, CRC64_RK4);

            buf += 128;
            len -= 128;
        }

        uint64x2_t zero = vdupq_n_u64(0);
        acc7 =
            veorq_u64(acc7, fold_128_neon(acc0, zero, CRC64_RK9, CRC64_RK10));
        acc7 =
            veorq_u64(acc7, fold_128_neon(acc1, zero, CRC64_RK11, CRC64_RK12));
        acc7 =
            veorq_u64(acc7, fold_128_neon(acc2, zero, CRC64_RK13, CRC64_RK14));
        acc7 =
            veorq_u64(acc7, fold_128_neon(acc3, zero, CRC64_RK15, CRC64_RK16));
        acc7 =
            veorq_u64(acc7, fold_128_neon(acc4, zero, CRC64_RK17, CRC64_RK18));
        acc7 =
            veorq_u64(acc7, fold_128_neon(acc5, zero, CRC64_RK19, CRC64_RK20));
        acc7 = veorq_u64(acc7, fold_128_neon(acc6, zero, CRC64_RK1, CRC64_RK2));

        uint64x2_t acc = acc7;

        while (len >= 16) {
            uint64x2_t data_vec = vld1q_u64((const uint64_t *)buf);
            acc = fold_128_neon(acc, data_vec, CRC64_RK1, CRC64_RK2);
            buf += 16;
            len -= 16;
        }

        uint8_t final_buf[32];
        vst1q_u8(final_buf, vreinterpretq_u8_u64(acc));

        if (len > 0) {
            memcpy(final_buf + 16, buf, len);
            return crc64speed(0, final_buf, 16 + len);
        } else {
            return crc64speed(0, final_buf, 16);
        }
    }

    uint64_t blocks = len / 16;

    if (blocks < 2) {
        return crc64speed(crc, data, len);
    }

    uint64x2_t acc = vld1q_u64((const uint64_t *)buf);
    uint64x2_t crc_vec = vsetq_lane_u64(crc, vdupq_n_u64(0), 0);
    acc = veorq_u64(acc, crc_vec);
    buf += 16;
    blocks--;

    while (blocks > 0) {
        uint64x2_t data_vec = vld1q_u64((const uint64_t *)buf);
        acc = fold_128_neon(acc, data_vec, CRC64_RK1, CRC64_RK2);
        buf += 16;
        blocks--;
    }

    uint64_t remaining = len % 16;

    uint8_t final_buf[32];
    vst1q_u8(final_buf, vreinterpretq_u8_u64(acc));

    if (remaining > 0) {
        memcpy(final_buf + 16, buf, remaining);
        return crc64speed(0, final_buf, 16 + remaining);
    } else {
        return crc64speed(0, final_buf, 16);
    }
}

void crc64_simd_init(void) {
    crc_simd_available();
}

/* ============================================================================
 * CRC16 SIMD Implementation for ARM64
 *
 * CRC16-CCITT (polynomial 0x1021) is a non-reflected CRC.
 *
 * Non-reflected CRCs process data MSB-first, which requires:
 * 1. Byte-swapping data to get correct byte order for PMULL
 * 2. Positioning CRC in high bits
 * 3. Different folding constant interpretation
 *
 * This implementation uses the marzooqy/crc-clmul approach:
 * - Data is byte-swapped (reversed within 128-bit blocks)
 * - CRC goes in high 64 bits: SET(crc << 48, 0)
 * - For non-reflected: PMULL immediates swap acc_hi*k1 and acc_lo*k2
 * - Final reduction via table lookup
 *
 * Folding constants for CRC16-CCITT polynomial 0x1021:
 * - x^192 mod P = 0x650b (for high 64-bit lane)
 * - x^128 mod P = 0xaefc (for low 64-bit lane)
 * ============================================================================
 */

/* CRC16-CCITT folding constants for 128-bit blocks */
#define CRC16_K_HI UINT64_C(0x650b) /* x^192 mod P (for high 64-bit lane) */
#define CRC16_K_LO UINT64_C(0xaefc) /* x^128 mod P (for low 64-bit lane) */

/* Byte-swap 128 bits: reverse all 16 bytes */
static inline uint8x16_t crc16_bswap_128(uint8x16_t x) {
    /* Reverse bytes within each 64-bit lane, then swap lanes */
    uint8x16_t rev = vrev64q_u8(x);
    return vcombine_u8(vget_high_u8(rev), vget_low_u8(rev));
}

/* Fold helper for non-reflected CRC16
 * For non-reflected: we multiply acc_hi by k_hi, acc_lo by k_lo
 * This is different from reflected where we use 0x00 and 0x11 immediates
 */
static inline uint64x2_t crc16_fold_neon(uint64x2_t acc, uint64x2_t data,
                                         uint64_t k_hi, uint64_t k_lo) {
    /* Non-reflected folding:
     * - acc_hi (lane 1) gets multiplied by k_hi
     * - acc_lo (lane 0) gets multiplied by k_lo
     */
    poly128_t t1 = vmull_p64(vgetq_lane_u64(acc, 1), k_hi); /* acc_hi * k_hi */
    poly128_t t2 = vmull_p64(vgetq_lane_u64(acc, 0), k_lo); /* acc_lo * k_lo */
    uint64x2_t t1_vec = vreinterpretq_u64_p128(t1);
    uint64x2_t t2_vec = vreinterpretq_u64_p128(t2);
    return veorq_u64(veorq_u64(t1_vec, t2_vec), data);
}

uint16_t crc16_simd(uint16_t crc, const void *data, uint64_t len) {
    extern uint16_t crc16speed(uint16_t crc, const void *s, const uint64_t l);
    const uint8_t *buf = (const uint8_t *)data;

    /* Use table-based for small buffers - SIMD overhead not worth it */
    if (!crc_simd_available() || len < 64) {
        return crc16speed(crc, data, len);
    }

    /* Process 16-byte blocks using SIMD folding */
    uint64_t blocks = len / 16;
    if (blocks < 2) {
        return crc16speed(crc, data, len);
    }

    /* Load and byte-swap first 16 bytes */
    uint8x16_t data8 = vld1q_u8(buf);
    data8 = crc16_bswap_128(data8);
    uint64x2_t acc = vreinterpretq_u64_u8(data8);

    /* XOR initial CRC into high 16 bits of high 64-bit lane
     * For non-reflected CRC, the CRC value is conceptually at the MSB end
     */
    uint64_t crc_shifted = (uint64_t)crc << 48;
    uint64x2_t crc_vec = vsetq_lane_u64(crc_shifted, vdupq_n_u64(0), 1);
    acc = veorq_u64(acc, crc_vec);

    buf += 16;
    blocks--;

    /* Main folding loop */
    while (blocks > 0) {
        /* Load and byte-swap next 16 bytes */
        data8 = vld1q_u8(buf);
        data8 = crc16_bswap_128(data8);
        uint64x2_t data_vec = vreinterpretq_u64_u8(data8);

        /* Fold accumulator with new data */
        acc = crc16_fold_neon(acc, data_vec, CRC16_K_HI, CRC16_K_LO);

        buf += 16;
        blocks--;
    }

    /* Store folded result (byte-swap back to original order) */
    uint8x16_t acc8 = vreinterpretq_u8_u64(acc);
    acc8 = crc16_bswap_128(acc8);

    uint8_t final_buf[32];
    vst1q_u8(final_buf, acc8);

    /* Process remaining bytes through table lookup */
    uint64_t remaining = len % 16;
    if (remaining > 0) {
        memcpy(final_buf + 16, buf, remaining);
        return crc16speed(0, final_buf, 16 + remaining);
    } else {
        return crc16speed(0, final_buf, 16);
    }
}

void crc16_simd_init(void) {
    crc_simd_available();
}

#else /* No SIMD support */

bool crc_simd_available(void);

uint64_t crc64_simd(uint64_t crc, const void *data, uint64_t len) {
    extern uint64_t crc64speed(uint64_t crc, const void *s, const uint64_t l);
    return crc64speed(crc, data, len);
}

void crc64_simd_init(void) {
}

uint16_t crc16_simd(uint16_t crc, const void *data, uint64_t len) {
    extern uint16_t crc16speed(uint16_t crc, const void *s, const uint64_t l);
    return crc16speed(crc, data, len);
}

void crc16_simd_init(void) {
}

#endif /* Architecture selection */
