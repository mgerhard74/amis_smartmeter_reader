
#include "ThingSpeak.h"

#include "Log.h"
#define LOGMODULE   LOGMODULE_THINGSPEAK
#include "Network.h"
#include "Utils.h"

void ThingSpeakClass::init()
{
    _enabled = false;
    _lastSentMs = 0;
    _intervalMs = 30000;
    _lastResult = "ThingSpeak deaktiviert.";
}

void ThingSpeakClass::enable()
{
    if (_enabled) {
        return;
    }
    if (Network.inAPMode()) {
        return;
    }
    _enabled = true;
    _readerValues.isValid = false; // force fresh data

    uint32_t now = millis();
    if (now < 30000) {
        _lastSentMs = 30000 - _intervalMs; // earliest run is after uptime of 30 sec
    } else {
        _lastSentMs = now - _intervalMs; // run as soon as we got valid data
    }
}

void ThingSpeakClass::disable()
{
    if (!_enabled) {
        return;
    }
    _ticker.detach();
    _client.stop();
    _enabled = false;

}

void ThingSpeakClass::setInterval(unsigned int intervalSeconds)
{
    uint32_t newInterval = (uint32_t)intervalSeconds * 1000ul;

    if (_intervalMs == newInterval) {
        return;
    }
    _intervalMs = (uint32_t)intervalSeconds * 1000ul;
}

void ThingSpeakClass::setApiKeyWriite(const String &apiKeyWrite)
{
    if (_apiKeyWrite.compareTo(apiKeyWrite) == 0) {
        return;
    }
    _apiKeyWrite = apiKeyWrite;
    if (!_enabled) {
        return;
    }

    _readerValues.isValid = false; // force fresh data

    uint32_t now = millis();
    if (now < 30000) {
        _lastSentMs = 30000 - _intervalMs; // earliest run is after uptime of 30 sec
    } else {
        _lastSentMs = now - _intervalMs; // run as soon as we got valid data
    }
}

void ThingSpeakClass::onNewData(bool isValid, const uint32_t *readerValues, const char *timecode)
{
    if (!_enabled) {
        return;
    }
    _readerValues.isValid = isValid;
    if (!isValid) {
        return;
    }

    for(size_t i=0; i<8; i++) {
        _readerValues.values[i] = *readerValues++;
    }
    memcpy(_readerValues.timeCode, timecode, sizeof(_readerValues.timeCode));

    /* Wir müssen vom Interval noch eine Sekunde abziehen, da wir jede Sekunde neue Werte bekommen
       und das Versenden ebenfalls etwas Zeit braucht.
       Das hätte dann zur Folge, dass unsere Millisekundenwartezeit uns immer eine ganze Sekunde kostet  */
    if (millis() - _lastSentMs > _intervalMs - 1000) {
        if (!_ticker.active()) {
            _ticker.once_ms_scheduled(10, std::bind(&ThingSpeakClass::sendData, this));
        }
    }
}

void ThingSpeakClass::sendData()
{
    if (!_enabled) {
        return;
    }

    if (!_readerValues.isValid) {
        // Try again with our next datas
        return;
    }

    if (_apiKeyWrite.isEmpty()) {
        //_lastResult = "No ThingSpeak Write API Key configured.";
        _lastResult = "Kein ThingSpeak Write API Key angegeben.";
        _lastSentMs = millis();
        return;
    }

    _client.stop();

#if (THINGSPEAK_USE_SSL)
    #warning "Enabling SSL needs a lot of CPU. System may become unresponsible!"

    _client.setInsecure();
    if (!_client.connect("api.thingspeak.com", 443)) {
        //_lastResult = "Connecting https://api.thingspeak.com failed.";
        //_lastResult = "Verbindung zu https://api.thingspeak.com fehlgeschlagen.";
        char errmsg[100];
        _client.getLastSSLError(errmsg, sizeof(errmsg));
        _lastResult = errmsg;
        return;
    }
#else
    LOG_DP("Connecting to 'api.thingspeak.com'...");
    if (!_client.connect("api.thingspeak.com", 80)) {
        //_lastResult = "Connecting http://api.thingspeak.com failed.";
        //_lastResult = "Verbindung zu http://api.thingspeak.com fehlgeschlagen.";
        LOG_EP("Connecting 'api.thingspeak.com' failed.");
        return;
    }
    LOG_DP("Connected to 'api.thingspeak.com'.");
#endif

    String data = "api_key=" + _apiKeyWrite;
    for (size_t i=0; i<std::size(_readerValues.values); i++) {
        data += "&field" + String(i+1) + "=" + String(_readerValues.values[i]);
    }
    if (IFLOG_V()) {
        LOG_PRINTF_VP("Sending data: '%s' ...", Utils::escapeJson(data.c_str(), data.length(), 0xffffffff).c_str());
    } else {
        LOG_DP("Sending data ...");
    }
    _client.print(F("POST /update HTTP/1.1\r\n"
                  "Host: api.thingspeak.com\r\n"
                  "Connection: close\r\n"
                  "Content-Type: application/x-www-form-urlencoded\r\n"
                  "Content-Length: ")); _client.print(String(data.length()));  _client.print("\r\n"
                  "\r\n");
    _client.print(data);

    LOG_DP("Data sent.");
    _lastResult = _readerValues.timeCode;
    _lastSentMs = millis();
}

const String& ThingSpeakClass::getLastResult() {
    return _lastResult;
}

ThingSpeakClass ThingSpeak;

/* vim:set ts=4 et: */
