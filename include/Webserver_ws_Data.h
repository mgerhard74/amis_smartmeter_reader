#pragma once

#include "RingBuffer.h"

#include <ESPAsyncWebServer.h>
#include <Ticker.h>

class WebserverWsDataClass
{
    public:
        WebserverWsDataClass();
        void init(AsyncWebServer& server);
        void reload();

    private:
        typedef struct _clientRequest {
            uint32_t clientId;
            char *requestData;  // we allocate 1 byte more than requestLen and add a '\0'
            size_t requestLen;

            _clientRequest()
                : clientId(0)
                , requestData(nullptr)
                , requestLen(0)
            {}
        } _clientRequest_t;

        //void onLivedataStatus(AsyncWebServerRequest* request);
        void onWebsocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len);

        AsyncWebSocket _ws;
        AsyncAuthenticationMiddleware _simpleDigestAuth;

        //uint32_t _lastPublishStats[1] = { 0 };

        //std::mutex _mutex;

        Ticker _wsCleanupTicker;
        void wsCleanupTaskCb();

        Ticker _sendDataTicker;
        void sendDataTaskCb();

        void onWifiScanCompletedCb(int nFound);
        bool _wifiScanInProgress;

        void wsClientRequest(AsyncWebSocketClient *client, char* requestData, size_t requestlen);

        RingBuffer<_clientRequest_t, 8, true> _clientRequests;
};

/* vim:set ts=4 et: */
