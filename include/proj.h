#pragma once

#include <Arduino.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <Ticker.h>
#include "LittleFS.h"
#include <ESP8266mDNS.h>
#include <EEPROM.h>
#include <ESP8266HTTPClient.h>

#include "config.h"


struct kwhstruct {
    unsigned kwh_in;
    unsigned kwh_out;
    unsigned dow;
};

//extern AsyncWebServer server;
//extern AsyncWebSocket ws;
extern AsyncWebSocket *ws;

extern unsigned first_frame;
extern uint8_t dow;
extern uint8_t mon,myyear;
extern unsigned kwh_day_in[7];
extern unsigned kwh_day_out[7];
//extern const char PAGE_upgrade[];
//extern String lastMonth;

extern kwhstruct kwh_hist[7];
extern Ticker secTicker;
extern void sendZData();
extern void sendZDataWait();
extern void energieWeekUpdate();
extern void energieMonthUpdate();


/* vim:set ts=4 et: */
