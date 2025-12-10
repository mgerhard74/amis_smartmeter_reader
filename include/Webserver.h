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
        void setCredentials(bool auth_enabled, const String &auth_username, const String &auth_password);
        bool checkCredentials(AsyncWebServerRequest* request);

    private:
        void onNotFound(AsyncWebServerRequest *request);
        void onRequest_Upgrade(AsyncWebServerRequest *request);
        void reload();

        AsyncWebServer _server;

        WebserverLoginClass _websrvLogin;
        WebserverRestClass _websrvRest;
        WebserverUpdateClass _websrvUpdate;

        WebserverWsConsoleClass _websrvWsConsole;
        WebserverWsDataClass _websrvWsData;

        bool _auth_enabled;
        String _auth_username, _auth_password;
};

extern WebserverClass Webserver;

/* vim:set ts=4 et: */
