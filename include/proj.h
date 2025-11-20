#ifndef PROJ_H
#define PROJ_H
#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <AsyncMqttClient.h>
#include <AsyncJson.h> //needs to be declared AFTER #include <ESPAsyncWebServer.h>
#include <Ticker.h>
#include "flash_hal.h"
#include "LittleFS.h"
#include <ESP8266mDNS.h>
#include <EEPROM.h>
#include <ESP8266HTTPClient.h>

#include "config.h"

/// Debug Einstellungen:
/// DEBUGHW 0             keine Ausgaben
/// DEBUGHW 1             TCP Port 10000
/// DEBUGHW 2             DEBUG_OUTPUT 0 | 1 : Serial(Pin Txd) | Serial1(Pin GPIO2)
/// DEBUGHW 3             Websock
#define DEBUGHW 0
#define DEBUG_OUTPUT 0
#define LEDPIN 2          // LED via 470 to VCC
#define AP_PIN 14
//#define OTA
//#define STROMPREIS
#define VERSION "1.4.6"
#define APP_NAME "Amis"
  extern String dbg_string;
  extern char dbg[128];
#if DEBUG_OUTPUT==0
  #define S Serial
#else
  #define S Serial1
#endif
struct kwhstruct {
  unsigned kwh_in;
  unsigned kwh_out;
  unsigned dow;
};
extern int logPage;
extern AsyncWebServer server;
extern AsyncWebSocket ws;
extern WiFiClient dbg_client;
extern WiFiServer dbg_server;
extern unsigned things_cycle;
extern String things_up;
extern unsigned thingspeak_watch;

extern unsigned first_frame;
extern uint8_t dow;
extern uint8_t mon,myyear;
extern unsigned kwh_day_in[7];
extern unsigned kwh_day_out[7];
extern uint32_t clientId;
extern const char PAGE_upgrade[];
extern void printStackSize(String txt);
extern String lastMonth;

extern kwhstruct kwh_hist[7];
extern void mqtt_publish_state();
extern void mqtt_publish_ha_availability(bool);
extern String get_ha_availability_topic();
extern void mqtt_publish_ha_discovery();
extern void serverInit(unsigned mode);
extern void amisInit();
extern bool shouldReboot;
extern void wsClientRequest(AsyncWebSocketClient *client, size_t sz);
extern void printConfig();
extern void connectToWifi();
extern const char flashOk[];
extern void initOTA();
extern AsyncMqttClient mqttClient;
extern Ticker mqttTimer;
extern void mqtt_init();
extern Ticker secTicker;
extern bool inAPMode,mqttStatus;
extern void sendZData();
extern void sendZDataWait();
extern void writeEvent(String type, String src, String desc, String data);
extern void sendEventLog(uint32_t clientId,int page);
extern void  histInit();
extern void upgrade (bool save);
extern void postUpgrade ();
extern void energieWeekUpdate();
extern void energieMonthUpdate();
extern void writeMonthFile(uint8_t y,uint8_t m);
#endif
