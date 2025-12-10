#include "Webserver_ws_Console.h"

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

    //_simpleDigestAuth.setUsername(AUTH_USERNAME);
    //_simpleDigestAuth.setRealm("console websocket");

    reload();
}

void WebserverWsConsoleClass::reload()
{
    /*
    _ws.removeMiddleware(&_simpleDigestAuth);

    auto const& config = Configuration.get();

    if (config.Security.AllowReadonly) {
        return;
    }
    */
    _ws.enable(false);
    //_simpleDigestAuth.setPassword(config.Security.Password);
    //_ws.addMiddleware(&_simpleDigestAuth);
    _ws.closeAll();
    _ws.enable(true);
}

void WebserverWsConsoleClass::wsCleanupTaskCb()
{
    // see: https://github.com/me-no-dev/ESPAsyncWebServer#limiting-the-number-of-web-socket-clients
    _ws.cleanupClients();
}
/* vim:set ts=4 et: */
