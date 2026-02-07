#pragma once

/*
Logging:

* Levels of "messages":
        Info, Warning, Error, ...

* Modules of "messages":
        system, network, mqtt, ...

Support PROGMEM format-strings



Usage:
1.) define your module on start of source
eg: #define LOGMODULE   LOGMODULE_BIT_NETWORK

2.) Use LOG_IP, LOG_WP, LOG_EP, LOG_DP and LOG_VP macros like a printf statement.
This macros do following:
    I/W/E/D/V stands for Info/Warning/Error/Debugging/Verbose
    Puts the formatting string into ROM
    Processes parameters only if logging is enabled

So: Easy to use, if logging disabled no time spent


Current state:
    Prepared to split logging into: websocket, serial and file
    Results is nearly previous writeEvent() function
    See: Logfile.h

*/

//See also: https://docs.espressif.com/projects/esp-idf/en/v4.3/esp32/api-reference/system/log.html


// Number of entries returned to th websocket client per page
#define DEFAULT_ENTRIES_PER_PAGE        20

// Log-Modules
//eg: #define LOGMODULE     LOGMODULE_BIT_NETWORK
#define LOGMODULE_BIT_NONE              0x00000000
#define LOGMODULE_BIT_SETUP             0x00000100
#define LOGMODULE_BIT_NETWORK           0x00000200
#define LOGMODULE_BIT_AMISREADER        0x00000400
#define LOGMODULE_BIT_UPDATE            0x00000800
#define LOGMODULE_BIT_MODBUS            0x00001000
#define LOGMODULE_BIT_THINGSPEAK        0x00002000
#define LOGMODULE_BIT_MQTT              0x00004000
#define LOGMODULE_BIT_SYSTEM            0x00008000
#define LOGMODULE_BIT_WEBSERVER         0x00010000
#define LOGMODULE_BIT_REBOOTATMIDNIGHT  0x00020000
#define LOGMODULE_BIT_WEBSSOCKET        0x00040000
#define LOGMODULE_BIT_WATCHDOGPING      0x00080000
#define LOGMODULE_BIT_REMOTEONOFF       0x00100000
#define LOGMODULE_BIT_ALL               0xffffff00

// Log-Types (info/warning/error/debug/verbose)
#define LOGTYPE_BIT_NONE                0x00000000
#define LOGTYPE_BIT_INFO                0x00000001
#define LOGTYPE_BIT_WARN                0x00000002
#define LOGTYPE_BIT_ERROR               0x00000004
#define LOGTYPE_BIT_DEBUG               0x00000008
#define LOGTYPE_BIT_VERBOSE             0x00000010
#define LOGTYPE_BIT_ALL                 0x0000001f

// Log-Levels
#define LOGLEVEL_NONE                   0x00000000
#define LOGLEVEL_ERROR                  0x00000001
#define LOGLEVEL_WARNING                0x00000002
#define LOGLEVEL_INFO                   0x00000003
#define LOGLEVEL_DEBUG                  0x00000004
#define LOGLEVEL_VERBOSE                0x00000005


#include "Logfile.h"

// vim:set ts=4 sw=4 et:
