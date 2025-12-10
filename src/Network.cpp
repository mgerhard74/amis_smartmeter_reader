#include "Network.h"

#include "AmisReader.h"
#include "config.h"
#include "DefaultConfigurations.h"
#include "LedSingle.h"
#include "unused.h"

//#define DEBUG
#include "debug.h"

#if 0
#undef DBGOUT
#define DBGOUT(X)  Serial.print(X)
#define DBGPRINTF(fmt, args...)  Serial.printf(fmt, ##args)
#else
#define DBGPRINTF(fmt, args...) (void)(0)
#endif

#include <ArduinoJson.h>
#include <AsyncMqttClient.h>
#include <EEPROM.h>
#include <ESP8266mDNS.h>
#include <LittleFS.h>

//#include "proj.h"
extern AsyncMqttClient mqttClient;
extern Ticker mqttTimer;

extern void mqtt_init();
extern void upgrade (bool save);
extern void writeEvent(String, String, String, String);

void NetworkClass::init(bool apMode)
{
    WiFi.persistent(false);
    WiFi.disconnect(true, true);
    _isConnected = false;

    using std::placeholders::_1;
    _onStationModeGotIP = WiFi.onStationModeGotIP(std::bind(&NetworkClass::onStationModeGotIP, this, _1));
    _onStationModeDisconnected = WiFi.onStationModeDisconnected(std::bind(&NetworkClass::onStationModeDisconnected, this, _1));

    _apMode = apMode;
    if (!loadConfigWifi(_configWifi)) {
        _apMode = true;
    }
}

void NetworkClass::onStationModeGotIP(const WiFiEventStationModeGotIP& event)
{
    UNUSED_ARG(event);
    DBGOUT("WiFi onStationModeGotIP()\n");
    DBGPRINTF("%d\n", _tickerReconnect.active());
    _isConnected = true;
    _tickerReconnect.detach();
    LedBlue.turnBlink(4000, 10);
    String data = WiFi.SSID() + " " + WiFi.localIP().toString();
    eprintf("Connected to %s\n",data.c_str());
    if (Config.log_sys) {
        writeEvent("INFO", "wifi", "WiFi is connected", data);
    }
    /*
    data = "ip:" + event.ip.toString() + "/" + event.mask.toString() + "\ngw:" + event.gw.toString();
    if (Config.log_sys) {
        writeEvent("INFO", "wifi", "WiFi details", data);
    }
    */

    if (_configWifi.mdns && !MDNS.isRunning()) {
        if (MDNS.begin(Config.DeviceName)) {              // Start the mDNS responder for esp8266.local
            DBGOUT("mDNS responder started\n");
        } else {
            DBGOUT("Error setting up MDNS responder!\n");
        }
    }

#if DEBUGHW==1
    dbg_server.begin();
    //  dbg_server.setNoDelay(true);  Nicht benützen, bei WIFI nicht funktionell
#endif
    mqtt_init();
}

void NetworkClass::onStationModeDisconnected(const WiFiEventStationModeDisconnected& event)
{
    DBGOUT("WiFi onStationModeDisconnected() start\n");
    DBGPRINTF("%d\n", _tickerReconnect.active());
    _isConnected = false;
    LedBlue.turnOff();
    mqttTimer.detach(); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
    if (MDNS.isRunning()) {
        MDNS.end();
    }

    // in 2 Sekunden Versuch sich wieder verzubinden
    _tickerReconnect.detach();
#if 1
    _tickerReconnect.once(2, std::bind(&NetworkClass::connect, this));
#else
    _tickerReconnect.once_scheduled(2, std::bind(&NetworkClass::connect, this));
#endif
    if (Config.log_sys) {
        writeEvent("INFO", "wifi", "WiFi !!! DISCONNECT !!!", "Errorcode: " + String(event.reason));
    }
    DBGOUT("WiFi onStationModeDisconnected() end\n");
    DBGPRINTF("%d\n", _tickerReconnect.active());
}

bool NetworkClass::loadConfigWifi(NetworkConfigWifi_t &config)
{
    File configFile;
    DynamicJsonBuffer jsonBuffer;

    configFile = LittleFS.open("/config_wifi", "r");
    if (!configFile) {
        DBGOUT("[ ERR ] Failed to open config_wifi\n");
        writeEvent("ERROR", "wifi", "WiFi config fail", "");
#ifndef DEFAULT_CONFIG_WIFI_JSON
        return loadConfigWifiFromEEPROM(config);
#else
        if (loadConfigWifiFromEEPROM(config)) {
            return true;
        }
#endif
    }
    JsonObject *json = nullptr;
    if (configFile) {
        json = &jsonBuffer.parseObject(configFile);
        configFile.close();
    } else {
#ifdef DEFAULT_CONFIG_WIFI_JSON
        json = &jsonBuffer.parseObject(DEFAULT_CONFIG_WIFI_JSON);
#endif
    }
    if (json == nullptr || !json->success()) {
        DBGOUT("[ WARN ] Failed to parse config_wifi\n");
        writeEvent("ERROR", "wifi", "WiFi config error", "");
        return false;
    }

    config.pingrestart_do = (*json)[F("pingrestart_do")].as<bool>();
    config.pingrestart_ip = (*json)[F("pingrestart_ip")].as<String>();
    config.pingrestart_ip.trim();
    config.pingrestart_interval = (*json)[F("pingrestart_interval")].as<unsigned int>();
    config.pingrestart_max = (*json)[F("pingrestart_max")].as<unsigned int>();

    config.allow_sleep_mode = (*json)[F("allow_sleep_mode")].as<bool>();

    config.ssid = (*json)[F("ssid")].as<String>();
    config.wifipassword = (*json)[F("wifipassword")].as<String>();

    config.dhcp = (*json)[F("dhcp")].as<bool>();

    config.mdns = (*json)[F("mdns")].as<bool>();

    config.rfpower = 20;
    if ((*json)[F("rfpower")] != "") {
        config.rfpower = (*json)[F("rfpower")].as<int>();
        if (config.rfpower > 21) {
            config.rfpower = 21;
        }
    }

    String v;
    v = (*json)[F("ip_static")].as<String>();
    v.trim();
    config.ip_static.fromString(v);
    v = (*json)[F("ip_netmask")].as<String>();
    v.trim();
    config.ip_netmask.fromString(v);
    v = (*json)[F("ip_nameserver")].as<String>();
    v.trim();
    config.ip_nameserver.fromString(v);
    v = (*json)[F("ip_gateway")].as<String>();
    v.trim();
    config.ip_gateway.fromString(v);

    return true;
}

void NetworkClass::connect(void)
{
    DBGOUT("WiFi connect() start\n");
    _tickerReconnect.detach();
    if (_apMode) {
        WiFi.mode(WIFI_AP);
        WiFi.softAP("ESP8266_AMIS");
        DBGOUT(F("AP-Mode: 192.168.4.1\n"));
        LedBlue.turnBlink(500, 500);
        return;
    }

    WiFi.mode(WIFI_STA);
    if (!_configWifi.allow_sleep_mode) {
        // disable sleep mode
        DBGOUT(F("Wifi sleep mode disabled\n"));
        WiFi.setSleepMode(WIFI_NONE_SLEEP);
        if (Config.log_sys) {
            writeEvent("INFO", "wifi", "Wifi sleep mode disabled", "");
        }
    } else {
        // TODO ... sollte hier nicht auch was gemacht werden?
    }

    DBGOUT(F("Start Wifi\n"));
    WiFi.setOutputPower(_configWifi.rfpower);  // 0..20.5 dBm
    if (_configWifi.dhcp) {
        DBGOUT("DHCP\n");
        IPAddress ip_0_0_0_0;
        WiFi.config(ip_0_0_0_0, ip_0_0_0_0, ip_0_0_0_0, ip_0_0_0_0); // Enforce DHCP enabled (WiFi._useStaticIp = false)
        WiFi.hostname(Config.DeviceName);               /// !!!!!!!!!!!!!Funktioniert NUR mit DHCP !!!!!!!!!!!!!
    } else {
        WiFi.config(_configWifi.ip_static, _configWifi.ip_gateway, _configWifi.ip_netmask, _configWifi.ip_nameserver);
    }

    LedBlue.turnBlink(150, 150);
    _tickerReconnect.once_scheduled(60, std::bind(&NetworkClass::connect, this));
    WiFi.setAutoReconnect(false);
    WiFi.begin(_configWifi.ssid, _configWifi.wifipassword);

    DBGOUT(F("WiFi connect() end\n"));
}

bool NetworkClass::inAPMode(void)
{
    return _apMode;
}

bool NetworkClass::isConnected(void)
{
    return _isConnected;
}

const NetworkConfigWifi_t &NetworkClass::getConfigWifi(void)
{
    return _configWifi;
}

static String readStringFromEEPROM(int beginaddress)
{
    size_t cnt;
    char buffer[33];
    for(cnt = 0; cnt < sizeof(buffer)-1; cnt++) {
        char ch;
        ch = EEPROM.read(beginaddress + cnt);
        buffer[cnt] = ch;
        if (ch == 0) {
            break;
        }
    }
    buffer[sizeof(buffer) - 1] = 0;
    return String(buffer);
}

bool NetworkClass::loadConfigWifiFromEEPROM(NetworkConfigWifi_t &config)
{
    // Irgendwann zu Beginn des Projektes, dürfte die
    // Config im EEPROM abgelegt worden sein.
    // Also: Wenn keine /config_wifi vorhanden ist, versuchen
    // wir die Config aus dem EEPROM zu lesen und gleich
    // als /config_wifi zu speichern
    //
    // TODO: Brauchen wir das wirklich noch?

    EEPROM.begin(256);
    if(EEPROM.read(0) != 'C' || EEPROM.read(1) != 'F'  || EEPROM.read(2) != 'G') {
        EEPROM.end();
        return false;
    }

    uint8_t ip_addr[4];
    ip_addr[0] = EEPROM.read(12);
    ip_addr[1] = EEPROM.read(13);
    ip_addr[2] = EEPROM.read(14);
    ip_addr[3] = EEPROM.read(15);
    config.ip_nameserver = IPAddress(ip_addr[0], ip_addr[1], ip_addr[2], ip_addr[3]);

    config.dhcp = EEPROM.read(16) ?true :false;
    config.rfpower = EEPROM.read(26);
    if (config.rfpower > 21) {
        config.rfpower = 21;
    }

    ip_addr[0] = EEPROM.read(32);
    ip_addr[1] = EEPROM.read(33);
    ip_addr[2] = EEPROM.read(34);
    ip_addr[3] = EEPROM.read(35);
    config.ip_static = IPAddress(ip_addr[0], ip_addr[1], ip_addr[2], ip_addr[3]);

    ip_addr[0] = EEPROM.read(36);
    ip_addr[1] = EEPROM.read(37);
    ip_addr[2] = EEPROM.read(38);
    ip_addr[3] = EEPROM.read(39);
    config.ip_netmask = IPAddress(ip_addr[0], ip_addr[1], ip_addr[2], ip_addr[3]);

    ip_addr[0] = EEPROM.read(40);
    ip_addr[1] = EEPROM.read(41);
    ip_addr[2] = EEPROM.read(42);
    ip_addr[3] = EEPROM.read(43);
    config.ip_gateway = IPAddress(ip_addr[0], ip_addr[1], ip_addr[2], ip_addr[3]);

    config.ssid = readStringFromEEPROM(62);
    config.wifipassword = readStringFromEEPROM(94);

    EEPROM.end();

    File f = LittleFS.open("/config_wifi", "w");
    if (f) {
        DynamicJsonBuffer jsonBuffer;
        JsonObject &root = jsonBuffer.createObject();
        root["dhcp"] = config.dhcp;
        root["ip_gateway"] = config.ip_gateway.toString();
        root["ip_nameserver"] = config.ip_nameserver.toString();
        root["ip_netmask"] = config.ip_netmask.toString();
        root["ip_static"] = config.ip_static.toString();
        root["rfpower"] = config.rfpower;
        root["ssid"] = config.ssid;
        root["wifipassword"] = config.wifipassword;
        root["command"] = "/config_wifi";
        root.printTo(f);
        f.close();
    }
    return true;
}

NetworkClass Network;

/* vim:set ts=4 et: */
