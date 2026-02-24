#pragma once

#include <stddef.h>
#include <stdint.h>

#define SYSTEMMONITOR_STAT_WithFunctionName     1

#define SYSTEMMONITOR_STAT()    SystemMonitor.captureStat(__FILE__, __LINE__, __FUNCTION__)

class SystemMonitorClass
{
    public:
        SystemMonitorClass();
        void captureStat(const char *filename, const uint32_t lineno=0, const char *functionname=nullptr);

        typedef struct {
            uint32_t value;
            const char *filename;
#if (SYSTEMMONITOR_STAT_WithFunctionName)
            const char *functionname;
#endif
            uint32_t lineno;
        } statInfo_t;

        const statInfo_t &getFreeHeap();
        const statInfo_t &getFreeStack(size_t context);
        const statInfo_t &getMaxFreeBlockSize();

    private:
        statInfo_t _freeHeapInfo;
        statInfo_t _freeStackInfo[3];
        statInfo_t _maxFreeBlockSizeInfo;
};

extern SystemMonitorClass SystemMonitor;

/* vim:set ts=4 et: */
