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

    uintptr_t bottom, top;

    // BEARSSL
    bottom = stack_thunk_get_stack_bot();
    top = stack_thunk_get_stack_top() + sizeof(uint32_t); //  stack_thunk_get_stack_top() return address of last element
    if (stack_current > bottom && stack_current < top) {
        stack_bot = bottom;
        stack_top = top;
        context = 2; // seems we're in the wifi/ssl stack ... aka "bearssl" - Seems has a size of 6200 bytes and allocated using malloc() (see define _stackSize )
        return;
    }

    // CONT/USER
    bottom = reinterpret_cast<uintptr_t>(&g_pcont->stack[0]);
    top = bottom + sizeof(g_pcont->stack);
    if (stack_current >= bottom && stack_current < top) {
        // On g_pcont->stackguard1 (0x3fffefcc) we find the first stackguard (value=0xfeefeffe)
        // See cont_util.cpp / CONT_STACKGUARD
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
    // TODO(anyone): find out exact bottom, top and length of sys-stack
    // Seems the userstack (g_pcont) itself is within the systemstack.
    //   see: core_esp8266_main.cpp, app_entry_redefinable()
    //           and disable_extra4k_at_link_time()
    stack_top = reinterpret_cast<uintptr_t>(g_pcont);
    stack_bot = stack_top - 2048; // Not knowing ... guessing: 2kB
    context = 0;
}


#if 0
// Helper filling stack with our functionaddress
extern int callMe(int i, int max);
extern int callMeInc(int i, int max);
int callMeInc(int i, int max) { (void)(max); return i+1;}
int callMe(int i, int max) {
    if (i >= max) {
        return i;
    }
    i = callMe(i+1,max);
    i = callMeInc(i+1, max);
    return i;
}

static uint32_t *stackCapturePtr = nullptr;
static uint32_t stackCapture = 0, stackTop = 0, stackBot = 0;
void Utils::dumpStack(void)
{
    if (!stackCapturePtr) {
        return;
    }

    Serial.begin(115200, SERIAL_8N1); // Setzen wir ggf fürs debgging gleich mal einen default Wert
    Serial.printf_P("\n");
    // sp captured: 0x3fffee50
    Serial.printf_P("sp captured: 0x%08x\n", stackCapture);
    Serial.printf_P("sp_bot: 0x%08x\n", stackBot);
    Serial.printf_P("sp_top: 0x%08x\n", stackTop);
    Serial.printf_P("g_pcont: %p\n", g_pcont);
    // stackguard1: 0x3fffefcc = 0xfeefeffe
    Serial.printf_P("stackguard1: %p = 0x%08x\n", &g_pcont->stack_guard1, g_pcont->stack_guard1);
    // stackguard2: 0x3fffffd0 = 0xfeefeffe
    Serial.printf_P("stackguard2: %p = 0x%08x\n", &g_pcont->stack_guard2, g_pcont->stack_guard2);


    const uintptr_t callmeaddr = (uintptr_t)(&callMe);
    size_t i = 0;
    for (uint32_t addr = stackBot; addr < stackTop; addr+=16, i+=4) {
        Serial.printf_P("0x%08x: 0x%08x 0x%08x 0x%08x 0x%08x",
                        addr,
                        stackCapturePtr[i], stackCapturePtr[i+1],
                        stackCapturePtr[i+2], stackCapturePtr[i+3]);
        if ((i & 0x200) == 0) {
            delay(0);
        }
        if (stackCapturePtr[i] == callmeaddr || stackCapturePtr[i+1] == callmeaddr ||
            stackCapturePtr[i+2] == callmeaddr || stackCapturePtr[i+3] == callmeaddr) {
            Serial.printf_P("   0x%08x", callmeaddr);
        }

        if (addr >= stackCapture && addr + 16 < stackCapture) {
            Serial.print("<-------");
        }

        Serial.print("\n");
    }
    free(stackCapturePtr); stackCapturePtr=nullptr;
}

void Utils::captureStack(void)
{
    // call this from Network::onStationModeGotIPCb() as there we're in sys context

    if (stackCapturePtr) {
        return;
    }

    int context;
    uintptr_t stack_bot, stack_top, stack_current;
    Utils::ESP8266getStackInfo(context, stack_bot, stack_top, stack_current);
    if (context != 0) {
        return;
    }
    stackCapture = stack_current;
    stackTop = stack_top;
    stackBot = stack_bot;
    stackCapturePtr = (uint32_t *)malloc(stackTop - stackBot);
    if (!stackCapturePtr) {
        return;
    }
    callMe(0,256); // 64
    memcpy(stackCapturePtr, (void *)(stack_bot), stackTop - stackBot);
}
#endif

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
