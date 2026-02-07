
//#define DEBUG
#include "amis_debug.h"

#include "config.h"
#include "AmisReader.h"
#include "DefaultConfigurations.h"
#include "ModbusSmartmeterEmulation.h"
#include "RebootAtMidnight.h"
#include "RemoteOnOff.h"
#include "ThingSpeak.h"
#include "Webserver.h"

#include <ArduinoJson.h>
#include <LittleFS.h>

extern void writeEvent(String type, String src, String desc, String data);

void ConfigClass::init()
{
    webserverTryGzipFirst = true;
}

void ConfigClass::loadConfigGeneral()
{
    File configFile;
    configFile = LittleFS.open("/config_general", "r");

    if (!configFile) {
        DBG("[ WARN ] Failed to open config_general");
        writeEvent("ERROR", "Allgemein", "Could not open /config_general", "");
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
        DBG("[ WARN ] Failed to parse config_general");
        writeEvent("ERROR", "Allgemein", "Error parsing /config_general", "");
        return;
    }
    //json.prettyPrintTo(Serial);

    DeviceName = (*json)[F("devicename")].as<String>();
    DeviceName.trim();

    use_auth = (*json)[F("use_auth")].as<bool>();
    auth_passwd = (*json)[F("auth_passwd")].as<String>();
    auth_user = (*json)[F("auth_user")].as<String>();

    log_sys = (*json)[F("log_sys")].as<bool>();

    smart_mtr = (*json)[F("smart_mtr")].as<bool>(); 
    shelly_smart_mtr_udp = (*json)[F("shelly_smart_mtr_udp")].as<bool>();
    shelly_smart_mtr_udp_device_index = (*json)[F("shelly_smart_mtr_udp_device_index")].as<int>();
    shelly_smart_mtr_udp_offset = (*json)[F("shelly_smart_mtr_udp_offset")].as<int>();
    shelly_smart_mtr_udp_hardware_id_appendix = (*json)[F("shelly_smart_mtr_udp_hardware_id_appendix")].as<String>();

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
    AmisReader.setKey(Config.amis_key.c_str());
    RemoteOnOff.config(Config.switch_url_on, Config.switch_url_off, Config.switch_on, Config.switch_off, Config.switch_intervall);

    ThingSpeak.setInterval(Config.thingspeak_iv);
    ThingSpeak.setApiKeyWriite(Config.write_api_key);
    ThingSpeak.setEnabled(Config.thingspeak_aktiv);

    Webserver.setCredentials(Config.use_auth, Config.auth_user, Config.auth_passwd);
    Webserver.setTryGzipFirst(Config.webserverTryGzipFirst);
    // TODO(anyone): Apply more settings but we must first check setup() as there are prior some MODULE.init() calls
#if 0

    // RemoteOnOffClass
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
