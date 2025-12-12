#pragma once

#include <ESPAsyncWebServer.h>
#include <Ticker.h>

class WebserverWsConsoleClass
{
    public:
        WebserverWsConsoleClass();
        void init(AsyncWebServer& server);
        void reload();

    private:
        AsyncWebSocket _ws;
        AsyncAuthenticationMiddleware _simpleDigestAuth;

        Ticker _wsCleanupTicker;
        void wsCleanupTaskCb();
};

/* vim:set ts=4 et: */
