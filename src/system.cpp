#include <AsyncJson.h>
#include <LittleFS.h>

#include "proj.h"

extern uint32_t a_result[10];
extern int valid;

void histInit () {
  File f;
  uint8_t j,ibuffer[10];
  for (unsigned i=0;i<7;i++) {
    //if (LittleFS.exists("/hist"+String(i))) LittleFS.rename("/hist"+String(i),"/hist_in"+String(i));  // change old version filenames
    f=LittleFS.open("/hist_in"+String(i), "r");
    if (f) {
       j=f.read(ibuffer,8);
       f.close();
       ibuffer[j]=0;
       f.close();
       //kwh_day_in[i]=f.parseInt();
       kwh_day_in[i]=atoi((char*)ibuffer);
    }
    else kwh_day_in[i]=0;
    f=LittleFS.open("/hist_out"+String(i), "r");
    if (f) {
       j=f.read(ibuffer,8);
       f.close();
       ibuffer[j]=0;
       f.close();
       //kwh_day_in[i]=f.parseInt();
       kwh_day_out[i]=atoi((char*)ibuffer);
       //kwh_day_out[i]=f.parseInt();
    }
    else kwh_day_out[i]=0;
  }
  f=LittleFS.open("/monate","r");
  if (f) {
    while (f.available()) {
      lastMonth=f.readStringUntil('\n');
    }
  }
  f.close();
  lastMonth.remove(4);  // 0..3 bleibt: yymm Origin Monat
}

void energieWeekUpdate() {             // Wochentabelle Energie an Webclient senden
  if (ws.count() && valid==5) {   // // ws-connections  && dow 1..7
    DynamicJsonBuffer jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();
    int x=dow-2;        // gestern
    if (x < 0) x=6;
    root[F("today_in")] =a_result[0];
    root[F("today_out")]=a_result[1];
    root[F("yestd_in")] =kwh_day_in[x];
    root[F("yestd_out")]=kwh_day_out[x];
    for (unsigned i=0; i < 7; i++) {
      if (kwh_hist[i].kwh_in != 0 || kwh_hist[i].kwh_out != 0) {
        JsonArray& data = root.createNestedArray("data"+String(i));
        data.add(kwh_hist[i].dow);
        data.add(kwh_hist[i].kwh_in);
        data.add(kwh_hist[i].kwh_out);
      }
    }
    size_t len = root.measureLength();
    AsyncWebSocketMessageBuffer *buffer = ws.makeBuffer(len);
    if(buffer) {
      root.printTo((char *)buffer->get(), len + 1);
      ws.textAll(buffer);
    }
  }
}

void energieMonthUpdate() {             // Monatstabelle Energie an Webclient senden
  //if (ws.count() && valid==5) {   // // ws-connections  && dow 1..7
  if (ws.count() ) {   // // ws-connections  && dow 1..7
    DynamicJsonBuffer jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();
    JsonArray &items = root.createNestedArray("monthlist");       // Key name JS
    File f = LittleFS.open("/monate", "r");
    while(f.available()) {
      items.add(f.readStringUntil('\n'));           // das ist Text, kein JSON-Obj!
    }
    f.close();
    size_t len = root.measureLength();
    AsyncWebSocketMessageBuffer *buffer = ws.makeBuffer(len);
    if(buffer) {
      root.printTo((char *)buffer->get(), len + 1);
      ws.textAll(buffer);
    }
  }
}
