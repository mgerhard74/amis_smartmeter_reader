#include "proj.h"
//#define DEBUG
#ifndef DEBUG
  #define eprintf( fmt, args... )
  #define DBGOUT(...)
#else
  #if DEBUGHW>0
    #define FOO(...) __VA_ARGS__
    #define DBGOUT dbg_string+= FOO
    #if (DEBUGHW==2)
      #define eprintf(fmt, args...) S.printf(fmt, ##args)
    #elif (DEBUGHW==1 || DEBUGHW==3)
      #define eprintf(fmt, args...) {sprintf(dbg,fmt, ##args);dbg_string+=dbg;dbg[0]=0;}
    #endif
  #else
    #define eprintf( fmt, args... )
    #define DBGOUT(...)
  #endif
#endif

/* ping restart config */
unsigned int pingrestart_tickCounter;
unsigned int pingrestart_pingFails;
bool pingrestart_ping_running;
AsyncPing ping; // non-blocking
void pingrestart_ping();
/* ping restart config end */

bool pf;

void secTick();
#if DEBUGHW==1
  WiFiServer dbg_server(10000);
  WiFiClient dbg_client;
#endif
#ifdef STROMPREIS
String strompreis="";
#endif // strompreis
Ticker uniTicker,secTicker;
strConfig config;
//AsyncMqttClient mq_client;                    // ThingsPeak Client
WiFiClient thp_client;
unsigned things_cycle;
uint32_t a_result[10];
uint8_t key[16];
String things_up;
unsigned thingspeak_watch;
bool new_data,new_data3,ledbit,ledflag;
unsigned first_frame;
uint8_t dow_local,dow;
uint8_t mon,myyear,mon_local;
unsigned kwh_day_in[7];
unsigned kwh_day_out[7];
unsigned last_mon_in;
unsigned last_mon_out;
uint32_t clientId;
unsigned prev_millis;
int logPage=-1;
uint8_t updates;
String lastMonth;
#if DEBUGHW>0
  char dbg[128];
  String dbg_string;
#endif // DEBUGHW
kwhstruct kwh_hist[7];
bool inAPMode,mqttStatus,hwTest;
ADC_MODE(ADC_VCC);
int switch_last = 0;
signed int Saldomittelwert[5];

void setup(){
  Serial.begin(9600,SERIAL_8E1);      // Schnittstelle zu Amis-Z?er
  Serial.setTimeout(10);              // f. readBytes in amis.cpp
  pinMode(AP_PIN,INPUT_PULLUP);
  #if LEDPIN
  digitalWrite(LEDPIN,HIGH);
  pinMode(LEDPIN, OUTPUT);
  #endif // if LEDPIN == Serial1.txd: reroute pin function
  #if DEBUGHW==2
    #if DEBUG_OUTPUT==0
      Serial.begin(115200);
    #elif DEBUG_OUTPUT==1
      Serial1.begin(115200);
    #endif
  #endif // DEBUGHW
  #ifdef OTA
  initOTA();
  #endif // OTA
  LittleFS.begin();                 // always true! SPIFF.begin does autoformat!!!
  bool test=false;
  File f = LittleFS.open("/index.html", "r");
  if(f) f.close();
  else test=true;
  if (!test) {
    f = LittleFS.open("/custom.css", "r");
    if(f) f.close();
    else test=true;
  }
  if (test) {                     // keine html-Files
    serverInit(1);                // /upgrade.html als /
    upgrade(0);
    return;
  }
  serverInit(0);                  // /init.html als /
  generalInit();
  histInit();
  connectToWifi();  // and MQTT and NTP
  secTicker.attach_scheduled(1,secTick);
  if (config.smart_mtr)  meter_init();
  if (config.log_sys) writeEvent("INFO", "sys", "System setup completed, running", "");

  // initiate ping restart check
  if (config.pingrestart_do) {
    pingrestart_tickCounter = 0;
    pingrestart_pingFails = 0;
    pingrestart_ping_running = false;
    if (config.log_sys) writeEvent("INFO", "wifi", "Ping restart check enabled", "");
  }
  shouldReboot = false;
}

void loop(){
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

  if(shouldReboot){
    shouldReboot = false;
    secTicker.detach();
    mqttTimer.detach();
    if (config.log_sys) writeEvent("INFO", "sys", "System is going to reboot", "");
    DBGOUT("Rebooting...");
    delay(300);
    //ESP.wdtDisable();           // bootet 2x ???
    ESP.restart();
    while (1)    delay(1);
  }
  if (config.thingspeak_aktiv && thingspeak_watch>10) {
    if (config.log_sys) writeEvent("INFO", "mqtt", "Connection lost long time", "reboot");
    DBGOUT("thingspeak_watch reset");
    shouldReboot=true;
  }
  if (ws.count()) {      // ws-connections
    if (new_data3) {
      new_data3=false;
      sendZData();
    }
  }
  //if (WiFi.isConnected())
  amis_poll();                 // Rev. 20.4.2023: auch im AP-Modus abfragen
  #ifdef LEDPIN
  if (inAPMode) {
    if (millis() > prev_millis) {
      prev_millis=millis()+500;
      digitalWrite(LEDPIN,digitalRead(LEDPIN)^1);
    }
  }
  else {
    if (ledflag && ledbit) {
        digitalWrite(LEDPIN,LOW);
    }
  }
  #endif
  if (hwTest) {
    for (unsigned i=0;i < 200; i++)  {
      Serial.write(i);
      delay(1);
    }
  }
  if (logPage >=0) {
    sendEventLog(clientId,logPage);
    logPage=-1;
  }
  if (pf) {
    pf=false;
//    prnt();
  }
  delay(10);
  #ifdef LEDPIN
  if (ledflag) {
    ledflag=false;
    digitalWrite(LEDPIN,HIGH);
  }
  #endif
}


void pingrestart_ping() {
  pingrestart_tickCounter++;

  if (!config.pingrestart_do || pingrestart_tickCounter < config.pingrestart_interval) {
      return; // noch nicht genug Zeit vergangen oder ping deaktiviert
  }

  pingrestart_tickCounter = 0; // zurücksetzen

  /* callback for each answer/timeout of ping */
  //ping.on(true,[](const AsyncPingResponse& response){
  //  return false; //do not stop
  //});
  
  /* callback for end of ping */
  ping.on(false,[](const AsyncPingResponse& response){
    DBGOUT("Ping done, Result = " + String(response.answer) + ", RTT = " + String(response.total_time));

    if (response.answer) {
      if (pingrestart_pingFails > 0) {
        pingrestart_pingFails++;
        if (config.log_sys) writeEvent("INFO", "wifi", "Ping " + String(pingrestart_pingFails) + "/" + String(config.pingrestart_max) + " to " + config.pingrestart_ip + " successful, RTT = " + String(response.total_time), "");
      }
      pingrestart_pingFails = 0; // fehlerzähler zurücksetzen
    } else {
      pingrestart_pingFails++;
      if (config.log_sys) writeEvent("WARN", "wifi", "Ping " + String(pingrestart_pingFails) + "/" + String(config.pingrestart_max) + " to " + config.pingrestart_ip + " failed!", "");

      if (pingrestart_pingFails >= config.pingrestart_max) {
        if (config.log_sys) writeEvent("WARN", "wifi", "Max ping failures reached, initiating reboot ...", "");
        shouldReboot = true; // neustart erforderlich
      }
    }

    pingrestart_ping_running = false;
    return true; //doesn't matter
  });
  
  if (!pingrestart_ping_running) {
    DBGOUT("Ping to " + config.pingrestart_ip);

    pingrestart_ping_running = true;
    ping.begin(config.pingrestart_ip.c_str(), 1, 900U); // 1 ping, 900ms timeout
  } else {
    DBGOUT("Ping still running");
  }
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
  things_cycle++;
  #ifdef LEDPIN
  if (things_cycle % 4==0) ledflag=true;
  #endif
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
      
     if ((millis()/1000 > 43200) && (config.reboot0))      // Reboot wenn uptime > 12h
      {
          writeEvent("INFO", "sys", "Reboot uptime>12h", "");
		  delay(10);
          ESP.restart();
      }
    }
  }

  // Wifi Switch on/off
  if ((config.switch_url_on != "") && (config.switch_url_off != ""))
  {
    signed int xsaldo;
    xsaldo = (a_result[4] - a_result[5]);
    for (unsigned i = 4; i > 0; i--)
    {
      Saldomittelwert[i] = Saldomittelwert[i - 1];
    }
    Saldomittelwert[0] = xsaldo;
    signed int xsaldo_mw = 0;
    for (unsigned i = 0; i < 5; i++)
    {
      xsaldo_mw = xsaldo_mw + Saldomittelwert[i];
    }
    xsaldo_mw = xsaldo_mw / 5;
    unsigned int sek = (millis() / 1000) % 5;
    if (config.switch_intervall > 0)
    {
      sek = (millis() / 1000) % config.switch_intervall;
    }

    if ((xsaldo_mw < config.switch_on) && (switch_last != 1) && (sek == 0))
    {
      HTTPClient http;
      WiFiClient client;
      http.begin(client, config.switch_url_on);
      int httpCode = http.GET();
      if (httpCode == HTTP_CODE_OK)
      {
        // writeEvent("INFO", "sys", "Switch on Url sent", "");
      }
      http.end();
      switch_last = 1;
    }
    if ((xsaldo_mw > config.switch_off) && (switch_last != 2) && (sek == 0))
    {
      HTTPClient http;
      WiFiClient client;
      http.begin(client, config.switch_url_off);
      int httpCode = http.GET();
      if (httpCode == HTTP_CODE_OK)
      {
        // writeEvent("INFO", "sys", "Switch off Url sent", "");
      }
      http.end();
      switch_last = 2;
    }
  }

  // Thingspeak aktualisieren
  if (config.thingspeak_aktiv && things_cycle >= config.thingspeak_iv && new_data && valid==5) {
    things_cycle=0;
    thingspeak_watch++;
    new_data = false;

    thp_client.stop();
    if (thp_client.connect("api.thingspeak.com", 80)) {
      String data="api_key=" + String(config.write_api_key);
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

  // perform ping restart check
  pingrestart_ping();
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