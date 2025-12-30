#include "proj.h"
#include "AmisReader.h"
#include "ThingSpeak.h"

//#define DEBUG
#include "debug.h"

void sendZDataWait() {
    // Zählerdaen ausgeben (jedoch noch keine Werte verfügbar)
    DynamicJsonBuffer jsonBuffer;
    JsonObject &doc = jsonBuffer.createObject();
    doc["now"] = valid;
    doc["uptime"] = millis() / 1000;
    doc["serialnumber"] = AmisReader.getSerialNumber();
    //  doc["things_up"] = ThingSpeak.getLastResult();
    String buffer;
    doc.printTo(buffer);
    ws->textAll(buffer);
}

void sendZData() {
    // Zählerdaen ausgeben
    DynamicJsonBuffer jsonBuffer;
    JsonObject &doc = jsonBuffer.createObject();
    doc["now"] = timecode;
    doc["1_8_0"] = a_result[0];
    doc["2_8_0"] = a_result[1];
    doc["3_8_1"] = a_result[2];
    doc["4_8_1"] = a_result[3];
    doc["1_7_0"] = a_result[4];
    doc["2_7_0"] = a_result[5];
    doc["3_7_0"] = a_result[6];
    doc["4_7_0"] = a_result[7];
    doc["1_128_0"] = a_result[8];
    doc["uptime"] = millis() / 1000;
    doc["things_up"] = ThingSpeak.getLastResult();
    doc["serialnumber"] = AmisReader.getSerialNumber();

    String buffer;
    doc.printTo(buffer);
    ws->textAll(buffer);
}

/* vim:set ts=4 et: */
