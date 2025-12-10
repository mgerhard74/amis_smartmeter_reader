#include "proj.h"
#include "AmisReader.h"
#include "ThingSpeak.h"

//#define DEBUG
#include "debug.h"

void sendEventLog(uint32_t clientId, int page) {
  DynamicJsonBuffer jsonBuffer;
  JsonObject &root = jsonBuffer.createObject();
  root["page"] = page;                                     // Key name JS
  JsonArray &items = root.createNestedArray("list");       // Key name JS
  File eventlog = LittleFS.open("/eventlog.json", "r");
  int first = (page - 1) * 20;
  int last = page * 20;
  int i = 0;
  while(eventlog.available()) {
    String item = String();
    item = eventlog.readStringUntil('\n');
    if(i >= first && i < last) {
      items.add(item);
    }
    i++;
  }
  eventlog.close();
  float pages = i / 20.0;
  root["haspages"] = ceil(pages);
  String buffer;
  root.prettyPrintTo(buffer);
  ws->text(clientId,buffer);
}

void sendZDataWait() {
  DynamicJsonBuffer jsonBuffer;
  JsonObject &doc = jsonBuffer.createObject();
  doc["now"] = valid;
  doc["uptime"] = millis()/1000;
  doc["serialnumber"] = AmisReader.getSerialNumber();
//  doc["things_up"] = ThingSpeak.getLastResult();
  size_t len = doc.measureLength();
  AsyncWebSocketMessageBuffer *buffer = ws->makeBuffer(len);
  if(buffer) {
    doc.printTo((char *)buffer->get(), len + 1);
    ws->textAll(buffer);
  }
}

void sendZData() {
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
  doc["uptime"] = millis()/1000;
  doc["things_up"] = ThingSpeak.getLastResult();
  doc["serialnumber"] = AmisReader.getSerialNumber();

  size_t len = doc.measureLength();
  AsyncWebSocketMessageBuffer *buffer = ws->makeBuffer(len);
  if(buffer) {
    doc.printTo((char *)buffer->get(), len + 1);
    ws->textAll(buffer);
  }
}
