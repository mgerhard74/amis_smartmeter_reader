/*
    Handles GET requests at http://<espiIp>/rest
*/

#include "Webserver_Rest.h"

#include "AmisReader.h"
#include "config.h"
#include "Databroker.h"
#include "Webserver.h"

#include <AsyncJson.h>

void WebserverRestClass::init(AsyncWebServer& server)
{
    using std::placeholders::_1;

    server.on("/rest", HTTP_GET, std::bind(&WebserverRestClass::onRestRequest, this, _1));
}

void WebserverRestClass::onRestRequest(AsyncWebServerRequest* request)
{   if (!Webserver.checkCredentials(request)) {
        return;
    }

    AsyncResponseStream *response = request->beginResponseStream(F("application/json; charset=utf-8"));
    DynamicJsonBuffer jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();

    if (Databroker.valid != 5) {
        root[F("error")] = F("No actual valid result available");
    } else {
        signed int saldo = (Databroker.results_u32[4] - Databroker.results_u32[5] - Config.rest_ofs);

        // For 64 bits seems we must define ARDUINOJSON_USE_LONG_LONG ... but 32 Bits unsigned is valid till 2106
        root["time"] = static_cast<uint32_t>(Databroker.ts);
        if (Config.rest_var == 0) {
            root[F("1.8.0")] = Databroker.results_u32[0];
            root[F("2.8.0")] = Databroker.results_u32[1];
            root[F("3.8.1")] = Databroker.results_u32[2];
            root[F("4.8.1")] = Databroker.results_u32[3];
            root[F("1.7.0")] = Databroker.results_u32[4];
            root[F("2.7.0")] = Databroker.results_u32[5];
            root[F("3.7.0")] = Databroker.results_u32[6];
            root[F("4.7.0")] = Databroker.results_u32[7];
            root[F("1.128.0")] = Databroker.results_i32[0];
        } else {
            root[F("1_8_0")] = Databroker.results_u32[0];
            root[F("2_8_0")] = Databroker.results_u32[1];
            root[F("3_8_1")] = Databroker.results_u32[2];
            root[F("4_8_1")] = Databroker.results_u32[3];
            root[F("1_7_0")] = Databroker.results_u32[4];
            root[F("2_7_0")] = Databroker.results_u32[5];
            root[F("3_7_0")] = Databroker.results_u32[6];
            root[F("4_7_0")] = Databroker.results_u32[7];
            root[F("1_128_0")] = Databroker.results_i32[0];
        }
        if (Config.rest_neg) {
            saldo = -saldo;
        }
        root[F("saldo")] = saldo;
        root[F("serialnumber")] = AmisReader.getSerialNumber();
        //root.prettyPrintTo(*response);
    }
    root.printTo(*response);
    jsonBuffer.clear();
    request->send(response);
}

/* vim:set ts=4 et: */
