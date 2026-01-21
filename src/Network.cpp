#include "ProjectConfiguration.h"

#include "Network.h"

#include "AmisReader.h"
#include "Application.h"
#include "config.h"
#include "DefaultConfigurations.h"
#include "LedSingle.h"
#include "Log.h"
#define LOGMODULE   LOGMODULE_NETWORK
#include "ModbusSmartmeterEmulation.h"
#include "Mqtt.h"
#include "SystemMonitor.h"


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
    LOG_DP("WiFi NetworkClass::onStationModeGotIP() start");
    _isConnected = true;
    _tickerReconnect.detach();

    LOGF_IP("WiFi connected to %s channel %" PRId8 " with local IP " PRsIP, WiFi.SSID().c_str(), WiFi.channel(), PRIPVal(WiFi.localIP()));
    LOGF_VP("mask=" PRsIP ", gateway=" PRsIP, PRIPVal(event.mask), PRIPVal(event.gw));

    restartMDNSIfNeeded();

    Mqtt.networkOnStationModeGotIP(event);

    LedBlue.turnBlink(4000, 10);
    LOG_DP("WiFi NetworkClass::onStationModeGotIP() end");
    SYSTEMMONITOR_STAT();
}

void NetworkClass::onStationModeDisconnected(const WiFiEventStationModeDisconnected& event)
{
    LOG_DP("WiFi NetworkClass::onStationModeDisconnected() start");
    if (!_isConnected) {
        // seems this gets called even we were not connected ..,. skip it
        LOG_DP("were not connected");
    } else {
        LOGF_IP("WiFi disconnected! Errorcode: %d", (int)event.reason);
        _isConnected = false;
        Mqtt.networkOnStationModeDisconnected(event);
        restartMDNSIfNeeded(); // MDNS.end();
    }

    // in 2 Sekunden Versuch sich wieder verzubinden
    _tickerReconnect.detach();
#if 1
    _tickerReconnect.once(2, std::bind(&NetworkClass::connect, this));
#else
    _tickerReconnect.once_scheduled(2, std::bind(&NetworkClass::connect, this));
#endif
    LedBlue.turnBlink(150, 150);
    LOG_DP("WiFi NetworkClass::onStationModeDisconnected() end");
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
        LOGF_EP("Could not open %s", "/config_wifi");
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
        LOGF_EP("Failed parsing %s", "/config_wifi");
        return false;
    }

    config.pingrestart_do = (*json)[F("pingrestart_do")].as<bool>();
    IPAddress pingrestart_ip;
    pingrestart_ip.fromString((*json)[F("pingrestart_ip")] | "");
    config.pingrestart_ip[0] = pingrestart_ip[0];
    config.pingrestart_ip[1] = pingrestart_ip[1];
    config.pingrestart_ip[2] = pingrestart_ip[2];
    config.pingrestart_ip[3] = pingrestart_ip[3];
    config.pingrestart_interval = (*json)[F("pingrestart_interval")].as<unsigned int>();
    config.pingrestart_max = (*json)[F("pingrestart_max")].as<unsigned int>();

    config.allow_sleep_mode = (*json)[F("allow_sleep_mode")].as<bool>();

    strlcpy(config.ssid, (*json)[F("ssid")] | "", sizeof(config.ssid));
    strlcpy(config.wifipassword, (*json)[F("wifipassword")] | "", sizeof(config.wifipassword));

    config.channel = (*json)[F("channel")].as<int32_t>();
    if (config.channel < 0 || config.channel > 13) {
        config.channel = 0;
    }

    config.dhcp = (*json)[F("dhcp")].as<bool>();

    config.mdns = (*json)[F("mdns")].as<bool>();

    config.rfpower = 20;
    if ((*json)[F("rfpower")] != "") {
        config.rfpower = (*json)[F("rfpower")].as<int>();
        if (config.rfpower > 21) {
            config.rfpower = 21;
        }
    }

    IPAddress ip_static;
    ip_static.fromString((*json)[F("ip_static")] | "");
    config.ip_static[0] = ip_static[0];
    config.ip_static[1] = ip_static[1];
    config.ip_static[2] = ip_static[2];
    config.ip_static[3] = ip_static[3];

    IPAddress ip_netmask;
    ip_netmask.fromString((*json)[F("ip_netmask")] | "");
    config.ip_netmask[0] = ip_netmask[0];
    config.ip_netmask[1] = ip_netmask[1];
    config.ip_netmask[2] = ip_netmask[2];
    config.ip_netmask[3] = ip_netmask[3];

    IPAddress ip_nameserver;
    ip_nameserver.fromString((*json)[F("ip_nameserver")] | "");
    config.ip_nameserver[0] = ip_nameserver[0];
    config.ip_nameserver[1] = ip_nameserver[1];
    config.ip_nameserver[2] = ip_nameserver[2];
    config.ip_nameserver[3] = ip_nameserver[3];

    IPAddress ip_gateway;
    ip_gateway.fromString((*json)[F("ip_gateway")] | "");
    config.ip_gateway[0] = ip_gateway[0];
    config.ip_gateway[1] = ip_gateway[1];
    config.ip_gateway[2] = ip_gateway[2];
    config.ip_gateway[3] = ip_gateway[3];

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
        WiFi.setSleepMode(WIFI_NONE_SLEEP); // listenInterval=0
        LOG_DP("Wifi sleep mode disabled");
    } else {
        // TODO(anyone) ... sollte hier nicht auch was gemacht werden?
        // WIFI_NONE_SLEEP = 0, WIFI_LIGHT_SLEEP = 1, WIFI_MODEM_SLEEP = 2
        // WiFi.getSleepMode()
    }

    LOGF_DP("Starting Wifi in Station-Mode");
    WiFi.setOutputPower(_configWifi.rfpower);  // 0..20.5 dBm
    if (_configWifi.dhcp) {
        LOGF_DP("Using DHCP");
        IPAddress ip_0_0_0_0;
        WiFi.config(ip_0_0_0_0, ip_0_0_0_0, ip_0_0_0_0, ip_0_0_0_0); // Enforce DHCP enabled (WiFi._useStaticIp = false)
        String validHostname = getValidHostname(Config.DeviceName);
        WiFi.hostname(validHostname);               /// !!!!!!!!!!!!!Funktioniert NUR mit DHCP !!!!!!!!!!!!!
    } else {
        LOGF_DP("Using static IP configuration");
        WiFi.config(_configWifi.ip_static, _configWifi.ip_gateway, _configWifi.ip_netmask, _configWifi.ip_nameserver, _configWifi.ip_nameserver);
    }

    _tickerReconnect.once_scheduled(60, std::bind(&NetworkClass::connect, this));
    WiFi.setAutoReconnect(false);
    LOGF_IP("Connecting to ssid: %s, channel: %d", _configWifi.ssid, _configWifi.channel);

    WiFi.begin(_configWifi.ssid, _configWifi.wifipassword, _configWifi.channel);
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

static void readStringFromEEPROM(int beginaddress, size_t size, char *buffer, size_t bufferlen)
{
    if (size == 0 || bufferlen == 0) {
        return;
    }

    for(size_t cnt = 0; cnt < size && cnt < bufferlen-1; cnt++) {
        char ch;
        ch = EEPROM.read(beginaddress + cnt);
        *buffer++ = ch;
        if (ch == 0) {
            return;
        }
    }
    *buffer = 0;
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

    config.ip_nameserver[0] = EEPROM.read(12);
    config.ip_nameserver[1] = EEPROM.read(13);
    config.ip_nameserver[2] = EEPROM.read(14);
    config.ip_nameserver[3] = EEPROM.read(15);

    config.dhcp = EEPROM.read(16) ?true :false;
    config.rfpower = EEPROM.read(26);
    if (config.rfpower > 21) {
        config.rfpower = 21;
    }

    config.ip_static[0] = EEPROM.read(32);
    config.ip_static[1] = EEPROM.read(33);
    config.ip_static[2] = EEPROM.read(34);
    config.ip_static[3] = EEPROM.read(35);

    config.ip_netmask[0] = EEPROM.read(36);
    config.ip_netmask[1] = EEPROM.read(37);
    config.ip_netmask[2] = EEPROM.read(38);
    config.ip_netmask[3] = EEPROM.read(39);

    config.ip_gateway[0] = EEPROM.read(40);
    config.ip_gateway[1] = EEPROM.read(41);
    config.ip_gateway[2] = EEPROM.read(42);
    config.ip_gateway[3] = EEPROM.read(43);

    readStringFromEEPROM(62, 32, config.ssid, sizeof(config.ssid));
    readStringFromEEPROM(94, 32, config.wifipassword, sizeof(config.wifipassword));

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


void NetworkClass::restartMDNSIfNeeded()
{
    if (Network.inAPMode()) {
        return;
    }

    // Totally strange:
    //   MDNS.isRunning() returns true even if we called MSND.end() previously ... check this
    // So stop ... and if needed start
    bool isRunning = MDNS.isRunning();
    bool doRestart = false;
    if (isRunning && _configWifi.mdns && _isConnected) {
        doRestart = true;
    } else {
        doRestart = false;
    }
    LOGF_VP("MDNS: isRunning=%d doRestart=%d _isConnected=%d _configWifi.mdns=%d",
            isRunning, doRestart, _isConnected, _configWifi.mdns);

    MDNS.end();

    if (_configWifi.mdns && _isConnected) {
        LOGF_IP("(Re)starting MDNS responder.");

        // Hostname kann nur MDNS_DOMAIN_LABEL_MAXLENGTH Zeichen lang werden !
        if (!MDNS.begin(Config.DeviceName)) {
            LOGF_EP("Error setting up MDNS responder!");
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

        LOG_DP("MDNS (re)started");
    }
}


String NetworkClass::getValidHostname(const char *hostname)
{
    // see: LwipIntf::hostname  --> Max 32 chars
    //
    // see RFC952: - 24 chars max
    //             - only a..z A..Z 0..9 '-'
    //             - no '-' as last char

    char rHostname[32 + 1];

    const char *s = hostname;
    char *t = rHostname;
    char * const te = t + sizeof(rHostname) - 1;

    // copy and transform chars from Config.DeviceName into rHostname
    while (*s && t < te) {
        if (isalnum(*s)) {
            *t++ = *s++;
        } else if (*s == ' ' || *s == '_' || *s == '-' || *s == '+' || *s == '!' || *s == '?' || *s == '*') {
            *t++ = '-';
            s++;
        } else {
            s++; // skip that character
        }
    }
    *t = 0;

    // remove trailing '-'
    t--;
    while (t >= &rHostname[0] && *t == '-') {
        *t-- = 0;
    }

    if (!rHostname[0]) {
        // Do now allow an empty hostname
        snprintf_P(rHostname, sizeof(rHostname), PSTR("%s-%08" PRIx32), APP_NAME, ESP.getChipId());
    }
    return rHostname;
}


NetworkClass Network;

/* vim:set ts=4 et: */
