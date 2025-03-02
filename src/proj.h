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

/* ping restart checks begin */
#include <AsyncPing.h>
/* ping restart checks end*/

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
#define VERSION "1.4.1"
#define APP_NAME "Amis"
  extern String dbg_string;
  extern char dbg[128];
#if DEBUG_OUTPUT==0
  #define S Serial
#else
  #define S Serial1
#endif
struct strConfig {
  String DeviceName;
  bool mqtt_retain;
  unsigned mqtt_qos;
  unsigned mqtt_keep;
  String mqtt_pub;
  String mqtt_sub;
  uint8_t rfpower;
  bool mdns;
  bool use_auth;
  bool log_sys;
  bool smart_mtr;
  bool log_amis;
  String auth_passwd;
  String auth_user;
  bool thingspeak_aktiv;
  unsigned thingspeak_iv;
  unsigned channel_id;
  String read_api_key;
  String write_api_key;
  unsigned channel_id2;
  String read_api_key2;
  unsigned rest_var;
  signed rest_ofs;
  bool rest_neg;
  bool reboot0;
  signed switch_on;
  signed switch_off;
  String switch_url_on;
  String switch_url_off;
  unsigned switch_intervall;
  bool pingrestart_do;
  String pingrestart_ip;
  unsigned pingrestart_interval;
  unsigned pingrestart_max;
};
struct kwhstruct {
  unsigned kwh_in;
  unsigned kwh_out;
  unsigned dow;
};
extern int valid;
extern int logPage;
extern AsyncWebServer server;
extern AsyncWebSocket ws;
extern WiFiClient dbg_client;
extern WiFiServer dbg_server;
extern unsigned things_cycle;
extern uint32_t a_result[9];
extern uint8_t key[16];           // Amis-Key
extern String things_up;
extern unsigned thingspeak_watch;
extern bool new_data,new_data3,amisNotOnline,ledbit;
extern unsigned first_frame;
extern uint8_t dow,dow_local;
extern uint8_t mon,year,mon_local;
extern char timecode[16];
extern unsigned kwh_day_in[7];
extern unsigned kwh_day_out[7];
extern uint32_t clientId;
extern const char PAGE_upgrade[];
extern char *stack_start;
extern void printStackSize(String txt);
extern String lastMonth;
extern AsyncServer* meter_server;

extern kwhstruct kwh_hist[7];
extern void mqtt_publish_state();
extern strConfig config;
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
extern Ticker uniTicker,secTicker;
extern bool inAPMode,mqttStatus,hwTest;
extern void generalInit();
extern String  printIP(IPAddress adress);
extern void sendZData();
extern void sendZDataWait();
extern void writeEvent(String type, String src, String desc, String data);
extern void sendEventLog(uint32_t clientId,int page);
extern void amis_poll();
extern void  histInit();
extern void upgrade (bool save);
extern void postUpgrade ();
extern void energieWeekUpdate();
extern void energieMonthUpdate();
extern void writeMonthFile(uint8_t y,uint8_t m);
extern void meter_init();
#endif
