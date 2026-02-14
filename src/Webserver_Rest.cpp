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
        root[Config_restValueKeys[Config.rest_var][0]] = Databroker.results_u32[0];
        root[Config_restValueKeys[Config.rest_var][1]] = Databroker.results_u32[1];
        root[Config_restValueKeys[Config.rest_var][2]] = Databroker.results_u32[2];
        root[Config_restValueKeys[Config.rest_var][3]] = Databroker.results_u32[3];
        root[Config_restValueKeys[Config.rest_var][4]] = Databroker.results_u32[4];
        root[Config_restValueKeys[Config.rest_var][5]] = Databroker.results_u32[5];
        root[Config_restValueKeys[Config.rest_var][6]] = Databroker.results_u32[6];
        root[Config_restValueKeys[Config.rest_var][7]] = Databroker.results_u32[7];
        root[Config_restValueKeys[Config.rest_var][8]] = Databroker.results_i32[0];
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
