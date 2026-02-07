#include "Log.h"
#include "amis_debug.h"
#include "unused.h"
#include "Utils.h"

#include <AsyncJson.h>
#include <LittleFS.h>

#include <stdarg.h>


#define LOGMODULE   LOGMODULE_BIT_SYSTEM

static bool fileSkipLines(File &f, uint32_t lines)
{
    size_t rlen;
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

    // get extension and cut off extensinon
    char *ext = f;
    while (strchr(ext, '.')) {
        ext = strchr(ext, '.') + 1;
    }
    if (ext == f) {
        ext = &empty;
    } else {
        ext[-1] = 0;  // ext now points to eg : "txt"
    }

    // ".prev01." + ext ==> 8 + strlen(ext) + '\0'
    while(strlen(f) + strlen(ext) + 8 + 1 > LFS_NAME_MAX) {
        f[strlen(f)-1] = 0;
    }

    char newExtension[LFS_NAME_MAX];
    snprintf(newExtension, sizeof(newExtension), ".prev%u.%s", prevNo, ext);
    strcat(f, newExtension); // NOLINT
}


void LogfileClass::init(const char *filename)
{
    strlcpy(_filename, filename, std::size(_filename));
    _reset();
}
extern AsyncWebSocket *ws;
void LogfileClass::loop()
{
    // Send requested pages of the log file to a client
    // Just handle one request per loop() call to keep system running
#if 1
    if (requestedLogPageClients.size() == 0) {
        return;
    }

    DynamicJsonBuffer jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();
    root["pages"] = noOfPages();
    _requestedLogPageClient_t request = requestedLogPageClients.front();
    // Adapt pagno to be in valid range
    if (request.pageNo == 0) {
        request.pageNo = 1;
    } else if (request.pageNo > noOfPages()) {
        request.pageNo = noOfPages();
    }
    root["page"] = request.pageNo;

    if (_noOfEntriesInFile) {
        JsonArray &items = root.createNestedArray("list");
        File f = LittleFS.open(_filename, "r");
        if (f) {
            uint32_t firstEntry, lastEntry;

            _pageToEntries(request.pageNo, firstEntry, lastEntry);
            if (fileSkipLines(f, firstEntry-1) ) {
                for(uint32_t currentEntry = firstEntry; currentEntry < lastEntry && f.available(); currentEntry++) {
                    items.add(f.readStringUntil('\n'));
                }
            }
            f.close();
        }
    }

    String buffer;
    root.printTo(buffer);
    if (!request.webSocket->text(request.clientId, buffer)) {
        LOG_EP("Could not send logfile via websocket.");
    }

    requestedLogPageClients.erase(requestedLogPageClients.begin());
#else
    if(_requestedLogPageClientIdx == 0) {
        return;
    }

    DynamicJsonBuffer jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();
    root["pages"] = noOfPages();
    for(size_t i=0; i < _requestedLogPageClientIdx; i++) {
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
                    for(uint32_t currentEntry = firstEntry; currentEntry < lastEntry && f.available(); currentEntry++) {
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

void LogfileClass::_reset()
{
    requestedLogPageClients.clear();
    logMask = LOGMODULE_BIT_ALL | LOGTYPE_BIT_ALL;
    _size = 0xffffffff;
}


extern char timecode[13];
void LogfileClass::log(uint32_t type, uint32_t module, bool use_progmem, const char *format, ...)
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

    const char *modulName = "";
    if (module & LOGMODULE_BIT_SETUP) {
        modulName = "setup";
    } else if (module & LOGMODULE_BIT_NETWORK) {
        modulName = "network";
    } else if (module & LOGMODULE_BIT_AMISREADER) {
        modulName = "reader";
    } else if (module & LOGMODULE_BIT_MODBUS) {
        modulName = "modbus";
    } else if (module & LOGMODULE_BIT_THINGSPEAK) {
        modulName = "thingspeak";
    } else if (module & LOGMODULE_BIT_MQTT) {
        modulName = "mqtt";
    } else if (module & LOGMODULE_BIT_SYSTEM) {
        modulName = "system";
    } else if (module & LOGMODULE_BIT_WEBSERVER) {
        modulName = "webserver";
    } else if (module & LOGMODULE_BIT_UPDATE) {
        modulName = "update";
    } else if (module & LOGMODULE_BIT_REBOOTATMIDNIGHT) {
        modulName = "rebootAtMidnight";
    } else if (module & LOGMODULE_BIT_WEBSSOCKET) {
        modulName = "websocket";
    } else if (module & LOGMODULE_BIT_WATCHDOGPING) {
        modulName = "watchdogPing";
    } else if (module & LOGMODULE_BIT_REMOTEONOFF) {
        modulName = "remoteOnOff";
    } else if (module & LOGMODULE_BIT_SHELLY) {
        modulName = "shelly";
    }

#if 0
// neues Format
    // Type[IWEVD] Module[Name] millis() time() message
    _size += f.printf("%c %s %lu %llu", typeChar, modulName, millis(), time(NULL));
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

    const char *t="";
    if (type & LOGTYPE_BIT_ERROR) {
        t = "ERROR";
    } else if (type & LOGTYPE_BIT_WARN) {
        t = "WARN";
    } else if (type & LOGTYPE_BIT_DEBUG) {
        t = "DEBUG";
    } else if (type & LOGTYPE_BIT_VERBOSE) {
        t = "VERBOSE";
    } else if (type & LOGTYPE_BIT_INFO) {
        t = "INFO";
    }

    char temp[128];
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

    DBG_NOCTX("[LOG][%s][%s] %s", t, modulName, buffer);

    // R"({"type":"%s","src":"%s","time":"","desc":"%s","data":""})"
    _size += f.printf(R"({"ms":%u,"type":"%s","src":"%s","time":"%s","data":"","desc":"%s"})",
                (unsigned int) millis(), t, modulName, timecode, buffer);
    _size += f.write('\n');

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
    for(uint32_t i=_keepPreviousFiles; i > 1; i--) {
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

    // Set the size
    _size = f.size();

    // Do not count if we're already to big!
    if (_maxSize && _size >= _maxSize) {
        f.close();
        _startNewFile();
        return _noOfEntriesInFile;
    }

    // Count number of entries
    size_t rlen;
    char buffer[128];
    for(;;) {
        rlen = f.readBytes(buffer, std::size(buffer));
        if (rlen == 0) {
            return false;
        }
        for (size_t i=0; i<rlen; ) {
            if (buffer[i++] == '\n') {
                _noOfEntriesInFile++;
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
        LOG_WP("websocketRequestPage(): Maximum requests reached (%u)", _requestedLogPageClientsMax);
        return false;
    }
    LOG_VP("websocketRequestPage(): Client %u requested page %u.", clientId, pageNo);
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


void LogfileClass::setTypes(uint32_t types)
{
    logMask &= (~LOGTYPE_BIT_ALL);
    logMask |= (types & LOGTYPE_BIT_ALL);
}
void LogfileClass::enableType(uint32_t type)
{
    logMask |= (type & LOGTYPE_BIT_ALL);
}
void LogfileClass::disableType(uint32_t type)
{
    logMask &= (~(type & LOGTYPE_BIT_ALL));
}
void LogfileClass::setModules(uint32_t modules)
{
    logMask &= (~LOGMODULE_BIT_ALL);
    logMask |= (modules & LOGMODULE_BIT_ALL);
}
void LogfileClass::enableModule(uint32_t module)
{
    logMask |= (module & LOGMODULE_BIT_ALL);
}
void LogfileClass::disableModule(uint32_t module)
{
    logMask &= (~(module & LOGMODULE_BIT_ALL));
}

void LogfileClass::setLoglevel(uint32_t loglevel)
{
    uint32_t logtypes = LOGTYPE_BIT_NONE;
    if (loglevel >= LOGLEVEL_VERBOSE) {
        logtypes = LOGTYPE_BIT_ERROR | LOGTYPE_BIT_WARN | LOGTYPE_BIT_INFO | LOGTYPE_BIT_DEBUG | LOGTYPE_BIT_VERBOSE;
    } else if (loglevel >= LOGLEVEL_DEBUG) {
        logtypes = LOGTYPE_BIT_ERROR | LOGTYPE_BIT_WARN | LOGTYPE_BIT_INFO | LOGTYPE_BIT_DEBUG;
    } else if (loglevel >= LOGLEVEL_INFO) {
        logtypes = LOGTYPE_BIT_ERROR | LOGTYPE_BIT_WARN | LOGTYPE_BIT_INFO;
    } else if (loglevel >= LOGLEVEL_WARNING) {
        logtypes = LOGTYPE_BIT_ERROR | LOGTYPE_BIT_WARN;
    } else if (loglevel >= LOGLEVEL_ERROR) {
        logtypes = LOGTYPE_BIT_ERROR;
    }
    setTypes(logtypes);
}
uint32_t LogfileClass::getLoglevel()
{
    if (logMask & LOGTYPE_BIT_VERBOSE) {
        return LOGLEVEL_VERBOSE;
    }
    if (logMask & LOGTYPE_BIT_DEBUG) {
        return LOGLEVEL_DEBUG;
    }
    if (logMask & LOGTYPE_BIT_INFO) {
        return LOGLEVEL_INFO;
    }
    if (logMask & LOGTYPE_BIT_WARN) {
        return LOGLEVEL_WARNING;
    }
    if (logMask & LOGTYPE_BIT_ERROR) {
        return LOGLEVEL_ERROR;
    }
    return LOGLEVEL_NONE;
}


LogfileClass Log;


/* vim:set ts=4 et: */
