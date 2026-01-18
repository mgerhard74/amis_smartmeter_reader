#include "proj.h"
#include "AmisReader.h"
#include "Databroker.h"
#include "ThingSpeak.h"

//#define DEBUG
#include "debug.h"

void sendZDataWait() {
    // Zählerdaen ausgeben (jedoch noch keine Werte verfügbar)
    DynamicJsonBuffer jsonBuffer;
    JsonObject &doc = jsonBuffer.createObject();
    doc["now"] = 0;
    // For 64 bits seems we must define ARDUINOJSON_USE_LONG_LONG ... but 32 Bits unsigned is valid till 2106
    doc["time"] = static_cast<uint32_t>(time(NULL));
    doc["valid"] = Databroker.valid;
    doc["uptime"] = millis() / 1000;
    doc["things_up"] = ThingSpeak.getLastResult(); //  Error-/Infotext or hexstring(without 0x) of timestamp from counter last transmitted
    doc["serialnumber"] = AmisReader.getSerialNumber();

    String buffer;
    doc.printTo(buffer);
    jsonBuffer.clear();
    ws->textAll(buffer);
}

void sendZData() {
    // Zählerdaen ausgeben
    DynamicJsonBuffer jsonBuffer;
    JsonObject &doc = jsonBuffer.createObject();

    doc["now"] = Databroker.timeCp48Hex;
    doc["time"] = static_cast<uint32_t>(time(NULL));
    doc["1_8_0"] = Databroker.results_u32[0];
    doc["2_8_0"] = Databroker.results_u32[1];
    doc["3_8_1"] = Databroker.results_u32[2];
    doc["4_8_1"] = Databroker.results_u32[3];
    doc["1_7_0"] = Databroker.results_u32[4];
    doc["2_7_0"] = Databroker.results_u32[5];
    doc["3_7_0"] = Databroker.results_u32[6];
    doc["4_7_0"] = Databroker.results_u32[7];
    doc["1_128_0"] = Databroker.results_i32[0];
    doc["uptime"] = millis() / 1000;
    doc["things_up"] = ThingSpeak.getLastResult();      //  Error-/Infotext or hexstring(without 0x) of timestamp from counter last transmitted
    doc["serialnumber"] = AmisReader.getSerialNumber();

    String buffer;
    doc.printTo(buffer);
    jsonBuffer.clear();
    ws->textAll(buffer);
}

/* vim:set ts=4 et: */
