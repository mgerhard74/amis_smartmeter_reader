#pragma once

#include <Ticker.h>
#include <WString.h>

class RemoteOnOffClass {
public:
    RemoteOnOffClass();
    void init();
    void config(String &urlOn, String &urlOff,
                int switchOnSaldoW, int switchOffSaldoW,
                unsigned int switchIntervalSec,
                bool honorHttpResult=false);
    bool enable();
    void disable();
    void onNewValidData(uint32_t v1_7_0, uint32_t v2_7_0);
    void prepareReboot();

private:
    String _urlOn, _urlOff;
    unsigned long _switchIntervalMs;
    int _switchOnSaldoW, _switchOffSaldoW;
    bool _honorHttpResult;

    bool _enabled = false;

    unsigned long _lastSentStateMs;

    int _saldoHistory[5];
    size_t _saldoHistoryLen;
    int _saldoHistorySum;

    typedef enum {
        undefined = -1,
        off,            // sende UrlOff
        on              // sende UrlOn
    } switchState_t;
    switchState_t _lastSentState = undefined;
    bool _retry;

    void sendURL(switchState_t state);
    void loop();
    Ticker _ticker;
};

extern RemoteOnOffClass RemoteOnOff;

/* vim:set ts=4 et: */
