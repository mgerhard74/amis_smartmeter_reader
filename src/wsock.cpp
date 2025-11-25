#include "proj.h"
#include "AmisReader.h"
#include "Network.h"
#include "Reboot.h"

//#define DEBUG
#include "debug.h"

//#include "MiniAsyncHttpClient.h"

extern void historyInit(void);

extern const char *__COMPILED_DATE_TIME_UTC_STR__;
extern const char *__COMPILED_GIT_HASH__;
extern const char *__COMPILED_GIT_BRANCH__;

void sendWeekData() {
  File f;
  uint8_t ibuffer[12];      //12870008
  unsigned i,j;
  String s;
  s="{\"weekdata\":[[";
  for (i=0; i<7;i++) {
    f=LittleFS.open("/hist_in"+String(i), "r");
    if (f) {
       j=f.read(ibuffer,10);
       ibuffer[j]=',';
       ibuffer[j+1]=0;
       f.close();
       s+=(char*)ibuffer;
    }
    else s+="0,";
  }
  s.remove(s.length()-1,1);
  s+="],[";
  for (i=0; i<7;i++) {
    f=LittleFS.open("/hist_out"+String(i), "r");
    if (f) {
       j=f.read(ibuffer,10);
       ibuffer[j]=',';
       ibuffer[j+1]=0;
       f.close();
       s+=(char*)ibuffer;
    }
    else s+="0,";
  }
  s.remove(s.length()-1,1);
  s+="]]}";
  ws.text(clientId,s);
}

void  sendEventLog(uint32_t clientId,int page) {
  DynamicJsonBuffer jsonBuffer;
  JsonObject &root = jsonBuffer.createObject();
  root["page"] = page;                                     // Key name JS
  JsonArray &items = root.createNestedArray("list");       // Key name JS
  File eventlog = LittleFS.open("/eventlog.json", "r");
  int first = (page - 1) * 20;
  int last = page * 20;
  int i = 0;
  while(eventlog.available()) {
    String item = String();
    item = eventlog.readStringUntil('\n');
    if(i >= first && i < last) {
      items.add(item);
    }
    i++;
  }
  eventlog.close();
  float pages = i / 20.0;
  root["haspages"] = ceil(pages);
  String buffer;
  root.prettyPrintTo(buffer);
  ws.text(clientId,buffer);
}

void sendZDataWait() {
  DynamicJsonBuffer jsonBuffer;
  JsonObject &doc = jsonBuffer.createObject();
  doc["now"] = valid;
  doc["uptime"] = millis()/1000;
  doc["serialnumber"] = AmisReader.getSerialNumber();
//  doc["things_up"] = things_up;
  size_t len = doc.measureLength();
  AsyncWebSocketMessageBuffer *buffer = ws.makeBuffer(len);
  if(buffer) {
    doc.printTo((char *)buffer->get(), len + 1);
    ws.textAll(buffer);
  }
}

void sendZData() {
  DynamicJsonBuffer jsonBuffer;
  JsonObject &doc = jsonBuffer.createObject();
  doc["now"] = timecode;
  doc["1_8_0"] = a_result[0];
  doc["2_8_0"] = a_result[1];
  doc["3_8_1"] = a_result[2];
  doc["4_8_1"] = a_result[3];
  doc["1_7_0"] = a_result[4];
  doc["2_7_0"] = a_result[5];
  doc["3_7_0"] = a_result[6];
  doc["4_7_0"] = a_result[7];
  doc["1_128_0"] = a_result[8];
  doc["uptime"] = millis()/1000;
  doc["things_up"] = things_up;
  doc["serialnumber"] = AmisReader.getSerialNumber();

  size_t len = doc.measureLength();
  AsyncWebSocketMessageBuffer *buffer = ws.makeBuffer(len);
  if(buffer) {
    doc.printTo((char *)buffer->get(), len + 1);
    ws.textAll(buffer);
  }
}

void printScanResult(int nFound) {
  DynamicJsonBuffer jsonBuffer;
  JsonObject &root = jsonBuffer.createObject();
  JsonArray &array = jsonBuffer.createArray();
  String buffer;
  for (int i = 0; i < nFound; ++i) {           // esp_event_legacy.h
    buffer="";
    root[F("ssid")]    = WiFi.SSID(i);
    root[F("rssi")]    = WiFi.RSSI(i);
    root[F("channel")] = WiFi.channel(i);
    root[F("encrpt")]  = String(WiFi.encryptionType(i));     // 0...5
    root.printTo(buffer);
    array.add(buffer);
  }
  buffer="";
  array.printTo(buffer);
  buffer="{\"stations\":"+buffer+"}";
  ws.text(clientId,buffer);
  WiFi.scanDelete();
}

void sendStatus(uint32_t clientId) {
  struct ip_info info;
  FSInfo fsinfo;
  if(!LittleFS.info(fsinfo)) {
    DBGOUT(F("[ WARN ] Error getting info on LittleFS"));
  }
  DynamicJsonBuffer jsonBuffer;
  JsonObject &root = jsonBuffer.createObject();
  root[F("sdk")] = ESP.getSdkVersion();
  root[F("chipid")] = String(ESP.getChipId(), HEX);
  root[F("version")] = VERSION;
  root[F("app_name")] = APP_NAME;
  root[F("app_compiled_time_utc")] = __COMPILED_DATE_TIME_UTC_STR__;
  root[F("app_compiled_git_branch")] = __COMPILED_GIT_BRANCH__;
  root[F("app_compiled_git_hash")] = __COMPILED_GIT_HASH__;
  root[F("core")] = ESP.getCoreVersion();
  root[F("sketchsize")] = ESP.getSketchSize();
  root[F("freesize")] = ESP.getFreeSketchSpace();
  root[F("littlefs_size")] = fsinfo.totalBytes;
  root[F("littlefs_used")] = fsinfo.usedBytes;
  root[F("max_free_blocksz")] = ESP.getMaxFreeBlockSize();
  root[F("heap_free")] = ESP.getFreeHeap();
  root[F("heap_fragment")] = ESP.getHeapFragmentation();
//  root[F("heap_alloc_max")] = ESP.getMaxAllocHeap();
  root[F("flashspeed")] = ESP.getFlashChipSpeed();
  root[F("flashsize")] = ESP.getFlashChipSize();
  root[F("flashmode")] = String(ESP.getFlashChipMode());
  root[F("cpu")] = ESP.getCpuFreqMHz();
  root[F("reset_reason")] = ESP.getResetReason();
  if (Network.inAPMode()) {
    wifi_get_ip_info(SOFTAP_IF, &info);
    struct softap_config conf;
    wifi_softap_get_config(&conf);
    root[F("mac")] = WiFi.softAPmacAddress();
  } else {
    wifi_get_ip_info(STATION_IF, &info);
    struct station_config conf;
    wifi_station_get_config(&conf);
    root[F("ssid")] = String(reinterpret_cast<char *>(conf.ssid));
    root[F("dns")] = WiFi.dnsIP().toString();
    root[F("mac")] = WiFi.macAddress();
    root[F("channel")] = WiFi.channel();
    root[F("rssi")] = WiFi.RSSI();
  }
  IPAddress ipaddr = IPAddress(info.ip.addr);
  IPAddress gwaddr = IPAddress(info.gw.addr);
  IPAddress nmaddr = IPAddress(info.netmask.addr);
  root[F("deviceip")] = ipaddr.toString();
  root[F("gateway")] = gwaddr.toString();
  root[F("netmask")] = nmaddr.toString();
  //root["loadaverage"] = systemLoadAverage();
  //if (ADC_MODE_VALUE == ADC_VCC) {
  root[F("vcc")] = ESP.getVcc();
  //root["vcc"] = "N/A (TOUT) ";
  root[F("mqttStatus")] = mqttStatus ? "connected":"N/A";
  root[F("ntpSynced")] = "N/A";
  String buffer;
  root.printTo(buffer);
  ws.text(clientId,buffer);
}

void send_data_file(const char *filename, uint32_t clientId) {
  DBGOUT("send file: "+String(filename)+'\n');
  File configFile = LittleFS.open(filename, "r");
  if(configFile) {
    ws.text(clientId,configFile.readString());
    configFile.close();
  }
  else eprintf("File %s not found\n",filename);
}

void clearHist() {
  String s="";
  for (unsigned j=0; j<7;j++ ) {
    kwh_day_in[j]=0;
    kwh_day_out[j]=0;
    kwh_hist[j].kwh_in=0;
    kwh_hist[j].kwh_out=0;
    kwh_hist[j].dow=0;
    s="/hist_in"+String(j);
    LittleFS.remove(s);
    s="/hist_out"+String(j);
    LittleFS.remove(s);
  }
  first_frame=0;
}

void  wsClientRequest(AsyncWebSocketClient *client, size_t sz) {
  DynamicJsonBuffer jsonBuffer;
  JsonObject &root = jsonBuffer.parseObject((char *)(client->_tempObject));
  if(!root.success()) {
    DBGOUT(F("[ WARN ] Couldn't parse WebSocket message"));
    free(client->_tempObject);
    client->_tempObject = NULL;
    return;
  }
  // Web Browser sends some commands, check which command is given
  clientId=client->id();
  const char *command = root["command"];
  if(strcmp(command, "ping") != 0)
    eprintf("[ INFO ] command: %s\n",command);
  // Check whatever the command is and act accordingly
  if(strcmp(command, "remove") == 0) {
    const char *uid = root["file"];
    LittleFS.remove(uid);
  }
  if(strcmp(command,"weekfiles")==0) {
    long zstand;
    AmisReader.disable();
    clearHist();
    for (unsigned i=0;i<7;i++){
      zstand=root["week"][0][i].as<long>();
      if (zstand) {
        File f = LittleFS.open("/hist_in"+String(i), "w");
        if(f) {
          f.print(zstand);
          f.close();
        }
      }
      zstand=root["week"][1][i].as<long>();
      if (zstand) {
        File f = LittleFS.open("/hist_out"+String(i), "w");
        if(f) {
          f.print(zstand);
          f.close();
        }
      }
    }
    historyInit();
    AmisReader.enable();
  }
  else if(strcmp(command,"monthlist")==0) {
    AmisReader.disable();
    LittleFS.remove("/monate");
    File f = LittleFS.open("/monate", "a");
    if (f) {
      int arrSize=root["month"].size();
      for (int i=0;i<arrSize;i++) {
        f.print(root["month"][i].as<String>());
        f.print('\n');
      }
      f.close();
      historyInit();
    }
    AmisReader.enable();
  }
  else if((strcmp(command, "/config_general")==0) || (strcmp(command, "/config_wifi")==0) || (strcmp(command, "/config_mqtt")==0)) {
    File f = LittleFS.open(command, "w+");
    if(f) {
      size_t len = root.measurePrettyLength();
      root.prettyPrintTo(f);
      //root.prettyPrintTo(dbg_string);
      f.close();
      eprintf("[ INFO ] %s stored in the LittleFS (%u bytes)\n",command,len);
      if (strcmp(command, "/config_general")==0) {
        Config.loadConfigGeneral();
        Config.applySettingsConfigGeneral();
      }
    }
  }
  else if(strcmp(command, "status") == 0) {
    sendStatus(client->id());
  }
  else if(strcmp(command, "ping") == 0) {
    ws.text(client->id(),F("{\"pong\":\"\"}"));
    //eprintf("ping\n");
  }
  else if(strcmp(command, "restart") == 0) {
    Reboot.startReboot();
  }
  else if(strcmp(command, "geteventlog") == 0) {
    logPage = root["page"];
    //Log in main.loop bearbeiten, Timeout!!!
  }
  else if(strcmp(command, "clearevent") == 0) {
    LittleFS.remove("/eventlog.json");
    writeEvent("WARN", "sys", "Event log cleared!", "");
  }
  else if(strcmp(command, "scan_wifi") == 0) {
    WiFi.scanNetworksAsync(printScanResult, true);
  }
  else if(strcmp(command, "getconf") == 0) {
    send_data_file("/config_general",client->id());
    send_data_file("/config_wifi",client->id());
    send_data_file("/config_mqtt",client->id());
  }
  else if(strcmp(command, "energieWeek") == 0) {
    energieWeekUpdate();         // Wochentagfiles an Webclient senden
  }
  else if(strcmp(command, "energieMonth") == 0) {
    energieMonthUpdate();         // Monatstabelle an Webclient senden
  }
  else if(strcmp(command, "weekdata") == 0) {
    sendWeekData();                   // die hist_inx + hist_outx Fileinhalte für save konfig
  }
  else if(strcmp(command, "clearhist") == 0) {
    clearHist();
  }
  else if(strcmp(command, "clearhist2") == 0) {
    LittleFS.remove("/monate");
  }
  else if(strcmp(command, "ls") == 0) {
    DynamicJsonBuffer jsonBuffer;
    JsonObject &doc = jsonBuffer.createObject();
    Dir dir = LittleFS.openDir("/");
    unsigned i=0;
    while (dir.next()) {
      File f = dir.openFile("r");
      //eprintf("%s \t %u\n",dir.fileName().c_str(),f.size());
      doc[String(i)]=dir.fileName()+' '+String(f.size());
      i++;
    }
    doc["ls"]=i;
    String buffer;
    doc.printTo(buffer);
    //DBGOUT(buffer+"\n");
    ws.text(clientId,buffer);
  }
  else if(strcmp(command, "clear") == 0) {
    LittleFS.remove(F("/config_general"));
    LittleFS.remove(F("/config_wifi"));
    LittleFS.remove(F("/config_mqtt"));
//    LittleFS.remove(F("/.idea"));
  }
  else if(strcmp(command, "print") == 0) {
    const char *uid = root["file"];
    ws.text(clientId,uid); // ws.text
    int i;
    uint8_t ibuffer[65];
    File f = LittleFS.open(uid, "r");
    if(f) {
      do {
        i=f.read(ibuffer, 64);
        ibuffer[i]=0;
        ws.text(clientId,(char *)ibuffer); // ws.text
      } while (i);
      f.close();
    }
    else ws.text(clientId,"no file\0");
  }
  else if(strcmp(command, "print2") == 0) {
    //ws.text(clientId,"prn\0"); // ws.text
    eprintf("prn\n");
    int val=0;
    uint8_t ibuffer[10];      //12870008
    File f;
    unsigned i,j;
    for (i=0;i<7;i++) {
      f=LittleFS.open("/hist_in"+String(i), "r");
      if (f) {
       ///val=f.parseInt();    !!!!!!!!!! parseInt() ist buggy, enorme Zeit- und Resourcenverschwendung!!!!!!!!!!!!!!!!!
       j=f.read(ibuffer,8);
       ibuffer[j]=0;
       f.close();
       val=atoi((char*)ibuffer);
       eprintf("%d %d\n",i,val);
//       ws.text(clientId,ibuffer); // ws.text
      }
      //else ws.text(clientId,"no file\0");
      else {
          eprintf("no file\n");
      }
    }
  } else if (!strcmp(command, "dev-tools-button1")) {
#if 0
    auto as = new MiniAsyncHttpClient();
    as->initGET("https://www.google.at");
    as->initGET("https://www.google.at:90");
    as->initGET("http://benutzer1:passwort1@www.google.at:90");
    as->initGET("http://benutzer2:passwort2@www.google.at:90/@hh");
    as->initGET("http://benutzer3:passwort3@www.google.at:90/@hh");
    as->initGET("benutzer4:passwort4@www.google.at:90/@hh");
    as->initGET("benutzer5:passwort5@www.google.at/index.html");
    delete as;
#endif
  } else if (!strcmp(command, "dev-tools-button2")) {

  }


  free(client->_tempObject);
  client->_tempObject = NULL;
}
