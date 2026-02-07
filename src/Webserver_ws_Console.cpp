/*
    Websocket socket ws://<espiIp>/console which
    Forward all messages to a websocket (like a debugging colsole)
    Be careful: Do not "overload" the WiFi interface with a lot of messages!
*/

#include "Webserver_ws_Console.h"
#include "config.h"

#include <ESPAsyncWebServer.h>

WebserverWsConsoleClass::WebserverWsConsoleClass()
    : _ws("/console")
{
}

void WebserverWsConsoleClass::init(AsyncWebServer& server)
{
    server.addHandler(&_ws);
    //MessageOutput.register_ws_output(&_ws);

    _wsCleanupTicker.attach_scheduled(1, std::bind(&WebserverWsConsoleClass::wsCleanupTaskCb, this));

    _simpleDigestAuth.setRealm("console websocket");

    reload();
}

void WebserverWsConsoleClass::reload()
{
    _ws.removeMiddleware(&_simpleDigestAuth);

    if (!Config.use_auth) {
        return;
    }

    _ws.enable(false);
    _simpleDigestAuth.setUsername(Config.auth_user.c_str());
    _simpleDigestAuth.setPassword(Config.auth_passwd.c_str());
    _ws.addMiddleware(&_simpleDigestAuth);
    //_ws.setAuthentication(Config.auth_user.c_str(), Config.auth_passwd.c_str());
    _ws.closeAll();
    _ws.enable(true);
}

void WebserverWsConsoleClass::wsCleanupTaskCb()
{
    // see: https://github.com/me-no-dev/ESPAsyncWebServer#limiting-the-number-of-web-socket-clients
    _ws.cleanupClients();
}
/* vim:set ts=4 et: */
