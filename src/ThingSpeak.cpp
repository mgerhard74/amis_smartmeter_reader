#include "ThingSpeak.h"

#include "Log.h"
#define LOGMODULE   LOGMODULE_THINGSPEAK
#include "Network.h"
#include "Utils.h"

#define THINGSPEAK_LOG_MAX_CONNECTION_ATTEMPS   2

void ThingSpeakClass::init()
{
    _enabled = false;
    _lastSentMs = 0;
    _intervalMs = 30000;
    _lastResult = "ThingSpeak deaktiviert.";
    _continuousConnectionErrors = 0;
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
    _readerValues.isValid = false;

    if (_apiKeyWrite[0] == 0) {
        _lastResult = F("Kein ThingSpeak Write API Key angegeben.");
    } else {
        _lastResult = F("Warte auf gültige Daten");
    }

    uint32_t now = millis();
    if (now < 30000) {
        _lastSentMs = 30000 - _intervalMs;
    } else {
        _lastSentMs = now - _intervalMs;
    }

    _continuousConnectionErrors = 0;
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
    _intervalMs = newInterval;
}

void ThingSpeakClass::setApiKeyWrite(const char *apiKeyWrite)
{
    if (!strncmp(_apiKeyWrite, apiKeyWrite, sizeof(_apiKeyWrite))) {
        return;
    }
    strlcpy(_apiKeyWrite, apiKeyWrite, sizeof(_apiKeyWrite));

    if (!_enabled) {
        return;
    }

    _readerValues.isValid = false;

    if (_apiKeyWrite[0] == 0) {
        _lastResult = F("Kein ThingSpeak Write API Key angegeben.");
    }

    uint32_t now = millis();
    if (now < 30000) {
        _lastSentMs = 30000 - _intervalMs;
    } else {
        _lastSentMs = now - _intervalMs;
    }
}

void ThingSpeakClass::onNewData(bool isValid, const uint32_t *readerValues, time_t ts)
{
    if (!_enabled) {
        return;
    }

    _readerValues.isValid = isValid;
    if (!isValid) {
        return;
    }

    for (size_t i = 0; i < std::size(_readerValues.values); i++) {
        _readerValues.values[i] = *readerValues++;
    }
    _readerValues.ts = ts;

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
        return;
    }

    if (_apiKeyWrite[0] == 0) {
        _lastResult = F("Kein ThingSpeak Write API Key angegeben.");
        _lastSentMs = millis();
        return;
    }

    _client.stop();

    // Timeout setzen → verhindert Blockieren
    _client.setTimeout(2000);
    yield();

    if (!Network.isConnected()) {
        LOG_DP("Skipping - network not connected.");
        _lastResult = F("Keine WiFi Netzwerkverbindung");

        // Retry in ~10 Sekunden
        _lastSentMs = millis() - _intervalMs + 10000;
        _ticker.once_ms_scheduled(10000, std::bind(&ThingSpeakClass::sendData, this));

        yield();
        return;
    }

#if (THINGSPEAK_USE_SSL)
    _client.setInsecure();

    if (!_client.connect("api.thingspeak.com", 443)) {
        char errmsg[100];
        _client.getLastSSLError(errmsg, sizeof(errmsg));
        _lastResult = errmsg;

        yield();
        return;
    }
#else
    LOG_DP("Connecting to 'api.thingspeak.com'...");

    if (!_client.connect("api.thingspeak.com", 80)) {
        _lastResult = F("Verbindung zu http://api.thingspeak.com fehlgeschlagen.");

        _continuousConnectionErrors++;
        if (_continuousConnectionErrors <= THINGSPEAK_LOG_MAX_CONNECTION_ATTEMPS) {
            LOG_EP("Connecting 'api.thingspeak.com' failed.");
        } else if (_continuousConnectionErrors == THINGSPEAK_LOG_MAX_CONNECTION_ATTEMPS) {
            LOGF_WP("%u continuous connecting errors. Stopping logging thingspeak connecting errors.", _continuousConnectionErrors);
        }

        // 🔧 Retry statt Stillstand
        _lastSentMs = millis() - _intervalMs + 10000;

        yield();
        return;
    }

    LOG_DP("Connected to 'api.thingspeak.com'.");
#endif

    if (_continuousConnectionErrors > THINGSPEAK_LOG_MAX_CONNECTION_ATTEMPS) {
        LOGF_IP("Connected to 'api.thingspeak.com' after %u failed connection attempts.", _continuousConnectionErrors);
    }
    _continuousConnectionErrors = 0;

    String data = "api_key=" + String(_apiKeyWrite);
    for (size_t i = 0; i < std::size(_readerValues.values); i++) {
        data += "&field" + String(i + 1) + "=" + String(_readerValues.values[i]);
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
                    "Content-Length: "));
    _client.print(String(data.length()));
    _client.print("\r\n\r\n");

    _client.print(data);

    LOG_DP("Data sent.");

    // 🔧 Flush + yield gegen Hänger
    _client.flush();
    yield();

    _lastResult = String(static_cast<uint32_t>(_readerValues.ts), HEX);
    _lastSentMs = millis();
}

const String& ThingSpeakClass::getLastResult()
{
    return _lastResult;
}

ThingSpeakClass ThingSpeak;

/* vim:set ts=4 et: */
