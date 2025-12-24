#include "SystemMonitor.h"

#include "unused.h"

#include <Arduino.h>


SystemMonitorClass::SystemMonitorClass()
{
    _freeHeapInfo.value = -1;
    _freeHeapInfo.filename = "";
    _freeStackInfo.value = -1;
    _freeStackInfo.filename = "";
    _maxFreeBlockSizeInfo.value = -1;
    _maxFreeBlockSizeInfo.filename = "";
    SYSTEMMONITOR_STAT();
}


static void updateStat(SystemMonitorClass::statInfo_t &stat,
                       uint32_t value, const char *filename,
                       const uint32_t lineno, const char *functionname)
{
    stat.value = value;
    stat.filename = filename;
    stat.lineno = lineno;
#if (SYSTEMMONITOR_STAT_WithFunctionName)
    stat.functionname = functionname;
#else
    UNUSED_ARG(functionname);
#endif
}

/*
Use of ESP.getFreeContStack() depends on ESP.resetFreeContStack()
So to get available Stacksize, we just calculate it!
Details: See
    cont_t* g_pcont __attribute__((section(".noinit")));
*/

extern cont_t* g_pcont;
static uint32_t ESP8266getFreeStack()
{
#if 0
    // TODO(StefanOberhumer): Das Thema context (system/user) und stack am 8266 nochmals durchschauen
    register uint32_t current_sp asm("a1");                     // Beispielwert 0x3fffee70 im sys context
                                                                // Beispielwert 0x3fffec30 im user context
    // das sollte eigentlich 'StackStart' des user-context sein
    uint32_t cont_stack_end = (uint32_t) g_pcont->stack_end;    // 0x3fffffd0 im sys & user context
    // uint32_t cont_stack_end = (uint32_t) &g_pcont->stack[0]; // 0x3fffefd0 im sys / user_context

    current_sp -= 16;
    return cont_stack_end - current_sp;
#else
    // ESP.resetFreeContStack():
    return ESP.getFreeContStack();
#endif
}

void SystemMonitorClass::captureStat(const char *filename, const uint32_t lineno, const char *functionname)
{
    uint32_t freeHeap = ESP.getFreeHeap();
    if (freeHeap < _freeHeapInfo.value) {
        updateStat(_freeHeapInfo, freeHeap, filename, lineno, functionname);
    }

    // see https://github.com/esp8266/Arduino/issues/5148
    //ESP.resetFreeContStack();
    uint32_t freeStack = ESP8266getFreeStack();
    if (freeStack > _freeStackInfo.value) {
        updateStat(_freeStackInfo, freeStack, filename, lineno, functionname);
    }

    uint32_t maxFreeBlockSize = ESP.getMaxFreeBlockSize();
    if (maxFreeBlockSize < _maxFreeBlockSizeInfo.value) {
        updateStat(_maxFreeBlockSizeInfo, maxFreeBlockSize, filename, lineno, functionname);
    }
}


const SystemMonitorClass::statInfo_t &SystemMonitorClass::getFreeHeap()
{
    return _freeHeapInfo;
}


const SystemMonitorClass::statInfo_t &SystemMonitorClass::getFreeStack()
{
    return _freeStackInfo;
}


const SystemMonitorClass::statInfo_t &SystemMonitorClass::getMaxFreeBlockSize()
{
    return _maxFreeBlockSizeInfo;
}


SystemMonitorClass SystemMonitor;


/* vim:set ts=4 et: */
