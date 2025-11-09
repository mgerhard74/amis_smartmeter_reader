#include "Utils.h"

//#include <Arduino.h>
#include <LittleFS.h>

bool UtilsClass::fileExists(const char *fname)
{
    File f;
    f = LittleFS.open(fname, "r");
    if (f) {
        f.close();
        return true;
    }
    return false;
}

bool UtilsClass::MbusCP48IToTm(struct tm &t, const uint8_t *mbusdata)
{
    // MBUS CP48 - Datum & Uhrzeit im I-Format[6Bytes-CP48] (VIF=0x6d) ==> struct tm
    //        Verwandt dazu:          F-Format[4Bytes-CP32] und G-Format[2Bytes-CP16]

    memset(&t, 0, sizeof(t));

    // https://github.com/rscada/libmbus/blob/cb28aca76e79d7852e3bf3c052c46f664d0fcdf5/mbus/mbus-protocol.c#L825
    // https://github.com/volkszaehler/vzlogger/blob/0b2440707bc7e5b4bfc4455601bccb6fa0e1c20e/src/protocols/MeterOMS.cpp#L591
    // https://github.com/volkszaehler/vzlogger/issues/176#issuecomment-118855799

    if ((mbusdata[1] & 0x80) == 0x80) {     // check for time invalid at bit 16
        return false;
    }

    // tm_yday = // days since January 1

    t.tm_sec   = mbusdata[0] & 0x3f; // (C99/C++11) 0..60 // MBUS: 0..59
    t.tm_isdst = (mbusdata[0] & 0x40) ? 1 : 0;  // day saving time
    // bool leypYear = (mbusdata[0] & 0x80) ? 1 : 0;  // Schaltjahr

    t.tm_min   = mbusdata[1] & 0x3f; // C: 0..59
    t.tm_hour  = mbusdata[2] & 0x1f; // C: 0..23

    t.tm_wday = mbusdata[2] >> 5; // C 0..6 // MBUS: 1..7 (1=MON...7=SUN, 0=notSpecified)
    if (t.tm_wday == 0) { // struct tm is 0-6 based... (0=SUN, 1=MON...6=SAT ... days since Sunday)
        return false;
    }
    if (t.tm_wday == 7) {
        t.tm_wday = 0;
    }
    t.tm_mday  = mbusdata[3] & 0x1f; // C: 1..31 // MBUS: 1..31
    t.tm_mon   = mbusdata[4] & 0x0f; // C: 0..11 // MBUS: 1..12
    t.tm_mon   -= 1; // struct tm is 0-11 based... (months since January)

    t.tm_year  = 100 + (((mbusdata[3] & 0xe0) >> 5) |  // MBUS: 0..99
                        ((mbusdata[4] & 0xf0) >> 1));  // tm_year is number of years since 1900.

#if 0
    // some more (for us not needed data) are available: week and daylightsaving-deviation
    uint8_t week = mbusdata[5] & 0x3f; // MBUS: 1..53 (0 not defined)
    int8_t daylightDeviation = mbusdata[5] >> 6; // MBUS: (0= no daylight saving time)
    if ((mbusdata[1] & 0x40) == 0) {
        daylightDeviation = -daylightDeviation;
    }
#endif
    if (t.tm_hour > 23) {
        return false;
    }
    if (t.tm_min > 59) {
        return false;
    }
    if (t.tm_sec > 59) {
        return false;
    }
    if (!UTILS_IS_VALID_YEAR(1900 + t.tm_year)) { // our current valid range: 2025...2039
        return false;
    }
    if (t.tm_mon < 0 || t.tm_mon > 11) {
        return false;
    }
    if (t.tm_mday == 0) {
        return false;
    }

    return true;
}

uint8_t UtilsClass::hexchar2Num(const char v)
{
    if (v >= 'A' && v <= 'F') {
        return v - 'A' + 10;
    } else if (v >= 'a' && v <= 'f') {
        return v - 'a' + 10;
    } else if (v >= '0' && v <= '9') {
        return v - '0';
    }
    return v;
}

/* vim:set ts=4 et: */
