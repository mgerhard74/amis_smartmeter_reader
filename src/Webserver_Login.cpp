/*
    Handles GET requests at http://<espiIp>/login
*/

#include "Webserver_Login.h"

#include "config.h"
#include "Log.h"
#define LOGMODULE LOGMODULE_WEBSERVER

void WebserverLoginClass::init(AsyncWebServer& server)
{
    using std::placeholders::_1;

    server.on("/login", HTTP_GET, std::bind(&WebserverLoginClass::onRestRequest, this, _1));
}

void WebserverLoginClass::onRestRequest(AsyncWebServerRequest* request)
{
    LOGF_DP("Login attemp from " PRsIP, PRIPVal(request->client()->remoteIP()));
    DBGOUT("login "+remoteIP+"\n");
    if (!Config.use_auth) {
        request->send(200, F("text/plain"), F("Success"));
        return;
    }
    if (!request->authenticate(Config.auth_user.c_str(), Config.auth_passwd.c_str())) {
        LOGF_EP("Invalid login attemp from " PRsIP, PRIPVal(request->client()->remoteIP()));
        return request->requestAuthentication(Config.DeviceName.c_str());
    }
    request->send(200, F("text/plain"), F("Success"));
    LOGF_IP("Login from " PRsIP, PRIPVal(request->client()->remoteIP()));
}

/* vim:set ts=4 et: */
