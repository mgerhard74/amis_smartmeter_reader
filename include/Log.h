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
//eg: #define LOGMODULE     LOGMODULE_NETWORK
#define LOGMODULE_SETUP             0x00
#define LOGMODULE_NETWORK           0x01
#define LOGMODULE_AMISREADER        0x02
#define LOGMODULE_UPDATE            0x03
#define LOGMODULE_MODBUS            0x04
#define LOGMODULE_THINGSPEAK        0x05
#define LOGMODULE_MQTT              0x06
#define LOGMODULE_SYSTEM            0x07
#define LOGMODULE_WEBSERVER         0x08
#define LOGMODULE_REBOOTATMIDNIGHT  0x09
#define LOGMODULE_WEBSSOCKET        0x0a
#define LOGMODULE_WATCHDOGPING      0x0b
#define LOGMODULE_REMOTEONOFF       0x0c
#define LOGMODULE_SHELLY            0x0d
#define LOGMODULE_LAST              0x0d
#define LOGMODULE_ALL               0xff

// Log-Types (info/warning/error/debug/verbose)
#define LOGTYPE_BIT_NONE                0x00
#define LOGTYPE_BIT_INFO                0x01
#define LOGTYPE_BIT_WARN                0x02
#define LOGTYPE_BIT_ERROR               0x04
#define LOGTYPE_BIT_DEBUG               0x08
#define LOGTYPE_BIT_VERBOSE             0x10
#define LOGTYPE_BIT_ALL                 0x1f

// Log-Levels
#define LOGLEVEL_NONE                   0x00000000
#define LOGLEVEL_ERROR                  0x00000001
#define LOGLEVEL_WARNING                0x00000002
#define LOGLEVEL_INFO                   0x00000003
#define LOGLEVEL_DEBUG                  0x00000004
#define LOGLEVEL_VERBOSE                0x00000005


#if 1
// Helper for printing IP Numbers:
// Using IP.toString().c_str() creates a temporary String() object
// which could increase memory fragmentation.
// So instead:
//    printf("ip=%s netmask=%s", ipnumber.toString().c_str(), netmask.toString().c_str());
// use
//    printf("ip=" PRsIP " netmask=" PRsIP, PRIPVal(ipnumber), PRIPVal(netmask));
#define PRsIP                           "%d.%d.%d.%d"
#define PRIPVal(__IP)                   __IP[0], __IP[1], __IP[2], __IP[3]
#else
#define PRsIP                           "%s"
#define PRIPVal(__IP)                   __IP.toString().c_str()
#endif


#include "Logfile.h"

// vim:set ts=4 sw=4 et:
