#pragma once

#include <time.h>
#include <WString.h>

#define UTILS_IS_VALID_YEAR(YEAR) (((YEAR) >= 2025) && ((YEAR) < 2040))

class Utils {
    public:
        static void init();

        static time_t littleFSsetTimeStamp(time_t timeStamp);

        static bool fileExists(const char *fname);
        static bool dirExists(const char *fname);

        static bool MbusCP48IToTm(struct tm &t, const uint8_t *mbusdata);
        static uint8_t hexchar2Num(const char v);
        static int getContext(void);
        static void ESP8266getStackInfo(int& context, uintptr_t& stack_bot, uintptr_t& stack_top, uintptr_t& stack_current);

        // std::optional gibnts erst mit C++17 (-std=gnu++17)
        //static std::optional<uint8_t> calculateChecksumISO1155(const char *v, bool only7bits);
        //static std::optional<uint8_t> calculateChecksumISO1155(const uint8_t *v, size_t l, bool only7bits);
        static size_t calculateChecksumISO1155(const char *v, bool only7bits);
        static size_t calculateChecksumISO1155(const uint8_t *v, size_t l, bool only7bits);

        static String escapeJson(const char *str, size_t strlen/*=0xffffffff*/, size_t maxlen/*=0xffffffff*/);

#if 0
        static void captureStack(void);
        static void dumpStack(void);
#endif
};

/* vim:set ts=4 et: */
