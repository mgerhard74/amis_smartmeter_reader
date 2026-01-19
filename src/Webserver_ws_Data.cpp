/*
    Handle websocket communication of socket ws://<espiIp>/ws
*/

#include "ProjectConfiguration.h"

#include "Webserver_ws_Data.h"

#include "Application.h"
#include "config.h"
#include "Exception.h"
#include "Log.h"
#define LOGMODULE LOGMODULE_WEBSSOCKET
#include "Mqtt.h"
#include "Network.h"
#include "Reboot.h"
#include "SystemMonitor.h"
#include "Webserver.h"
#include "Utils.h"
#include "__compiled_constants.h"

#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

static void wsSendFile(const char *filename, AsyncWebSocketClient *client);
static void sendStatus(AsyncWebSocketClient *client);
static void sendWeekData(AsyncWebSocketClient *client);
static void wsSendRuntimeConfigAll(AsyncWebSocket *ws);
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
    _wifiScanInProgress = false;

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
    if(type == WS_EVT_ERROR) {
        LOGF_VP("Error: WebSocket[%s][%u] error(%u): %s", server->url(), client->id(), *((uint16_t *) arg), (char *) data);
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
extern void energieWeekUpdate();
extern void energieMonthUpdate();

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
    if (!tempObjectLength) {
        return;
    }
    //LOGF_EP("Websock %d", (int)tempObjectLength);
    DynamicJsonBuffer jsonBuffer;
    JsonObject &root = jsonBuffer.parseObject((char *)(client->_tempObject));
    if (!root.success()) {
        /*
        TODO(anyone): sending
        {"comm

        and":"set-loglevel", "module":10, "level":5}

        raises an exception

        if (root == JsonObject::invalid()) { ... does not help as JsonObject::invalid().sucess() returns false
        */
        LOGF_EP("Parsing failed: message[%u]='%s'",
                            tempObjectLength,
                            Utils::escapeJson((char *)(client->_tempObject), tempObjectLength, 64).c_str());
        return;
    }
    // Web Browser sends some commands, check which command is given
    const char *command = root["command"];

    if(!strcmp(command, "ping")) {
        // handle "ping" explicit here as it's the most used command
        ws->text(client->id(),F("{\"pong\":\"\"}"));
        return;
    }

    // Check whatever the command is and act accordingly
    LOGF_DP("websocket command: '%s'", command);

    if(strcmp(command, "remove") == 0) {
        const char *filename = root["file"];
        if (filename && filename[0]) {
            LittleFS.remove(filename);
        }
    } else if(strcmp(command,"weekfiles")==0) {
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
            root.prettyPrintTo(f);
            f.close();
            LOGF_DP("%s saved on LittleFS.", command);
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
        //Logausgabe in main.loop() bzw Log.loop() bearbeiten, Timeout!!!
        uint32_t page = root["page"].as<uint32_t>();
        if (Log.websocketRequestPage(ws, client->id(), page)) {
            ws->text(client->id(), R"({"r":0,"m":"OK"})");
        } else {
            ws->text(client->id(), R"({"r":1,"m":"Error: No slot available"})");
        }
    } else if(strcmp(command, "clearevent") == 0) {
        Log.clear();
    } else if(strcmp(command, "scan_wifi") == 0) {
        if (!_wifiScanInProgress) {
            _wifiScanInProgress = true;
            using std::placeholders::_1;
            WiFi.scanNetworksAsync(std::bind(&WebserverWsDataClass::onWifiScanCompletedCb, this, _1), true);
        }
    } else if(strcmp(command, "getconf") == 0) {
        wsSendFile("/config_general", client);
        wsSendFile("/config_wifi", client);
        wsSendFile("/config_mqtt", client);
        wsSendRuntimeConfigAll(ws);
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
    } else if (!strcmp(command, "set-amisreader")) {
        // Im AP Modus wird die Config nicht reloaded.
        // Danmit aber zumindest der Key auch im AP Modus upgedatet werden kann,
        // sendet der WebClient einen extra 'set-amisreader'-Request dafür
        const char *key = root[F("key")].as<const char*>();
        if (key) {
            AmisReader.setKey(key);
            ws->text(client->id(), R"({"r":0,"m":"OK"})");
        }
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
                ESP.wdtFeed();
            }
        }
        doc["ls"] = i;
        String buffer;
        doc.printTo(buffer);
        ws->text(client->id(), buffer);
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
        Log.remove(true);
        //    LittleFS.remove(F("/.idea"));
    } else if (strcmp(command, "test") == 0) {
        doSerialHwTest = !doSerialHwTest;
    } else if(strcmp(command, "print") == 0) {
        const char *uid = root["file"];
        ws->text(client->id(), uid); // ws.text
        int i;
        uint8_t ibuffer[65];
        File f = LittleFS.open(uid, "r");
        if(f) {
            do {
            i=f.read(ibuffer, 64);
            ibuffer[i]=0;
            ws->text(client->id(), (char *)ibuffer); // ws.text
            } while (i);
            f.close();
        } else {
            ws->text(client->id(), "no file\0");
        }
    } else if (!strcmp(command, "factory-reset-reboot")) {
        // Remove all files (Format), Clear EEprom
        if (!LittleFS.format()) {
            LOG_EP("LittleFS.format() failed!");
        }
        EEPROMClear();
        Reboot.startReboot();
    } else if (!strcmp(command, "set-runtime-useFilesFromFirmware")) {
        const char *onOff = root[F("value")].as<const char*>();
        if (onOff) {
            ApplicationRuntime.webUseFilesFromFirmware(strcmp(onOff, "on") == 0);
            ws->text(client->id(), R"({"r":0,"m":"OK"})");
        }
    } else if (!strcmp(command, "set-loglevel")) {
        // {"command":"set-loglevel", "module":5, "level":5} // THINGSPEAK
        // {"command":"set-loglevel", "module":6, "level":5} // MQTT
        // {"command":"set-loglevel", "module":11, "level":5} // WATCHDOGPING
        // {"command":"set-loglevel", "module":10, "level":5} // WEBSOCKET
        uint32_t level = CONFIG_LOG_DEFAULT_LEVEL;
        uint32_t module = LOGMODULE_ALL;
        if (root.containsKey(F("level"))) {
            level = root[F("level")].as<uint32_t>();
        }
        if (root.containsKey(F("module"))) {
            module = root[F("module")].as<uint32_t>();
        }
        Log.setLoglevel(level, module);
        ws->text(client->id(), R"({"r":0,"m":"OK"})");
    } else if (!strcmp(command, "dev-remove-webdeveloper-files")) {
        // Remove use self uploaded and old/unused files from filesystem
        const char *filenamesToDelete[] = {
            "amis.css",
            "chart.js",
            "cust.js",
            "index.html",
            "jquery371slim.js",
            "pure-min.css"
        };
        for (size_t i=0; i<std::size(filenamesToDelete); i++) {
            String filename = "/" + String(filenamesToDelete[i]);
            LittleFS.remove(filename);
            filename += ".gz";
            LittleFS.remove(filename);
            filename += ".md5";
            LittleFS.remove(filename);
        }
    } else if (!strcmp(command, "dev-tools-button1")) {
#if (AMIS_DEVELOPER_MODE)
        DynamicJsonBuffer jsonBuffer;
        JsonObject &doc = jsonBuffer.createObject();
        doc["AAAA"]="ABCDEF";
        String buffer;
        String buffer1;
        doc.printTo(buffer);
        doc.printTo(Serial);
        Serial.printf("\r\nlen = %d\r\n", buffer.length());

        doc.prettyPrintTo(buffer);
        doc.prettyPrintTo(Serial);
        Serial.printf("\r\nlen = %d\r\n", buffer.length());

        size_t len, prlen;
        len = root.measureLength();
        Serial.printf("measureLength() = %d\r\n", len);
        prlen = root.measurePrettyLength();
        Serial.printf("measurePrettyLength() = %d\r\n", prlen);

        char x[100];
        memset(&x, '9', sizeof(x));
        doc.printTo(x, 3); // sollte nur 2 Zeichen schreiben und eine '\0'
        x[10] = 0;
        Serial.printf("len(x)= %d\r\n", strlen(x));
        Serial.printf("x= %s\r\n", x);
        Serial.printf("x[2]...x[4] = %d %d %d\r\n", x[2], x[3], x[4]);

        memset(&x, '9', sizeof(x));
        doc.printTo(x,99);
        Serial.printf("len(x)= %d\r\n", strlen(x));
        Serial.printf("x= %s\r\n", x);
        Serial.printf("x[l]...x[l+2] = %d %d %d\r\n", x[strlen(x)], x[strlen(x)+1], x[strlen(x)+2]);

        Serial.printf("heap= %u\r\n", ESP.getFreeHeap());
        doc.printTo(buffer1);
        Serial.printf("len(buffer)= %d\r\n", buffer1.length());
        //Serial.printf("capacity(buffer)= %d\r\n", buffer1.capacity);
        Serial.printf("buffer = %s\r\n", buffer1.c_str());
        Serial.printf("heap= %u\r\n", ESP.getFreeHeap());
#endif
        // TODO(StefanOberhumer): Figure out why  dev-tools-button1 works and dev-tools-button2 crashes
        // Works
        const SystemMonitorClass::statInfo_t freeHeap = SystemMonitor.getFreeHeap();
        String x = String(freeHeap.value) + String(freeHeap.filename) + String(freeHeap.lineno) + String(freeHeap.functionname);
        ws->text(client->id(), x);

    } else if (!strcmp(command, "dev-tools-button2")) {
        // Crashes
        const SystemMonitorClass::statInfo_t freeHeap = SystemMonitor.getFreeHeap();
        //ws->text(client->id(), String(freeHeap.value) + String(freeHeap.filename) + String(freeHeap.lineno) + freeHeap.functionname);
        String x = String(freeHeap.value) + String(freeHeap.filename) + String(freeHeap.lineno) + freeHeap.functionname;
        ws->text(client->id(), x);
    } else if (!strcmp(command, "dev-get-systemmonitor-stat")) {
        const SystemMonitorClass::statInfo_t freeHeap = SystemMonitor.getFreeHeap();
        const SystemMonitorClass::statInfo_t freeStack = SystemMonitor.getFreeStack();
        const SystemMonitorClass::statInfo_t maxFreeBlockSize = SystemMonitor.getMaxFreeBlockSize();
        {
            String x = String(freeHeap.value) + String(freeHeap.filename) + String(freeHeap.lineno) + String(freeHeap.functionname);
            ws->text(client->id(), x);
        }
        {
            String x = String(freeStack.value) + String(freeStack.filename) + String(freeStack.lineno) + String(freeStack.functionname);
            ws->text(client->id(), x);
        }
        {
            String x = String(maxFreeBlockSize.value) + String(maxFreeBlockSize.filename) + String(maxFreeBlockSize.lineno) + String(maxFreeBlockSize.functionname);
            ws->text(client->id(), x);
        }
    } else if (!strcmp(command, "dev-set-reader-serial")) {
        const char *ret_msg = R"({"r":1,"m":"Error"})"; // error
        if (root.containsKey(F("value"))) {
            const uint8_t vv = root[F("value")].as<uint8_t>();
            AmisReader.end();
            AmisReader.init(vv);
            if (vv != 1) {
                // Wenn die erste serielle nicht für den Reader verwendet wird, nehmen wir diese fürs Loggen
                Serial.begin(115200, SERIAL_8N1);
            }
            ret_msg = R"({"r":0,"m":"OK"})";
        }
        ws->text(client->id(), ret_msg);
    } else if (!strcmp(command, "dev-raise-exception")) {
        const uint32_t no = root[F("value")].as<unsigned>();
        Exception_Raise(no);
    } else if (!strcmp(command, "dev-remove-exceptiondumpsall")) {
        Exception_RemoveAllDumps();
    } else if (!strcmp(command, "dev-getHostByName")) {
        const char* value = root[F("value")].as<const char*>();
        if (value && value[0]) {
            IPAddress ipAddr;
            uint32_t timeout = 10000;
            if (root.containsKey(F("timeout"))) {
                timeout = root[F("timeout")].as<uint32_t>();
            }
            int r;
            r = WiFi.hostByName(value, ipAddr, timeout);
            char ret_msg[128];
            snprintf(ret_msg, sizeof(ret_msg), R"({"r":%d,"v":"%s"})", r, ipAddr.toString().c_str());
            ws->text(client->id(), ret_msg);
        }
    }
    SYSTEMMONITOR_STAT();
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

    if(!_wifiScanInProgress || nFound == 0) {
        WiFi.scanDelete();
        _wifiScanInProgress = false;
        return;
    }

    DynamicJsonBuffer jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();
    JsonArray &array = jsonBuffer.createArray();
    String buffer;
    for (int i = 0; i < nFound; ++i) {           // esp_event_legacy.h
        root[F("ssid")]    = WiFi.SSID(i);
        root[F("rssi")]    = WiFi.RSSI(i);
        root[F("channel")] = WiFi.channel(i);
        root[F("encrpt")]  = String(WiFi.encryptionType(i));     // 0...5
        root[F("bssid")]  = WiFi.BSSIDstr(i);
        buffer = "";
        root.printTo(buffer);
        array.add(buffer);
    }
    WiFi.scanDelete();
    _wifiScanInProgress = false;

    buffer = "";
    array.printTo(buffer);
    jsonBuffer.clear();
    buffer = "{\"stations\":" + buffer + "}";

    // If we have a new Wifi-Scan-Result: Publish to *ALL* clients
    ws->textAll(buffer);
    SYSTEMMONITOR_STAT();
}

static void sendStatus(AsyncWebSocketClient *client)
{
    // TODO(anyone): This creates a "Reset" if running with debug-build.
    struct ip_info info;
    FSInfo fsinfo;
    if (!LittleFS.info(fsinfo)) {
        LOGF_EP("Error getting info on LittleFS");
        memset(&fsinfo, 0, sizeof(fsinfo));
    }

    DynamicJsonBuffer jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();
    root[F("chipid")] = String(ESP.getChipId(), HEX);
    root[F("cpu")] = ESP.getCpuFreqMHz();
    root[F("reset_reason")] = ESP.getResetReason();

    root[F("core")] = ESP.getCoreVersion();
    root[F("sdk")] = ESP.getSdkVersion();
    root[F("version")] = APP_VERSION_STR;
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
        memset(&conf, 0, sizeof(conf));
        wifi_softap_get_config(&conf);

        root[F("status_wifi_mac")] = WiFi.softAPmacAddress();
    } else {
        wifi_get_ip_info(STATION_IF, &info);

        struct station_config conf;
        memset(&conf, 0, sizeof(conf));
        wifi_station_get_config(&conf);

        char ssid_str[sizeof(conf.ssid)+1];
        memcpy(ssid_str, conf.ssid, sizeof(conf.ssid));
        ssid_str[sizeof(ssid_str) - 1] = 0;

        root[F("status_wifi_ssid")] = ssid_str;
        root[F("status_wifi_dns")] = WiFi.dnsIP().toString();
        root[F("status_wifi_mac")] = WiFi.macAddress();
        root[F("status_wifi_channel")] = WiFi.channel();
        root[F("status_wifi_rssi")] = WiFi.RSSI();
        root[F("status_wifi_bssid")] = WiFi.BSSIDstr();
    }
    IPAddress ipaddr = IPAddress(info.ip.addr);
    IPAddress gwaddr = IPAddress(info.gw.addr);
    IPAddress nmaddr = IPAddress(info.netmask.addr);
    root[F("status_wifi_ip")] = ipaddr.toString();
    root[F("status_wifi_gateway")] = gwaddr.toString();
    root[F("status_wifi_netmask")] = nmaddr.toString();
    //root["loadaverage"] = systemLoadAverage();
    //if (ADC_MODE_VALUE == ADC_VCC) {
    root[F("vcc")] = ESP.getVcc();
    //root["vcc"] = "N/A (TOUT) ";
    root[F("mqttStatus")] = Mqtt.isConnected() ?"connected" :"N/A";
    root[F("ntpSynced")] = "N/A";

    String buffer;
    root.printTo(buffer);
    jsonBuffer.clear();
    client->text(buffer);
    SYSTEMMONITOR_STAT();
}

static void wsSendFile(const char *filename, AsyncWebSocketClient *client) {
    LOGF_DP("Sending file '%s' to client %u", filename, client->id());
    File f = LittleFS.open(filename, "r");
    if (f) {
        client->text(f.readString());
        f.close();
    } else {
        LOGF_EP("File '%s' not found", filename);
    }
}

static void wsSendRuntimeConfigAll(AsyncWebSocket *ws)
{
    // send the runtime-config to all clients
    DynamicJsonBuffer jsonBuffer;
    JsonObject &doc = jsonBuffer.createObject();
    doc[F("command")] = F("config_runtime");
    doc[F("webUseFilesFromFirmware")] = ApplicationRuntime.webUseFilesFromFirmware();
    String buffer;
    doc.printTo(buffer);
    jsonBuffer.clear();
    ws->textAll(buffer);
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
