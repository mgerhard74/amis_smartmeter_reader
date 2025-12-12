/*
    Handles GET requests at http://<espiIp>/login
*/

#include "Webserver_Login.h"

#include "config.h"

extern void writeEvent(String, String, String, String);

void WebserverLoginClass::init(AsyncWebServer& server)
{
    using std::placeholders::_1;

    server.on("/login", HTTP_GET, std::bind(&WebserverLoginClass::onRestRequest, this, _1));
}

void WebserverLoginClass::onRestRequest(AsyncWebServerRequest* request)
{
    String remoteIP = request->client()->remoteIP().toString();
    DBGOUT("login "+remoteIP+"\n");
    if (!Config.use_auth) {
        request->send(200, F("text/plain"), F("Success"));
        return;
    }
    if (!request->authenticate(Config.auth_user.c_str(), Config.auth_passwd.c_str())) {
        if (Config.log_sys) {
            writeEvent("WARN", "websrv", "New login attempt", remoteIP);
        }
        eprintf("login fail %s %s\n",Config.auth_user.c_str(), Config.auth_passwd.c_str());
        return request->requestAuthentication(Config.DeviceName.c_str());
    }
    request->send(200, F("text/plain"), F("Success"));
    DBGOUT(F("login ok\n"));
    if (Config.log_sys) {
        writeEvent("INFO", "websrv", "Login success!", remoteIP);
    }
}

/* vim:set ts=4 et: */
