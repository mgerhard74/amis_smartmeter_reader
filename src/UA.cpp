// SPDX-License-Identifier: GPL-2.0-or-later

#include "UA.h"

#include <stddef.h>

typedef union {              // für 32Bit endiness wandeln
    uint32_t u32;
    uint8_t bytes[sizeof(u32)];
} UNION_U32B;


uint16_t UA::swap2(uint16_t v) {
    return (v >> 8) | ((v & 0xff) << 8);
}

uint32_t UA::swap4(uint32_t v) {
    return (v >> 24) | ((v & 0xff0000) >> 8) | ((v & 0xff00) << 8) | (v << 24) ;
}

#if not (HAVE_UA2_ACCESS)
void WriteU16LE(void *p, uint16_t v)
{
    size_t pn = (size_t) p;

    if((pn & 0x01u) == 0) { // we're already aligned to 2
        *(uint16_t*)p = SWAP_LE2(v);
        return;
    }

    uint8_t *pp = (uint8_t *) p;
    uint8_t *vv = (uint8_t *) &v;
#if (BYTE_ORDER == LITTLE_ENDIAN)
    *pp++ = vv[0]; *pp = vv[1];
#else
    *pp++ = vv[1]; *pp = vv[0];
#endif
}
#endif // #if not (HAVE_UA2_ACCESS)

#if not (HAVE_UA4_ACCESS)
uint32_t UA::ReadU32LE(const void *p)
{
    size_t pn = (size_t) p;

    if((pn & 0x03) == 0) { // we're already aligned to 4
        return SWAP_LE4(*(const uint32_t*)p);
    }
    UNION_U32B r;
    const uint8_t *pp = (const uint8_t *) p;

    r.bytes[0] = *pp++;
    r.bytes[1] = *pp++;
    r.bytes[2] = *pp++;
    r.bytes[3] = *pp;
    return SWAP_LE4(r.u32);
}

void UA::WriteU32LE(void *p, uint32_t v)
{
    size_t pn = (size_t) p;

    if((pn & 0x03) == 0) { // we're already aligned to 4
        *(uint32_t*)p = SWAP_LE4(v);
        return;
    }

    uint8_t *pp = (uint8_t *) p;
    uint8_t *vv = (uint8_t *) &v;
#if (BYTE_ORDER == LITTLE_ENDIAN)
    *pp++ = vv[0]; *pp++ = vv[1]; *pp++ = vv[2]; *pp = vv[3];
#else
    *pp++ = vv[3]; *pp++ = vv[2]; *pp++ = vv[1]; *pp = vv[0];
#endif
}
#endif // #if not (HAVE_UA4_ACCESS)




#if 0
typedef struct  __attribute__((__packed__))
{
uint8_t x;
uint32_t y;
} test;

void ttest1(uint32_t *p) {
    *p = (*p)+1;
}

void ttest2(void *p) {
    UA::WriteU32LE(p, UA::ReadU32LE(p)+1);
}

void test_ua(void) {
    test t;

    MsgOut.printf("%08x\r\n", (size_t)(&t));
    MsgOut.printf("%08x\r\n", (size_t)(&t.y));
    uint64_t ms = micros64();
    for (size_t i = 0; i< 5000; i++) {
      ttest1(&t.y);
    }
    uint64_t me = micros64();
    MsgOut.printf("%llu -> %llu = %llu\r\n", ms, me, me-ms);

    ms = micros64();
    for (size_t i = 0; i< 5000; i++) {
      ttest2(&t.y);
    }
    me = micros64();
    MsgOut.printf("%llu -> %llu = %llu\r\n", ms, me, me-ms);
}
#endif


// vim:set ts=4 sw=4 et:
