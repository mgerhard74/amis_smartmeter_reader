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


#include "Exception.h"

#include "Utils.h"

#include <EEPROM.h>
#include <LittleFS.h>
#include <time.h>
#include <user_interface.h>


#define EXCEPTIONS_MAX_SAVED_ON_DISC    50  //    "/crashes/0.dump" ... "/crashes/49.dump"


extern void writeEvent(String, String, String, String);

struct __attribute__((__packed__)) exceptionInformation_s {
    uint8_t version;

    // jetzt die "struct rst_info"
    struct rst_info rst_info;

    uint32 excsave1;

    uint32_t stack, stack_end;

    // und nun noch ein paar Zeitinfos
    unsigned long ms;   // millis();
    time_t ts;          // time(NULL);
};


extern "C" void custom_crash_callback(struct rst_info * rst_info, uint32_t stack, uint32_t stack_end)
{
    static bool _inCallback = false;

    if (_inCallback) {
        return;
    }
    _inCallback = true;

    struct exceptionInformation_s exin;
    exin.version = 1;
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


void Exception_DumpLastCrashToFile()
{
    struct exceptionInformation_s exin = {};

    EEPROM.begin(sizeof(exin));
    EEPROM.get(0, exin.version);
    if (exin.version != 1) {
        EEPROM.end();
        return;
    }
    EEPROM.get(0, exin);

    EEPROM.begin(sizeof(exin.version));
    EEPROM.put(0, 0); // write exin.version = 0
    EEPROM.end();

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

    f.printf("ESP.getResetReason() = %s\n", ESP.getResetReason().c_str());
    f.printf("\n");
    f.printf("rst_info.reason = %" PRIu32 "\n", exin.rst_info.reason);
    f.printf("rst_info.exccause = %" PRIu32 "\n", exin.rst_info.exccause);
    f.printf("rst_info.epc1 = 0x%08" PRIx32 "\n", exin.rst_info.epc1);
    f.printf("rst_info.epc2 = 0x%08" PRIx32 "\n", exin.rst_info.epc2);
    f.printf("rst_info.epc3 = 0x%08" PRIx32 "\n", exin.rst_info.epc3);
    f.printf("rst_info.excvaddr = 0x%08" PRIx32 "\n", exin.rst_info.excvaddr);
    f.printf("rst_info.depc = 0x%08" PRIx32 "\n", exin.rst_info.depc);
    f.printf("excsave1 = 0x%08" PRIx32 "\n", exin.excsave1);
    f.printf("stack = 0x%08" PRIx32 "\n", exin.stack);
    f.printf("stack_end = 0x%08" PRIx32 "\n", exin.stack_end);
    f.printf("millis() = 0x%08" PRIx32 "\n", (unsigned int) exin.ms);
    f.printf("time() = 0x%016" PRIx64 "\n", exin.ts);

    char buffer[30]; // 24 should be enough
    time_t ts;
    struct tm timeinfo;
    ts = exin.ts;                  //"2025-12-18 12:01:02 UTC\0"
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S UTC", gmtime_r(&ts, &timeinfo));
    f.printf("%s\n", buffer);

    f.printf("\n");

    if (exin.rst_info.reason == REASON_EXCEPTION_RST) {
        bool div_zero = (exin.rst_info.exccause == 0) && (exin.rst_info.epc1 == 0x4000dce5u);
        if (div_zero) {
            exin.rst_info.exccause = 6;
            exin.rst_info.epc1 = exin.excsave1;
        }
    }
    else if (exin.rst_info.reason == REASON_SOFT_WDT_RST) {
        f.print("Soft WDT reset\n");
        f.printf("\nException (%d):\nepc1=0x%08x epc2=0x%08x epc3=0x%08x excvaddr=0x%08x depc=0x%08x\n",
            exin.rst_info.exccause, /* Address executing at time of Soft WDT level-1 interrupt */ exin.rst_info.epc1, 0, 0, 0, 0);
        return;
    } else {
        f.print("Generic Reset\n");
    }
    f.printf("Exception (%d):\nepc1=0x%08x epc2=0x%08x epc3=0x%08x excvaddr=0x%08x depc=0x%08x\n",
                exin.rst_info.exccause, exin.rst_info.epc1, exin.rst_info.epc2, exin.rst_info.epc3, exin.rst_info.excvaddr, exin.rst_info.depc);
    f.close();

    writeEvent("INFO", "sys", "Crashinfo written", fname);
}

/* vim:set ts=4 et: */
