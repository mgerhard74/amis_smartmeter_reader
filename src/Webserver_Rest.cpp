/*
    Handles GET requests at http://<espiIp>/rest
*/

#include "Webserver_Rest.h"

#include "AmisReader.h"
#include "config.h"

#include <AsyncJson.h>

void WebserverRestClass::init(AsyncWebServer& server)
{
    using std::placeholders::_1;

    server.on("/rest", HTTP_GET, std::bind(&WebserverRestClass::onRestRequest, this, _1));
}

void WebserverRestClass::onRestRequest(AsyncWebServerRequest* request)
{
    if (valid != 5) {
        return;
    }

    signed int saldo = (a_result[4] - a_result[5] - Config.rest_ofs);

    AsyncResponseStream *response = request->beginResponseStream(F("application/json;charset=UTF-8"));
    DynamicJsonBuffer jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();

    if (Config.rest_neg) {
        saldo = -saldo;
    }
    if (Config.rest_var == 0) {
        root[F("time")] = a_result[9];
        root[F("1.8.0")] = a_result[0];
        root[F("2.8.0")] = a_result[1];
        root[F("3.8.1")] = a_result[2];
        root[F("4.8.1")] = a_result[3];
        root[F("1.7.0")] = a_result[4];
        root[F("2.7.0")] = a_result[5];
        root[F("3.7.0")] = a_result[6];
        root[F("4.7.0")] = a_result[7];
        root[F("1.128.0")] = a_result[8];
    } else {
        root[F("time")] = a_result[9];
        root[F("1_8_0")] = a_result[0];
        root[F("2_8_0")] = a_result[1];
        root[F("3_8_1")] = a_result[2];
        root[F("4_8_1")] = a_result[3];
        root[F("1_7_0")] = a_result[4];
        root[F("2_7_0")] = a_result[5];
        root[F("3_7_0")] = a_result[6];
        root[F("4_7_0")] = a_result[7];
        root[F("1_128_0")] = a_result[8];
    }
    root[F("saldo")] = saldo;
    root[F("serialnumber")] = AmisReader.getSerialNumber();
    //root.prettyPrintTo(*response);
    root.printTo(*response);
    request->send(response);
}

/* vim:set ts=4 et: */
