#pragma once

#include <Arduino.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <AsyncJson.h> //needs to be declared AFTER #include <ESPAsyncWebServer.h>
#include <Ticker.h>
#include "LittleFS.h"
#include <ESP8266mDNS.h>
#include <EEPROM.h>
#include <ESP8266HTTPClient.h>
#include "amis_debug.h"
#include "config.h"

//#define STROMPREIS
#define VERSION "1.5.4"
#define APP_NAME "Amis"
struct kwhstruct {
    unsigned kwh_in;
    unsigned kwh_out;
    unsigned dow;
};
extern int logPage;
//extern AsyncWebServer server;
//extern AsyncWebSocket ws;
extern AsyncWebSocket *ws;
extern unsigned first_frame;
extern uint8_t dow;
extern uint8_t mon,myyear;
extern unsigned kwh_day_in[7];
extern unsigned kwh_day_out[7];
extern uint32_t clientId;
//extern const char PAGE_upgrade[];
//extern String lastMonth;

extern kwhstruct kwh_hist[7];
extern Ticker secTicker;
extern void sendZData();
extern void sendZDataWait();
extern void writeEvent(String type, String src, String desc, String data);
extern void sendEventLog(uint32_t clientId, int page);
extern void energieWeekUpdate();
extern void energieMonthUpdate();


/* vim:set ts=4 et: */
