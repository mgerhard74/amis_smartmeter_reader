#include "Network.h"

#include "AmisReader.h"
#include "config.h"
#include "LedSingle.h"

//#define DEBUG
#include "debug.h"

#include <AsyncJson.h>
#include <AsyncMqttClient.h>
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
    _isConnected = false;
    LedBlue.turnOff();
    mqttTimer.detach(); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
    if (MDNS.isRunning()) {
        MDNS.end();
    }

    // in 2 Sekunden Versuch sich wieder verzubinden
    _tickerReconnect.once_scheduled(2, std::bind(&NetworkClass::connect, this));
    if (Config.log_sys) {
        writeEvent("INFO", "wifi", "WiFi !!! DISCONNECT !!!", "Errorcode: " + String(event.reason));
    }
    DBGOUT("WiFi disconnect\n");
}

bool NetworkClass::loadConfigWifi(NetworkConfigWifi_t &config)
{
    File configFile;
    DynamicJsonBuffer jsonBuffer;

    configFile = LittleFS.open("/config_wifi", "r");
    if (!configFile) {
        DBGOUT("[ ERR ] Failed to open config_wifi\n");
        writeEvent("ERROR", "wifi", "WiFi config fail", "");
        return false;
    }

    JsonObject &json = jsonBuffer.parseObject(configFile);
    configFile.close();
    if (!json.success()) {
        DBGOUT("[ WARN ] Failed to parse config_wifi\n");
        writeEvent("ERROR", "wifi", "WiFi config error", "");
        return false;
    }

    config.pingrestart_do = json[F("pingrestart_do")].as<bool>();
    config.pingrestart_ip = json[F("pingrestart_ip")].as<String>();
    config.pingrestart_interval = json[F("pingrestart_interval")].as<unsigned int>();
    config.pingrestart_max = json[F("pingrestart_max")].as<unsigned int>();

    config.allow_sleep_mode = json[F("allow_sleep_mode")].as<bool>();

    config.ssid = json[F("ssid")].as<String>();
    config.wifipassword = json[F("wifipassword")].as<String>();

    config.dhcp = json[F("dhcp")].as<bool>();

    config.mdns = json[F("mdns")].as<bool>();

    config.rfpower = 20;
    if (json[F("rfpower")] != "") {
        config.rfpower = json[F("rfpower")].as<int>();
    }

    const char *v;
    v = json[F("ip_static")].as<const char *>();
    config.ip_static.fromString(v);
    v = json[F("ip_netmask")].as<const char *>();
    config.ip_netmask.fromString(v);
    v = json[F("ip_nameserver")].as<const char *>();
    config.ip_nameserver.fromString(v);
    v = json[F("ip_gateway")].as<const char *>();
    config.ip_gateway.fromString(v);

    return true;
}

void NetworkClass::connect(void)
{
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
        // TODO ... sollte hier nicht aus was gemacht werden?
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

    WiFi.setAutoReconnect(false);
    WiFi.begin(_configWifi.ssid, _configWifi.wifipassword);

    LedBlue.turnBlink(150, 150);
    _tickerReconnect.once_scheduled(60, std::bind(&NetworkClass::connect, this));
    DBGOUT(F("WiFi begin\n"));
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

NetworkClass Network;

/* vim:set ts=4 et: */
