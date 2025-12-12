// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <stdint.h>
#include <stddef.h> // needed for size_t
#include <time.h>   // needed for time_t

/* Check and "documentate" sizes of some data types */
static_assert(sizeof(int) == 4);
static_assert(sizeof(long) == 4);
static_assert(sizeof(long long) == 8);

static_assert(sizeof(unsigned int) == 4);
static_assert(sizeof(unsigned long) == 4);
static_assert(sizeof(unsigned long long) == 8);

static_assert(sizeof(int16_t) == 2);
static_assert(sizeof(uint16_t) == 2);
static_assert(sizeof(int32_t) == 4);
static_assert(sizeof(uint32_t) == 4);
static_assert(sizeof(int64_t) == 8);
static_assert(sizeof(uint64_t) == 8);

static_assert(sizeof(size_t) == 4);

static_assert(sizeof(time_t) == 8);

static_assert(sizeof(float) == 4);
static_assert(sizeof(double) == 8);


// für Konvertierung 4 und 8 Byte Variablen
typedef union {
    int i;
    unsigned int ui;
    long l;
    unsigned long ul;
    float f;
    char c[4];
    unsigned char uc[4];
    int8_t i8[4];
    uint8_t ui8[4];
    int16_t i16[2];
    uint16_t ui16[2];
    int32_t i32t;
    uint32_t ui32t;
} U4ByteValues;
static_assert(sizeof(U4ByteValues) == 4);

typedef union {
    long long ll;
    unsigned long long ull;
    double d;
    char c[8];
    unsigned char uc[8];
    int8_t i8[8];
    uint8_t ui8[8];
    int16_t i16[4];
    uint16_t ui16[4];
    int32_t i32t[2];
    uint32_t ui32t[2];
    int64_t i64t;
    uint64_t ui64t;
    time_t timet;
} U8ByteValues;
static_assert(sizeof(U8ByteValues) == 8);


// vim:set ts=4 sw=4 et:
