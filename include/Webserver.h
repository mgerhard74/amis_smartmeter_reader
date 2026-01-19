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
        void init();
        void reloadCredentials();
        bool checkCredentials(AsyncWebServerRequest* request);

    private:
        void reload();
        void onNotFound(AsyncWebServerRequest *request);
        void responseBinaryDataWithETagCache(AsyncWebServerRequest* request, const char *contentType, bool utf8, const char *contentEncoding, const uint8_t* content, size_t len, const char *md5sum);

        AsyncWebServer _server;
        AsyncStaticWebHandler *_staticFilesServer;

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
