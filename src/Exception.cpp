// Versuch eine minimale Crash info abzuspeichern.
// Ein vollständiger Stackdump macht eigentlich nur im "build_type = debug" Sinn.
// Aber Mithilfe des Elf-files sollte man zumindest die Abbruchstelle erruieren können.
// Das Mapfile ist mWn nicht nötig (build_flags = -Wl,-Map,output.map)
// In dem Amis-Leser-Projekt ist der Stacktrace auch meist sehr groß (~5kB) ... da dauert
// das Schreiben ins EEPROM oder RTC-Memory zu lange oder geht sich vom Speicher her gar nicht aus.
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
#include "RTC.h"
#include "Log.h"
#define LOGMODULE   LOGMODULE_SYSTEM
#include "Utils.h"
#include "framework/replacement/core_esp8266_postmortem.h"
#include "__compiled_constants.h"

#include <LittleFS.h>
#include <time.h>
#include <user_interface.h>

extern "C" {
    extern uint32_t _iram_start;            // üblicherweise 0x40100000
    extern uint32_t _iram_end;              // üblicherweise 0x40108000
    extern uint32_t _irom0_text_start;
    extern uint32_t _irom0_text_end;
}

extern "C" uint32_t __crc_val;

#define EXCEPTIONS_MAX_SAVED_ON_DISC        10  //  "/crashes/0.dump" ... "/crashes/9.dump"
#define EXCEPTIONS_MAX_SAVED_STR_LEN        32  //  max length of saved strings
#define EXCEPTIONS_MAX_SAVED_STACK_VALUES   32  //  max stack values
                                                //  aktuell hätten wir noch 92 Bytes frei ==> ergäbe 55 Stackwerte


static uint32_t _interrestingAdressesStart = (uint32_t)&_irom0_text_start;
static uint32_t _interrestingAdressesEnd = (uint32_t)&_irom0_text_end;

struct __attribute__((__packed__, aligned(4))) exceptionInformationV2_s {
    uint32_t crc32;     // crc32 of whole struct (including the following strings and this(crc32) field with a value of 0)

    uint8_t magic;      // 0x82
    exception_extra_info_type_t exception_extra_info_type;
    exception_context_t exception_context;
    uint8_t interrestingStackValueCnt;

    // jetzt die "struct rst_info"
    struct rst_info rst_info;

    uint32 excsave1;

    uint32_t stack;

    // und nun noch ein paar Zeitinfos
    time_t ts;              // time(NULL);
    uint32_t ms;            // millis();
//    time_t tsCompiled;      // __COMPILED_DATE_TIME_UTC_TIME_T__

    uint32_t interrestingStackValues[EXCEPTIONS_MAX_SAVED_STACK_VALUES];

    union {
        struct __attribute__((__packed__)) {
            int line;
        } panic;
        struct __attribute__((__packed__)) {
            uint32_t addr;
        } stacksmash;
    } u_exception_extra_info;

    union {
        struct __attribute__((__packed__)) {
            char file[EXCEPTIONS_MAX_SAVED_STR_LEN + 1];
            char func[EXCEPTIONS_MAX_SAVED_STR_LEN + 1];
            char what[EXCEPTIONS_MAX_SAVED_STR_LEN + 1];
        } strings_ffw;
        struct __attribute__((__packed__)) {
            char unhandled_exception[EXCEPTIONS_MAX_SAVED_STR_LEN + 1];
        } strings_ue;
    } u_exception_strings;
    char _dummy_filler_multple4[1]; // extend size of struct to a multple of 4
};
static_assert(sizeof(exceptionInformationV2_s) % 4 == 0,  "Total struct size must be a multiple of 4 for RTC memory");
static_assert(offsetof(exceptionInformationV2_s, rst_info) % 4 == 0, "rst_info member offset must be a multiple of 4");
static_assert(offsetof(exceptionInformationV2_s, interrestingStackValues) % 4 == 0, "interrestingStackValues member offset must be a multiple of 4");
static_assert(RTC_EXCEPTION_MAXSIZE >= sizeof(exceptionInformationV2_s), "exceptionInformationV2_s mismatch RTC_EXCEPTION_MAXSIZE");


/*
Stack durchsuchen und "interessante" Adressen sammeln (die wir für Codeadressen halten)
"onlyInterresting" ...Adressen von Funktionen, die wir auch "beinflussen" können also in  "unserem" Code liegen
Das Problem ist ja, dass wir ja einfach nur gesamten Stack vor uns haben und nicht wissen,
ob der Wert am Stack eine Rücksprungadresse oder was anderes ist.
Wir könnten derzeit aber maximal 87 Stackwerte im RTC speichern (speichern nur 32 damit Platz für was anderes bleibt).
Aus diesem Grund filtern wir nur die Werte raus, die irgenwo in unserem eigen Code liegen
*/

/*
static bool isCodeAddress(uint32_t addr) {
    bool inIromCode = (addr >= (uint32_t)&_irom0_text_start) && (addr < (uint32_t)&_irom0_text_end);
    bool inIramCode = (addr >= (uint32_t)&_iram_start) && (addr < (uint32_t)&_iram_end);
    return inIromCode || inIramCode;
}
*/

static inline size_t fetchStackValues(uint32_t start, uint32_t end, uint32_t *stackValues, size_t &foundCnt, size_t maxentries, bool onlyInterresting, bool uniqueAdressesOnly)
{
    uint32_t *s, *e;
    uint32_t stackValue;

    s = (uint32_t*) start;
    e = (uint32_t*) end;

    for (uint32_t *curr_sp = s; curr_sp < e && foundCnt < maxentries; curr_sp++) {
        stackValue = *curr_sp;
        /*
        if (onlyInterresting){
            if (stackValue == (uint32_t)&Exception_postmortem_report_callback) {
                continue;
            }
        }
        */
        if (!onlyInterresting || (stackValue >= _interrestingAdressesStart && stackValue < _interrestingAdressesEnd)) { // isCodeAddress(stackValue)

            // Check if we have "stackValue" already in our list
            bool isNewAddress = true;
            if (uniqueAdressesOnly) {
                for (size_t i=0; isNewAddress && i < foundCnt; i++) {
                    if (stackValues[i] == stackValue) {
                        isNewAddress = false;
                        break;
                    }
                }
            }
            if (isNewAddress) { // not in list -> add it
                stackValues[foundCnt++] = stackValue;
            }
        }
    }

    return foundCnt;
}

#define EXINFO_ON_STACK 0

#if (!EXINFO_ON_STACK)
// Declare exInfo here: In case of a crash we don't have to "allocate" it on the stack!
// But: Needs 288 bytes DRAM !
static exceptionInformationV2_s exInfo = {};
#endif

extern "C" void Exception_postmortem_report_callback(const postmortem_rst_info_t *postmortem_rst_info, bool *doSerialReport)
{
    // System hat eine Exception erreicht - Dammed!!
    // Nicht direkt ins Filesystem schreiben - LittleFS ist ev zerschossen und funktioniert womöglich nicht mehr!
    // Möglichst keine großartigen mallocs() oder new mehr machen - ev haben wir keinen Speicher verfügbar!
    // Keine großen Strukturen/Variablen deklarieren - ev haben wir keinen Stack mehr!
    // Versuchen wir noch ein paar Daten vor dem Reboot zu sichern, damit wir die nach dem Reboot als crashfile "sauber" abspeichern können
    // Die übliche - serielle Dump Ausgabe - macht bei dieser Hardware Konfiguration  keinen Sinn!
    // Wir sind eingeschränkt, mit dem was wir speichern können (Platz/Zeit): Deswegen nur Stack-Value-Referenzen auf "eigene" Funktionen


    // Gather reset information and store it in the eeprom (called from our own postmortem_report() function)
    //
    // After rebooting (more memory and file system avaliable) a dump gets
    // written to the filesystem using Exception_DumpLastCrashToFile()

    static bool _inCallback = false;

    if (_inCallback) {
        return;
    }

    if (doSerialReport) {
        // Beim ersten Aufruf ist der Pointer doSerialReport gesetzt.
        // Dafür sind nur minimale Infos in postmortem_rst_info vorhanden
        // Hier entscheiden wir, ob wir das auch über die serielle Schnittstelle ausgeben
        // Üblicherweise (Non-Developer): Keine serielle Ausgabe

        //Serial.begin(115200, SERIAL_8N1);
        *doSerialReport = false;
        return;
    }

    _inCallback = true;

#if (EXINFO_ON_STACK)
    exceptionInformationV2_s exInfo = {};
#else
    memset(&exInfo, 0, sizeof(exInfo));
#endif

    exInfo.magic = 0x82;
    memcpy(&exInfo.rst_info, &postmortem_rst_info->rst_info, sizeof(exInfo.rst_info));
    exInfo.excsave1 = postmortem_rst_info->excsave1;
    exInfo.stack = postmortem_rst_info->sp_dump;
    exInfo.exception_context = postmortem_rst_info->exception_context;
    //exInfo.stack_end = postmortem_rst_info->stack_end;
    exInfo.ms = millis();
    exInfo.ts = time(NULL);
    exInfo.exception_extra_info_type = postmortem_rst_info->exception_extra_info_type;

    if (exInfo.exception_extra_info_type == exception_type_assert) {
        exInfo.u_exception_extra_info.panic.line = postmortem_rst_info->u_exception_extra_info.panic.line;
        if (postmortem_rst_info->u_exception_extra_info.panic.file) {
            strncpy_P(exInfo.u_exception_strings.strings_ffw.file, postmortem_rst_info->u_exception_extra_info.panic.file, sizeof(exInfo.u_exception_strings.strings_ffw.file)-1);
            exInfo.u_exception_strings.strings_ffw.file[sizeof(exInfo.u_exception_strings.strings_ffw.file)-1] = 0;
        }
        if (postmortem_rst_info->u_exception_extra_info.panic.func) {
            strncpy_P(exInfo.u_exception_strings.strings_ffw.func, postmortem_rst_info->u_exception_extra_info.panic.func, sizeof(exInfo.u_exception_strings.strings_ffw.func)-1);
            exInfo.u_exception_strings.strings_ffw.func[sizeof(exInfo.u_exception_strings.strings_ffw.func)-1] = 0;
        }
        if (postmortem_rst_info->u_exception_extra_info.panic.what) {
            strncpy_P(exInfo.u_exception_strings.strings_ffw.what, postmortem_rst_info->u_exception_extra_info.panic.what, sizeof(exInfo.u_exception_strings.strings_ffw.what)-1);
            exInfo.u_exception_strings.strings_ffw.what[sizeof(exInfo.u_exception_strings.strings_ffw.what)-1] = 0;
        }
    } else if (exInfo.exception_extra_info_type == exception_type_panic) {
        exInfo.u_exception_extra_info.panic.line = postmortem_rst_info->u_exception_extra_info.panic.line;
        if (postmortem_rst_info->u_exception_extra_info.panic.file) {
            strncpy_P(exInfo.u_exception_strings.strings_ffw.file, postmortem_rst_info->u_exception_extra_info.panic.file, sizeof(exInfo.u_exception_strings.strings_ffw.file)-1);
            exInfo.u_exception_strings.strings_ffw.file[sizeof(exInfo.u_exception_strings.strings_ffw.file)-1] = 0;
        }
        if (postmortem_rst_info->u_exception_extra_info.panic.func) {
            strncpy_P(exInfo.u_exception_strings.strings_ffw.func, postmortem_rst_info->u_exception_extra_info.panic.func, sizeof(exInfo.u_exception_strings.strings_ffw.func)-1);
            exInfo.u_exception_strings.strings_ffw.func[sizeof(exInfo.u_exception_strings.strings_ffw.func)-1] = 0;
        }
    } else if (exInfo.exception_extra_info_type == exception_type_unhandled_exception) {
        if (postmortem_rst_info->u_exception_extra_info.unhandled_exception.unhandled_exception) {
            strncpy_P(exInfo.u_exception_strings.strings_ue.unhandled_exception, postmortem_rst_info->u_exception_extra_info.unhandled_exception.unhandled_exception, sizeof(exInfo.u_exception_strings.strings_ue.unhandled_exception)-1);
            exInfo.u_exception_strings.strings_ue.unhandled_exception[sizeof(exInfo.u_exception_strings.strings_ue.unhandled_exception)-1] = 0;
        }
    } else if (exInfo.rst_info.reason == REASON_EXCEPTION_RST) {
        // Setting excause to 6 (=div by 0)  was already done in our own postmortem_report()
    } else if (exInfo.rst_info.reason == REASON_SOFT_WDT_RST) {

    } else if (exInfo.rst_info.reason == REASON_USER_STACK_SMASH) {
        exInfo.u_exception_extra_info.stacksmash.addr =  postmortem_rst_info->u_exception_extra_info.stacksmash.addr;
    } else {
        //ets_printf_P(PSTR("\nGeneric Reset\n"));
    }

    // Now grab some stack values

    const uint32_t cont_stack_start = (uint32_t) &(g_pcont->stack);
    const uint32_t cont_stack_end = (uint32_t) g_pcont->stack_end;

    // amount of stack taken by interrupt or exception handler
    // and everything up to __wrap_system_restart_local
    // ~(determined empirically, might break)~
    uint32_t offset = 0;
    if (postmortem_rst_info->rst_info.reason == REASON_SOFT_WDT_RST) {
        // Stack Tally
        // 256 User Exception vector handler reserves stack space
        //     directed to _xtos_l1int_handler function in Boot ROM
        //  48 wDev_ProcessFiq - its address appears in a vector table at 0x3FFFC27C
        //  16 ?unnamed? - index into a table, pull out pointer, and call if non-zero
        //     appears near near wDev_ProcessFiq
        //  32 pp_soft_wdt_feed_local - gather the specifics and call __wrap_system_restart_local
        offset =  32 + 16 + 48 + 256;
    }
    else if (postmortem_rst_info->rst_info.reason == REASON_EXCEPTION_RST) {
        // Stack Tally
        // 256 Exception vector reserves stack space
        //     filled in by "C" wrapper handler
        //  16 Handler level 1 - enable icache
        //  64 Handler level 2 - exception report
        offset = 64 + 16 + 256;
    }
    else if (postmortem_rst_info->rst_info.reason == REASON_WDT_RST) {
        offset = 16;
    }
    else if (postmortem_rst_info->rst_info.reason == REASON_USER_SWEXCEPTION_RST) {
        offset = 16;
    }

    uint32_t sp_dump = exInfo.stack;
    size_t interrestingStackValueCnt = 0;
    if (sp_dump > stack_thunk_get_stack_bot() && sp_dump <= stack_thunk_get_stack_top()) {
        // BearSSL we dump the BSSL second stack and then reset SP back to the main cont stack
        // ets_printf_P(PSTR("\nctx: bearssl\nsp: %08x end: %08x offset: %04x\n"), sp_dump, stack_thunk_get_stack_top(), offset);
        // print_stack(sp_dump + offset, stack_thunk_get_stack_top());
        fetchStackValues(sp_dump + offset, stack_thunk_get_stack_top(), &exInfo.interrestingStackValues[0], interrestingStackValueCnt, std::size(exInfo.interrestingStackValues), true, true);
        offset = 0; // No offset needed anymore, the exception info was stored in the bssl stack
        sp_dump = stack_thunk_get_cont_sp();
    }

    uint32_t stack_end;
    if (sp_dump > cont_stack_start && sp_dump < cont_stack_end) {
        //ets_printf_P(PSTR("\nctx: cont\n"));
        stack_end = cont_stack_end;
    }
    else {
        //ets_printf_P(PSTR("\nctx: sys\n"));
        stack_end = 0x3fffffb0;
        // it's actually 0x3ffffff0, but the stuff below ets_run
        // is likely not really relevant to the crash
    }

    // print_stack(sp_dump + offset, stack_end);
    fetchStackValues(sp_dump + offset, stack_end, &exInfo.interrestingStackValues[0], interrestingStackValueCnt, std::size(exInfo.interrestingStackValues), true, true);

    exInfo.interrestingStackValueCnt = interrestingStackValueCnt;

    // Create a CRC32 over the structure (crc which is within structure is 0)
    exInfo.crc32 = crc32(&exInfo, sizeof(exInfo));

    // Write all the Info to RTC Memory - Should be honored on upcoming boot
    ESP.rtcUserMemoryWrite(RTC_EXCEPTION_OFFSET, (uint32_t *) &exInfo ,sizeof(exInfo));
}

void Exception_InstallPostmortem(int target)
{
    if (target == 1) {
        postmortem_report_early_callback = Exception_postmortem_report_callback;
    } else if (target == 2) {
    } else {
        postmortem_report_early_callback = nullptr;
    }
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
#error buffer gets to small!
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
    // Gather reset information from RTC memory or ESP.getResetInfoPtr()
    //
    // Write the dump information to the filesystem

#if (EXINFO_ON_STACK)
    exceptionInformationV2_s exInfo;
#endif
    struct rst_info rst_info;

    char tsBuffer[30]; // 24 bytes should be enough
    time_t ts;
    struct tm timeinfo;
    bool infoIsFrom_getResetInfoPtr;

    if (!ESP.rtcUserMemoryRead(RTC_EXCEPTION_OFFSET, (uint32_t *) &exInfo ,sizeof(exInfo))) {
        return;
    }

    if (exInfo.magic  != 0x82) {
        // No info in RTC memory: Use Information from ESP.getResetInfoPtr()
        memset(&exInfo, 0, sizeof(exInfo));
        memcpy(&exInfo.rst_info, ESP.getResetInfoPtr(), sizeof(rst_info));
        infoIsFrom_getResetInfoPtr = true;
    } else {
        uint32_t crc32_v = 0;
        ESP.rtcUserMemoryWrite(RTC_EXCEPTION_OFFSET, &crc32_v ,sizeof(crc32_v)) ; // Clear RTC memory - exInfo.crc32
        ESP.rtcUserMemoryWrite(RTC_EXCEPTION_OFFSET+1, &crc32_v ,sizeof(crc32_v)) ; // Clear RTC memory - exInfo.magic...
        crc32_v = exInfo.crc32;
        exInfo.crc32 = 0;
        if( crc32_v != crc32(&exInfo, sizeof(exInfo)) ) {
            memset(&exInfo, 0, sizeof(exInfo));
            memcpy(&exInfo.rst_info, ESP.getResetInfoPtr(), sizeof(rst_info));
            infoIsFrom_getResetInfoPtr = true;
        } else {
            infoIsFrom_getResetInfoPtr = false;
        }
    }
    memcpy(&rst_info, &exInfo.rst_info, sizeof(rst_info)); // get aligned values

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
    /// So the dump file has time of exception or at minimum the timestamp of the version the exception writes the dump
    time_t previousFSTime;
    time_t exceptionTime;
    exceptionTime = (exInfo.ts > __COMPILED_DATE_TIME_UTC_TIME_T__) ?exInfo.ts :__COMPILED_DATE_TIME_UTC_TIME_T__;
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
        f.printf_P(PSTR("Info from RTC memory save."));
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
    f.printf_P(PSTR("excsave1 = 0x%08" PRIx32 "\n"), exInfo.excsave1);
    f.printf_P(PSTR("stack = 0x%08" PRIx32 "\n"), exInfo.stack);
    //f.printf_P(PSTR("stack_end = 0x%08" PRIx32 "\n"), exInfo.stack_end);
    f.printf_P(PSTR("context = %" PRIu8 "\n"), exInfo.exception_context);
    f.printf_P(PSTR("extra_info_type = %" PRIu8 "\n"), exInfo.exception_extra_info_type);
    f.write('\n');

    f.printf_P(PSTR("millis() = 0x%08" PRIx32 "\n"), exInfo.ms);
    ts = exInfo.ts;                  //"2025-12-18 12:01:02 UTC\0"
    strftime(tsBuffer, sizeof(tsBuffer), "%Y-%m-%d %H:%M:%S UTC", gmtime_r(&ts, &timeinfo));
    f.printf_P(PSTR("time() = 0x%016" PRIx64 " (%s)\n"), ts, tsBuffer);
    f.write('\n');

    f.printf_P(PSTR("This gitHash = %s\n"), __COMPILED_GIT_HASH__);
    f.printf_P(PSTR("This compiled[UTC] = %s\n"), __COMPILED_DATE_TIME_UTC_STR__);
    f.printf_P(PSTR("This pio environment = %s\n"), PIOENV);
    f.printf_P(PSTR("Firmware crc32 checksum = 0x%08x\n"), __crc_val);
    f.write('\n');

    /* now build a block match exception like postmortem (but without a real stack trace) */
    cut_here(f);

    const char *panic_file = exInfo.u_exception_strings.strings_ffw.file;
    const char *panic_func = exInfo.u_exception_strings.strings_ffw.func;
    const char *panic_what = exInfo.u_exception_strings.strings_ffw.what;
    const char *unhandled_exception = exInfo.u_exception_strings.strings_ue.unhandled_exception;

    if (exInfo.exception_extra_info_type == exception_type_assert) {
        f.printf_P(PSTR("\nPanic %s:%d %s"), panic_file, exInfo.u_exception_extra_info.panic.line, panic_func);
        if (panic_what[0]) {
            f.printf_P(PSTR(": Assertion '%s' failed."), panic_what);
        }
    } else if (exInfo.exception_extra_info_type == exception_type_panic) {
        f.printf_P(PSTR("\nPanic %s:%d %s\n"), panic_file, exInfo.u_exception_extra_info.panic.line, panic_func);
    } else if (exInfo.exception_extra_info_type == exception_type_unhandled_exception) {
        f.printf_P(PSTR("\nUnhandled C++ exception: %s\n"), unhandled_exception);
    } else if (exInfo.exception_extra_info_type == exception_type_abort) {
        f.printf_P(PSTR("\nAbort called\n"));
    } else if (rst_info.reason == REASON_EXCEPTION_RST) {
        if (rst_info.exccause == 6) {
            f.printf_P(PSTR("\nDivision by 0\n"));
            rst_info.epc1 = exInfo.excsave1;
        } else {
            f.printf_P(PSTR("\nGeneric Exception\n"));
        }
    } else if (rst_info.reason == REASON_SOFT_WDT_RST) {
        f.printf_P(PSTR("\nSoft WDT reset\n")); /* Address executing at time of Soft WDT level-1 interrupt */
        rst_info.epc2 = rst_info.epc3 = rst_info.excvaddr = rst_info.depc = 0;
    } else if (rst_info.reason == REASON_USER_STACK_SMASH) {
        f.printf_P(PSTR("\nStack smashing detected\n"));
    } else {
        f.printf_P(PSTR("\nGeneric Reset\n"));
    }

    f.printf_P(PSTR("\nException (%d):\nepc1=0x%08x epc2=0x%08x epc3=0x%08x excvaddr=0x%08x depc=0x%08x\n"),
                    rst_info.exccause, rst_info.epc1, rst_info.epc2, rst_info.epc3, rst_info.excvaddr, rst_info.depc);

    f.printf_P(PSTR("\n>>>stack>>>\n\nctx: "));
    if (exInfo.exception_context == exception_context_bearssl) {
        f.printf_P(PSTR("bearssl\n"));
    } else if (exInfo.exception_context == exception_context_sys) {
        f.printf_P(PSTR("sys\n"));
    } else {
        f.printf_P(PSTR("cont\n"));
    }

    /* fake the stacke trace but include our 'interrestingStackValues' we captured */
    // das stimmt zwar so nicht - bessere Info haben wir aber nicht
    f.printf_P(PSTR("sp: %08x end: %08x offset: %04x\n"), exInfo.stack, 0xffffffff, 0);

    const uint32_t *interrestingStackValues = &exInfo.interrestingStackValues[0];
    // Elements in interrestingStackValues must be multiple of 4 as we always print 4 items
    static_assert(((sizeof(exInfo.interrestingStackValues)/sizeof(exInfo.interrestingStackValues[0])) & 3) == 0);
    for (size_t i=0; i < exInfo.interrestingStackValueCnt; i+=4) {
        // Wert des Stack stimmt nicht wirklich!
        // Dient nur dazu das Ausgabeformat für den Exception Decoder zu generieren
        f.printf_P(PSTR("%08x:  %08x %08x %08x %08x\n"),
                exInfo.stack + (0x10 * i),
                interrestingStackValues[i], interrestingStackValues[i+1],
                interrestingStackValues[i+2], interrestingStackValues[i+3]);
    }
    f.write('\n');

    f.printf_P(PSTR("\n<<<stack<<<\n"));
    cut_here(f);

    f.printf_P(PSTR("\nRun:\n${HOME}/.platformio/packages/toolchain-xtensa@1.40802.0/bin/xtensa-lx106-elf-addr2line -fipC -e firmware.elf"));
    const char *PSTR_0x08x = PSTR(" 0x%08x");
    if (rst_info.epc1) {
        f.printf_P(PSTR_0x08x, rst_info.epc1);
    }
    if (rst_info.epc2) {
        f.printf_P(PSTR_0x08x, rst_info.epc2);
    }
    if (rst_info.epc3) {
        f.printf_P(PSTR_0x08x, rst_info.epc3);
    }
    if (exInfo.excsave1) {
        f.printf_P(PSTR_0x08x, exInfo.excsave1);
    }

    for (size_t i=0; i < exInfo.interrestingStackValueCnt; i++) {
        if (interrestingStackValues[i]) {
            f.printf_P(PSTR_0x08x, interrestingStackValues[i]);
        }
    }

    f.printf_P(PSTR("\nto get more information\n"));
    f.close();
    Utils::littleFSsetTimeStamp(previousFSTime);

    LOGF_IP("Crashinfo %s written", fname.c_str());
}


/* Declare _nullValue extern, so the compiler does not know its value */
extern uint32_t _nullValue[2];
uint32_t _nullValue[2] = {0,0};

void Exception_Raise(unsigned int no) {
    if (no < 1 || no > 12) {
        return;
    }

    if (no == 1) {
        // Exception (0)
        LOG_EP("Divide by 0");
        _nullValue[1] = 1 / _nullValue[0];
        LOG_EP("Divide by 0 done");
    } else if (no == 2) {
        // Exception (28)
        LOG_EP("Read nullptr");
        _nullValue[0] = *(uint32_t*)(_nullValue[1]);
        LOG_EP("Read nullptr done");
    } else if (no == 3) {
        // Exception (29)
        LOG_EP("Write nullptr");
        *(char *)_nullValue[0] = 0;
        LOG_EP("Write nullptr done");
    } else if (no == 4) {
        LOG_EP("Unaligned read access uint32_t");
        asm volatile (
            "movi    a3, 1\n\t"
            "or      %0, %0, a3   \n\t" // or given address(= &_nullValue[0]) with 0x01 (==> unaligned)
            "l32i.n  a2, %0, 0    \n\t" // load from address(unaligned) into a2
            :
            : "r" (&_nullValue[0])      // Input: Addresse of &_nullValue[0]
            : "a2", "a3", "memory"      // "Clobber": a2, a3 and memory are changed
        );
        LOG_EP("Unaligned read access uint32_t done");
    } else if (no == 5) {
        LOG_EP("Unaligned write access uint32_t");
        asm volatile (
            "movi    a3, 1\n\t"
            "or      %0, %0, a3   \n\t" // or given address(= &_nullValue[0]) with 0x01 (==> unaligned)
            "s32i.n  a3, %0, 0    \n\t" // write a3 to address(unaligned)
            :
            : "r" (&_nullValue[0])      // Input: Addresse of &_nullValue[0]
            : "a3", "memory"            // "Clobber": a3 and memory are changed
        );
        LOG_EP("Unaligned write access uint32_t done");
    } else if (no == 6) {
        LOG_EP("Hardware WDT ... wait");
        ESP.wdtDisable();
        for (;;)
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
        for (;;)
        {
          // stay in an infinite loop doing nothing
          // this way other process can not be executed
        }
        LOG_EP("Software WDT done.");
    } else if (no == 8) {
        LOG_EP("assert() ... wait");
#ifdef NDEBUG
        #define NDEBUG 1
        #include <assert.h>
        #define NDEBUG_DO_UNDEF 1
#endif
        assert(1 == 0);
#if NDEBUG_DO_UNDEF
        #undef NDEBUG_DO_UNDEF
        #undef NDEBUG
        #include <assert.h>
#endif
        LOG_EP("assert() done.");
    } else if (no == 9) {
        LOG_EP("abort() ... wait");
        abort();
        LOG_EP("abort() done.");
    } else if (no == 10) {
        LOG_EP("panic() ... wait");
        panic();
        LOG_EP("panic() done.");
    } else if (no == 11) {
        LOG_EP("__unhandled_exception() ... wait");
        std::terminate();
        LOG_EP("__unhandled_exception done.");
    } else if (no == 12) {
        LOG_EP("asm volatile('break 1, 15;'); ... wait");
        asm volatile("break 1, 15;");
        LOG_EP("asm volatile('break 1, 15;'); done.");
    }
}


/*
https://www.esp8266.com/viewtopic.php?f=13&t=20206

void hw_wdt_disable(){
//  see: void EspClass::rebootIntoUartDownloadMode()  --> CLEAR_PERI_REG_MASK(PERIPHS_HW_WDT, 0x1);
{
  *((volatile uint32_t*) 0x60000900) &= ~(1); // Hardware WDT OFF
}

void hw_wdt_enable(){
  *((volatile uint32_t*) 0x60000900) |= 1; // Hardware WDT ON
}

*/

/* vim:set ts=4 et: */
