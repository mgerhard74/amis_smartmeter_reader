// Versuch eine minimale Crash info abzuspeichern.
// Ein vollständiger Stackdump macht eigentlich nur im "build_type = debug" Sinn.
// Aber Mithilfe des Mapfiles (build_flags = -Wl,-Map,output.map) sollte man zumindest
// die Abbruchstelle erruieren können.
// In dem Amis-Leser-Projekt ist der Stacktrace auch meist sehr groß (~5kB) ... da dauert
// das Schreiben ins EEPROM zu lange oder geht sich vom Speicher her gar nicht aus.
// Also hier nur die minimal Info abspeichern!
//
// Zusätzliche Infos zu dem Thema:
//  * Library EspSaveCrash: https://github.com/krzychb/EspSaveCrash
//  * Arduino Framework: https://github.com/esp8266/Arduino/blob/master/cores/esp8266/core_esp8266_postmortem.cpp
//                       https://arduino-esp8266.readthedocs.io/en/latest/exception_causes.html
//
// Manual decoding of Addr2Line:
//    .platformio/packages/toolchain-xtensa@1.40802.0/bin/xtensa-lx106-elf-addr2line -fipC -e firmware.elf
//      see: .platformio/platforms/espressif8266/monitor/filter_exception_decoder.py

#include "Exception.h"
#include "Log.h"
#define LOGMODULE   LOGMODULE_SYSTEM
#include "Utils.h"
#include "__compiled_constants.h"

#include <EEPROM.h>
#include <LittleFS.h>
#include <time.h>
#include <user_interface.h>


extern "C" {
    extern uint32_t _iram_start;
    extern uint32_t _iram_end;
    extern uint32_t _irom0_text_start;
    extern uint32_t _irom0_text_end;
}

#define EXCEPTIONS_MAX_SAVED_ON_DISC    10  //    "/crashes/0.dump" ... "/crashes/9.dump"


static uint32_t _interrestingAdressesStart = (uint32_t)&_irom0_text_start;
static uint32_t _interrestingAdressesEnd = (uint32_t)&_irom0_text_end;

struct __attribute__((__packed__)) exceptionInformationV1_s {
    uint8_t version;                // Oberste Bit (0x80) kennzeichnet, ob die Info bereits gelesen und verarbeitet wurde

    // jetzt die "struct rst_info"
    struct rst_info rst_info;

    uint32 excsave1;

    uint32_t stack, stack_end;

    // und nun noch ein paar Zeitinfos
    uint32_t ms;            // millis();
    time_t ts;              // time(NULL);
    time_t tsCompiled;      // __COMPILED_DATE_TIME_UTC_TIME_T__

    uint8_t interrestingStackValueCnt;
    uint32_t interrestingStackValues[32];
};


/* Stack durchsuchen und "interessante" Adressen sammeln (die wir für Codeadressen halten)*/
static inline void fetchinterrestingStackValues(uint32_t start, uint32_t end, struct exceptionInformationV1_s *exin)
{
    uint32_t *s, *e;
    uint32_t stackValue;

    s = (uint32_t*) start;
    e = (uint32_t*) end;

    size_t foundCnt = 0;

    for (uint32_t *curr = s; curr < e && foundCnt < std::size(exin->interrestingStackValues); curr++) {
        stackValue = *curr;
        if(stackValue >= _interrestingAdressesStart && stackValue <= _interrestingAdressesEnd) {

            // Check if we have "stackValue" already in our list
            bool found = false;
            for (size_t i=0; i < foundCnt; i++) {
                if (exin->interrestingStackValues[i] == stackValue) {
                    found = true;
                    break;
                }
            }
            if (!found) { // not in list -> add it
                exin->interrestingStackValues[foundCnt++] = stackValue;
            }
        }
    }

    exin->interrestingStackValueCnt = foundCnt;
}


extern "C" void custom_crash_callback(struct rst_info *rst_info, uint32_t stack, uint32_t stack_end)
{
    static bool _inCallback = false;

    if (_inCallback) {
        return;
    }
    _inCallback = true;

    struct exceptionInformationV1_s exin;

    memset(&exin, 0 , sizeof(exin));

    exin.version = 0x81;
    exin.rst_info = *rst_info;
    exin.stack = stack;
    exin.stack_end = stack_end;
    exin.ms = millis();
    exin.ts = time(NULL);
    exin.tsCompiled = __COMPILED_DATE_TIME_UTC_TIME_T__;

    // siehe: framework-arduinoespressif8266/cores/esp8266/core_esp8266_postmortem.cpp
    bool div_zero = (exin.rst_info.exccause == 0) && (exin.rst_info.epc1 == 0x4000dce5u);
    if (div_zero) {
        // In place of the detached 'ILL' instruction., redirect attention
        // back to the code that called the ROM divide function.
        __asm__ __volatile__("rsr.excsave1 %0\n\t" : "=r"(exin.excsave1) :: "memory");
    } else {
        exin.excsave1 = 0;
    }

    // Try fetching some stackvalues which could be a function address
    fetchinterrestingStackValues(stack, stack_end, &exin);

    EEPROM.begin(sizeof(exin));
    //memcpy(EEPROM.getDataPtr(), &exin, sizeof(exin));
    EEPROM.put(0, exin);
    EEPROM.commit();
    EEPROM.end();
    // _inCallback = false; // never set it to false
}


// Aufbereitung wie bei einem "echten dump" (core_esp8266_postmortem.cpp)
static inline void cut_here(File &f)
{
    for (auto i = 0; i < 15; i++ ) {
        f.write('-');
    }
    f.printf_P(PSTR(" CUT HERE FOR EXCEPTION DECODER "));
    for (auto i = 0; i < 15; i++ ) {
        f.write('-');
    }
    f.write('\n');
}


static inline String getCrashFilename(unsigned no)
{
#if EXCEPTIONS_MAX_SAVED_ON_DISC > 100
#error buffer get to small!
#endif
    char buffer[17]; // "/crashes/xx.dump"

    snprintf_P(buffer, sizeof(buffer), PSTR("/crashes/%u.dump"), no % EXCEPTIONS_MAX_SAVED_ON_DISC);
    return String(buffer);
}


void Exception_RemoveAllDumps()
{
    for (unsigned i=0; i < EXCEPTIONS_MAX_SAVED_ON_DISC; i++) {
        LittleFS.remove(getCrashFilename(i+1).c_str());
    }
}


void Exception_DumpLastCrashToFile()
{
    struct exceptionInformationV1_s exin = {};
    struct rst_info rst_info;

    char tsBuffer[30]; // 24 bytes should be enough
    time_t ts;
    struct tm timeinfo;
    bool infoIsFrom_getResetInfoPtr;

    EEPROM.begin(sizeof(exin));
    EEPROM.get(0, exin.version);
    if (!(exin.version & 0x80)) { // exin.version = 0x01 | 0x80 (dump available)
        // No info in eeprom: Use Information from ESP.getResetInfoPtr()
        EEPROM.end();
        memset(&exin, 0, sizeof(exin));
        memcpy(&rst_info, ESP.getResetInfoPtr(), sizeof(rst_info));
        infoIsFrom_getResetInfoPtr = true;
    } else {
        EEPROM.get(0, exin);
        memcpy(&rst_info, &exin.rst_info, sizeof(rst_info)); // get aligned values

        exin.version &= 0x7f;
        EEPROM.begin(sizeof(exin.version));
        EEPROM.put(0, exin.version); // write exin.version = 0x01 | (not available)
        EEPROM.end();
        infoIsFrom_getResetInfoPtr = false;
    }

    if (rst_info.reason == REASON_SOFT_RESTART || rst_info.reason == REASON_DEFAULT_RST) {
        // Skip dumping SoftwareRestart or PowerOn
        return;
    }

    LittleFS.mkdir(F("/crashes"));

    unsigned i;
    File f;
    String fname;
    for (i=0; i < EXCEPTIONS_MAX_SAVED_ON_DISC; i++) {
        fname = getCrashFilename(i);
        if (Utils::fileExists(fname.c_str())) {
            continue;
        }
        break;
    }
    if (i == EXCEPTIONS_MAX_SAVED_ON_DISC) {
        i = 0; // Start overwriting old saved dumps
        fname = getCrashFilename(i);
    }

    // Remove file for next crash saving
    LittleFS.remove(getCrashFilename(i+1).c_str());

    // Set filetime to at minimum compiled time
    /// So the dump file hast time of exception or at minimum the timestamp of the version the exception occured
    time_t previousFSTime;
    time_t exceptionTime;
    exceptionTime = (exin.ts > exin.tsCompiled) ?exin.ts :exin.tsCompiled;
    previousFSTime = Utils::littleFSsetTimeStamp(exceptionTime);

    // Now write dump
    f = LittleFS.open(fname.c_str(), "w");
    if (!f) {
        LOGF_EP("Could not create %s", fname.c_str());
        return;
    }
    if (infoIsFrom_getResetInfoPtr) {
        f.printf_P(PSTR("Info from getResetInfoPtr()."));
    } else {
        f.printf_P(PSTR("Info from EEProm save."));
    }
    f.write('\n');
    f.printf_P(PSTR("ESP.getResetReason() = %s\n"), ESP.getResetReason().c_str());
    f.write('\n');
    f.printf_P(PSTR("rst_info.reason = 0x%08" PRIx32 "\n"), rst_info.reason);
    f.printf_P(PSTR("rst_info.exccause = 0x%08" PRIx32 "\n"), rst_info.exccause);
    f.printf_P(PSTR("rst_info.epc1 = 0x%08" PRIx32 "\n"), rst_info.epc1);
    f.printf_P(PSTR("rst_info.epc2 = 0x%08" PRIx32 "\n"), rst_info.epc2);
    f.printf_P(PSTR("rst_info.epc3 = 0x%08" PRIx32 "\n"), rst_info.epc3);
    f.printf_P(PSTR("rst_info.excvaddr = 0x%08" PRIx32 "\n"), rst_info.excvaddr);
    f.printf_P(PSTR("rst_info.depc = 0x%08" PRIx32 "\n"), rst_info.depc);
    f.printf_P(PSTR("excsave1 = 0x%08" PRIx32 "\n"), exin.excsave1);
    f.printf_P(PSTR("stack = 0x%08" PRIx32 "\n"), exin.stack);
    f.printf_P(PSTR("stack_end = 0x%08" PRIx32 "\n"), exin.stack_end);
    f.write('\n');

    f.printf_P(PSTR("millis() = 0x%08" PRIx32 "\n"), exin.ms);
    ts = exin.ts;                  //"2025-12-18 12:01:02 UTC\0"
    strftime(tsBuffer, sizeof(tsBuffer), "%Y-%m-%d %H:%M:%S UTC", gmtime_r(&ts, &timeinfo));
    f.printf_P(PSTR("time() = 0x%016" PRIx64 " (%s)\n"), ts, tsBuffer);
    f.write('\n');

    ts = exin.tsCompiled;
    strftime(tsBuffer, sizeof(tsBuffer), "%Y-%m-%d %H:%M:%S UTC", gmtime_r(&ts, &timeinfo));
    f.printf_P(PSTR("Exception version compiled[UTC] = 0x%016" PRIx64 " (%s)\n"),ts,  tsBuffer);
    f.write('\n');

    f.printf_P(PSTR("This gitHash = %s\n"), __COMPILED_GIT_HASH__);
    f.printf_P(PSTR("This compiled[UTC] = %s\n"), __COMPILED_DATE_TIME_UTC_STR__);
    f.printf_P(PSTR("This pio environment = %s\n"), PIOENV);
    f.write('\n');

    /* now build a block match exception like postmortem (but without a real stack trace) */
    cut_here(f);
    if (rst_info.reason == REASON_EXCEPTION_RST) {
        bool div_zero = (rst_info.exccause == 0) && (rst_info.epc1 == 0x4000dce5u);
        if (div_zero) {
            rst_info.exccause = 6;
            rst_info.epc1 = exin.excsave1;
        }
    } else if (rst_info.reason == REASON_SOFT_WDT_RST) {
        f.printf_P(PSTR("\nSoft WDT reset\n")); /* Address executing at time of Soft WDT level-1 interrupt */
        rst_info.epc2 = rst_info.epc3 = rst_info.excvaddr = rst_info.depc = 0;
    /*
    } else if (rst_info.reason == REASON_USER_STACK_SMASH) {
        f.print("\nStack smashing detected\n");
    } else if (rst_info.reason == REASON_USER_STACK_OVERFLOW) {
        f.print("\nStack overflow detected\n");
    */
    } else {
        f.printf_P(PSTR("\nGeneric Reset\n"));
    }

    f.printf_P(PSTR("\nException (%d):\nepc1=0x%08x epc2=0x%08x epc3=0x%08x excvaddr=0x%08x depc=0x%08x\n"),
                    rst_info.exccause, rst_info.epc1, rst_info.epc2, rst_info.epc3, rst_info.excvaddr, rst_info.depc);

    f.printf_P(PSTR("\n>>>stack>>>\n"));
    if (exin.stack_end == 0x3fffffb0) {
        f.printf_P(PSTR("\nctx: sys\n"));
    } else {
        f.printf_P(PSTR("\nctx: cont\n"));
    }

    /* fake the stacke trace but include our 'interrestingStackValues' we captured */
    // das stimmt zwar so nicht - bessere Info haben wir aber nicht
    f.printf_P(PSTR("sp: %08x end: %08x offset: %04x\n"), exin.stack, exin.stack_end, 0);

    // Elements in interrestingStackValues must be multiple of 4
    static_assert(((sizeof(exin.interrestingStackValues)/sizeof(exin.interrestingStackValues[0])) & 3) == 0);

    for (size_t i=0; i < exin.interrestingStackValueCnt; i+=4) {
        f.printf_P(PSTR("%08x:  %08x %08x %08x %08x\n"),
                exin.stack + (0x10 * i),
                exin.interrestingStackValues[i], exin.interrestingStackValues[i+1],
                exin.interrestingStackValues[i+2], exin.interrestingStackValues[i+3]);
    }
    f.write('\n');
    f.printf_P(PSTR("\n<<<stack<<<\n"));
    cut_here(f);
    f.close();
    Utils::littleFSsetTimeStamp(previousFSTime);

    LOGF_IP("Crashinfo %s written", fname.c_str());
}


/* Declare _nullValue extern, so the compiler does not know its value */
extern volatile uint32_t _nullValue[2];
volatile uint32_t _nullValue[2] = {0,0};

void Exception_Raise(unsigned int no) {
    if (no < 1 || no > 7) {
        return;
    }

    //Set 115200Bd as writing dump to serial (possible with 300Bd) can take too long (Hardware WDT)!
    //Serial.begin(115200, SERIAL_8N1);

    if (no == 1) {
        // Exception (0)
        LOG_EP("Divide by 0");
        _nullValue[1] = 1 / _nullValue[0];
        LOG_EP("Divide by 0 done");
    } else if (no == 2) {
        // Exception (28)
        LOG_EP("Read nullptr");
        _nullValue[0] = *(uint32_t*)(_nullValue[0]);
        LOG_EP("Read nullptr done");
    } else if (no == 3) {
        // Exception (29)
        LOG_EP("Write nullptr");
        *(char *)_nullValue[0] = 0;
        LOG_EP("Write nullptr done");
    } else if (no == 4) {
        // TODO(StefanOberhumer): Exception gets not raised ... check why
        LOG_EP("Unaligned read access uint32_t");
        uintptr_t pu32 = (uintptr_t)&_nullValue[0];
        uint32_t u32 = *(uint32_t*)(pu32+1);
        Serial.printf("pu32=%08x\r\n", pu32+1);
        _nullValue[1] = u32;
        Serial.printf("u32=%08x\r\n", u32);
        LOG_EP("Unaligned read access uint32_t done");

        LOG_EP("Unaligned read access float");
        uintptr_t pf = (uintptr_t)&_nullValue[0];
        float f = *(uint32_t*)(pf+1);
        Serial.printf("pf=%08x\r\n", pf+1);
        _nullValue[1] = (uint32_t) f;
        Serial.printf("f=%f\r\n", f);
        LOG_EP("Unaligned read access float done");

    } else if (no == 5) {
        // TODO(StefanOberhumer): Exception gets not raised ... check why
        LOG_EP("Unaligned write access uint32_t");
        uintptr_t pu32 = (uintptr_t)&_nullValue[0];
        Serial.printf("pu32=%08x\r\n", pu32+1);
        *(uint32_t*)(pu32+1) = 0xa1b2c3d4;
        Serial.printf("value=%08x\r\n", _nullValue[1]);
        LOG_EP("Unaligned write access done uint32_t");

        LOG_EP("Unaligned write access float");
        uintptr_t pf = (uintptr_t)&_nullValue[0];
        Serial.printf("p=%08x\r\n", pf+1);
        *(float*)(pf+1) = 0xa1b2c3d4;
        Serial.printf("value=%08x\r\n", _nullValue[1]);
        LOG_EP("Unaligned write access done float");

    } else if (no == 6) {
        LOG_EP("Hardware WDT ... wait");
        ESP.wdtDisable();
        for(;;)
        {
          // stay in an infinite loop doing nothing
          // this way other process can not be executed
          //
          // Note:
          // Hardware wdt kicks in if software wdt is unable to perfrom
          // Nothing will be saved in EEPROM for the hardware wdt
        }
        LOG_EP("Hardware WDT done.");
    } else if (no == 7) {
        LOG_EP("Software WDT ... wait");
        for(;;)
        {
          // stay in an infinite loop doing nothing
          // this way other process can not be executed
        }
        LOG_EP("Software WDT done.");
    }
}


/*
https://www.esp8266.com/viewtopic.php?f=13&t=20206

void hw_wdt_disable(){
  *((volatile uint32_t*) 0x60000900) &= ~(1); // Hardware WDT OFF
}

void hw_wdt_enable(){
  *((volatile uint32_t*) 0x60000900) |= 1; // Hardware WDT ON
}

*/

/* vim:set ts=4 et: */
