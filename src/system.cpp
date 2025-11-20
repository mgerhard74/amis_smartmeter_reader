#include "proj.h"

#include "AmisReader.h"
#include "LedSingle.h"

//#define DEBUG
#include "debug.h"

WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;
Ticker wifiReconnectTimer;

void onWifiConnect(const WiFiEventStationModeGotIP& event) {
  LedBlue.turnBlink(4000, 10);
  String data = WiFi.SSID() + " " + WiFi.localIP().toString();
  eprintf("Connected to %s\n",data.c_str());
  if (Config.log_sys) writeEvent("INFO", "wifi", "WiFi is connected", data);
  if (Config.mdns) {
    if (MDNS.begin(Config.DeviceName)) {              // Start the mDNS responder for esp8266.local
      DBGOUT("mDNS responder started\n"); }
    else DBGOUT("Error setting up MDNS responder!\n");
  }
#if DEBUGHW==1
  dbg_server.begin();
//  dbg_server.setNoDelay(true);  Nicht benützen, bei WIFI nicht funktionell
#endif
  if (Config.log_sys) writeEvent("INFO", "sys", "System setup completed, running", "");
  mqtt_init();

}

void onWifiDisconnect(const WiFiEventStationModeDisconnected& event) {
  LedBlue.turnOff();
  mqttTimer.detach(); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
  wifiReconnectTimer.once_scheduled(5, connectToWifi);
  if (Config.log_sys) writeEvent("INFO", "wifi", "WiFi !!! DISCONNECT !!!", "");
  DBGOUT("WiFi disconnect\n");
}

void connectToWifi() {
  bool err=false;
  DynamicJsonBuffer jsonBuffer;
  File configFile;

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
    if (Config.log_sys) writeEvent("ERROR", "wifi", "WiFi config error", "");
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
      LedBlue.turnBlink(500, 500);
  }
  else {
    WiFi.mode(WIFI_STA);

    if (json[F("allow_sleep_mode")] == false) {
      // disable sleep mode
      DBGOUT(F("Wifi sleep mode disabled\n"));
      WiFi.setSleepMode(WIFI_NONE_SLEEP);
      if (Config.log_sys) writeEvent("INFO", "wifi", "Wifi sleep mode disabled", "");
    }

    // configure ping restart check
    Config.pingrestart_do = json[F("pingrestart_do")].as<bool>();
    Config.pingrestart_ip = json[F("pingrestart_ip")].as<String>();
    Config.pingrestart_interval = json[F("pingrestart_interval")].as<int>();;
    Config.pingrestart_max = json[F("pingrestart_max")].as<int>();;

    DBGOUT(F("Start Wifi\n"));
    bool dhcp=json[F("dhcp")];
    if (dhcp) DBGOUT("DHCP\n");
    if (dhcp) WiFi.hostname(Config.DeviceName);               /// !!!!!!!!!!!!!Funktioniert NUR mit DHCP !!!!!!!!!!!!!
    const char *ssid = json[F("ssid")];
    const char *wifipassword = json[F("wifipassword")];
    int rfpower=20;
    Config.mdns=json[F("mdns")];
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
