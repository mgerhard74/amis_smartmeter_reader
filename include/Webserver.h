#pragma once

#include "Webserver_Login.h"
#include "Webserver_Rest.h"
#include "Webserver_Update.h"
#include "Webserver_ws_Console.h"
#include "Webserver_ws_Data.h"

#define WEBSERVER_HTTP_PORT     80

class WebserverClass
{
    public:
        WebserverClass();
        void init(bool upgradeMode);
        void reload();

    private:
        void onNotFound(AsyncWebServerRequest *request);
        void onRequest_Upgrade(AsyncWebServerRequest *request);

        AsyncWebServer _server;

        WebserverLoginClass _websrvLogin;
        WebserverRestClass _websrvRest;
        WebserverUpdateClass _websrvUpdate;

        WebserverWsConsoleClass _websrvWsConsole;
        WebserverWsDataClass _websrvWsData;
};

extern WebserverClass Webserver;

/* vim:set ts=4 et: */
