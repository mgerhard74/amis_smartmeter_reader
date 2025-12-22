#pragma once

#ifndef THINGSPEAK_USE_SSL
#define THINGSPEAK_USE_SSL 0
#endif

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
        void init() {};
        void enable();
        void disable();
        void setEnabled(bool enabled) { if (enabled) enable(); else disable(); }
        void setInterval(unsigned int intervalSeconds);
        void setApiKeyWriite(const String &apiKeyWrite);
        void onNewData(bool isValid, const uint32_t *readerValues=nullptr, const char *timecode=nullptr);
        const String &getLastResult();
    private:
        void sendData();
        void armTimerFirstRun();

        THINGSPEAK_WIFICLIENT _client;
        bool _enabled=false;
        uint32_t _lastSentMs=0;
        uint32_t _intervalMs=30000;
        String _lastResult = "ThingSpeak deaktiviert.";
        String _apiKeyWrite;
        struct {
            uint32_t values[8];
            bool isValid;
            char timeCode[13];
        } _readerValues;
        Ticker _ticker;
};

extern ThingSpeakClass ThingSpeak;

/* vim:set ts=4 et: */
