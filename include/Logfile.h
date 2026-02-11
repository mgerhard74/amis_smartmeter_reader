#pragma once

#include <LittleFS.h>

#include <ESPAsyncWebServer.h>


// Following macros:
// LOG_I        where:  I = Type (I/W/E/D/V) availble
// LOG_IP               F = like printf() otherwise like print()
// LOGF_I               P = Formatstring or String is not in RAM ... it is stored in PROGMEM like F() macro does
// LOGF_IP


#define IFLOG_I()                   (Log._logLevelBits[LOGMODULE] & (LOGTYPE_BIT_INFO))
#define LOG_PRINTF_I(FORMAT, ...)   LOG_PRINTF(LOGTYPE_BIT_INFO, LOGMODULE, FORMAT, ##__VA_ARGS__)
#define LOG_PRINTF_IP(FORMAT, ...)  LOG_PRINTFP(LOGTYPE_BIT_INFO, LOGMODULE, FORMAT, ##__VA_ARGS__)
#define LOGF_I(FORMAT, ...)         if (IFLOG_I()) { LOG_PRINTF_I(FORMAT, ##__VA_ARGS__); } ((void)(0))
#define LOGF_IP(FORMAT, ...)        if (IFLOG_I()) { LOG_PRINTF_IP(FORMAT, ##__VA_ARGS__); } ((void)(0))
#define LOG_PRINT_I(STR)            LOG_PRINT(LOGTYPE_BIT_INFO, LOGMODULE, STR)
#define LOG_PRINT_IP(STR)           LOG_PRINTP(LOGTYPE_BIT_INFO, LOGMODULE, STR)
#define LOG_I(STR)                  if (IFLOG_I()) { LOG_PRINT_I(STR); } ((void)(0))
#define LOG_IP(STR)                 if (IFLOG_I()) { LOG_PRINT_IP(STR); } ((void)(0))

#define IFLOG_W()                   (Log._logLevelBits[LOGMODULE] & (LOGTYPE_BIT_WARN))
#define LOG_PRINTF_W(FORMAT, ...)   LOG_PRINTF(LOGTYPE_BIT_WARN, LOGMODULE, FORMAT, ##__VA_ARGS__)
#define LOG_PRINTF_WP(FORMAT, ...)  LOG_PRINTFP(LOGTYPE_BIT_WARN, LOGMODULE, FORMAT, ##__VA_ARGS__)
#define LOGF_W(FORMAT, ...)         if (IFLOG_W()) { LOG_PRINTF_W(FORMAT, ##__VA_ARGS__); } ((void)(0))
#define LOGF_WP(FORMAT, ...)        if (IFLOG_W()) { LOG_PRINTF_WP(FORMAT, ##__VA_ARGS__); } ((void)(0))
#define LOG_PRINT_W(STR)            LOG_PRINT(LOGTYPE_BIT_WARN, LOGMODULE, STR)
#define LOG_PRINT_WP(STR)           LOG_PRINTP(LOGTYPE_BIT_WARN, LOGMODULE, STR)
#define LOG_W(STR)                  if (IFLOG_W()) { LOG_PRINT_W(STR); } ((void)(0))
#define LOG_WP(STR)                 if (IFLOG_W()) { LOG_PRINT_WP(STR); } ((void)(0))

#define IFLOG_E()                   (Log._logLevelBits[LOGMODULE] & (LOGTYPE_BIT_ERROR))
#define LOG_PRINTF_E(FORMAT, ...)   LOG_PRINTF(LOGTYPE_BIT_ERROR, LOGMODULE, FORMAT, ##__VA_ARGS__)
#define LOG_PRINTF_EP(FORMAT, ...)  LOG_PRINTFP(LOGTYPE_BIT_ERROR, LOGMODULE, FORMAT, ##__VA_ARGS__)
#define LOGF_E(FORMAT, ...)         if (IFLOG_E()) { LOG_PRINTF_E(FORMAT, ##__VA_ARGS__); } ((void)(0))
#define LOGF_EP(FORMAT, ...)        if (IFLOG_E()) { LOG_PRINTF_EP(FORMAT, ##__VA_ARGS__); } ((void)(0))
#define LOG_PRINT_E(STR)            LOG_PRINT(LOGTYPE_BIT_ERROR, LOGMODULE, STR)
#define LOG_PRINT_EP(STR)           LOG_PRINTP(LOGTYPE_BIT_ERROR, LOGMODULE, STR)
#define LOG_E(STR)                  if (IFLOG_E()) { LOG_PRINT_E(STR); } ((void)(0))
#define LOG_EP(STR)                 if (IFLOG_E()) { LOG_PRINT_EP(STR); } ((void)(0))

#define IFLOG_D()                   (Log._logLevelBits[LOGMODULE] & (LOGTYPE_BIT_DEBUG))
#define LOG_PRINTF_D(FORMAT, ...)   LOG_PRINTF(LOGTYPE_BIT_DEBUG, LOGMODULE, FORMAT, ##__VA_ARGS__)
#define LOG_PRINTF_DP(FORMAT, ...)  LOG_PRINTFP(LOGTYPE_BIT_DEBUG, LOGMODULE, FORMAT, ##__VA_ARGS__)
#define LOGF_D(FORMAT, ...)         if (IFLOG_D()) { LOG_PRINTF_D(FORMAT, ##__VA_ARGS__); } ((void)(0))
#define LOGF_DP(FORMAT, ...)        if (IFLOG_D()) { LOG_PRINTF_DP(FORMAT, ##__VA_ARGS__); } ((void)(0))
#define LOG_PRINT_D(STR)            LOG_PRINT(LOGTYPE_BIT_DEBUG, LOGMODULE, STR)
#define LOG_PRINT_DP(STR)           LOG_PRINTP(LOGTYPE_BIT_DEBUG, LOGMODULE, STR)
#define LOG_D(STR)                  if (IFLOG_D()) { LOG_PRINT_D(STR); } ((void)(0))
#define LOG_DP(STR)                 if (IFLOG_D()) { LOG_PRINT_DP(STR); } ((void)(0))

#define IFLOG_V()                   (Log._logLevelBits[LOGMODULE] & (LOGTYPE_BIT_VERBOSE))
#define LOG_PRINTF_V(FORMAT, ...)   LOG_PRINTF(LOGTYPE_BIT_VERBOSE, LOGMODULE, FORMAT, ##__VA_ARGS__)
#define LOG_PRINTF_VP(FORMAT, ...)  LOG_PRINTFP(LOGTYPE_BIT_VERBOSE, LOGMODULE, FORMAT, ##__VA_ARGS__)
#define LOGF_V(FORMAT, ...)         if (IFLOG_V()) { LOG_PRINTF_V(FORMAT, ##__VA_ARGS__); } ((void)(0))
#define LOGF_VP(FORMAT, ...)        if (IFLOG_V()) { LOG_PRINTF_VP(FORMAT, ##__VA_ARGS__); } ((void)(0))
#define LOG_PRINT_V(STR)            LOG_PRINT(LOGTYPE_BIT_VERBOSE, LOGMODULE, STR)
#define LOG_PRINT_VP(STR)           LOG_PRINTP(LOGTYPE_BIT_VERBOSE, LOGMODULE, STR)
#define LOG_V(STR)                  if (IFLOG_V()) { LOG_PRINT_V(STR); } ((void)(0))
#define LOG_VP(STR)                 if (IFLOG_V()) { LOG_PRINT_VP(STR); } ((void)(0))


#define LOG_PRINTF(TYPE, MODULE, FORMAT, ...)   Log.printf(TYPE, MODULE, false, FORMAT, ##__VA_ARGS__)
#define LOG_PRINTFP(TYPE, MODULE, FORMAT, ...)  Log.printf(TYPE, MODULE, true, PSTR(FORMAT), ##__VA_ARGS__)

// TODO(StefanOberhumer): Replace following two macros with "real" print() function calls
#define LOG_PRINT(TYPE, MODULE, STR)            Log.printf(TYPE, MODULE, false, STR)
#define LOG_PRINTP(TYPE, MODULE, STR)           Log.printf(TYPE, MODULE, true, PSTR(STR))

#define LOG_ENABLE_UNNEEDED_FUNCTIONS   0

class LogfileClass {
public:
    void init(const char *filename);
    void loop();
    void clear();

    void setLogLevelBits(uint32_t loglevelbits, uint32_t module);

#if (LOG_ENABLE_UNNEEDED_FUNCTIONS)
    void enableLogLevelBits(uint32_t loglevelbits, uint32_t module);
    void disableLogLevelBits(uint32_t loglevelbits, uint32_t module);
#endif

    void setLoglevel(uint32_t loglevel, uint32_t module);
#if (LOG_ENABLE_UNNEEDED_FUNCTIONS)
    uint32_t getLoglevel(uint32_t module);
#endif

    uint32_t noOfEntries();
    uint32_t noOfPages(uint32_t entriesPerPage=DEFAULT_ENTRIES_PER_PAGE);
    bool websocketRequestPage(AsyncWebSocket *webSocket, uint32_t clientId, uint32_t pageNo);

    void printf(uint32_t type, uint32_t module, bool use_progmem, const char *format, ...) __attribute__ ((format (printf, 5, 6)));

    void remove(bool allPrevious=false);

    uint32_t _logLevelBits[LOGMODULE_LAST+1]; // we need it public readable but we should not set it directly (so use _)

private:
    void _prevFilename(unsigned int prevNo, char f[LFS_NAME_MAX]);
    void _reset();
    void _startNewFile();
    void _pageToEntries(uint32_t pagNo, uint32_t &entryFrom, uint32_t &entryTo, uint32_t entriesPerPage=DEFAULT_ENTRIES_PER_PAGE);
    const char *_getModuleName(uint32_t module);

    char _filename[LFS_NAME_MAX]; // this includes already trailing '\0' --> so only 31 chars for filename

    const size_t _maxSize = 50000;
    const uint32_t _maxEntries = 0;
    static const uint32_t _keepPreviousFiles = 2;

    size_t _size;
    uint32_t _noOfEntriesInFile;

    typedef struct _requestedLogPageClient {
        AsyncWebSocket *webSocket;
        uint32_t clientId;
        uint32_t pageNo;
        _requestedLogPageClient()
            : webSocket(nullptr)
            , clientId(0)
            , pageNo(0)
        {
        }
    } _requestedLogPageClient_t;


    const size_t _requestedLogPageClientsMax = 3;
    std::list<_requestedLogPageClient_t> requestedLogPageClients;
    //std::vector<_requestedLogPageClient_t> requestedLogPageClients; // <list> is better as we always handle just one request
};

extern LogfileClass Log;

/* vim:set ts=4 et: */
