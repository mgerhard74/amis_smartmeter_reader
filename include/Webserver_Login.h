#pragma once

#include <ESPAsyncWebServer.h>

class WebserverLoginClass
{
    public:
        void init(AsyncWebServer& server);

    private:
        void onRestRequest(AsyncWebServerRequest* request);
};

/* vim:set ts=4 et: */
