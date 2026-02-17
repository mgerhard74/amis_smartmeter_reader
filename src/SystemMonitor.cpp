#include "SystemMonitor.h"

#include "Utils.h"
#include "unused.h"

#include <Arduino.h>


SystemMonitorClass::SystemMonitorClass()
{
    _freeHeapInfo.value = 0xffffffff;
    _freeHeapInfo.filename = "";
    for (size_t i = 0; i < std::size(_freeStackInfo); i++) {
        _freeStackInfo[i].value = 0xffffffff;
        _freeStackInfo[i].filename = "";
    }
    _maxFreeBlockSizeInfo.value = 0xffffffff;
    _maxFreeBlockSizeInfo.filename = "";
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


void SystemMonitorClass::captureStat(const char *filename, const uint32_t lineno, const char *functionname)
{
    int context;
    uintptr_t stack_bot, stack_top, stack_current;
    Utils::ESP8266getStackInfo(context, stack_bot, stack_top, stack_current);

    uint32_t freeHeap = ESP.getFreeHeap();
    if (freeHeap < _freeHeapInfo.value) {
        updateStat(_freeHeapInfo, freeHeap, filename, lineno, functionname);
    }

    // see https://github.com/esp8266/Arduino/issues/5148
    //ESP.resetFreeContStack();
    uint32_t freeStack = stack_current - stack_bot; // ESP.getFreeContStack();
    if (freeStack < _freeStackInfo[context].value) {
        updateStat(_freeStackInfo[context], freeStack, filename, lineno, functionname);
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


const SystemMonitorClass::statInfo_t &SystemMonitorClass::getFreeStack(size_t context)
{
    return _freeStackInfo[context];
}


const SystemMonitorClass::statInfo_t &SystemMonitorClass::getMaxFreeBlockSize()
{
    return _maxFreeBlockSizeInfo;
}


SystemMonitorClass SystemMonitor;


/* vim:set ts=4 et: */
