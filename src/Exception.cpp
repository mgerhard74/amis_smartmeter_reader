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
#include "Exception.h"

#include "Utils.h"

#include <EEPROM.h>
#include <LittleFS.h>
#include <time.h>
#include <user_interface.h>


#define EXCEPTIONS_MAX_SAVED_ON_DISC    50  //    "/crashes/0.dump" ... "/crashes/49.dump"

extern const char *__COMPILED_DATE_TIME_UTC_STR__;
extern const char *__COMPILED_GIT_HASH__;

extern void writeEvent(String, String, String, String);

struct __attribute__((__packed__)) exceptionInformationV1_s {
    uint8_t version;                // Oberste Bit (0x80) kennzeichnet, ob die Info bereits gelesen und verarbeitet wurde

    // jetzt die "struct rst_info"
    struct rst_info rst_info;

    uint32 excsave1;

    uint32_t stack, stack_end;

    // und nun noch ein paar Zeitinfos
    uint32_t ms;    // millis();
    time_t ts;      // time(NULL);
};


extern "C" void custom_crash_callback(struct rst_info *rst_info, uint32_t stack, uint32_t stack_end)
{
    static bool _inCallback = false;

    if (_inCallback) {
        return;
    }
    _inCallback = true;

    struct exceptionInformationV1_s exin;
    exin.version = 0x81;
    exin.rst_info = *rst_info;
    exin.stack = stack;
    exin.stack_end = stack_end;
    exin.ms = millis();
    exin.ts = time(NULL);

    // siehe: framework-arduinoespressif8266/cores/esp8266/core_esp8266_postmortem.cpp
    bool div_zero = (exin.rst_info.exccause == 0) && (exin.rst_info.epc1 == 0x4000dce5u);
    if (div_zero) {
        // In place of the detached 'ILL' instruction., redirect attention
        // back to the code that called the ROM divide function.
        __asm__ __volatile__("rsr.excsave1 %0\n\t" : "=r"(exin.excsave1) :: "memory");
    }

    EEPROM.begin(sizeof(exin));
    //memcpy(EEPROM.getDataPtr(), &exin, sizeof(exin));
    EEPROM.put(0, exin);
    EEPROM.commit();
    EEPROM.end();
    _inCallback = false;
}


static void cut_here(File &f)
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


void Exception_DumpLastCrashToFile()
{
    struct exceptionInformationV1_s exin = {};
    struct rst_info rst_info;

    EEPROM.begin(sizeof(exin));
    EEPROM.get(0, exin.version);
    if (exin.version & 0x80) { // exin.version = 0x01 | 0x80 (dump available)
        EEPROM.end();
        return;
    }

    EEPROM.get(0, exin);
    memcpy(&rst_info, &exin.rst_info, sizeof(rst_info)); // get aligned values

    EEPROM.begin(sizeof(exin.version));
    EEPROM.put(0, 0x01); // write exin.version = 0x01 | (not available)
    EEPROM.end();

    if (rst_info.reason == REASON_SOFT_RESTART) {
        // Skip dumping software-restart
        return;
    }

    LittleFS.mkdir("/crashes");

    unsigned i;
    File f;
    String fname;
    for (i=0; i < EXCEPTIONS_MAX_SAVED_ON_DISC; i++) {
        fname = "/crashes/" + String(i) + ".dump";
        if (Utils::fileExists(fname.c_str())) {
            continue;
        }
        break;
    }
    if (i == EXCEPTIONS_MAX_SAVED_ON_DISC) {
        i = 0; // Start overwriting old saved dumps
        fname = "/crashes" + String("/") + String(i) + ".dump";
    }
    f = LittleFS.open(fname.c_str(), "w");
    if (!f) {
        return;
    }

    f.printf_P(PSTR("ESP.getResetReason() = %s\n"), ESP.getResetReason().c_str());
    f.write('\n');
    f.printf_P(PSTR("rst_info.reason = %08" PRIx32 "\n"), rst_info.reason);
    f.printf_P(PSTR("rst_info.exccause = %08" PRIx32 "\n"), rst_info.exccause);
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
    f.printf_P(PSTR("time() = 0x%016" PRIx64 "\n"), exin.ts);
    f.write('\n');
    f.printf_P(PSTR("gitHash = %s\n"), __COMPILED_GIT_HASH__);
    f.printf_P(PSTR("compiled[UTC] = %s\n"), __COMPILED_DATE_TIME_UTC_STR__);
    f.printf_P(PSTR("pio environment = %s\n"), PIOENV);

    char buffer[30]; // 24 should be enough
    time_t ts;
    struct tm timeinfo;
    ts = exin.ts;                  //"2025-12-18 12:01:02 UTC\0"
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S UTC", gmtime_r(&ts, &timeinfo));
    f.printf_P(PSTR("%s\n"), buffer);

    f.write('\n');

    /* now build a block mach exception like postmortem (but without stack trace) */
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
    f.printf_P(PSTR("sp: %08x end: %08x offset: %04x\n"), exin.stack, exin.stack_end, 0); // das timmt zwar so nicht - bessere Info haben wir aber nicht
    f.write('\n');
    f.printf_P(PSTR("\n<<<stack<<<\n"));
    cut_here(f);
    f.close();

    writeEvent("INFO", "sys", "Crashinfo written", fname);
}

/* vim:set ts=4 et: */
