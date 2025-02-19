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

WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;
Ticker wifiReconnectTimer;
bool ms100_flag;
char *stack_start;

void printStackSize(String txt){
    char stack;
    Serial1.println(txt);
    Serial1.print (F("stack size "));
    Serial1.println (stack_start - &stack);
    Serial1.println(ESP.getFreeHeap());
}

void onWifiConnect(const WiFiEventStationModeGotIP& event) {
  #if LEDPIN
    ledbit=true;
  #endif // LEDPIN
  String data = WiFi.SSID() + " " + WiFi.localIP().toString();
  eprintf("Connected to %s\n",data.c_str());
  if (config.log_sys) writeEvent("INFO", "wifi", "WiFi is connected", data);
  if (config.mdns) {
    if (MDNS.begin(config.DeviceName)) {              // Start the mDNS responder for esp8266.local
      DBGOUT("mDNS responder started\n"); }
    else DBGOUT("Error setting up MDNS responder!\n");
  }
#if DEBUGHW==1
  dbg_server.begin();
//  dbg_server.setNoDelay(true);  Nicht benützen, bei WIFI nicht funktionell
#endif
  if (config.log_sys) writeEvent("INFO", "sys", "System setup completed, running", "");
  mqtt_init();

}

void onWifiDisconnect(const WiFiEventStationModeDisconnected& event) {
  #if LEDPIN
  ledbit=false;
  #endif // LEDPIN
  mqttTimer.detach(); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
  wifiReconnectTimer.once_scheduled(5, connectToWifi);
  if (config.log_sys) writeEvent("INFO", "wifi", "WiFi !!! DISCONNECT !!!", "");
  DBGOUT("WiFi disconnect\n");
}

//void printConfig() {
//  File file = LittleFS.open("/config_wifi","r");
//  if (!file) {
//    Serial.println(F("Failed to read file"));
//    return;
//  }
//  while (file.available()) {
//    Serial.print((char)file.read());
//  }
//  Serial.println();
//  file.close();
//}

void connectToWifi() {
  bool err=false;
  DynamicJsonBuffer jsonBuffer;
  File configFile;
//    // Update mit neuer FW
//    configFile = LittleFS.open("/config_wifi", "w+");
//    configFile.print("{\"ssid\": \"NETGEAR\",\"wifipassword\": \"passwordphrase\",\"dhcp\": 0,\"ip_static\": \"192.168.2.20\",\"ip_netmask\": \"255.255.255.0\",\"ip_gateway\": \"192.168.2.1\",\"ip_nameserver\": \"192.168.2.1\",\"rfpower\":\"\",\"mdns\": 0}");
//    configFile.close();
  DBGOUT("Connecting to Wi-Fi...\n");
  configFile = LittleFS.open("/config_wifi", "r");
  if(!configFile) {
    DBGOUT("[ ERR ] Failed to open config_wifi\n");
    writeEvent("ERROR", "wifi", "WiFi config fail", "");
    upgrade(1);
    return;
    ///err=true;
  }
  JsonObject &json = jsonBuffer.parseObject(configFile);
  configFile.close();
  if(!json.success()) {
    DBGOUT("[ WARN ] Failed to parse config_wifi\n");
    if (config.log_sys) writeEvent("ERROR", "wifi", "WiFi config error", "");
    err=true;
  }
  //json.prettyPrintTo(S);
  wifiConnectHandler = WiFi.onStationModeGotIP(onWifiConnect);
  wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWifiDisconnect);
  
  if (digitalRead(AP_PIN)==LOW || err) {
      WiFi.mode(WIFI_AP);
      WiFi.softAP( "ESP8266_AMIS");
      DBGOUT(F("AP-Mode: 192.168.4.1\n"));
      inAPMode=true;
  }
  else {
    WiFi.mode(WIFI_STA);
    
    if (json[F("allow_sleep_mode")] == false) {
      // meks sleep mode deaktivieren
      DBGOUT(F("Wifi sleep mode disabled\n"));
      WiFi.setSleepMode(WIFI_NONE_SLEEP);
      if (config.log_sys) writeEvent("INFO", "wifi", "Wifi sleep mode disabled", "");
    }

    config.pingrestart_do = json[F("pingrestart_do")].as<bool>();
    config.pingrestart_ip = json[F("pingrestart_ip")].as<String>();
    config.pingrestart_interval = json[F("pingrestart_interval")].as<int>();;
    config.pingrestart_max = json[F("pingrestart_max")].as<int>();;

    DBGOUT(F("Start Wifi\n"));
    bool dhcp=json[F("dhcp")];
    if (dhcp) DBGOUT("DHCP\n");
    if (dhcp) WiFi.hostname(config.DeviceName);               /// !!!!!!!!!!!!!Funktioniert NUR mit DHCP !!!!!!!!!!!!!
    const char *ssid = json[F("ssid")];
    const char *wifipassword = json[F("wifipassword")];
    int rfpower=20;
    config.mdns=json[F("mdns")];
    if (json[F("rfpower")]!="") rfpower = json[F("rfpower")];
    WiFi.setOutputPower(rfpower);  // 0..20.5 dBm
    if (!dhcp) {
      const char *ip_static = json[F("ip_static")];
      const char *ip_netmask = json[F("ip_netmask")];
      const char *ip_gateway = json[F("ip_gateway")];
      const char *ip_nameserver = json[F("ip_nameserver")];
      IPAddress ipStatic;
      IPAddress ipNetmask;
      IPAddress ipNameserver;
      IPAddress ipGateway;
      ipStatic.fromString(ip_static);
      ipNetmask.fromString(ip_netmask);
      ipNameserver.fromString(ip_nameserver);
      ipGateway.fromString(ip_gateway);
      WiFi.config(ipStatic, ipGateway, ipNetmask, ipNameserver);
    }
    WiFi.persistent(false);
    WiFi.setAutoReconnect(false);
    WiFi.begin(ssid, wifipassword);
    DBGOUT(F("WiFi begin\n"));
  }
}

#define CHR2BIN(c) (c-(c>='A'?55:48))

void hex2bin(String s, uint8_t *buf) {
  unsigned len = s.length();
  if (len != 32) return;

  for (unsigned i=0; i<len; ++i) {
    buf[i] = CHR2BIN(s.c_str()[i*2])<<4 | CHR2BIN(s.c_str()[i*2+1]);
  }
}

void generalInit() {
  File configFile = LittleFS.open("/config_general", "r");
  if(!configFile) {
    DBGOUT("[ WARN ] Failed to open config_general\n");
    writeEvent("ERROR", "Allgemein", "config fail", "");
    return;
  }
  DynamicJsonBuffer jsonBuffer;
  JsonObject &json = jsonBuffer.parseObject(configFile);
  configFile.close();
  if(!json.success()) {
    DBGOUT("[ WARN ] Failed to parse config_general\n");
    writeEvent("ERROR", "Allgemein", "config error", "");
    return;
  }
  //json.prettyPrintTo(Serial);
  config.DeviceName=json[F("devicename")].as<String>();
  config.use_auth=json[F("use_auth")].as<bool>();
  config.auth_passwd=json[F("auth_passwd")].as<String>();
  config.auth_user=json[F("auth_user")].as<String>();
  config.log_sys=json[F("log_sys")].as<bool>();
  config.smart_mtr=json[F("smart_mtr")].as<bool>();

  String akey=json[F("amis_key")].as<String>();
  hex2bin(akey, (uint8_t*)&key);  // key Variable belegen
  config.thingspeak_aktiv=json[F("thingspeak_aktiv")].as<bool>();
  config.channel_id=json[F("channel_id")].as<int>();
  config.write_api_key=json[F("write_api_key")].as<String>();
  config.read_api_key=json[F("read_api_key")].as<String>();
  config.thingspeak_iv=json[F("thingspeak_iv")].as<int>();
  if (config.thingspeak_iv < 30)  config.thingspeak_iv=30;
  config.channel_id2=json[F("channel_id2")].as<int>();
  config.read_api_key2=json[F("read_api_key2")].as<String>();
  config.rest_var=json[F("rest_var")].as<int>();
  config.rest_ofs=json[F("rest_ofs")].as<int>();
  config.rest_neg=json[F("rest_neg")].as<bool>();
  config.reboot0=json[F("reboot0")].as<bool>();
  config.switch_on=json[F("switch_on")].as<int>();
  config.switch_off=json[F("switch_off")].as<int>();
  config.switch_url_on=json[F("switch_url_on")].as<String>();
  config.switch_url_off=json[F("switch_url_off")].as<String>();
}

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
    while (f.available()) lastMonth=f.readStringUntil('\n');
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
