#pragma once

#include <LittleFS.h>

#include <ESPAsyncWebServer.h>


#define IFLOG_I()               ((Log.logMask & (LOGTYPE_BIT_INFO | LOGMODULE)) == (LOGTYPE_BIT_INFO | LOGMODULE))
#define DOLOG_I(FORMAT, ...)    DOLOG(LOGTYPE_BIT_INFO, LOGMODULE, FORMAT, ##__VA_ARGS__)
#define DOLOG_IP(FORMAT, ...)   DOLOGP(LOGTYPE_BIT_INFO, LOGMODULE, FORMAT, ##__VA_ARGS__)
#define LOG_I(FORMAT, ...)      if (IFLOG_I()) { DOLOG_I(FORMAT, ##__VA_ARGS__); } ((void)(0))
#define LOG_IP(FORMAT, ...)     if (IFLOG_I()) { DOLOG_IP(FORMAT, ##__VA_ARGS__); } ((void)(0))

#define IFLOG_W()               ((Log.logMask & (LOGTYPE_BIT_WARN | LOGMODULE)) == (LOGTYPE_BIT_WARN | LOGMODULE))
#define DOLOG_W(FORMAT, ...)    DOLOG(LOGTYPE_BIT_WARN, LOGMODULE, FORMAT, ##__VA_ARGS__)
#define DOLOG_WP(FORMAT, ...)   DOLOGP(LOGTYPE_BIT_WARN, LOGMODULE, FORMAT, ##__VA_ARGS__)
#define LOG_W(FORMAT, ...)      if (IFLOG_W()) { DOLOG_W(FORMAT, ##__VA_ARGS__); } ((void)(0))
#define LOG_WP(FORMAT, ...)     if (IFLOG_W()) { DOLOG_WP(FORMAT, ##__VA_ARGS__); } ((void)(0))

#define IFLOG_E()               ((Log.logMask & (LOGTYPE_BIT_ERROR | LOGMODULE)) == (LOGTYPE_BIT_ERROR | LOGMODULE))
#define DOLOG_E(FORMAT, ...)    DOLOG(LOGTYPE_BIT_ERROR, LOGMODULE, FORMAT, ##__VA_ARGS__)
#define DOLOG_EP(FORMAT, ...)   DOLOGP(LOGTYPE_BIT_ERROR, LOGMODULE, FORMAT, ##__VA_ARGS__)
#define LOG_E(FORMAT, ...)      if (IFLOG_E()) { DOLOG_E(FORMAT, ##__VA_ARGS__); } ((void)(0))
#define LOG_EP(FORMAT, ...)     if (IFLOG_E()) { DOLOG_EP(FORMAT, ##__VA_ARGS__); } ((void)(0))

#define IFLOG_D()               ((Log.logMask & (LOGTYPE_BIT_DEBUG | LOGMODULE)) == (LOGTYPE_BIT_DEBUG | LOGMODULE))
#define DOLOG_D(FORMAT, ...)    DOLOG(LOGTYPE_BIT_DEBUG, LOGMODULE, FORMAT, ##__VA_ARGS__)
#define DOLOG_DP(FORMAT, ...)   DOLOGP(LOGTYPE_BIT_DEBUG, LOGMODULE, FORMAT, ##__VA_ARGS__)
#define LOG_D(FORMAT, ...)      if (IFLOG_D()) { DOLOG_D(FORMAT, ##__VA_ARGS__); } ((void)(0))
#define LOG_DP(FORMAT, ...)     if (IFLOG_D()) { DOLOG_DP(FORMAT, ##__VA_ARGS__); } ((void)(0))

#define IFLOG_V()               ((Log.logMask & (LOGTYPE_BIT_VERBOSE | LOGMODULE)) == (LOGTYPE_BIT_VERBOSE | LOGMODULE))
#define DOLOG_V(FORMAT, ...)    DOLOG(LOGTYPE_BIT_VERBOSE, LOGMODULE, FORMAT, ##__VA_ARGS__)
#define DOLOG_VP(FORMAT, ...)   DOLOGP(LOGTYPE_BIT_VERBOSE, LOGMODULE, FORMAT, ##__VA_ARGS__)
#define LOG_V(FORMAT, ...)      if (IFLOG_V()) { DOLOG_V(FORMAT, ##__VA_ARGS__); } ((void)(0))
#define LOG_VP(FORMAT, ...)     if (IFLOG_V()) { DOLOG_VP(FORMAT, ##__VA_ARGS__); } ((void)(0))


#define DOLOG(TYPE, MODULE, FORMAT, ...)      Log.log(TYPE, MODULE, false, FORMAT, ##__VA_ARGS__)
#define DOLOGP(TYPE, MODULE, FORMAT, ...)     Log.log(TYPE, MODULE, true, PSTR(FORMAT), ##__VA_ARGS__)



class LogfileClass {
public:
    void init(const char *filename);
    void loop();
    void clear();

    void setTypes(uint32_t types);
    void enableType(uint32_t type);
    void disableType(uint32_t type);

    void setModules(uint32_t modules);
    void enableModule(uint32_t module);
    void disableModule(uint32_t module);

    void setLoglevel(uint32_t loglevel);
    uint32_t getLoglevel();

    uint32_t noOfEntries();
    uint32_t noOfPages(uint32_t entriesPerPage=DEFAULT_ENTRIES_PER_PAGE);
    bool websocketRequestPage(AsyncWebSocket *webSocket, uint32_t clientId, uint32_t pageNo);

    void log(uint32_t type, uint32_t module, bool use_progmem, const char *format, ...) __attribute__ ((format (printf, 5, 6)));

    void remove(bool allPrevious=false);

    uint32_t logMask;

private:
    void _prevFilename(unsigned int prevNo, char f[LFS_NAME_MAX]);
    void _reset();
    void _startNewFile();
    void _pageToEntries(uint32_t pagNo, uint32_t &entryFrom, uint32_t &entryTo, uint32_t entriesPerPage=DEFAULT_ENTRIES_PER_PAGE);


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
