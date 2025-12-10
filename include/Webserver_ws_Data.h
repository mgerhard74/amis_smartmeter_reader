#pragma once

#include <ESPAsyncWebServer.h>
#include <Ticker.h>

class WebserverWsDataClass
{
    public:
        WebserverWsDataClass();
        void init(AsyncWebServer& server);
        void reload();

    private:
        //void onLivedataStatus(AsyncWebServerRequest* request);
        void onWebsocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len);

        AsyncWebSocket _ws;
        //AsyncAuthenticationMiddleware _simpleDigestAuth;

        //uint32_t _lastPublishStats[1] = { 0 };

        //std::mutex _mutex;

        Ticker _wsCleanupTicker;
        void wsCleanupTaskCb();

        Ticker _sendDataTicker;
        void sendDataTaskCb();
};

/* vim:set ts=4 et: */
