/*
    Handle websocket communication of socket ws://<espiIp>/ws
*/

#include "Webserver_ws_Data.h"

#include "config.h"
#include "FileBlob.h"
#include "Mqtt.h"
#include "Network.h"
#include "Reboot.h"
#include "Webserver.h"
#include "unused.h"
#include "Utils.h"

#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

static void wsSendFile(const char *filename, AsyncWebSocketClient *client);
static void sendStatus(AsyncWebSocketClient *client);
static void sendWeekData(AsyncWebSocketClient *client);
static void clearHist();
static bool EEPROMClear();

extern bool doSerialHwTest;


AsyncWebSocket *ws;

WebserverWsDataClass::WebserverWsDataClass()
    : _ws("/ws")
{
    ws = &_ws;
}

void WebserverWsDataClass::init(AsyncWebServer& server)
{
    _subscribedClientsWifiScanLen = 0;

    using std::placeholders::_1;
    using std::placeholders::_2;
    using std::placeholders::_3;
    using std::placeholders::_4;
    using std::placeholders::_5;
    using std::placeholders::_6;

    server.addHandler(&_ws);
    _ws.onEvent(std::bind(&WebserverWsDataClass::onWebsocketEvent, this, _1, _2, _3, _4, _5, _6));

    _wsCleanupTicker.attach_scheduled(1, std::bind(&WebserverWsDataClass::wsCleanupTaskCb, this));
    _sendDataTicker.attach_ms_scheduled(100, std::bind(&WebserverWsDataClass::sendDataTaskCb, this));

    _simpleDigestAuth.setRealm("data websocket");

    reload();
}

void WebserverWsDataClass::reload()
{
    _ws.removeMiddleware(&_simpleDigestAuth);

    if (!Config.use_auth) {
        return;
    }

    _ws.enable(false);
    _simpleDigestAuth.setUsername(Config.auth_user.c_str());
    _simpleDigestAuth.setPassword(Config.auth_passwd.c_str());
    _ws.addMiddleware(&_simpleDigestAuth);
    //_ws.setAuthentication(Config.auth_user.c_str(), Config.auth_passwd.c_str());
    _ws.closeAll();
    _subscribedClientsWifiScanLen = 0;
    _ws.enable(true);
}

void WebserverWsDataClass::wsCleanupTaskCb()
{
    // see: https://github.com/me-no-dev/ESPAsyncWebServer#limiting-the-number-of-web-socket-clients
    _ws.cleanupClients();
}

void WebserverWsDataClass::sendDataTaskCb()
{
    // do nothing if no WS client is connected
    if (_ws.count() == 0) {
        return;
    }
    // Do some periodic sending stuff here
}


void WebserverWsDataClass::onWebsocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len)
{
    UNUSED_ARG(server);

    if(type == WS_EVT_ERROR) {
        eprintf("Error: WebSocket[%s][%u] error(%u): %s\n", server->url(), client->id(), *((uint16_t *) arg), (char *) data);
        return;
    }

    if (type == WS_EVT_DATA) {
        AwsFrameInfo *info = (AwsFrameInfo *)arg;
        uint64_t index = info->index;
        uint64_t infolen = info->len;

        if(info->final && info->index == 0 && infolen == len) {
            //the whole message is in a single frame and we got all of it's data
            client->_tempObject = malloc(infolen + 1);
            if (client->_tempObject) {
                memcpy((uint8_t *)(client->_tempObject), data, len);
                ((uint8_t *)client->_tempObject)[len] = 0;
                wsClientRequest(client, infolen);
            }

        } else {
            //message is comprised of multiple frames or the frame is split into multiple packets
            if (index == 0) {
                if (info->num == 0 && client->_tempObject == NULL) {
                    client->_tempObject = malloc(infolen + 1);
                }
            }
            if (client->_tempObject) {
                memcpy((uint8_t *)(client->_tempObject) + index, data, len);
                if ((index + len) == infolen) {
                    if (info->final) {
                        ((uint8_t *)client->_tempObject)[infolen] = 0;
                        wsClientRequest(client, infolen);
                    }
                }
            }
        }

        if (info->final && client->_tempObject) {
            free(client->_tempObject);
            client->_tempObject = NULL;
        }
    } // type == WS_EVT_DATA
}


#include "AmisReader.h"
#include "proj.h"
extern uint32_t clientId;
extern int logPage;
extern unsigned first_frame;
extern unsigned kwh_day_in[7];
extern unsigned kwh_day_out[7];
/*struct kwhstruct {
  unsigned kwh_in;
  unsigned kwh_out;
  unsigned dow;
};*/
extern kwhstruct kwh_hist[7];
extern void historyInit(void);
extern void writeEvent(String, String, String, String);
extern void energieWeekUpdate();
extern void energieMonthUpdate();
extern const char *__COMPILED_DATE_TIME_UTC_STR__;
extern const char *__COMPILED_GIT_HASH__;
extern const char *__COMPILED_GIT_BRANCH__;

static void clearHist() {
    String s="";
    for (unsigned j=0; j<7;j++ ) {
        kwh_day_in[j] = 0;
        kwh_day_out[j] = 0;
        kwh_hist[j].kwh_in = 0;
        kwh_hist[j].kwh_out = 0;
        kwh_hist[j].dow = 0;
        s = "/hist_in" + String(j);
        LittleFS.remove(s);
        s = "/hist_out" + String(j);
        LittleFS.remove(s);
    }
    first_frame = 0;
}

void WebserverWsDataClass::wsClientRequest(AsyncWebSocketClient *client, size_t tempObjectLength) {
    UNUSED_ARG(tempObjectLength);

    DynamicJsonBuffer jsonBuffer;
    JsonObject &root = jsonBuffer.parseObject((char *)(client->_tempObject));
    if (!root.success()) {
        DBGOUT(F("[ WARN ] Couldn't parse WebSocket message"));
        return;
    }
    // Web Browser sends some commands, check which command is given
    const char *command = root["command"];

    if(!strcmp(command, "ping")) {
        // handle "ping" explicit here as it's the most used command
        ws->text(client->id(),F("{\"pong\":\"\"}"));
        return;
    }

    clientId = client->id();

    // Check whatever the command is and act accordingly
    eprintf("[ INFO ] command: %s\n",command);

    if(strcmp(command, "remove") == 0) {
        const char *filename = root["file"];
        if (filename && filename[0]) {
            LittleFS.remove(filename);
        }
    } if(strcmp(command,"weekfiles")==0) {
        uint32_t zstand;
        AmisReader.disable();
        clearHist();
        for (unsigned i=0; i<7; i++){
            zstand = root["week"][0][i].as<uint32_t>();
            if (zstand) {
            File f = LittleFS.open("/hist_in"+String(i), "w");
            if(f) {
                f.print(zstand);
                f.close();
            }
            }
            zstand = root["week"][1][i].as<uint32_t>();
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
    } else if(strcmp(command,"monthlist")==0) {
        AmisReader.disable();
        LittleFS.remove("/monate");
        File f = LittleFS.open("/monate", "w");
        if (f) {
            int arrSize = root["month"].size();
            for (int i=0; i<arrSize; i++) {
                f.print(root["month"][i].as<String>());
                f.print('\n');
            }
            f.close();
            historyInit();
        }
        AmisReader.enable();
    } else if((strcmp(command, "/config_general")==0) || (strcmp(command, "/config_wifi")==0) || (strcmp(command, "/config_mqtt")==0)) {
        File f = LittleFS.open(command, "w");
        if(f) {
            //size_t len = root.measurePrettyLength();
            root.prettyPrintTo(f);
            f.close();
            eprintf("[ INFO ] %s stored in the LittleFS (%u bytes)\n", command, len);
            if (strcmp(command, "/config_general")==0) {
                Config.loadConfigGeneral();
                Config.applySettingsConfigGeneral();
            } else if (strcmp(command, "/config_mqtt")==0) {
                Mqtt.reloadConfig();
            }
        }
    } else if(strcmp(command, "status") == 0) {
        sendStatus(client);
    } else if(strcmp(command, "restart") == 0) {
        Reboot.startReboot();
    } else if(strcmp(command, "geteventlog") == 0) {
        logPage = root["page"];
    //Log in main.loop bearbeiten, Timeout!!!
    } else if(strcmp(command, "clearevent") == 0) {
        LittleFS.remove("/eventlog.json");
        writeEvent("WARN", "sys", "Event log cleared!", "");
    } else if(strcmp(command, "scan_wifi") == 0) {
        if (_subscribedClientsWifiScanLen < std::size(_subscribedClientsWifiScan)) {
            using std::placeholders::_1;
            _subscribedClientsWifiScan[_subscribedClientsWifiScanLen++] = clientId;
            if (_subscribedClientsWifiScanLen == 1) {
                WiFi.scanNetworksAsync(std::bind(&WebserverWsDataClass::onWifiScanCompletedCb, this, _1), true);
            }
        }
    } else if(strcmp(command, "getconf") == 0) {
        wsSendFile("/config_general", client);
        wsSendFile("/config_wifi", client);
        wsSendFile("/config_mqtt", client);
    } else if(strcmp(command, "energieWeek") == 0) {
        energieWeekUpdate();         // Wochentagfiles an Webclient senden
    } else if(strcmp(command, "energieMonth") == 0) {
        energieMonthUpdate();         // Monatstabelle an Webclient senden
    } else if(strcmp(command, "weekdata") == 0) {
        sendWeekData(client);                   // die hist_inx + hist_outx Fileinhalte für save konfig
    } else if(strcmp(command, "clearhist") == 0) {
        clearHist();
    } else if(strcmp(command, "clearhist2") == 0) {
        LittleFS.remove("/monate");
    } else if(strcmp(command, "ls") == 0) {
        String path = root["path"].as<String>();
        if (path.isEmpty()) {
            path = "/";
        }
        DynamicJsonBuffer jsonBuffer;
        JsonObject &doc = jsonBuffer.createObject();
        unsigned i=0;
        if (Utils::dirExists(path.c_str())) {
            Dir dir = LittleFS.openDir(path);
            while (dir.next()) {
                File f = dir.openFile("r");
                //eprintf("%s \t %u\n",dir.fileName().c_str(),f.size());
                if (f) {
                    char puffer[86];                // 31+1 + 11+1 + 20+1 + 20  +1
                    snprintf(puffer, sizeof(puffer),  "%-31s %11u %20lld %20lld",
                                    dir.fileName().c_str(),
                                    f.size(),
                                    f.getCreationTime(),
                                    f.getLastWrite());
                    f.close();
                    doc[String(i)] = puffer;
                } else {
                    doc[String(i)] = dir.fileName();
                }
                i++;
            }
        }
        doc["ls"]=i;
        String buffer;
        doc.printTo(buffer);
        //DBGOUT(buffer+"\n");
        ws->text(clientId, buffer);
    } else if(strcmp(command, "rm") == 0) {
        String path = root["path"].as<String>();
        if (path.isEmpty()) {
            return;
        }
        const char *p = path.c_str();
        if (Utils::fileExists(p)) {
            LittleFS.remove(p);
        }  // LittleFS hat in Wirklichkeit kein mkdir() und rmdir()
        /* else if (Utils::dirExists(p)) {
            LittleFS.rmdir(p);
        }*/
    } else if (strcmp(command, "clear") == 0) {
        LittleFS.remove(F("/config_general"));
        LittleFS.remove(F("/config_wifi"));
        LittleFS.remove(F("/config_mqtt"));
        //    LittleFS.remove(F("/.idea"));
    } else if (strcmp(command, "test") == 0) {
        doSerialHwTest = !doSerialHwTest;
    } else if(strcmp(command, "print") == 0) {
        const char *uid = root["file"];
        ws->text(clientId,uid); // ws.text
        int i;
        uint8_t ibuffer[65];
        File f = LittleFS.open(uid, "r");
        if(f) {
            do {
            i=f.read(ibuffer, 64);
            ibuffer[i]=0;
            ws->text(clientId,(char *)ibuffer); // ws.text
            } while (i);
            f.close();
        } else {
            ws->text(clientId, "no file\0");
        }
    } else if(strcmp(command, "print2") == 0) {
        //ws.text(clientId,"prn\0"); // ws.text
        eprintf("prn\n");
        uint8_t ibuffer[10];      //12870008
        File f;
        unsigned i,j;
        for (i=0; i<7; i++) {
            f = LittleFS.open("/hist_in"+String(i), "r");
            if (f) {
                ///val=f.parseInt();    !!!!!!!!!! parseInt() ist buggy, enorme Zeit- und Resourcenverschwendung!!!!!!!!!!!!!!!!!
                j=f.read(ibuffer,8);
                ibuffer[j]=0;
                f.close();
                eprintf("%d %d\n", i, atoi((char*)ibuffer));
        //       ws.text(clientId,ibuffer); // ws.text
            }
            //else ws.text(clientId,"no file\0");
            else {
                eprintf("no file\n");
            }
        }
    } else if (!strcmp(command, "factory-reset-reboot")) {
        // Remove all files (Format), Clear EEprom
        if (!LittleFS.format()) {
            writeEvent("ERROR","littlfs","LittleFS.format() failed!", "");
        }
        EEPROMClear();
        Reboot.startReboot();
    } else if (!strcmp(command, "extract-files")) {
        // Delete all files contained in the image from filesystem and
        // start recreation/extraction from image into filesystem
        FileBlobs.remove(true);
        FileBlobs.checkIsChanged();
/*  } else if (!strcmp(command, "set-developer-mode")) {
        const char *onOff = root[F("value")].as<const char*>();
        if (onOff) {
            Config.developerModeEnabled = (bool)(strcmp(onOff, "on") == 0);
        }
*/
    } else if (!strcmp(command, "set-webserverTryGzipFirst")) {
        const char *onOff = root[F("value")].as<const char*>();
        if (onOff) {
            Config.webserverTryGzipFirst = (bool)(strcmp(onOff, "on") == 0);
            Webserver.setTryGzipFirst(Config.webserverTryGzipFirst);
            ws->text(clientId, "{\"r\":0,\"m\":\"OK\"}");
        }
    } else if (!strcmp(command, "dev-tools-button1")) {

    } else if (!strcmp(command, "dev-tools-button2")) {

    }
}

static void sendWeekData(AsyncWebSocketClient *client)
{
    File f;
    uint8_t ibuffer[12];      //12870008
    unsigned i,j;
    String s;

    s = "{\"weekdata\":[[";
    for (i=0; i<7; i++) {
        f = LittleFS.open("/hist_in"+String(i), "r");
        if (f) {
        j = f.read(ibuffer, 10);
        ibuffer[j] = ',';
        ibuffer[j+1] = 0;
        f.close();
        s += (char*)ibuffer;
        }
        else {
            s += "0,";
        }
    }
    s.remove(s.length()-1, 1);
    s += "],[";
    for (i=0; i<7; i++) {
        f = LittleFS.open("/hist_out"+String(i), "r");
        if (f) {
            j = f.read(ibuffer, 10);
            ibuffer[j] = ',';
            ibuffer[j+1] = 0;
            f.close();
            s += (char*)ibuffer;
        } else {
            s += "0,";
        }
    }
    s.remove(s.length()-1, 1);
    s += "]]}";
    client->text(s);
}

void WebserverWsDataClass::onWifiScanCompletedCb(int nFound)
{

    if(_subscribedClientsWifiScanLen == 0 || nFound == 0) {
        WiFi.scanDelete();
        _subscribedClientsWifiScanLen = 0;
        return;
    }

    DynamicJsonBuffer jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();
    JsonArray &array = jsonBuffer.createArray();
    String buffer;
    for (int i = 0; i < nFound; ++i) {           // esp_event_legacy.h
        buffer = "";
        root[F("ssid")]    = WiFi.SSID(i);
        root[F("rssi")]    = WiFi.RSSI(i);
        root[F("channel")] = WiFi.channel(i);
        root[F("encrpt")]  = String(WiFi.encryptionType(i));     // 0...5
        root.printTo(buffer);
        array.add(buffer);
    }
    buffer = "";
    array.printTo(buffer);
    buffer = "{\"stations\":"+buffer+"}";

    for (size_t i=0; i < _subscribedClientsWifiScanLen; i++) {
        ws->text(_subscribedClientsWifiScan[i], buffer);
    }
    _subscribedClientsWifiScanLen = 0;
    WiFi.scanDelete();
}

static void sendStatus(AsyncWebSocketClient *client)
{
    struct ip_info info;
    FSInfo fsinfo;
    if (!LittleFS.info(fsinfo)) {
        DBGOUT(F("[ WARN ] Error getting info on LittleFS"));
    }

    DynamicJsonBuffer jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();
    root[F("chipid")] = String(ESP.getChipId(), HEX);
    root[F("cpu")] = ESP.getCpuFreqMHz();
    root[F("reset_reason")] = ESP.getResetReason();

    root[F("core")] = ESP.getCoreVersion();
    root[F("sdk")] = ESP.getSdkVersion();
    root[F("version")] = VERSION;
    root[F("app_name")] = APP_NAME;
    root[F("app_compiled_time_utc")] = __COMPILED_DATE_TIME_UTC_STR__;
    root[F("app_compiled_git_branch")] = __COMPILED_GIT_BRANCH__;
    root[F("app_compiled_git_hash")] = __COMPILED_GIT_HASH__;
    root[F("app_compiled_build_environment")] = PIOENV; // PIOENV wird in platform.ini beim Compilieren gesetzt

    root[F("library_ArduinoJson")] = ARDUINOJSON_VERSION;
    root[F("library_AsyncMqttClient")] = "0.9.0";
    root[F("library_AsyncPing_esp8266")] = "95ac7e4ce1d4b41087acc0f7d8109cfd1f553881";
    root[F("library_ESPAsyncTCP")] = "2.0.0";
    root[F("library_ESPAsyncWebServer")] = ASYNCWEBSERVER_VERSION;

#if 0
    root[F("flashchipid")] = String(ESP.getFlashChipId());
    // ESP.getHeapStats()
    root["heap_total"] = ESP.getHeapSize();
    root["heap_used"] = ESP.getHeapSize() - ESP.getFreeHeap();
    root["heap_max_block"] = ESP.getMaxAllocHeap();
    root["heap_min_free"] = ESP.getMinFreeHeap();
    root["psram_total"] = ESP.getPsramSize();
    root["psram_used"] = ESP.getPsramSize() - ESP.getFreePsram();
#endif

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
    root[F("mqttStatus")] = Mqtt.isConnected() ?"connected" :"N/A";
    root[F("ntpSynced")] = "N/A";

    String buffer;
    root.printTo(buffer);
    client->text(buffer);
}

static void wsSendFile(const char *filename, AsyncWebSocketClient *client) {
    DBGOUT("send file: " + String(filename)+'\n');
    File f = LittleFS.open(filename, "r");
    if (f) {
        client->text(f.readString());
        f.close();
    } else {
        eprintf("File %s not found\n",filename);
    }
}

static bool EEPROMClear()
{
    EEPROM.begin(256);
    for (size_t i=0; i<256; i++) {
        if (EEPROM.read(i) != 0) { // Don't write EEPROM every time
            EEPROM.write(i, 0);
        }
    }
    return EEPROM.end();
}


/* vim:set ts=4 et: */
