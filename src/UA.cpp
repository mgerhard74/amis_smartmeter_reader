// SPDX-License-Identifier: GPL-2.0-or-later

#include "UA.h"

#include <stddef.h>

typedef union {              // für 16Bit endiness wandeln
    uint16_t u16;
    uint8_t bytes[sizeof(u16)];
} UNION_U16B;

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

void UA::swap_mem2(void *p) {
    uint8_t h;
    uint8_t *v = (uint8_t *)p;
    h = v[0]; v[0] = v[1];  v[1] = h;
}

void UA::swap_mem4(void *p) {
    uint8_t h;
    uint8_t *v = (uint8_t *)p;
    h = v[0]; v[0] = v[3];  v[0] = h;
    h = v[1]; v[1] = v[2];  v[2] = h;
}

#if not (HAVE_UA2_ACCESS)
uint16_t UA::ReadU16LE(const void *p)
{
    size_t pn = (size_t) p;

    if((pn & 0x01) == 0) { // we're already aligned to 2
        return SWAP_LE2(*(const uint16_t*)p);
    }
    UNION_U16B r;
    const uint8_t *pp = (const uint8_t *) p;

    r.bytes[0] = *pp++;
    r.bytes[1] = *pp;
    return SWAP_LE2(r.u16);
}

void UA::WriteU16LE(void *p, uint16_t v)
{
    size_t pn = (size_t) p;

    if((pn & 0x01) == 0) { // we're already aligned to 2
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


#if 0
#include "unions.h"
#include <Arduino.h>
void writeEvent(String type, String src, String desc, String data);
void validate_ua(void) {
    bool r = true;
    unsigned char buffer[32];
    unsigned char *p;

    // Do test just once after running 80sec
    static bool test_done=false;
    if (test_done) {
        return;
    }
    if (millis() < 80000) {
        return;
    }
    test_done = true;
    writeEvent("INFO", "UA", "validate_ua()", "started");

    // Also check union "conversation" float<->bytes<->integers
    // Just to be sure the compiler does it the right way
    U4ByteValues b4;
    b4.f = -1672608870000.0;
    if (b4.ui32t != 0xD3C2B7A1) {
        writeEvent("INFO", "UA", "Unionconversation b4.ui32t", "failed");
        writeEvent("INFO", "UA", "b4.f     " + String(b4.f), "");
        writeEvent("INFO", "UA", "b4.ui32t " + String(b4.ui32t), "");
        r = false;
    }
    if (b4.i32t != -742213727) {
        writeEvent("INFO", "UA", "Unionconversation b4.i32t", "failed");
        writeEvent("INFO", "UA", "b4.f    " + String(b4.f), "");
        writeEvent("INFO", "UA", "b4.i32t " + String(b4.i32t), "");
        r = false;
    }

    writeEvent("INFO", "UA", "validate_ua() 16bit tests", "started");
    for (size_t offset = 0; offset < 16; offset++) { // run thru alignments 0...15
        p = &buffer[offset];

        memset(buffer, 0, sizeof(buffer));
        UA::WriteU16LE(p, 0xDCBA);
        if (memcmp(p, "\xBA\xDC", 2)) {
            writeEvent("INFO", "UA", "UA::WriteU16LE()", "failed");
            r = false;
        }
        if (UA::ReadU16LE(p) != 0xDCBA) {
            writeEvent("INFO", "UA", "UA::ReadU16LE()" + String(UA::ReadU16LE(p)), "failed");
            r = false;
        }
        if (UA::ReadU16BE(p) != 0xBADC) {
            writeEvent("INFO", "UA", "UA::ReadU16BE()" + String(UA::ReadU16BE(p)), "failed");
            r = false;
        }


        memset(buffer, 0, sizeof(buffer));
        UA::WriteU16BE(p, 0xDCBA);
        if (memcmp(p, "\xDC\xBA", 2)) {
            writeEvent("INFO", "UA", "UA::WriteU16BE()", "failed");
            r = false;
        }
        if (UA::ReadU16LE(p) != 0xBADC) {
            writeEvent("INFO", "UA", "UA::ReadU16LE() " + String(UA::ReadU16LE(p)), "failed");
            r = false;
        }
        if (UA::ReadU16BE(p) != 0xDCBA) {
            writeEvent("INFO", "UA", "UA::ReadU16BE()" + String(UA::ReadU16BE(p)), "failed");
            r = false;
        }
    }
    writeEvent("INFO", "UA", "validate_ua() 16bit tests", "ended");


    writeEvent("INFO", "UA", "validate_ua() 32bit tests", "started");
    for (size_t offset = 0; offset < 16; offset++) { // run thru alignments 0...15
        p = &buffer[offset];
        memset(buffer, 0, sizeof(buffer));
        UA::WriteU32LE(p, 0xA1B7C2D3);
        if (memcmp(p, "\xD3\xC2\xB7\xA1", 4)) {
            writeEvent("INFO", "UA", "UA::WriteU32LE()", "failed");
            r = false;
        }
        if (UA::ReadU32LE(p) != 0xA1B7C2D3) {
            writeEvent("INFO", "UA", "UA::ReadU32LE() " + String(UA::ReadU32LE(p)), "failed");
            r = false;
        }
        if (UA::ReadS32LE(p) != -1581792557) {
            writeEvent("INFO", "UA", "UA::ReadS32LE() " + String(UA::ReadS32LE(p)), "failed");
            r = false;
        }


        memset(buffer, 0, sizeof(buffer));
        UA::WriteU32BE(p, 0xA1B7C2D3); // ABCD
        if (memcmp(p, "\xA1\xB7\xC2\xD3", 4)) {
            writeEvent("INFO", "UA", "UA::WriteU32BE()", "failed");
            r = false;
        }
        /*
        Nicht implementiert, weil wir das aktuell (noch) nicht brauchen

        if (UA::ReadU32BE(p) != 0xD3C2B7A1) {
            writeEvent("INFO", "UA", "UA::ReadU32BE()", "failed");
            r = false;
        }
        if (UA::ReadS32BE(p) != -1581792557) {
            writeEvent("INFO", "UA", "UA::ReadS32BE()", "failed");
            r = false;
        }
        */
    }
    writeEvent("INFO", "UA", "validate_ua() 32bit tests", "ended");

    if (r) {
        writeEvent("INFO", "UA", "validate_ua()", "success");
    } else {
        writeEvent("INFO", "UA", "validate_ua()", "failed");
    }
}
#endif


// vim:set ts=4 sw=4 et:
