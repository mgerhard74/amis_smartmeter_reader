/*
    Handle websocket communication of socket ws://<espiIp>/ws
*/

#include "ProjectConfiguration.h"

#include "Webserver_ws_Data.h"

#include "Application.h"
#include "config.h"
#include "Exception.h"
#include "Json.h"
#include "Log.h"
#define LOGMODULE LOGMODULE_WEBSSOCKET
#include "Mqtt.h"
#include "Network.h"
#include "Reboot.h"
#include "SystemMonitor.h"
#include "Webserver.h"
#include "Utils.h"
#include "__compiled_constants.h"

#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

#include "Webserver_ws_Data_cmd_status.inl"

static void wsSendFile(const char *filename, AsyncWebSocketClient *client);
static void sendWeekData(AsyncWebSocketClient *client);
static void wsSendRuntimeConfigAll(AsyncWebSocket *ws);
static void clearHist();
static bool EEPROMClear();

extern bool doSerialHwTest;

extern "C" {
    extern uint32_t _iram_start;
    extern uint32_t _iram_end;              // sample:
    extern uint32_t _text_start;            //  &_text_start        = 0x40100000
    extern uint32_t _text_end;              //  &_text_end          = 0x40107409
    extern uint32_t _irom0_text_start;      //  &_irom0_text_start  = 0x40201010
    extern uint32_t _irom0_text_end;        //  &_irom0_text_end    = 0x402871a0
/*
    _dport0_rodata_start = ABSOLUTE(.);
    _dport0_rodata_end = ABSOLUTE(.);
    _dport0_literal_start = ABSOLUTE(.);
    _dport0_literal_end = ABSOLUTE(.);
    _dport0_data_start = ABSOLUTE(.);
    _dport0_data_end = ABSOLUTE(.);
    _flash_code_end // müsste gleich _irom0_text_end sein
    _lit4_start
    _lit4_end
*/
}

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
    _sendDataTicker.attach_ms_scheduled(10, std::bind(&WebserverWsDataClass::sendDataTaskCb, this));

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
    _simpleDigestAuth.setUsername(Config.auth_user);
    _simpleDigestAuth.setPassword(Config.auth_passwd);
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
    // 1.) Do some periodic sending stuff here
    //
    // till now: nothing periodic to do


    // 2.) Handle our _clientRequests queue
    _clientRequest_t request;

    // max waste 70ms request per call of this function
    uint32_t start = millis();
    do {
        if (!_clientRequests.pop(request)) {
            return;
        }
        if (request.requestData) {
            AsyncWebSocketClient *client = _ws.client(request.clientId);
            wsClientRequest(client, request.requestData, request.requestLen);
            free(request.requestData);
        }
    } while (millis() - start < 70);
}


void WebserverWsDataClass::onWebsocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len)
{
    if (type == WS_EVT_ERROR) {
        LOGF_VP("Error: WebSocket[%s][%u] error(%u): %s", server->url(), client->id(), *((uint16_t *) arg), (char *) data);
        return;
    }

    if (type == WS_EVT_DATA) {
        AwsFrameInfo *info = (AwsFrameInfo *)arg;
        uint64_t index = info->index;
        uint64_t infolen = info->len;

        if (!infolen) {
            return;
        }

        if (info->final && info->index == 0 && infolen == len) {
            //the whole message is in a single frame and we got all of it's data
            _clientRequest_t request;
            request.requestData = (char *) malloc(infolen + 1);
            if (request.requestData && infolen) {
                request.clientId = client->id();
                memcpy(request.requestData, data, len);
                request.requestData[len] = 0;
                request.requestLen = infolen;
                if (!_clientRequests.push(request)) {
                    LOG_EP("Request queue full!");
                }
            }
            SYSTEMMONITOR_STAT();
            return;
        }

        //message is comprised of multiple frames or the frame is split into multiple packets
        if (index == 0) {
            if (info->num == 0 && client->_tempObject == nullptr) {
                client->_tempObject = malloc(infolen + 1);
            } else {
                client->_tempObject = nullptr;
            }
        }

        if (client->_tempObject) {
            memcpy((uint8_t *)(client->_tempObject) + index, data, len);

            if (info->final) {
                _clientRequest_t request;
                request.requestData = (char *) client->_tempObject;
                request.requestData[infolen] = 0;
                client->_tempObject = nullptr;
                request.clientId = client->id();
                request.requestLen = infolen;
                if (!_clientRequests.push(request)) {
                    LOG_EP("Request queue full!");
                }
            }
        }
        SYSTEMMONITOR_STAT();
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
    for (unsigned j=0; j<7;j++ ) {
        kwh_day_in[j] = 0;
        kwh_day_out[j] = 0;
        kwh_hist[j].kwh_in = 0;
        kwh_hist[j].kwh_out = 0;
        kwh_hist[j].dow = 0;
    }

    char fname[11]; //  "/hist_in6\0" "/hist_out6\0"

    strlcpy(fname, "/hist_in6", sizeof(fname));
    for (unsigned j=0; j<7;j++ ) {
        fname[8] = '0' + j;
        LittleFS.remove(fname);
    }

    strlcpy(fname, "/hist_out6", sizeof(fname));
    for (unsigned j=0; j<7;j++ ) {
        fname[9] = '0' + j;
        LittleFS.remove(fname);
    }

    first_frame = 0;
}

void WebserverWsDataClass::wsClientRequest(AsyncWebSocketClient *client, char* requestData, size_t requestLength) {
    // *client can be nullptr as request is processed async and client can be already disconnected!

    /*
    if (client) {
        LOGF_VP("wsClientRequest() client=" PRsIP ":%d, len=%u", PRIPVal(client->remoteIP()), client->remotePort(), requestLength);
    }
    LOGF_VP("wsClientRequest() strlen= %d", strlen(requestData));
    */

    /*
    {"command":"ping"}
    {"command":"getconf"}
    {"command":"energieWeek"}
    {"command":"energieMonth","jahr":22}
    {"command":"geteventlog","page":1}
    {"command":"status"}
    {"command":"scan_wifi"}
    {  // 16 Objects
        "command": "/config_wifi",
        "ssid": "xxxxx-ssid",
        "wifipassword": "password",
        "dhcp": false,
        "ip_static": "192.168.xx.yy",
        "ip_netmask": "255.255.255.0",
        "ip_gateway": "192.168.xx.aa",
        "ip_nameserver": "192.168.11.1",
        "rfpower": "21",
        "mdns": true,
        "allow_sleep_mode": false,
        "pingrestart_do": false,
        "pingrestart_ip": "192.168.xx.cc",
        "pingrestart_interval": "60",
        "pingrestart_max": "5",
        "channel": "0"
    }
    { // 32 Objects
        "command": "/config_general",
        "devicetype": "AMIS-Reader",
        "devicename": "Amis-1",
        "auth_passwd": "password",
        "auth_user": "user",
        "use_auth": true,
        "log_sys": true,
        "amis_key": "1234567890abcdef1234567890abcdef",
        "thingspeak_aktiv": true,
        "channel_id": "1234567",
        "write_api_key": "writePASSWORD",
        "read_api_key": "readPASSWORD",
        "thingspeak_iv": "30",
        "channel_id2": "",
        "read_api_key2": "",
        "rest_var": "0",
        "rest_ofs": "0",
        "rest_neg": false,
        "smart_mtr": true,
        "developerModeEnabled": true,
        "switch_on": "0",
        "switch_off": "0",
        "switch_url_on": "http://url.on",
        "switch_url_off": "http://url.off",
        "switch_intervall": "60",
        "reboot0": true,
        "shelly_smart_mtr_udp": false,
        "shelly_smart_mtr_udp_device": "shellypro3em",
        "shelly_smart_mtr_udp_offset": "0",
        "shelly_smart_mtr_udp_hardwareID": "",
        "shelly_smart_mtr_udp_device_index": "0",
        "shelly_smart_mtr_udp_hardware_id_appendix": ""
    }
    {   // 13 Objects
        "command": "/config_mqtt",
        "mqtt_enabled": true,
        "mqtt_broker": "192.168.aa.dd",
        "mqtt_port": "1883",
        "mqtt_user": "",
        "mqtt_password": "",
        "mqtt_clientid": "mqclientID1",
        "mqtt_qos": "0",
        "mqtt_retain": true,
        "mqtt_keep": "8",
        "mqtt_pub": "amis/out/",
        "mqtt_will": "lastwill1",
        "mqtt_ha_discovery": true
    }
    */

    // add space for 5 "additional" objects and some extra bytes
    DynamicJsonDocument root(JSON_OBJECT_SIZE(32) + JSON_OBJECT_SIZE(5) + 32); // ~624 bytes

    // Do not change 'requestData' ... but then we need more memory!
    // error = deserializeJson(root, (const char*) requestData, requestLength);

    // Following line changes 'requestData'! But thats ok for us here since we've allocated the buffer.
    DeserializationError error = deserializeJson(root, requestData, requestLength);

    if (error) {
        /*
        TODO(anyone): sending
        >>>
        {"comm

        and":"set-loglevel", "module":10, "level":5}
        <<<

        raises an exception
        */
        LOGF_EP("Parsing failed: message[%u]='%s' jsonError='%s'",
                            requestLength,
                            Utils::escapeJson(requestData, requestLength, 64).c_str(),
                            error.c_str());
        /*
        File f_request = LittleFS.open("/jsonRequest.json", "w");
        if (f_request) {
            f_request.write((uint8_t*)requestData, requestLength);
            f_request.close();
        }
        */
        return;
    }

    // Web Browser sends some commands, check which command is given
    const char *command = root["command"];
    if (command == nullptr) {
        return;
    }

    if (!strcmp(command, "ping")) {
        // handle "ping" explicit here as it's the most used command
        if (client) {
            ws->text(client->id(), F("{\"pong\":\"\"}"));
        }
        return;
    }

    // Check whatever the command is and act accordingly
    LOGF_DP("websocket command: '%s'", command);

    if (strcmp(command, "remove") == 0) {
        const char *filename = root["file"];
        if (filename && filename[0]) {
            LittleFS.remove(filename);
        }
    } else if (strcmp(command,"weekfiles")==0) {
        uint32_t zstand;
        AmisReader.disable();
        clearHist();
        for (unsigned i=0; i<7; i++){
            zstand = root["week"][0][i].as<uint32_t>();
            if (zstand) {
                File f = LittleFS.open("/hist_in"+String(i), "w");
                if (f) {
                    f.print(zstand);
                    f.close();
                }
            }
            zstand = root["week"][1][i].as<uint32_t>();
            if (zstand) {
                File f = LittleFS.open("/hist_out"+String(i), "w");
                if (f) {
                    f.print(zstand);
                    f.close();
                }
            }
        }
        historyInit();
        AmisReader.enable();
    } else if (strcmp(command,"monthlist")==0) {
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
    } else if ((strcmp(command, "/config_general")==0) || (strcmp(command, "/config_wifi")==0) || (strcmp(command, "/config_mqtt")==0)) {
        File f = LittleFS.open(command, "w");
        if (f) {
            serializeJsonPretty(root, f);
            //f.write((const uint8_t *)client->_tempObject, tempObjectLength);
            f.close();
            LOGF_DP("%s saved on LittleFS.", command);
            if (strcmp(command, "/config_general")==0) {
                Config.loadConfigGeneral();
                Config.applySettingsConfigGeneral();
            } else if (strcmp(command, "/config_mqtt")==0) {
                Mqtt.reloadConfig();
            }
        }
    } else if (strcmp(command, "status") == 0) {
        if (client) {
            sendStatus(client);
        }
    } else if (strcmp(command, "restart") == 0) {
        Reboot.startReboot();
    } else if (strcmp(command, "geteventlog") == 0) {
        //Logausgabe in main.loop() bzw Log.loop() bearbeiten, Timeout!!!
        if (!client) {
            return;
        }
        uint32_t page = root["page"].as<uint32_t>();
        if (Log.websocketRequestPage(ws, client->id(), page)) {
            ws->text(client->id(), R"({"r":0,"m":"OK"})");
        } else {
            ws->text(client->id(), R"({"r":1,"m":"Error: No slot available"})");
        }
    } else if (strcmp(command, "clearevent") == 0) {
        Log.clear();
    } else if (strcmp(command, "scan_wifi") == 0) {
        if (!_wifiScanInProgress) {
            _wifiScanInProgress = true;
            using std::placeholders::_1;
            WiFi.scanNetworksAsync(std::bind(&WebserverWsDataClass::onWifiScanCompletedCb, this, _1), true);
        }
    } else if (strcmp(command, "getconf") == 0) {
        if (client) {
            wsSendFile("/config_general", client);
            wsSendFile("/config_wifi", client);
            wsSendFile("/config_mqtt", client);
        }
        wsSendRuntimeConfigAll(ws);
    } else if (strcmp(command, "energieWeek") == 0) {
        energieWeekUpdate();        // Wochentagfiles an Webclient senden
    } else if (strcmp(command, "energieMonth") == 0) {
        energieMonthUpdate();       // Monatstabelle an Webclient senden
    } else if (strcmp(command, "weekdata") == 0) {
        if (client) {
            sendWeekData(client); // die hist_inx + hist_outx Fileinhalte für save konfig
        }
    } else if (strcmp(command, "clearhist") == 0) {
        clearHist();
    } else if (strcmp(command, "clearhist2") == 0) {
        LittleFS.remove("/monate");
    } else if (!strcmp(command, "set-amisreader")) {
        // Im AP Modus wird die Config nicht reloaded.
        // Danmit aber zumindest der Key auch im AP Modus upgedatet werden kann,
        // sendet der WebClient einen extra 'set-amisreader'-Request dafür
        const char *key = root[F("key")];
        if (key) {
            AmisReader.setKey(key);
        }
    } else if (strcmp(command, "ls") == 0) {
        if (!client) {
            return;
        }
        String path;
        if (root.containsKey(F("path"))) {
            path = root[F("path")].as<String>();
        }
        if (path.isEmpty()) {
            path = "/";
        }

        unsigned fileCnt=0;
        if (Utils::dirExists(path.c_str())) {
            Dir dir = LittleFS.openDir(path);

            while (dir.next()) {
                fileCnt++;
                ESP.wdtFeed();
            }
        }
        DynamicJsonDocument doc(JSON_OBJECT_SIZE(1)+5+path.length()+1 +                 // "path":path
                                JSON_OBJECT_SIZE(1)+3 +                                 // "ls":fileCnt
                                JSON_OBJECT_SIZE(fileCnt)+(3*fileCnt)+(86*fileCnt) +    // "1": "fileinfo1"
                                32);                                                    // silence our low-memory check!
        if (!doc.capacity()) {
            return; // Out of memory
        }
        if (fileCnt) {
            fileCnt = 0;
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
                    doc[String(fileCnt)] = puffer;
                } else {
                    doc[String(fileCnt)] = dir.fileName();
                }
                fileCnt++;
                ESP.wdtFeed();
            }
        }
        doc["ls"] = fileCnt;
        doc["path"] = path;
        String buffer;
        SERIALIZE_JSON_LOG(doc, buffer);
        ws->text(client->id(), buffer);
    } else if (strcmp(command, "rm") == 0) {
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
    } else if (strcmp(command, "print") == 0) {
        if (!client) {
            return;
        }
        const char *uid = root["file"];
        ws->text(client->id(), uid); // ws.text
        uint8_t ibuffer[65];
        File f = LittleFS.open(uid, "r");
        if (f) {
            int i;
            do {
                i = f.read(ibuffer, 64);
                ibuffer[i] = 0;
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
            if (client) {
                ws->text(client->id(), R"({"r":0,"m":"OK"})");
            }
        }
    } else if (!strcmp(command, "set-loglevel")) {
        // {"command":"set-loglevel", "module":5, "level":5} // THINGSPEAK
        // {"command":"set-loglevel", "module":6, "level":5} // MQTT
        // {"command":"set-loglevel", "module":11, "level":5} // WATCHDOGPING
        // {"command":"set-loglevel", "module":10, "level":5} // WEBSOCKET
        // {"command":"set-loglevel", "level":5} // ALL
        // {"command":"set-loglevel", "module":2, "level":0} // READER
        uint32_t level = CONFIG_LOG_DEFAULT_LEVEL;
        uint32_t module = LOGMODULE_ALL;
        if (root.containsKey(F("level"))) {
            level = root[F("level")].as<uint32_t>();
        }
        if (root.containsKey(F("module"))) {
            module = root[F("module")].as<uint32_t>();
        }
        Log.setLoglevel(level, module);
        if (client) {
            ws->text(client->id(), R"({"r":0,"m":"OK"})");
        }
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
        if (!client) {
            return;
        }
#if (AMIS_DEVELOPER_MODE)
        DynamicJsonBuffer jsonBuffer;
        JsonObject doc = jsonBuffer.createObject();
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
        String info = String(freeHeap.value) + String(freeHeap.filename) + String(freeHeap.lineno) + String(freeHeap.functionname);
        ws->text(client->id(), info);

    } else if (!strcmp(command, "dev-tools-button2")) {
        // Crashes
        const SystemMonitorClass::statInfo_t freeHeap = SystemMonitor.getFreeHeap();
        //ws->text(client->id(), String(freeHeap.value) + String(freeHeap.filename) + String(freeHeap.lineno) + freeHeap.functionname);
        LOGF_IP("TEXT %p %p", &_text_start, &_text_end);
        LOGF_IP("IROM %p %p", &_irom0_text_start, &_irom0_text_end);
        //LOGF_IP("TEXT 0x%08x 0x%08x", _text_start, _text_end);
        //LOGF_IP("IROM 0x%08x 0x%08x", _irom0_text_start, _irom0_text_end);
        //LOGF_IP("IRAM 0x%08x 0x%08x", _iram_start, _iram_end);
        LOGF_IP("Filename=%p Funcname=%p", freeHeap.filename, freeHeap.functionname);
        //LOGF_IP("Func=%s ", freeHeap.functionname);
        //LOGF_IP("Filename=%s ", freeHeap.filename);
        LOGF_IP("Func=%S ", freeHeap.functionname);
        LOGF_IP("Filename=%S ", freeHeap.filename);

        //String x = String(freeHeap.value) + String(freeHeap.filename) + String(freeHeap.lineno) + String(freeHeap.functionname);
        //ws->text(client->id(), x);
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
        if (client) {
            ws->text(client->id(), ret_msg);
        }
    } else if (!strcmp(command, "dev-raise-exception")) {
        const uint32_t no = root[F("value")].as<unsigned>();
        Exception_Raise(no);
    } else if (!strcmp(command, "dev-remove-exceptiondumpsall")) {
        Exception_RemoveAllDumps();
    } else if (!strcmp(command, "dev-getHostByName")) {
        if (!client) {
            return;
        }
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
    uint8_t ibuffer[12];      // "4294967295,\0"
    unsigned i,j;
    String s;

    if (!s.reserve(300)) { // 300 should be maximum ever needed
        return;
    }

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

    if (!_wifiScanInProgress) {
        WiFi.scanDelete();
        _wifiScanInProgress = false;
        return;
    }

    /*
    {
        "stations": [
            "{\"ssid\":\"meine-ssid1234567890123456789012\",\"rssi\":-2147483647,\"channel\":-2147483647,\"encrpt\":\"255\",\"bssid\":\"aa:bb:cc:dd:ee:ff\"}",
            "{\"ssid\":\"meiNE-ssid1234567890123456789012\",\"rssi\":-2147483647,\"channel\":-2147483647,\"encrpt\":\"255\",\"bssid\":\"11:bb:cc:dd:ee:ff\"}",
            "{\"ssid\":\"meine-ssID1234567890123456789012\",\"rssi\":-2147483647,\"channel\":-2147483647,\"encrpt\":\"255\",\"bssid\":\"aa:22:cc:dd:ee:ff\"}"
        ]
    }
    */
    DynamicJsonDocument root(JSON_OBJECT_SIZE(1)+10 + JSON_ARRAY_SIZE(nFound)+(148*nFound));
    if (root.capacity() == 0) {
        // out of memory
        WiFi.scanDelete();
        _wifiScanInProgress = false;
        return ;
    }

    JsonArray stations = root.createNestedArray("stations");
    for (int i = 0; i < nFound; ++i) {           // esp_event_legacy.h
        StaticJsonDocument<JSON_OBJECT_SIZE(5) + 148> singleStation;
        String singleStationStr = "";

        singleStation[F("ssid")]    = WiFi.SSID(i);
        singleStation[F("rssi")]    = WiFi.RSSI(i);
        singleStation[F("channel")] = WiFi.channel(i);
        singleStation[F("encrpt")]  = String(WiFi.encryptionType(i));
        singleStation[F("bssid")]  = WiFi.BSSIDstr(i);

        SERIALIZE_JSON_LOG(singleStation, singleStationStr);                 // als string
        stations.add(singleStationStr);
    }
    WiFi.scanDelete();
    _wifiScanInProgress = false;

    // If we have a new Wifi-Scan-Result: Publish to *ALL* clients
    String buffer;
    SERIALIZE_JSON_LOG(root, buffer);
    ws->textAll(buffer);
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
    /*
    {
        "command": "config_runtime",
        "webUseFilesFromFirmware": true
    }
    */
    StaticJsonDocument<JSON_OBJECT_SIZE(2)+96> doc;
    doc[F("command")] = F("config_runtime");
    doc[F("webUseFilesFromFirmware")] = ApplicationRuntime.webUseFilesFromFirmware();
    String buffer;
    SERIALIZE_JSON_LOG(doc, buffer);
    ws->textAll(buffer);
}

static bool EEPROMClear()
{
    EEPROM.begin(256);
    const char empty = 0xff;
    for (int i=0; i<256; i++) {
        EEPROM.put(i, empty);
    }
    return EEPROM.end();
}


/* vim:set ts=4 et: */
