// Helpers for unaligned acces with little and big endian

#pragma once

#include <stdint.h>

#ifndef LITTLE_ENDIAN
#define LITTLE_ENDIAN 1234
#endif
#ifndef BIG_ENDIAN
#define BIG_ENDIAN 4321
#endif

#ifndef BYTE_ORDER
#  if defined(WORDS_BIGENDIAN) || (defined(AC_APPLE_UNIVERSAL_BUILD) && defined(__BIG_ENDIAN__))
#    define BYTE_ORDER BIG_ENDIAN
#  else /* WORDS_BIGENDIAN */
#    define BYTE_ORDER LITTLE_ENDIAN
#  endif
#endif

#if !defined(BYTE_ORDER) || (BYTE_ORDER != LITTLE_ENDIAN && BYTE_ORDER != BIG_ENDIAN)
#error Define BYTE_ORDER to be equal to either LITTLE_ENDIAN or BIG_ENDIAN
#endif


// It seems there are some 8266 ESP12F out they allow unaligned access ?
// For now: Don't trust all the devices support it
#define HAVE_UA2_ACCESS 0
#define HAVE_UA4_ACCESS 0

#if (BYTE_ORDER == LITTLE_ENDIAN) // ESP8266 should be little endian!
#define SWAP_LE2(V) (V)
#define SWAP_LE4(V) (V)
#define SWAP_BE2(V) UA::swap2(V)
#define SWAP_BE4(V) UA::swap4(V)
#else
#define SWAP_LE2(V) UA::swap2(V)
#define SWAP_LE4(V) UA::swap4(V)
#define SWAP_BE2(V) (V)
#define SWAP_BE4(V) (V)
#endif


class UA {
public:
    static uint16_t swap2(uint16_t v);
    static uint32_t swap4(uint32_t v);

#if (HAVE_UA2_ACCESS)
    static void WriteU16LE(void *p, uint16_t v) { *(uint16_t*)p = SWAP_LE2(v); }
    static void WriteU16BE(void *p, uint16_t v) { *(uint16_t*)p = SWAP_BE2(v); }
#else
    static void WriteU16LE(void *p, uint16_t v);
    static void WriteU16BE(void *p, uint16_t v) { WriteU16LE(p, SWAP_BE2(v)); };
#endif

#if (HAVE_UA4_ACCESS)
    static int32_t  ReadS32LE(const void *p) { return SWAP_LE4(*(const int32_t*)p); }
    static uint32_t ReadU32LE(const void *p) { return SWAP_BE4(*(const uint32_t*)p); }
    static void WriteU32LE(void *p, uint32_t v) { *(uint32_t*)p = SWAP_LE4(v); }
    static void WriteU32BE(void *p, uint32_t v) { *(uint32_t*)p = SWAP_BE4(v); }
#else
    static int32_t  ReadS32LE(const void *p) { return (int32_t) ReadU32LE(p); };
    static uint32_t ReadU32LE(const void *p);
    static void WriteU32LE(void *p, uint32_t v);
    static void WriteU32BE(void *p, uint32_t v) { WriteU32LE(p, SWAP_BE4(v)); };
#endif
};


// vim:set ts=4 sw=4 et:
