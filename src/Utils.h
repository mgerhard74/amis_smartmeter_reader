#pragma once

#include <time.h>

#define UTILS_IS_VALID_YEAR(YEAR) (((YEAR) >= 2025) && ((YEAR) < 2040))

class UtilsClass {
    public:
        static bool fileExists(const char *fname);
        static bool MbusCP48IToTm(struct tm &t, const uint8_t *mbusdata);
        static uint8_t hexchar2Num(const char v);
};

/* vim:set ts=4 et: */
