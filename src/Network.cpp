#include "ProjectConfiguration.h"

#include "Network.h"

#include "AmisReader.h"
#include "Application.h"
#include "config.h"
#include "DefaultConfigurations.h"
#include "LedSingle.h"
#include "Log.h"
#define LOGMODULE   LOGMODULE_BIT_NETWORK
#include "ModbusSmartmeterEmulation.h"
#include "Mqtt.h"
#include "SystemMonitor.h"

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
#include <EEPROM.h>
#include <ESP8266mDNS.h>
#include <LittleFS.h>


extern const char *__COMPILED_GIT_HASH__;


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
    DBGOUT("WiFi onStationModeGotIP()\n");
    DBGPRINTF("%d\n", _tickerReconnect.active());
    _isConnected = true;
    _tickerReconnect.detach();
    LedBlue.turnBlink(4000, 10);
    LOG_IP("WiFi connected to %s with local IP %s", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
    LOG_VP("mask=%s, gateway=%s", event.mask.toString().c_str(), event.gw.toString().c_str());

    startMDNSIfNeeded();

#if DEBUGHW==1
    dbg_server.begin();
    //  dbg_server.setNoDelay(true);  Nicht benützen, bei WIFI nicht funktionell
#endif
    Mqtt.networkOnStationModeGotIP(event);
    SYSTEMMONITOR_STAT();
}

void NetworkClass::onStationModeDisconnected(const WiFiEventStationModeDisconnected& event)
{
    LOG_DP("WiFi onStationModeDisconnected() start");
    _isConnected = false;
    Mqtt.networkOnStationModeDisconnected(event);
    MDNS.end();

    // in 2 Sekunden Versuch sich wieder verzubinden
    _tickerReconnect.detach();
#if 1
    _tickerReconnect.once(2, std::bind(&NetworkClass::connect, this));
#else
    _tickerReconnect.once_scheduled(2, std::bind(&NetworkClass::connect, this));
#endif
    LedBlue.turnBlink(150, 150);
    LOG_IP("WiFi disconnected! Errorcode: %d", (int)event.reason);
    LOG_DP("WiFi onStationModeDisconnected() end");
    SYSTEMMONITOR_STAT();
}

bool NetworkClass::loadConfigWifi(NetworkConfigWifi_t &config)
{
    if (Application.inAPMode()) {
        // even skip loading any json in AP Mode (so we should not be able bricking the device)
        return false;
    }

    File configFile;
    configFile = LittleFS.open("/config_wifi", "r");
    if (!configFile) {
        LOG_EP("Could not open %s", "/config_wifi");
#ifndef DEFAULT_CONFIG_WIFI_JSON
        return loadConfigWifiFromEEPROM(config);
#else
        if (loadConfigWifiFromEEPROM(config)) {
            return true;
        }
#endif
    }

    DynamicJsonBuffer jsonBuffer;
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
        LOG_EP("Failed parsing %s", "/config_wifi");
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
    LOG_DP("WiFi connect() start");
    _tickerReconnect.detach();
    if (_apMode) {
        WiFi.mode(WIFI_AP);
        //WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
        WiFi.softAP("ESP8266_AMIS");
        LOG_IP("Stating AccessPoint-Mode: 192.168.4.1");
        LedBlue.turnBlink(500, 500);
        return;
    }

    WiFi.mode(WIFI_STA);
    if (!_configWifi.allow_sleep_mode) {
        WiFi.setSleepMode(WIFI_NONE_SLEEP);
        LOG_IP("Wifi sleep mode disabled");
    } else {
        // TODO(anyone) ... sollte hier nicht auch was gemacht werden?
    }

    LOG_DP("Starting Wifi in Station-Mode");
    WiFi.setOutputPower(_configWifi.rfpower);  // 0..20.5 dBm
    if (_configWifi.dhcp) {
        LOG_DP("Using DHCP");
        IPAddress ip_0_0_0_0;
        WiFi.config(ip_0_0_0_0, ip_0_0_0_0, ip_0_0_0_0, ip_0_0_0_0); // Enforce DHCP enabled (WiFi._useStaticIp = false)
        WiFi.hostname(getHostname(Config.DeviceName.c_str()));               /// !!!!!!!!!!!!!Funktioniert NUR mit DHCP !!!!!!!!!!!!!
    } else {
        LOG_DP("Using static IP configuration");
        WiFi.config(_configWifi.ip_static, _configWifi.ip_gateway, _configWifi.ip_netmask, _configWifi.ip_nameserver, _configWifi.ip_nameserver);
    }

    _tickerReconnect.once_scheduled(60, std::bind(&NetworkClass::connect, this));
    WiFi.setAutoReconnect(false);
    WiFi.begin(_configWifi.ssid, _configWifi.wifipassword);
    LedBlue.turnBlink(150, 150);
    LOG_DP("WiFi connect() end");
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
    // TODO(anyone): Brauchen wir das wirklich noch?

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


void NetworkClass::startMDNSIfNeeded()
{
    if (Network.inAPMode()) {
        return;
    }

    // Return if no state change
    if (MDNS.isRunning() == _configWifi.mdns) {
        return;
    }

    MDNS.end();

    if (!_configWifi.mdns) {
        LOG_IP("MDNS is disabled");
        return;
    }

    LOG_IP("Starting MDNS responder...");


    if (!MDNS.begin(Config.DeviceName)) {
        LOG_EP("Error setting up MDNS responder!");
        return;
    }

    MDNS.addService("http", "tcp", 80);
    MDNS.addService("amis-reader", "tcp", 80); // our rest api service (service "amis-reader" is not official)
    MDNS.addServiceTxt("amis-reader", "tcp", "git_hash", __COMPILED_GIT_HASH__);
    /*
    There is no 'modbus' service available
    see: https://www.dns-sd.org/servicetypes.html abd RFC2782

    if (Config.smart_mtr) {
        MDNS.addService("modbus", "tcp", SMARTMETER_EMULATION_SERVER_PORT);
    }*/

    LOG_IP("MDNS started");
}


String NetworkClass::getHostname(const char *hostname)
{
    // see: LwipIntf::hostname  --> Max 32 chars
    //
    // see RFC952: - 24 chars max
    //             - only a..z A..Z 0..9 '-'
    //             - no '-' as last char

    char rHostname[32 + 1];


    const char *s = hostname;
    char *t = rHostname;
    char *te = t + sizeof(rHostname);

    // copy and transform chars from Config.DeviceName into rHostname
    while (*s && t < te) {
        if (isalnum(*s)) {
            *t++ = *s++;
        } else if (*s == ' ' || *s == '_' || *s == '-' || *s == '+' || *s == '!' || *s == '?' || *s == '*') {
            *t++ = '-';
            s++;
        }
        /*
        else {
            skip that character
        }
        */
    }
    *t = 0;

    // remove trailing '-'
    t--;
    while (t >= &rHostname[0] && *t == '-') {
        *t-- = 0;
    }

    if (!rHostname[0]) {
        // Do now allow an empty hostname
        snprintf_P(rHostname, sizeof(rHostname), PSTR("%s-%" PRIx32), APP_NAME, ESP.getChipId());
    }
    return rHostname;
}


NetworkClass Network;

/* vim:set ts=4 et: */
