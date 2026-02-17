#include "Json.h"
#include "Log.h"
#define LOGMODULE   LOGMODULE_SYSTEM
#include "unused.h"
#include "Utils.h"

#include <LittleFS.h>

#include <stdarg.h>


#define LOGFILE_LINE_LEN_MAX    768

extern AsyncWebSocket *ws;

static bool fileSkipLines(File &f, uint32_t lines)
{
    size_t rlen, linelen = 0;
    char buffer[128];

    for (;lines != 0;) {
        rlen = f.readBytes(buffer, std::size(buffer));
        if (rlen == 0) {
            return false;
        }
        for (size_t i=0; i<rlen; ) {
            if (buffer[i++] == '\n') {
                lines--;
                if (lines == 0) {
                    f.seek(-rlen + i, SeekCur);
                    break;
                }
                linelen = 0;
            } else {
                if (++linelen > LOGFILE_LINE_LEN_MAX) {
                    return false;
                }
            }
        }
    }
    return true;
}

void LogfileClass::clear()
{
    // Start a new File or just clear current???
#if 1
    // We clear it as user requested!
    LittleFS.remove(_filename);
    _size = 0;
    _noOfEntriesInFile = 0;
#else
    _startNewFile();
#endif
    LOG_I("Event log cleared!");
}


void LogfileClass::_prevFilename(unsigned int prevNo, char f[LFS_NAME_MAX])
{
    char empty = 0;
    static_assert(_keepPreviousFiles < 100);

    strlcpy(f, _filename, LFS_NAME_MAX);

    // get extension and cut off extension
    char *ext = f;
    while (strchr(ext, '.')) {
        ext = strchr(ext, '.') + 1;
    }
    if (ext == f) {
        ext = &empty;
    } else {
        ext[-1] = 0;  // ext now points to eg : "txt" and _f had no extension anymore
    }

    char newExtension[LFS_NAME_MAX];
    snprintf_P(newExtension, sizeof(newExtension), PSTR(".prev%u.%s"), prevNo, ext);

    // strip f so that the new extension can be appended
    f[LFS_NAME_MAX - strlen(newExtension) - 1] = 0;

    strlcat(f, newExtension, LFS_NAME_MAX);
}


void LogfileClass::init(const char *filename)
{
    strlcpy(_filename, filename, std::size(_filename));
    _reset();
}

void LogfileClass::loop()
{
    // Send requested pages of the log file to a client
    // Just handle one request per loop() call to keep system running

    /* Example JSON:
    {
    "entries": 177,
    "pages": 9,
    "size": 18709,
    "page": 9,
    "loglines": [
        "{\"ms\":123260,\"type\":\"INFO\",\"src\":\"system\",\"ts\":1770762978,\"desc\":\"Event log cleared!\"}",
        "{\"ms\":1550200,\"type\":\"INFO\",\"src\":\"rebootAtMidnight\",\"ts\":1770764405,\"desc\":\"Starting scheduled reboot...\"}",
        "{\"ms\":1550308,\"type\":\"WARN\",\"src\":\"mqtt\",\"ts\":1770764405,\"desc\":\"Disconnected from server 192.168.xx.yy:1883 reason=TCP_DISCONNECTED\"}",
        "{\"ms\":1550428,\"type\":\"INFO\",\"src\":\"system\",\"ts\":1770764405,\"desc\":\"System is going to reboot\"}"
      ]
    }
    */

#if 1
    if (requestedLogPageClients.size() == 0) {
        return;
    }

    _requestedLogPageClient_t request = requestedLogPageClients.front();
    // Adapt pagno to be in valid range
    if (request.pageNo == 0) {
        request.pageNo = 1;
    } else if (request.pageNo > noOfPages()) {
        request.pageNo = noOfPages();
    }

    uint32_t firstEntry, lastEntry;
    _pageToEntries(request.pageNo, firstEntry, lastEntry);

    File f;
    size_t jsonBytesNeed = JSON_OBJECT_SIZE(5) + 64;  // Keystrings and 5 objects
    size_t pos = 0xffffffff;
    if (_noOfEntriesInFile) {
        f = LittleFS.open(_filename, "r");
        if (f) {
            if (fileSkipLines(f, firstEntry-1) ) {
                pos = f.position();
                for (uint32_t currentEntry = firstEntry; currentEntry < lastEntry && f.available(); currentEntry++) {
                    String s = f.readStringUntil('\n');
                    jsonBytesNeed += JSON_ARRAY_SIZE(1) + s.length();
                }
            }
        }
    }

    DynamicJsonDocument doc(jsonBytesNeed);
    doc["entries"] = noOfEntries();
    doc["pages"] = noOfPages();
    doc["size"] = _size;
    doc["page"] = request.pageNo;
    JsonArray loglines = doc.createNestedArray("loglines");

    if (_noOfEntriesInFile && pos != 0xffffffff) {
        f.seek(pos, SeekSet);
        for (uint32_t currentEntry = firstEntry; currentEntry < lastEntry && f.available(); currentEntry++) {
            loglines.add(f.readStringUntil('\n'));
        }
    }

    if (_noOfEntriesInFile && f) {
        f.close();
    }

    String buffer;
    SERIALIZE_JSON_LOG(doc, buffer);
    if (!request.webSocket->text(request.clientId, buffer)) {
        LOG_EP("Could not send logfile via websocket.");
    }

    requestedLogPageClients.erase(requestedLogPageClients.begin());
#else
    if (_requestedLogPageClientIdx == 0) {
        return;
    }

    DynamicJsonBuffer jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();
    root["pages"] = noOfPages();
    for (size_t i=0; i < _requestedLogPageClientIdx; i++) {
        _requestedLogPageClient_t *request = &_requestedLogPageClients[i];
        if (request->webSocket == nullptr) {
            // skip an already processed request
            continue;
        }

        // Adapt pagno to be in valid range
        if (request->pageNo == 0) {
            request->pageNo = 1;
        } else if (request->pageNo > noOfPages()) {
            request->pageNo = noOfPages();
        }

        root["page"] = request->pageNo;

        if (_noOfEntriesInFile) {
            JsonArray &items = root.createNestedArray("list");
            File f = LittleFS.open(_filename, "r");
            if (f) {
                uint32_t firstEntry, lastEntry;

                _pageToEntries(request->pageNo, firstEntry, lastEntry);
                if (fileSkipLines(f, firstEntry-1) ) {
                    for (uint32_t currentEntry = firstEntry; currentEntry < lastEntry && f.available(); currentEntry++) {
                        String entry = String();
                        entry = f.readStringUntil('\n');
                        items.add(entry);
                    }
                }
                f.close();
            }
        }

        String buffer;
        root.printTo(buffer);
        request->webSocket->text(request->clientId, buffer);

        // Just handle one request per loop() call !
        // TODO(StefanOberhumer) use list, erase(), push_back(), ...
        request->webSocket = nullptr;
        if (i == _requestedLogPageClientIdx - 1) {
            _requestedLogPageClientIdx = 0;
        }
        break;
    }
#endif
}

void LogfileClass::remove(bool allPrevious)
{
    LittleFS.remove(_filename);
    _size = 0xffffffff;

    if (allPrevious) {
        char filenamePrev[LFS_NAME_MAX];
        for (unsigned int prev=1; prev < _keepPreviousFiles; prev++) {
            _prevFilename(prev, filenamePrev);
            LittleFS.remove(filenamePrev);
        }
    }
}

void LogfileClass::_reset()
{
    requestedLogPageClients.clear();
    for (size_t i = 0; i < std::size(_logLevelBits); i++) {
        _logLevelBits[i] = CONFIG_LOG_DEFAULT_LEVEL;
    }
    _size = 0xffffffff;
}


static const char* _moduleNames[LOGMODULE_LAST+1] = {
    "setup",
    "network",
    "reader",
    "update",
    "modbus",
    "thingspeak",
    "mqtt",
    "system",
    "webserver",
    "rebootAtMidnight",
    "websocket",
    "watchdogPing",
    "remoteOnOff",
    "shelly"
};
const char *LogfileClass::_getModuleName(uint32_t module)
{
    if (module == LOGMODULE_ALL) {
        return "ALL";
    }
    if (module >= std::size(_moduleNames)) {
        return "UNDEFINED";
    }
    return _moduleNames[module];
}


void LogfileClass::printf(uint32_t type, uint32_t module, bool use_progmem, const char *format, ...)
{
    va_list args;
    uint32_t entries = noOfEntries();

    if (_maxSize && _size >= _maxSize) {
        _startNewFile();
    } else if (_maxEntries && entries >= _maxEntries) {
        _startNewFile();
    }

    File f = LittleFS.open(_filename, "a");
    if (!f) {
        return;
    }

    char typeChar = '?';
    if (type & LOGTYPE_BIT_ERROR) {
        typeChar = 'E';
    } else if (type & LOGTYPE_BIT_WARN) {
        typeChar = 'W';
    } else if (type & LOGTYPE_BIT_DEBUG) {
        typeChar = 'D';
    } else if (type & LOGTYPE_BIT_VERBOSE) {
        typeChar = 'V';
    } else if (type & LOGTYPE_BIT_INFO) {
        typeChar = 'I';
    }


#if 0
// neues Format
    // Type[IWEVD] Module[Name] millis() time() message
    // [18:04:50.591] V (7493926) temperature: Raw temperature value: 103
    _size += f.printf("%llu %c (%lu) %s: %s", time(NULL), typeChar, _getModuleName(module), millis(), "daten");
    va_start(args, format);
    if (use_progmem) {
        _size += f.printf_P(format, args);
    } else {
        _size += f.printf(format, args);
    }
    va_end(args);
    _size += f.write('\n');
#else
// altes Format
    #include "unused.h"
    UNUSED_ARG(typeChar);

    const char *typeStr = "";
    if (type & LOGTYPE_BIT_ERROR) {
        typeStr = "ERROR";
    } else if (type & LOGTYPE_BIT_WARN) {
        typeStr = "WARN";
    } else if (type & LOGTYPE_BIT_DEBUG) {
        typeStr = "DEBUG";
    } else if (type & LOGTYPE_BIT_VERBOSE) {
        typeStr = "VERBOSE";
    } else if (type & LOGTYPE_BIT_INFO) {
        typeStr = "INFO";
    }

    char temp[192];
    char* buffer = temp;
    size_t len;
    va_start(args, format);
    if (use_progmem) {
        len = vsnprintf_P(buffer, sizeof(temp), format, args);
    } else {
        len = vsnprintf(buffer, sizeof(temp), format, args);
    }
    va_end(args);
    if (len > sizeof(temp) - 1) { // same as (len >= sizeof(temp))
        buffer = new char[len + 1];
        if (!buffer) {
            return;
        }
        va_start(args, format);
        if (use_progmem) {
            len = vsnprintf_P(buffer, len + 1, format, args);
        } else {
            len = vsnprintf(buffer, len + 1, format, args);
        }
        va_end(args);
    }
    // R"({"type":"%s","src":"%s","time":"","desc":"%s","data":""})"
    _size += f.printf(R"({"c":%c, "ms":%u,"type":"%s","src":"%s","ts":%llu,"desc":"%s"})" "\n",
                '0'+Utils::getContext(), (unsigned int) millis(), typeStr, _getModuleName(module), time(NULL), buffer);
    va_end(args);

    if (buffer != temp) {
        delete[] buffer;
    }

#endif
    f.close();
    _noOfEntriesInFile++;
}


void LogfileClass::_startNewFile()
{
    if (_keepPreviousFiles == 0) {
        LittleFS.remove(_filename);
        _size = 0;
        _noOfEntriesInFile = 0;
        return;
    }

    char filename_new[LFS_NAME_MAX];
    char filename_old[LFS_NAME_MAX];
    for (uint32_t i=_keepPreviousFiles; i > 1; i--) {
        _prevFilename(i-1, filename_old);
        if (!Utils::fileExists(filename_old)) {
            continue;
        }
        _prevFilename(i, filename_new);
        LittleFS.rename(filename_old, filename_new);
    }
    _prevFilename(1, filename_new);
    LittleFS.rename(_filename, filename_new);
    //LittleFS.remove(_filename);
    _size = 0;
    _noOfEntriesInFile = 0;
}


uint32_t LogfileClass::noOfEntries()
{
    if (_size != 0xffffffff) {
        return _noOfEntriesInFile;
    }

    _noOfEntriesInFile = 0;
    _size = 0;

    File f;
    f = LittleFS.open(_filename, "r");
    if (!f) {
        return _noOfEntriesInFile;
    }

    // Get the size
    _size = f.size();

    // Do not count entries if we're already to big!
    if (_maxSize && _size >= _maxSize) {
        f.close();
        _startNewFile();
        return _noOfEntriesInFile;
    }

    // Count number of entries
    size_t rlen, linelen = 0;
    char buffer[128];
    for (;;) {
        rlen = f.readBytes(buffer, std::size(buffer));
        if (rlen == 0) {
            break;
        }
        for (size_t i=0; i<rlen; ) {
            if (buffer[i++] == '\n') {
                _noOfEntriesInFile++;
                linelen = 0;
            } else {
                if (++linelen > LOGFILE_LINE_LEN_MAX) {
                    // something is wrong with this logfile - it could blast our memory
                    // so: start a new logfile
                    f.close();
                    _startNewFile();
                    return _noOfEntriesInFile;
                }
            }
        }

    }
    f.close();
    return _noOfEntriesInFile;
}


bool LogfileClass::websocketRequestPage(AsyncWebSocket *webSocket, uint32_t clientId, uint32_t pageNo)
{
#if 1
    if (requestedLogPageClients.size() >= _requestedLogPageClientsMax) {
        LOGF_WP("websocketRequestPage(): Maximum requests reached (%u)", _requestedLogPageClientsMax);
        return false;
    }
    LOGF_DP("websocketRequestPage(): Client %u requested page %u.", clientId, pageNo);
    _requestedLogPageClient_t newRequest;
    newRequest.webSocket = webSocket;
    newRequest.clientId = clientId;
    newRequest.pageNo = pageNo;

    requestedLogPageClients.push_back(newRequest);

    return true;
#else
    if (_requestedLogPageClientIdx >= std::size(_requestedLogPageClients)) {
        return false;
    }
    _requestedLogPageClient_t *request = &_requestedLogPageClients[_requestedLogPageClientIdx++];

    request->webSocket = webSocket;
    request->clientId = clientId;
    request->pageNo = pageNo;

    return true;
#endif
}


uint32_t LogfileClass::noOfPages(uint32_t entriesPerPage)
{
    uint32_t entries = noOfEntries();

    if (entries % entriesPerPage) {
        entries += entriesPerPage - 1;
    }
    return entries / entriesPerPage;
}


void LogfileClass::_pageToEntries(uint32_t pagNo, uint32_t &entryFrom, uint32_t &entryTo, uint32_t entriesPerPage)
{
    entryFrom = (pagNo - 1) * entriesPerPage + 1;
    entryTo = entryFrom + entriesPerPage;           // dieser Eintrag wird nicht mehr ausgegeben
}


void LogfileClass::setLogLevelBits(uint32_t loglevelbits, uint32_t module)
{
    size_t i, m;

    LOGF_DP("Called setLogLevelBits(0x%02x, 0x%02x[=%s]);", loglevelbits, module, _getModuleName(module));

    if (module == LOGMODULE_ALL) {
        i = 0;
        m = std::size(_logLevelBits);
    } else if (module < std::size(_logLevelBits)) {
        i = module;
        m = module + 1;
    } else {
        return;
    }
    for (; i < m; i++) {
        _logLevelBits[i] = loglevelbits;
    }
}

#if (LOG_ENABLE_UNNEEDED_FUNCTIONS)
void LogfileClass::enableLogLevelBits(uint32_t loglevelbits, uint32_t module)
{
    size_t i, m;
    if (module == LOGMODULE_ALL) {
        i = 0;
        m = std::size(_logLevelBits);
    } else if (module < std::size(_logLevelBits)) {
        i = module;
        m = module + 1;
    } else {
        return;
    }
    for (; i < m; i++) {
        _logLevelBits[i] |= loglevelbits;
    }
}
void LogfileClass::disableLogLevelBits(uint32_t loglevelbits, uint32_t module)
{
    size_t i, m;
    if (module == LOGMODULE_ALL) {
        i = 0;
        m = std::size(_logLevelBits);
    } else if (module < std::size(_logLevelBits)) {
        i = module;
        m = module + 1;
    } else {
        return;
    }
    for (; i < m; i++) {
        _logLevelBits[i] &= (~loglevelbits);
    }
}
#endif

void LogfileClass::setLoglevel(uint32_t loglevel, uint32_t module)
{
    uint32_t loglevelbits = LOGTYPE_BIT_NONE;
    if (loglevel >= LOGLEVEL_VERBOSE) {
        loglevelbits = LOGTYPE_BIT_ERROR | LOGTYPE_BIT_WARN | LOGTYPE_BIT_INFO | LOGTYPE_BIT_DEBUG | LOGTYPE_BIT_VERBOSE;
    } else if (loglevel >= LOGLEVEL_DEBUG) {
        loglevelbits = LOGTYPE_BIT_ERROR | LOGTYPE_BIT_WARN | LOGTYPE_BIT_INFO | LOGTYPE_BIT_DEBUG;
    } else if (loglevel >= LOGLEVEL_INFO) {
        loglevelbits = LOGTYPE_BIT_ERROR | LOGTYPE_BIT_WARN | LOGTYPE_BIT_INFO;
    } else if (loglevel >= LOGLEVEL_WARNING) {
        loglevelbits = LOGTYPE_BIT_ERROR | LOGTYPE_BIT_WARN;
    } else if (loglevel >= LOGLEVEL_ERROR) {
        loglevelbits = LOGTYPE_BIT_ERROR;
    }
    setLogLevelBits(loglevelbits, module);
}


#if (LOG_ENABLE_UNNEEDED_FUNCTIONS)
#define MAX(A,B) (((A)>(B)) ?(A) :(B))
uint32_t LogfileClass::getLoglevel(uint32_t module)
{
    uint32_t loglevel=LOGLEVEL_NONE;

    size_t i, m;
    if (module == LOGMODULE_ALL) {
        i = 0;
        m = std::size(_logLevelBits);
    } else if (module < std::size(_logLevelBits)) {
        i = module;
        m = module + 1;
    } else {
        return loglevel;

    }
    for (; i < m; i++) {
        if (_logLevelBits[module] & LOGTYPE_BIT_VERBOSE) {
            loglevel = LOGLEVEL_VERBOSE;
            return loglevel;
        }
        if (_logLevelBits[module] & LOGTYPE_BIT_DEBUG) {
            loglevel = MAX(loglevel, LOGLEVEL_DEBUG);
        }
        if (_logLevelBits[module] & LOGTYPE_BIT_INFO) {
            loglevel = MAX(loglevel, LOGLEVEL_INFO);
        }
        if (_logLevelBits[module] & LOGTYPE_BIT_WARN) {
            loglevel = MAX(loglevel, LOGLEVEL_WARNING);
        }
        if (_logLevelBits[module] & LOGTYPE_BIT_ERROR) {
            loglevel = MAX(loglevel, LOGLEVEL_ERROR);
        }
    }
    return loglevel;
}
#endif


LogfileClass Log;


/* vim:set ts=4 et: */
