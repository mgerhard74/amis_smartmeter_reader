// Abhängig des Durchschnitts-Saldos der letzten 5 Zählerwerte
// (üblicherweise - die letzten 5 Sekunden) verglichen
// mit einem Aus- / Einschaltwert, wird eine URL aufgerufen.
//
// Das kann z.B. für eine via Netzwerk schaltbare Steckdose verwendet werden.
//
// Um nicht dauernd ein-/auszuschalten, wird ein Interval angegeben, welches
// beim Wechsel zwischen ein/aus nicht unterschritten wird.
//
// Sollte der AmisReader den Sync mit dem Zähler verlieren, wird
// dies einfach ignoriert (diese Werte werden nicht berücksichtigt)


#include "RemoteOnOff.h"

#include "Log.h"
#define LOGMODULE   LOGMODULE_REMOTEONOFF
#include "Network.h"

#include <WiFiClient.h>
#include <ESP8266HTTPClient.h>


#define LOOP_INTERVAL_SEC 1

RemoteOnOffClass::RemoteOnOffClass()
{
}

void RemoteOnOffClass::init()
{
    _saldoHistorySum = 0;
    _saldoHistoryLen = 0;
    _saldoHistory[0] = 0;
}

void RemoteOnOffClass::sendURL(switchState_t newState)
{
    // TODO(anyone): Prüfen, ob wir überhaupt mit dem Netzwerk verbunden sind

    if (_rebooting) {
        newState = off;
    }

    if (_lastSentState == newState) {
        return;
    }

    int httpResultCode;
    HTTPClient http;
    WiFiClient client;
    String *url = (newState == on) ?&_urlOn :&_urlOff;
    LOG_DP("Sending http get request: %s", *url->c_str());
    http.begin(client, *url);
    http.setReuse(false);
    //http.setTimeout(4000);
    httpResultCode = http.GET();
    http.end();
    _lastSentStateMs = millis();
    if (httpResultCode == HTTP_CODE_OK || !_honorHttpResult) {
        _lastSentState = newState;

    } else {
        // Failure
        _lastSentStateMs += 5000u - _switchIntervalMs; // Try again in 5 secs
    }
    LOG_DP("http result code = %d", httpResultCode);
}

bool RemoteOnOffClass::enable()
{
    if (_enabled) {
        return true;
    }
    if (Network.inAPMode()) {
        return false;
    }
    if (_urlOn.length() == 0 || _urlOff.length() == 0) {
        return false;
    }
    _lastSentStateMs = millis() - _switchIntervalMs; // Allow changing status immediately
    _ticker.attach_scheduled(LOOP_INTERVAL_SEC, std::bind(&RemoteOnOffClass::loop, this));
    _enabled = true;
    return true;

}

void RemoteOnOffClass::disable()
{
    if (!_enabled) {
        return;
    }
    sendURL(off);
    _ticker.detach();
    _enabled = false;
}

void RemoteOnOffClass::config(String &urlOn, String &urlOff,
                              int switchOnSaldoW, int switchOffSaldoW,
                              unsigned int switchIntervalSec,
                              bool honorHttpResult)
{
    if (switchOnSaldoW >= switchOffSaldoW) {
        return;
    }

    bool urlOffChanged;
    urlOffChanged = (urlOff.compareTo(_urlOff) == 0) ?false :true;


    // Ggf noch schnell ein "Off" senden
    bool wasEnabled = _enabled;
    if (_enabled && urlOffChanged) {
        disable();
    }

    _urlOn = urlOn;
    _urlOff = urlOff;
    _honorHttpResult = honorHttpResult;

    _switchIntervalMs = switchIntervalSec * 1000;
    if (_switchIntervalMs == 0) {
        _switchIntervalMs = 5000;
    }

    _switchOnSaldoW = switchOnSaldoW;
    _switchOffSaldoW = switchOffSaldoW;

    if (urlOffChanged) {
        _lastSentState = undefined;
    }

    if (wasEnabled && urlOffChanged) {
        enable();
    }

    if (_enabled) {
        // Allow honoring changes of saldos and switchInterval even we've not changed the URLs
        _lastSentStateMs = millis() - _switchIntervalMs;
    }
}


void RemoteOnOffClass::prepareReboot()
{
    // We're going down for reboot
    // Try very fast to send "off"
    if (!_enabled) {
        return;
    }

    _rebooting = true;

    if (_lastSentState == off) {
        _ticker.detach();
        _switchIntervalMs = 0xFFFFFFFF;
        return;
    }

    _lastSentState = undefined;
    _switchIntervalMs = 0; // Allow changing status immediately
    _saldoHistorySum = _saldoHistoryLen = 0; // Start a fresh calculation which saves uns some time in loop()
    _ticker.attach_ms_scheduled(10, std::bind(&RemoteOnOffClass::loop, this));  // try calling Off-URL in 10ms
}

void RemoteOnOffClass::loop() // Wird vom _ticker jede Sekunde aufgerufen falls enabled
{
    if (millis() - _lastSentStateMs < _switchIntervalMs) {
        return;
    }

    // den neuen Status ausrechnen
    switchState_t newState;
    if (_lastSentState == undefined) {
        newState = off;
    } else {
        newState = _lastSentState;
    }

    if(_saldoHistoryLen == std::size(_saldoHistory)) { // History muss vollständig gefüllt sein um den Mittelwert zu verwenden
        int saldo_mw = _saldoHistorySum / (int) std::size(_saldoHistory);
        //                     -200                X                200
        //  [state on]     _switchOnSaldoW  >=  saldo_mw  >=  _switchOffSaldoW      [state off]

        if (saldo_mw < _switchOnSaldoW) {
            newState = on;
        }
        if (_switchOffSaldoW < saldo_mw) {
            newState = off;
        }
    }

    sendURL(newState);
}

#define SIMULATE_VALUES 0
void RemoteOnOffClass::onNewValidData(uint32_t v1_7_0, uint32_t v2_7_0)
{
    if (!_enabled) {
        return;
    }
#if (SIMULATE_VALUES)
    time_t now = time(NULL);
    struct tm today;
    localtime_r(&now, &today);

    if (today.tm_sec < 20) {
        // wir haben 1000W Überschuß
        v1_7_0 = 1000;
        v2_7_0 = 0;
    } else if (today.tm_sec < 40) {
        v1_7_0 = 0;
        v2_7_0 = 0;
    } else {
        // wir verbrauchen 1000W
        v1_7_0 = 0;
        v2_7_0 = 1000;
    }
#endif
    // aktuelles Saldo (Erzeugung - Verbrauch) ganz hinten zur History hinzufügen
    // und _saldoHistorySum aktuell halten
    _saldoHistorySum -= _saldoHistory[0];
    for (size_t i=1; i < std::size(_saldoHistory); i++) {
        _saldoHistory[i-1] = _saldoHistory[i];
    }
    _saldoHistoryLen++;
    if (_saldoHistoryLen > std::size(_saldoHistory)) {
        _saldoHistoryLen = std::size(_saldoHistory);
    }
    _saldoHistory[std::size(_saldoHistory) - 1] = v1_7_0 - v2_7_0;
    _saldoHistorySum += v1_7_0 - v2_7_0;
}

RemoteOnOffClass RemoteOnOff;

/* vim:set ts=4 et: */
