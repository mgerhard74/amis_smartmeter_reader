#pragma once

#ifndef THINGSPEAK_USE_SSL
#define THINGSPEAK_USE_SSL 0
#endif

// 16 Zeichen lt. https://de.mathworks.com/help/thingspeak/channel-control.html
#define THINGSPEAK_KEY_MAXLEN   16

#if (THINGSPEAK_USE_SSL)
// This stalls our application as SSL is very memory and CPU intensive
// Also: needs ~100000 bytes more flash and ~370 bytes mor RAM
#include <WiFiClientSecure.h>
#define THINGSPEAK_WIFICLIENT   WiFiClientSecure
#else
#include <WiFiClient.h>
#define THINGSPEAK_WIFICLIENT   WiFiClient
#endif

#include <WString.h>
#include <Ticker.h>


class ThingSpeakClass {
    public:
        void init();
        void enable();
        void disable();
        void setEnabled(bool enabled) { if (enabled) enable(); else disable(); }
        void setInterval(unsigned int intervalSeconds);
        void setApiKeyWrite(const char *apiKeyWrite);
        void onNewData(bool isValid, const uint32_t *readerValues=nullptr, time_t ts=0);
        const String &getLastResult();
    private:
        void sendData();
        void armTimerFirstRun();

        THINGSPEAK_WIFICLIENT _client;
        uint32_t _continuousConnectionErrors;
        bool _enabled;
        uint32_t _lastSentMs;
        uint32_t _intervalMs;
        String _lastResult;
        char _apiKeyWrite[THINGSPEAK_KEY_MAXLEN + 1];
        struct {
            time_t ts;
            uint32_t values[8];
            bool isValid;
        } _readerValues;
        Ticker _ticker;
};

extern ThingSpeakClass ThingSpeak;

/* vim:set ts=4 et: */
