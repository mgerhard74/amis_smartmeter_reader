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

/* vim:set ts=4 et: */
