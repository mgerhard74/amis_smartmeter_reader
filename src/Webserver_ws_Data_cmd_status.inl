/*
    Handle websocket request: {"command":"status"}
*/



static String getStatusJsonInfoHardware()
{
    DynamicJsonDocument root(768);
    if (root.capacity() == 0) {
        // out of memory
        return "";
    }
#if 0
    root[F("flashchipid")] = String(ESP.getFlashChipId());
#endif
    root[F("chipid")] = String(ESP.getChipId(), HEX);
    root[F("cpu")] = ESP.getCpuFreqMHz();
    root[F("reset_reason")] = ESP.getResetReason();
    root[F("flashspeed")] = ESP.getFlashChipSpeed();
    root[F("flashsize")] = ESP.getFlashChipSize();
    root[F("flashmode")] = static_cast<uint8_t>(ESP.getFlashChipMode());
    //root["loadaverage"] = systemLoadAverage();
    //if (ADC_MODE_VALUE == ADC_VCC) {
    root[F("vcc")] = ESP.getVcc();

    String buffer;
    SERIALIZE_JSON_LOG(root, buffer);
    SYSTEMMONITOR_STAT();
    return buffer;
}

extern "C" uint32_t __crc_val;
static String getStatusJsonInfoApp()
{
/*
{
  "version": "1.5.9-dev2",
  "app_name": "Amis",
  "app_compiled_time_utc": "2026/02/17 03:10:59",
  "app_compiled_git_branch": "v1.5.9-dev2",
  "app_compiled_git_hash": "g2261baa",
  "app_compiled_build_environment": "esp12e",
  "firmware_crc32": "72de9595",
  "sketchsize": 598880,
  "freesize": 1495040
}
*/
    DynamicJsonDocument doc(384);
    if (doc.capacity() == 0) {
        // out of memory
        return "";
    }
    doc[F("version")] = APP_VERSION_STR;
    doc[F("app_name")] = APP_NAME;
    doc[F("app_compiled_time_utc")] = __COMPILED_DATE_TIME_UTC_STR__;
    doc[F("app_compiled_git_branch")] = __COMPILED_GIT_BRANCH__;
    doc[F("app_compiled_git_hash")] = __COMPILED_GIT_HASH__;
    doc[F("app_compiled_build_environment")] = PIOENV; // PIOENV wird in platform.ini beim Compilieren gesetzt
    doc[F("firmware_crc32")] = String(__crc_val, HEX);
    doc[F("sketchsize")] = ESP.getSketchSize();
    doc[F("freesize")] = ESP.getFreeSketchSpace();

    String buffer;
    SERIALIZE_JSON_LOG(doc, buffer);
    SYSTEMMONITOR_STAT();
    return buffer;
}


static String getStatusJsonInfoLibrary()
{
/*
{
  "core": "3.1.2",
  "sdk": "2.2.2-dev(38a443e)",
  "library_ArduinoJson": "6.21.5",
  "library_AsyncMqttClient": "3d93fc7f662e65366f8e2b0d88b108f874f035b9",
  "library_AsyncPing_esp8266": "95ac7e4ce1d4b41087acc0f7d8109cfd1f553881",
  "library_ESPAsyncUDP": "0.0.0-alpha+sha.697c75a025",
  "library_ESPAsyncTCP": "2.0.0",
  "library_ESPAsyncWebServer": "3.6.0"
}
*/
    DynamicJsonDocument doc(384);
    if (doc.capacity() == 0) {
        // out of memory
        return "";
    }
    doc[F("core")] = ESP.getCoreVersion();
    doc[F("sdk")] = ESP.getSdkVersion();
    doc[F("library_ArduinoJson")] = ARDUINOJSON_VERSION;
    doc[F("library_AsyncMqttClient")] = "3d93fc7f662e65366f8e2b0d88b108f874f035b9";
    doc[F("library_AsyncPing_esp8266")] = "95ac7e4ce1d4b41087acc0f7d8109cfd1f553881";
    doc[F("library_ESPAsyncUDP")] = "0.0.0-alpha+sha.697c75a025";
    doc[F("library_ESPAsyncTCP")] = "2.0.0";
    doc[F("library_ESPAsyncWebServer")] = ASYNCWEBSERVER_VERSION;

    String buffer;
    SERIALIZE_JSON_LOG(doc, buffer);
    SYSTEMMONITOR_STAT();
    return buffer;
}

static String getStatusJsonInfoMemory()
{
/*
{
  "max_free_blocksz": 23640,
  "heap_free": 24464,
  "heap_fragment": 4,
  "heap":         { "free": 19712, "line": 222, "file": "main.cpp", "func": "loop" },
  "minfreeblock": { "free": 11792, "line": 222, "file": "main.cpp", "func": "loop" },
  "stack0":       { "free": 4294961489, "line": 807, "file": "Webserver_ws_Data.cpp", "func": "getStatusJsonInfoHardware" },
  "stack1":       { "free": 3728,       "line": 101, "file": "main.cpp",              "func": "setup" },
  "stack2":       { "free": 4294967295, "line": 0,   "file": "",                      "func": "" }
}
*/
    DynamicJsonDocument doc(896);
    if (doc.capacity() == 0) {
        // out of memory
        return "";
    }
    doc[F("max_free_blocksz")] = ESP.getMaxFreeBlockSize();
    doc[F("heap_free")] = ESP.getFreeHeap();
    doc[F("heap_fragment")] = ESP.getHeapFragmentation();

#if 0
    // ESP.getHeapStats()
    doc[F("heap_total")] = ESP.getHeapSize();
    doc[F("heap_used")] = ESP.getHeapSize() - ESP.getFreeHeap();
    //  doc[F(F("heap_alloc_max"))] = ESP.getMaxAllocHeap();
    doc[F("heap_max_block")] = ESP.getMaxAllocHeap();
    doc[F("heap_min_free")] = ESP.getMinFreeHeap();
    doc[F("psram_total")] = ESP.getPsramSize();
    doc[F("psram_used")] = ESP.getPsramSize() - ESP.getFreePsram();
#endif
    const SystemMonitorClass::statInfo_t freeHeap = SystemMonitor.getFreeHeap();
    JsonObject heap = doc.createNestedObject(F("heap"));
    heap[F("value")] = freeHeap.value;
    heap[F("line")] = freeHeap.lineno;
    heap[F("file")] = String(freeHeap.filename);
    heap[F("func")] = String(freeHeap.functionname);

    const SystemMonitorClass::statInfo_t maxFreeBlockSize = SystemMonitor.getMaxFreeBlockSize();
    JsonObject maxfreeblock = doc.createNestedObject(F("maxfreeblock"));
    maxfreeblock[F("value")] = maxFreeBlockSize.value;
    maxfreeblock[F("line")] = maxFreeBlockSize.lineno;
    maxfreeblock[F("file")] = String(maxFreeBlockSize.filename);
    maxfreeblock[F("func")] = String(maxFreeBlockSize.functionname);

    for (int context = 0; context < 3; context++) {
        String stackctx = "stack" + String(context);
        const SystemMonitorClass::statInfo_t freeStack = SystemMonitor.getFreeStack(context);
        JsonObject stackContext = doc.createNestedObject(stackctx);
        stackContext[F("value")] = freeStack.value;
        stackContext[F("line")] = freeStack.lineno;
        stackContext[F("file")] = String(freeStack.filename);
        stackContext[F("func")] = String(freeStack.functionname);
    }
    String buffer;
    SERIALIZE_JSON_LOG(doc, buffer);
    SYSTEMMONITOR_STAT();
    return buffer;
}

static String getStatusJsonInfoFilesystem()
{
/*
{
  "littlefs_size": 2072576,
  "littlefs_used": 188416
}
*/
    DynamicJsonDocument doc(128);
    if (doc.capacity() == 0) {
        // out of memory
        return "";
    }

    FSInfo fsinfo;
    if (!LittleFS.info(fsinfo)) {
        LOGF_EP("Error getting info on LittleFS");
        fsinfo.totalBytes = 0;
        fsinfo.usedBytes = 0xffffffff;
    }

    doc[F("littlefs_size")] = fsinfo.totalBytes;
    doc[F("littlefs_used")] = fsinfo.usedBytes;

    String buffer;
    SERIALIZE_JSON_LOG(doc, buffer);
    SYSTEMMONITOR_STAT();
    return buffer;
}

static String getStatusJsonInfoNetwork()
{
/*
{
  "status_wifi_ap_mode": false,
  "status_wifi_ssid": "xxyyzzssid01234567890123456789012",
  "status_wifi_dns": "192.168.aa.bb",
  "status_wifi_mac": "11:22:33:44:55:66",
  "status_wifi_channel": 13,
  "status_wifi_rssi": -93,
  "status_wifi_bssid": "xx:yy:zz:aa:bb:cc",
  "status_wifi_dhcp": false,
  "status_wifi_ip": "192.168.aa.cc",
  "status_wifi_gateway": "192.168.aa.dd",
  "status_wifi_netmask": "255.255.255.0",
  "mqttStatus": "connected"
}
*/
    DynamicJsonDocument doc(768);
    if (doc.capacity() == 0) {
        // out of memory
        return "";
    }
    doc[F("status_wifi_ap_mode")] = Network.inAPMode();


    struct ip_info info;
    if (Network.inAPMode()) {
        wifi_get_ip_info(SOFTAP_IF, &info);

        struct softap_config conf;
        memset(&conf, 0, sizeof(conf));
        wifi_softap_get_config(&conf);

        doc[F("status_wifi_mac")] = WiFi.softAPmacAddress();
    } else {
        wifi_get_ip_info(STATION_IF, &info);

        struct station_config conf;
        memset(&conf, 0, sizeof(conf));
        wifi_station_get_config(&conf);

        char ssid_str[sizeof(conf.ssid)+1];
        memcpy(ssid_str, conf.ssid, sizeof(conf.ssid));
        ssid_str[sizeof(ssid_str) - 1] = 0;

        doc[F("status_wifi_ssid")] = ssid_str;
        doc[F("status_wifi_dns")] = WiFi.dnsIP().toString();
        doc[F("status_wifi_mac")] = WiFi.macAddress();
        doc[F("status_wifi_channel")] = WiFi.channel();
        doc[F("status_wifi_rssi")] = WiFi.RSSI();
        doc[F("status_wifi_bssid")] = WiFi.BSSIDstr();
        doc[F("status_wifi_dhcp")] = Network.getConfigWifi().dhcp;
    }
    IPAddress ipaddr = IPAddress(info.ip.addr);
    IPAddress gwaddr = IPAddress(info.gw.addr);
    IPAddress nmaddr = IPAddress(info.netmask.addr);
    doc[F("status_wifi_ip")] = ipaddr.toString();
    doc[F("status_wifi_gateway")] = gwaddr.toString();
    doc[F("status_wifi_netmask")] = nmaddr.toString();

    doc[F("mqttStatus")] = Mqtt.isConnected() ?"connected" :"N/A";

    String buffer;
    SERIALIZE_JSON_LOG(doc, buffer);
    SYSTEMMONITOR_STAT();
    return buffer;
}


static void sendStatus(AsyncWebSocketClient *client)
{
    String buffer;
    buffer = getStatusJsonInfoHardware();
    client->text(buffer);
    buffer = getStatusJsonInfoApp();
    client->text(buffer);
    buffer = getStatusJsonInfoLibrary();
    client->text(buffer);
    buffer = getStatusJsonInfoMemory();
    client->text(buffer);
    buffer = getStatusJsonInfoFilesystem();
    client->text(buffer);
    buffer = getStatusJsonInfoNetwork();
    client->text(buffer);
}

/* vim:set ts=4 et: */
