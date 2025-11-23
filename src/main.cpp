#include "proj.h"
//#define DEBUG
#include "debug.h"


#include "AmisReader.h"
#include "LedSingle.h"
#include "ModbusSmartmeterEmulation.h"
#include "Network.h"
#include "Reboot.h"
#include "RebootAtMidnight.h"
#include "RemoteOnOff.h"
#include "Utils.h"
#include "WatchdogPing.h"


extern const char *__COMPILED_DATE_TIME_UTC_STR__;
extern const char *__COMPILED_GIT_HASH__;
extern const char *__COMPILED_GIT_BRANCH__;


void secTick();
#if DEBUGHW==1
  WiFiServer dbg_server(10000);
  WiFiClient dbg_client;
#endif
#ifdef STROMPREIS
String strompreis="";
#endif // strompreis
Ticker secTicker;

//AsyncMqttClient mq_client;                    // ThingsPeak Client
WiFiClient thp_client;
unsigned things_cycle;
String things_up;
unsigned thingspeak_watch;
bool new_data_for_thingspeak,new_data_for_websocket;
unsigned first_frame=0;
static uint8_t dow_local;
static uint8_t mon_local;
unsigned kwh_day_in[7];
unsigned kwh_day_out[7];
unsigned last_mon_in;
unsigned last_mon_out;
uint32_t clientId;
int logPage=-1;
uint8_t updates;
String lastMonth;
#if DEBUGHW>0
  char dbg[128];
  String dbg_string;
#endif // DEBUGHW
kwhstruct kwh_hist[7];
bool mqttStatus;
ADC_MODE(ADC_VCC);


void setup(){
  #if DEBUGHW==2
    #if DEBUG_OUTPUT==0
      Serial.begin(115200);
    #elif DEBUG_OUTPUT==1
      Serial1.begin(115200);
    #endif
  #endif // DEBUGHW

  pinMode(AP_PIN, INPUT_PULLUP);
  // pinMode(AP_PIN, INPUT); digitalWrite(AP_PIN, HIGH);

  // Start filesystem early - so we can do some logging
  LittleFS.begin();

  // Log some booting information
  writeEvent("INFO", "sys", "System starting...", "");
  writeEvent("INFO", "sys", "  Version", VERSION);
  writeEvent("INFO", "sys", "  Compiled [UTC]", __COMPILED_DATE_TIME_UTC_STR__);
  writeEvent("INFO", "sys", "  Git branch", __COMPILED_GIT_BRANCH__);
  writeEvent("INFO", "sys", "  Git version/hash", __COMPILED_GIT_HASH__);

  // Set timezone to CET/CEST
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();

  // Start AMIS-Reader ... it can use the time to get the serailnumber
  AmisReader.init(1);  // Init mit Serieller Schnittstelle #1
  AmisReader.enable(); // und gleich enablen

  Reboot.init();

  #ifdef OTA
  initOTA();
  #endif // OTA

  if (!Utils::fileExists("/index.html") && !Utils::fileExists("/custom.css"))
  {                     // Nötige html files nicht vorhanden
    serverInit(1);      // /upgrade.html als /
    upgrade(0);
    return;
  }

  serverInit(0);                  // /init.html als /

  Config.loadConfigGeneral();
  Config.applySettingsConfigGeneral();

  histInit();

 // Start Network
  Network.init(digitalRead(AP_PIN) == LOW);
  NetworkConfigWifi_t networkConfigWifi = Network.getConfigWifi();
  Network.connect();

  secTicker.attach_scheduled(1,secTick);

  // Smart Meter Simulator
  ModbusSmartmeterEmulation.init();
  if (Config.smart_mtr) {
    ModbusSmartmeterEmulation.enable();
  }

  // initiate ping watchdog
  WatchdogPing.init();
  WatchdogPing.config(networkConfigWifi.pingrestart_ip.c_str(), networkConfigWifi.pingrestart_interval, networkConfigWifi.pingrestart_max);
  if (networkConfigWifi.pingrestart_do) {
    WatchdogPing.enable();
    if (Config.log_sys) {
      writeEvent("INFO", "wifi", "Ping restart check enabled", "");
    }
  }

  // Netzwerksteckdose (On/Off via Netzwerk)
  RemoteOnOff.init();
  RemoteOnOff.configOnce(Config.switch_url_on, Config.switch_url_off, Config.switch_on, Config.switch_off, Config.switch_intervall);

  // Reboot um Mitternacht?
  RebootAtMidnight.init();
  RebootAtMidnight.config();
  if (Config.reboot0) {
    RebootAtMidnight.enable();
  }

  if (Config.log_sys) {
    writeEvent("INFO", "sys", "System setup completed, running", "");
  }
}

void loop() {
  #if DEBUGHW==1
  if (dbg_string.length()) {          // Debug-Ausgaben TCP
    dbg_string+="\n";
    if (!dbg_client.connected()) dbg_client.stop();
    if (!dbg_client) dbg_client = dbg_server.available();
    if (dbg_client)  dbg_client.print(dbg_string);
    dbg_string="";
  }
  #elif DEBUGHW==2
  if (dbg_string.length()) {          // Debug-Ausgaben Serial
    S.print(dbg_string);
    dbg_string="";
  }
  #elif DEBUGHW==3
  if (dbg_string.length()) {          // Debug-Ausgaben Websock
    ws.text(clientId,dbg_string);
    //Serial1.println(dbg_string);
    dbg_string="";
  }
  #endif

  #ifdef OTA
  ArduinoOTA.handle();
  #endif

  Reboot.loop();

  if (Config.thingspeak_aktiv && thingspeak_watch>10) {
    if (Config.log_sys) writeEvent("INFO", "mqtt", "Connection lost long time", "reboot");
    DBGOUT("thingspeak_watch reset");
    Reboot.startReboot();
  }
  if (ws.count()) {      // ws-connections
    if (new_data_for_websocket) {
      new_data_for_websocket=false;
      sendZData();
    }
  }

  AmisReader.loop();  // Zähler auslesen

  if (logPage >=0) {
    sendEventLog(clientId,logPage);
    logPage=-1;
  }

  LedBlue.loop();
  WatchdogPing.loop();
}


void writeHistFileIn(int x, long val) {
  DBGOUT("hist_in "+String(x)+" "+String(val)+"\n");
  File f = LittleFS.open("/hist_in"+String(x), "w");
  if(f) {
    f.print(val);
    f.close();
  }
}
void writeHistFileOut(int x, long val) {
  DBGOUT("hist_out "+String(x)+" "+String(val)+"\n");
  File f = LittleFS.open("/hist_out"+String(x), "w");
  if(f) {
    f.print(val);
    f.close();
  }
}

void writeMonthFile(uint8_t y,uint8_t m) {
  String s=String(m);
  if (s.length()<2) s="0"+s;
  s=String(y)+s;
  eprintf("F: %u %u %s",myyear, mon, s.c_str());
  File f = LittleFS.open("/monate", "a");
  f.print(s+" ");
  f.print(a_result[0]);
  f.print(" ");
  f.print(a_result[1]);
  f.print('\n');          // f.println würde \r anfügen!
  f.close();
}

void secTick() {
  // wird jede Sekunde aufgerufen
  things_cycle++;

  if (ws.count()) {        // ws-connections
    if (first_frame==0) {
      sendZDataWait();
    }
  }
  if (valid==5){
    if (first_frame==3) { // 1. Zählerdatensatz nach reset
      first_frame=2;      // nächste action beim nächsten secTick
      int x=dow-2;        // gestern
      if (x < 0) x=6;
      if (x>6) x=0;
      if (kwh_day_in[x] ==0) {          // gestern noch keine Werte: momentanen Stand wegschreiben
        kwh_day_in[x]=a_result[0];      // 1.8.0 Bezug
        writeHistFileIn(x,a_result[0]);
      }
      if (kwh_day_out[x] ==0) {         // gestern noch keine Werte: momentanen Stand wegschreiben
        kwh_day_out[x]=a_result[1];     // 2.8.0 Lieferung
        writeHistFileOut(x,a_result[1]);
      }
      dow_local=dow;
      String s=String(mon);
      if (s.length()<2) s="0"+s;
      s=String(myyear)+s;
      if (s.compareTo(lastMonth)!=0) writeMonthFile(myyear,mon);  // Monat noch nicht im File
      mon_local=mon;
    }
    else if (first_frame==2) {        // Wochentabelle Energie erzeugen
      int x=dow-2;                    // gestern, idx ab 0
      if (x < 0) x=6;
      for (int i=0; i<7;i++) {
        int vg=x-1; if (vg <0) vg=6;
        if ((kwh_day_in[x] > kwh_day_in[vg]) && kwh_day_in[vg]) {
          kwh_hist[i].kwh_in = kwh_day_in[x] - kwh_day_in[vg];
          kwh_hist[i].dow = x;
        }
        else kwh_hist[i].kwh_in = 0;
        if ((kwh_day_out[x] > kwh_day_out[vg]) && kwh_day_out[vg]) {
          kwh_hist[i].kwh_out = kwh_day_out[x] - kwh_day_out[vg];
          kwh_hist[i].dow = x;
        }
        else kwh_hist[i].kwh_out = 0;
        x--; if (x <0) x=6;
      }
      updates=3;                      // Trigger WebClients Update
      first_frame=1;                  // Tabelle erzeugt, initialisierung abgeschlossen
    }
    if (dow_local != dow) {           // Tageswechsel, dow 1..7
      int x=dow-2;                    // gestern, idx ab 0
      if (x < 0) x=6;                 // x zeigt auf gestern
      kwh_day_in[x]=a_result[0];      // 1.8.0
      writeHistFileIn(x,a_result[0]);
      kwh_day_out[x]=a_result[1];     // 2.8.0
      writeHistFileOut(x,a_result[1]);
      dow_local=dow;
      if (mon_local != mon) {         // Monatswechsel
        writeMonthFile(myyear,mon);
        mon_local=mon;
      }
      first_frame=2;                  // Wochen- + Monatstabelle Energie neu erzeugen
    }
  }

  // Thingspeak aktualisieren
  if (Config.thingspeak_aktiv && things_cycle >= Config.thingspeak_iv && new_data_for_thingspeak && valid==5) {
    things_cycle=0;
    thingspeak_watch++;
    new_data_for_thingspeak = false;

    thp_client.stop();
    if (thp_client.connect("api.thingspeak.com", 80)) {
      String data="api_key=" + String(Config.write_api_key);
    #ifdef STROMPREIS
      for (unsigned i=0;i<7;i++)
        data += "&field" + (String(i+1))+"="+(String)(a_result[i]);
        data += "&field8="+strompreis;
    #else
      for (unsigned i=0;i<8;i++)
        data += "&field" + (String(i+1))+"="+(String)(a_result[i]);
    #endif // strompreis
      thp_client.println( "POST /update HTTP/1.1" );
      thp_client.println( "Host: api.thingspeak.com" );
      thp_client.println( "Connection: close" );
      thp_client.println( "Content-Type: application/x-www-form-urlencoded" );
      thp_client.println( "Content-Length: " + String( data.length() ) );
      thp_client.println();
      thp_client.println( data );
      //DBGOUT(data+"\n");
      things_up=timecode;
    }
    else things_up="failed";
    thingspeak_watch=0;
  }
  else {
    if (updates){
      switch (updates) {
        case 2:
          energieWeekUpdate();                   // Wochentabelle Energie senden
          break;
        case 1:
          energieMonthUpdate();                   // Wochentabelle Energie senden
          break;
      }
      updates--;
    }
  }
  ws.cleanupClients();   // beendete Webclients nicht mehr updaten
}

void  writeEvent(String type, String src, String desc, String data) {
  DynamicJsonBuffer jsonBuffer;
  JsonObject &root = jsonBuffer.createObject();
  root[F("type")] = type;
  root[F("src")] = src;
  root[F("desc")] = desc;
  root[F("data")] = data;
  root[F("time")] = timecode;
  File eventlog = LittleFS.open("/eventlog.json", "a");
  if(eventlog.size() > 50000) {
    eventlog.close();
    LittleFS.remove("/eventlog.json");
    File eventlog = LittleFS.open("/eventlog.json", "a");
  }
  root.printTo(eventlog);
  eventlog.print("\n");
  eventlog.close();
}
