#include "proj.h"
#include "AmisReader.h"
#include "Databroker.h"
#include "Json.h"
#include "Log.h"
#define LOGMODULE LOGMODULE_WEBSSOCKET
#include "ThingSpeak.h"

//#define DEBUG
#include "debug.h"

void sendZDataWait() {
    // Zählerdaen ausgeben (jedoch noch keine Werte verfügbar)
    /*
    Beispiel:
    {
        "now": 0,
        "time": 4294967295,
        "valid": 5,
        "uptime": 4294967294,
        "things_up": "Verbindung zu http://api.thingspeak.com fehlgeschlagen.",
        "serialnumber": "abcdefghijklmnopqrstuvwxyz789012"
    }
    */
    StaticJsonDocument<256> doc; // Keys are <const char *>  // CHECK

    doc["now"] = 0;
    // For 64 bits seems we must define ARDUINOJSON_USE_LONG_LONG ... but 32 Bits unsigned is valid till 2106
    doc["time"] = static_cast<uint32_t>(time(NULL));
    doc["valid"] = Databroker.valid;
    doc["uptime"] = millis() / 1000;
    doc["things_up"] = ThingSpeak.getLastResult(); //  Error-/Infotext or hexstring(without 0x) of timestamp from counter last transmitted
    doc["serialnumber"] = AmisReader.getSerialNumber();

    String buffer;
    SERIALIZE_JSON_LOG(doc, buffer);
    ws->textAll(buffer);
}

void sendZData() {
    // Zählerdaen ausgeben
    /*
    Beispiel:
    {
        "now": "324b703624",
        "time": 4294967288,
        "1.8.0": 4294967295,
        "2.8.0": 4294967294,
        "3.8.1": 4294967293,
        "4.8.1": 4294967292,
        "1.7.0": 4294967291,
        "2.7.0": 4294967290,
        "3.7.0": 4294967289,
        "4.7.0": 4294967288,
        "1.128.0": -2147483647,
        "uptime": 4294967287,
        "things_up": "Verbindung zu http://api.thingspeak.com fehlgeschlagen.",
        "serialnumber": "abcdefghijklmnopqrstuvwxyz789012"
    }
    */
    StaticJsonDocument<352> doc; // Keys are <const char *>  // CHECK

    doc["now"] = Databroker.timeCp48Hex;
    doc["time"] = static_cast<uint32_t>(time(NULL));
    doc[Config_restValueKeys[1][0]] = Databroker.results_u32[0];
    doc[Config_restValueKeys[1][1]] = Databroker.results_u32[1];
    doc[Config_restValueKeys[1][2]] = Databroker.results_u32[2];
    doc[Config_restValueKeys[1][3]] = Databroker.results_u32[3];
    doc[Config_restValueKeys[1][4]] = Databroker.results_u32[4];
    doc[Config_restValueKeys[1][5]] = Databroker.results_u32[5];
    doc[Config_restValueKeys[1][6]] = Databroker.results_u32[6];
    doc[Config_restValueKeys[1][7]] = Databroker.results_u32[7];
    doc[Config_restValueKeys[1][8]] = Databroker.results_i32[0];
    doc["uptime"] = millis() / 1000;
    doc["things_up"] = ThingSpeak.getLastResult();      //  Error-/Infotext or hexstring(without 0x) of timestamp from counter last transmitted
    doc["serialnumber"] = AmisReader.getSerialNumber();

    String buffer;
    SERIALIZE_JSON_LOG(doc, buffer);
    ws->textAll(buffer);
}

/* vim:set ts=4 et: */
