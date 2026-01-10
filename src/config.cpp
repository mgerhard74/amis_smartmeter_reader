
//#define DEBUG
#include "debug.h"

#include "config.h"
#include "AmisReader.h"
#include "Application.h"
#include "DefaultConfigurations.h"
#include "Log.h"
#define LOGMODULE   LOGMODULE_BIT_SYSTEM
#include "ModbusSmartmeterEmulation.h"
#include "Network.h"
#include "RebootAtMidnight.h"
#include "RemoteOnOff.h"
#include "ThingSpeak.h"
#include "Webserver.h"

#include <ArduinoJson.h>
#include <ESP8266mDNS.h>
#include <LittleFS.h>

void ConfigClass::init()
{
    webserverTryGzipFirst = true;
}

void ConfigClass::loadConfigGeneral()
{
    if (Application.inAPMode()) {
        // even skip loading any json in AP Mode (so we should not be able bricking the device)
        return;
    }

    File configFile;
    configFile = LittleFS.open("/config_general", "r");
    if (!configFile) {
        LOG_EP("Could not open %s", "/config_general");
#ifndef DEFAULT_CONFIG_GENERAL_JSON
        return;
#endif
    }

    DynamicJsonBuffer jsonBuffer;
    JsonObject *json = nullptr;
    if (configFile) {
        json = &jsonBuffer.parseObject(configFile);
        configFile.close();
    } else {
#ifdef DEFAULT_CONFIG_GENERAL_JSON
        json = &jsonBuffer.parseObject(DEFAULT_CONFIG_GENERAL_JSON);
#endif
    }
    if (json == nullptr || !json->success()) {
        LOG_EP("Failed parsing %s", "/config_general");
        return;
    }

    DeviceName = (*json)[F("devicename")].as<String>();
    DeviceName.trim();

    use_auth = (*json)[F("use_auth")].as<bool>();
    auth_passwd = (*json)[F("auth_passwd")].as<String>();
    auth_user = (*json)[F("auth_user")].as<String>();

    log_sys = (*json)[F("log_sys")].as<bool>();

    smart_mtr = (*json)[F("smart_mtr")].as<bool>();

    amis_key = (*json)[F("amis_key")].as<String>();
    amis_key.trim();

    thingspeak_aktiv = (*json)[F("thingspeak_aktiv")].as<bool>();
    channel_id = (*json)[F("channel_id")].as<unsigned int>();
    write_api_key = (*json)[F("write_api_key")].as<String>();
    write_api_key.trim();
    read_api_key = (*json)[F("read_api_key")].as<String>();
    read_api_key.trim();
    thingspeak_iv = (*json)[F("thingspeak_iv")].as<unsigned int>();
    if (thingspeak_iv < 30)  {
        thingspeak_iv = 30;
    }
    channel_id2 = (*json)[F("channel_id2")].as<unsigned int>();
    read_api_key2 = (*json)[F("read_api_key2")].as<String>();
    read_api_key2.trim();

    rest_var = (*json)[F("rest_var")].as<unsigned int>();
    rest_ofs = (*json)[F("rest_ofs")].as<int>();
    rest_neg = (*json)[F("rest_neg")].as<bool>();

    reboot0 = (*json)[F("reboot0")].as<bool>();

    switch_on = (*json)[F("switch_on")].as<int>();
    switch_off = (*json)[F("switch_off")].as<int>();
    switch_url_on = (*json)[F("switch_url_on")].as<String>();
    switch_url_on.trim();
    switch_url_off = (*json)[F("switch_url_off")].as<String>();
    switch_url_off.trim();
    switch_intervall = (*json)[F("switch_intervall")].as<unsigned int>();

    developerModeEnabled = (*json)[F("developerModeEnabled")].as<bool>();

    // 'webserverTryGzipFirst' wird nicht ausgewertet sondern nur bis zum nächsten Reboot mitgeführt
    // webserverTryGzipFirst = (*json)[F("webserverTryGzipFirst")].as<bool>();
}

void ConfigClass::applySettingsConfigGeneral()
{
    if (Config.log_sys) {
        Log.setLoglevel(LOGLEVEL_INFO);
    } else {
        Log.setLoglevel(LOGLEVEL_NONE);
    }

    AmisReader.setKey(Config.amis_key.c_str());

    RemoteOnOff.config(Config.switch_url_on, Config.switch_url_off, Config.switch_on, Config.switch_off, Config.switch_intervall);

    ThingSpeak.setInterval(Config.thingspeak_iv);
    ThingSpeak.setApiKeyWriite(Config.write_api_key);
    ThingSpeak.setEnabled(Config.thingspeak_aktiv);

    Webserver.setCredentials(Config.use_auth, Config.auth_user, Config.auth_passwd);
    Webserver.setTryGzipFirst(Config.webserverTryGzipFirst);

    MDNS.end(); // Config.Devicename könnte geändert worden sein! ==> ev MDNS neu starten
    Network.startMDNSIfNeeded();

    // TODO(anyone): Apply more settings but we must first check setup() as there are prior some MODULE.init() calls
#if 0

    // WatchdogPingClass

    // Smartmeter
    if (smart_mtr) {
        ModbusSmartmeterEmulation.enable();
    } else {
        ModbusSmartmeterEmulation.disable();
    }

    // Reboot at 00:00
    if (reboot0) {
        RebootAtMidnight.enable();
    } else {
        RebootAtMidnight.disable();
    }
#endif
}

ConfigClass Config;

/* vim:set ts=4 et: */
