// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <ESPAsyncWebServer.h>

class WebserverShellyClass
{
    public:
        void init(AsyncWebServer& server);

    private:
        void onShellyRequest(AsyncWebServerRequest* request);
        void onRpcGetRequest(AsyncWebServerRequest* request);
        void onRpcPostRequest(AsyncWebServerRequest* request);
        void onRpcPostBody(AsyncWebServerRequest* request, uint8_t *data, size_t len, size_t index, size_t total);

        bool sendRpcResult(AsyncWebServerRequest* request, const char *method);
        void sendRpcError(AsyncWebServerRequest* request, int code, const char *method);
};

/* vim:set ts=4 et: */
