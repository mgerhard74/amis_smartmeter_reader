#include "Webserver_ws_Data.h"
#include "config.h"

#include <ESPAsyncWebServer.h>

AsyncWebSocket *ws;
extern void wsClientRequest(AsyncWebSocketClient *client, size_t sz);

WebserverWsDataClass::WebserverWsDataClass()
    : _ws("/ws")
{
    ws = &_ws;
}

void WebserverWsDataClass::init(AsyncWebServer& server)
{
    using std::placeholders::_1;
    using std::placeholders::_2;
    using std::placeholders::_3;
    using std::placeholders::_4;
    using std::placeholders::_5;
    using std::placeholders::_6;

    server.addHandler(&_ws);
    _ws.onEvent(std::bind(&WebserverWsDataClass::onWebsocketEvent, this, _1, _2, _3, _4, _5, _6));

    _wsCleanupTicker.attach_scheduled(1, std::bind(&WebserverWsDataClass::wsCleanupTaskCb, this));
    _sendDataTicker.attach_ms_scheduled(100, std::bind(&WebserverWsDataClass::sendDataTaskCb, this));

    //_simpleDigestAuth.setUsername(AUTH_USERNAME);
    //_simpleDigestAuth.setRealm("console websocket");

    reload();
}

void WebserverWsDataClass::reload()
{
    /*
    _ws.removeMiddleware(&_simpleDigestAuth);

    auto const& config = Configuration.get();

    if (config.Security.AllowReadonly) {
        return;
    }
    */
    if (!Config.use_auth) {
        return;
    }

    _ws.enable(false);
    //_simpleDigestAuth.setPassword(config.Security.Password);
    //_ws.addMiddleware(&_simpleDigestAuth);
    _ws.setAuthentication(Config.auth_user.c_str(), Config.auth_passwd.c_str());
    _ws.closeAll();
    _ws.enable(true);
}

void WebserverWsDataClass::wsCleanupTaskCb()
{
    // see: https://github.com/me-no-dev/ESPAsyncWebServer#limiting-the-number-of-web-socket-clients
    _ws.cleanupClients();
}

void WebserverWsDataClass::sendDataTaskCb()
{
    // do nothing if no WS client is connected
    if (_ws.count() == 0) {
        return;
    }
}


void WebserverWsDataClass::onWebsocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len)
{
    if(type == WS_EVT_ERROR) {
        eprintf("Error: WebSocket[%s][%u] error(%u): %s\n", server->url(), client->id(), *((uint16_t *) arg), (char *) data);
        return;
    }

    if (type == WS_EVT_DATA) {
        AwsFrameInfo *info = (AwsFrameInfo *)arg;
        uint64_t index = info->index;
        uint64_t infolen = info->len;
        if(info->final && info->index == 0 && infolen == len) {
            //the whole message is in a single frame and we got all of it's data
            client->_tempObject = malloc(len+1);
            if(client->_tempObject != NULL) {
                memcpy((uint8_t *)(client->_tempObject), data, len);
            }
            ((uint8_t *)client->_tempObject)[len] = 0;
            wsClientRequest(client, infolen);

        } else {
            //message is comprised of multiple frames or the frame is split into multiple packets
            if (index == 0) {
                if (info->num == 0 && client->_tempObject == NULL) {
                    client->_tempObject = malloc(infolen+1);
                }
            }
            if (client->_tempObject != NULL) {
                memcpy((uint8_t *)(client->_tempObject) + index, data, len);
            }
            if ((index + len) == infolen) {
                if (info->final) {
                    ((uint8_t *)client->_tempObject)[infolen] = 0;
                    wsClientRequest(client, infolen);
                }
            }
        }
    } // type == WS_EVT_DATA
}



/* vim:set ts=4 et: */
