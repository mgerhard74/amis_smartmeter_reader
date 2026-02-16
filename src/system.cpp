#include "Databroker.h"
#include "Json.h"
#include "Log.h"
#define LOGMODULE LOGMODULE_WEBSSOCKET

#include <LittleFS.h>

#include "proj.h"

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
    if (Databroker.valid != 5) {
        return;
    }

    /*
        {
            "today_in": 57002601,
            "today_out": 250936,
            "yestd_in": 56991932,
            "yestd_out": 250858,
            "data0": [ 2, 17767, 71 ],
            "data1": [ 1, 16549, 1176 ],
            "data2": [ 0, 18147, 0 ],
            "data3": [ 6, 18942, 48 ],
            "data4": [ 5, 16393, 0 ],
            "data5": [ 4, 15813, 121 ],
            "data6": [ 4, 15813, 121 ]
        }
    */

    StaticJsonDocument<7*JSON_ARRAY_SIZE(3) + JSON_OBJECT_SIZE(11) + 80 + 32> root; // 80 = Keystrings

    int x = dow - 2;        // gestern
    if (x < 0) {
        x = 6;
    }

    root[F("today_in")]  = Databroker.results_u32[0];
    root[F("today_out")] = Databroker.results_u32[1];
    root[F("yestd_in")]  = kwh_day_in[x];
    root[F("yestd_out")] = kwh_day_out[x];
    for (size_t i=0; i < 7; i++) {
        if (kwh_hist[i].kwh_in != 0 || kwh_hist[i].kwh_out != 0) {
            JsonArray data = root.createNestedArray("data"+String(i));
            data.add(kwh_hist[i].dow);
            data.add(kwh_hist[i].kwh_in);
            data.add(kwh_hist[i].kwh_out);
        }
    }
    String buffer;
    SERIALIZE_JSON_LOG(root, buffer);
    ws->textAll(buffer);
}


void energieMonthUpdate() // Monatstabelle Energie an alle WebSock-Webclients senden
{
    /*
    {
        "monthlist":[
            "2403 45798763 41052",
            "2404 46105098 43623",
            "2405 46575911 48787"
        ]
    }
    */
    if (ws->count() == 0) {
        return; // No Websock clients
    }

    bool isFirst = true;
    String buffer = "{ \"monthlist\": [";
    File f = LittleFS.open("/monate", "r");
    while (f.available()) {
        if (isFirst) {
            isFirst = false;
        } else {
            buffer += ",";
        }
        buffer += "\"";
        buffer += f.readStringUntil('\n');           // das ist Text, kein JSON-Obj!
        buffer += "\"";
    }
    buffer += "]}";
    f.close();

    ws->textAll(buffer);
}

/* vim:set ts=4 et: */
