#include "Utils.h"

#include <cont.h>
#include <LittleFS.h>


/* Handle LittlSFS creation/modification timestamps */
static time_t _littleFS_TimeStamp = (time_t) 0;

static time_t _littleFS_GetTimeStampCb()
{
    if (_littleFS_TimeStamp != 0) {
        return _littleFS_TimeStamp;
    }
    return time(NULL);  // static time_t _defaultTimeCB(void) { return time(NULL); }
}

/*
time_t Utils::littleFSgetTimeStamp()
{
    return _littleFS_TimeStamp;
}
*/

time_t Utils::littleFSsetTimeStamp(time_t timeStamp)
{
    time_t oldTimeStamp = _littleFS_TimeStamp;
    _littleFS_TimeStamp = timeStamp;
    return oldTimeStamp;
}

void Utils::init()
{
    LittleFS.setTimeCallback(&_littleFS_GetTimeStampCb);
}


String Utils::escapeJson(const char *str, size_t strlen, size_t maxlen) {
    String escaped = "";
    if (str) {
        size_t m = 0;
        for (size_t i = 0; str[i] && i < strlen && m < maxlen; i++) {
            char c = str[i];
            m += 2;
            if (c == '"') escaped += "\\\"";
            else if (c == '\\') escaped += "\\\\";
            else if (c == '\b') escaped += "\\b";
            else if (c == '\f') escaped += "\\f";
            else if (c == '\n') escaped += "\\n";
            else if (c == '\r') escaped += "\\r";
            else if (c == '\t') escaped += "\\t";
            else { escaped += c; m--; }
        }
    } else {
        escaped = F("<nullptr>");
    }
    return escaped;
}



extern "C" {
    extern uint32_t stack_thunk_get_stack_top();
    extern uint32_t stack_thunk_get_stack_bot();
}
/*
Use of ESP.getFreeContStack() depends on ESP.resetFreeContStack()
So to get available Stacksize, we just calculate it!
Details: See
    cont_t* g_pcont __attribute__((section(".noinit")));
*/
void Utils::ESP8266getStackInfo(int& context, uintptr_t& stack_bot, uintptr_t& stack_top, uintptr_t& stack_current)
{
    static_assert(sizeof(g_pcont->stack[0]) == sizeof(uint32_t), "Stack element size mismatch!");

    __asm__ __volatile__ ("mov %0, a1\n" : "=r"(stack_current));

    // Stack moves from top to down (0xyyyyyyyy to 0 )
    // Stack on 8266: "Pre-Decrement" // sp is the last used address
    // And: "Empty Stack Top" ... g_pcont->stack[1024] seems never get set

    uintptr_t bottom, top;

    // BEARSSL
    bottom = stack_thunk_get_stack_bot();
    top = stack_thunk_get_stack_top();
    if (stack_current > bottom && stack_current <= top) {
        stack_bot = bottom;
        stack_top = top;
        context = 2; // seems we're in the wifi/ssl stack ... aka "bearssl" - Seems has a size of 6200 bytes and allocated using malloc() (see define _stackSize )
        return;
    }

    // CONT/USER
    bottom = reinterpret_cast<uintptr_t>(&g_pcont->stack[0]);
    top = bottom + sizeof(g_pcont->stack);
    if (stack_current >= bottom && stack_current <= top) {
        stack_bot = bottom;
        stack_top = top;
        context = 1; // normal(user) context ... aka  "cont" // seems to be 4096 bytes (see define CONT_STACKSIZE)
        return;
    }

    // SYS
    // Hm ... we're probaly in system stack (interrupts/ISR) or special SDK-callbacks ... aka "sys" - (unknown size)
    // Interrupts (HW-Timer, WiFi-Events(callbacks), GPIO-ISRs).
    // Seems I have to guess as  ..../framework-arduinoespressif8266/tools/sdk/ld/eagle.app.v6.common.ld.h disabled symbol _stack_sentry
    // Expecting 2kB stack in sys context
    // TODO(anyone): find out bottom, top and length of sys-stack
    stack_top = bottom;
    stack_bot = stack_top - 2048;
#if 0
    stack_top = 0x3FFFFFE0
    stack_top = 0x3FFFFFFF; // End of RAM  // 0x3ffc8000
    stack_bot = stack_top - 2048;
#endif
    context = 0;
}


/* getcontext: system or user determined based on stackpointer value */
int Utils::getContext(void)
{
    int context;
    uintptr_t stack_bot, stack_top, stack_current;
    ESP8266getStackInfo(context, stack_bot, stack_top, stack_current);
    return context;
}

bool Utils::fileExists(const char *fname)
{
    bool r = false;
    File f;
    f = LittleFS.open(fname, "r");
    if (f) {
        r = f.isFile();
        f.close();
    }
    return r;
}

bool Utils::dirExists(const char *fname)
{
    bool r = false;
    File f;
    f = LittleFS.open(fname, "r");
    if (f) {
        r = f.isDirectory();
        f.close();
    }
    return r;
}

bool Utils::MbusCP48IToTm(struct tm &t, const uint8_t *mbusdata)
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
    // tm.tm_isdst
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

uint8_t Utils::hexchar2Num(const char v)
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
