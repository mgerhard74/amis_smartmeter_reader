#include <AsyncJson.h>
#include <LittleFS.h>

#include "proj.h"

extern uint32_t a_result[10];
extern int valid;
extern String latestYYMMInHistfile;

void historyInit()
{
    File f;
    size_t i;
    int r_len;
    uint8_t ibuffer[10];

    for (i = 0; i < 7; i++) {
        f = LittleFS.open("/hist_in" + String(i), "r");
        if (f) {
            r_len = f.read(ibuffer, 8);
            f.close();
            ibuffer[r_len] = 0;
            //kwh_day_in[i] = f.parseInt();
            kwh_day_in[i] = atoi((const char*)ibuffer);
        } else {
            kwh_day_in[i] = 0;
        }

        f = LittleFS.open("/hist_out" + String(i), "r");
        if (f) {
            r_len = f.read(ibuffer, 8);
            f.close();
            ibuffer[r_len] = 0;
            //kwh_day_in[i] = f.parseInt();
            kwh_day_out[i] = atoi((const char*)ibuffer);
            //kwh_day_out[i] = f.parseInt();
        } else {
            kwh_day_out[i] = 0;
        }
    }


    latestYYMMInHistfile = "";
    f = LittleFS.open("/monate","r");
    if (f) {
        while (f.available()) {
            latestYYMMInHistfile = f.readStringUntil('\n');
        }
    }
    f.close();
    latestYYMMInHistfile.remove(4);  // 0..3 bleibt: Also yymm
}


void energieWeekUpdate() // Wochentabelle Energie an alle WebSock-Webclients senden
{
  if (ws->count() == 0) {
      return; // No Websock clients
  }
  if (valid != 5) {
      return;
  }

  DynamicJsonBuffer jsonBuffer;
  JsonObject &root = jsonBuffer.createObject();

  int x = dow - 2;        // gestern
  if (x < 0) {
    x = 6;
  }

  root[F("today_in")]  = a_result[0];
  root[F("today_out")] = a_result[1];
  root[F("yestd_in")]  = kwh_day_in[x];
  root[F("yestd_out")] = kwh_day_out[x];
  for (size_t i=0; i < 7; i++) {
      if (kwh_hist[i].kwh_in != 0 || kwh_hist[i].kwh_out != 0) {
          JsonArray& data = root.createNestedArray("data"+String(i));
          data.add(kwh_hist[i].dow);
          data.add(kwh_hist[i].kwh_in);
          data.add(kwh_hist[i].kwh_out);
      }
  }
  String buffer;
  root.printTo(buffer);
  jsonBuffer.clear();
  ws->textAll(buffer);
}


void energieMonthUpdate() // Monatstabelle Energie an alle WebSock-Webclients senden
{
  if (ws->count() == 0) {
      return; // No Websock clients
  }

  DynamicJsonBuffer jsonBuffer;
  JsonObject &root = jsonBuffer.createObject();
  JsonArray &items = root.createNestedArray("monthlist");       // Key name JS
  File f = LittleFS.open("/monate", "r");
  while (f.available()) {
      items.add(f.readStringUntil('\n'));           // das ist Text, kein JSON-Obj!
  }
  f.close();

  String buffer;
  root.printTo(buffer);
  jsonBuffer.clear();
  ws->textAll(buffer);
}

/* vim:set ts=4 et: */
