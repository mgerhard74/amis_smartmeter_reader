
//#define DEBUG
#include "debug.h"

#include "config.h"
#include "AmisReader.h"
#include "ModbusSmartmeterEmulation.h"
#include "RebootAtMidnight.h"

#include <ArduinoJson.h>
#include <LittleFS.h>

extern void writeEvent(String type, String src, String desc, String data);

void ConfigClass::LoadGeneral()
{
    File configFile;
    configFile = LittleFS.open("/config_general", "r");

    if (!configFile) {
        DBGOUT("[ WARN ] Failed to open config_general\n");
        writeEvent("ERROR", "Allgemein", "Could not open /config_general", "");
        return;
    }

    DynamicJsonBuffer jsonBuffer;
    JsonObject &json = jsonBuffer.parseObject(configFile);
    configFile.close();

    if (!json.success()) {
        DBGOUT("[ WARN ] Failed to parse config_general\n");
        writeEvent("ERROR", "Allgemein", "Error parsing /config_general", "");
        return;
    }
    //json.prettyPrintTo(Serial);

    DeviceName=json[F("devicename")].as<String>();

    use_auth=json[F("use_auth")].as<bool>();
    auth_passwd=json[F("auth_passwd")].as<String>();
    auth_user=json[F("auth_user")].as<String>();

    log_sys=json[F("log_sys")].as<bool>();

    smart_mtr=json[F("smart_mtr")].as<bool>();

    amis_key=json[F("amis_key")].as<String>();

    thingspeak_aktiv=json[F("thingspeak_aktiv")].as<bool>();
    channel_id=json[F("channel_id")].as<unsigned int>();
    write_api_key=json[F("write_api_key")].as<String>();
    read_api_key=json[F("read_api_key")].as<String>();
    thingspeak_iv=json[F("thingspeak_iv")].as<unsigned int>();
    if (Config.thingspeak_iv < 30)  {
        Config.thingspeak_iv=30;
    }
    channel_id2=json[F("channel_id2")].as<unsigned int>();
    read_api_key2=json[F("read_api_key2")].as<String>();

    rest_var=json[F("rest_var")].as<unsigned int>();
    rest_ofs=json[F("rest_ofs")].as<int>();
    rest_neg=json[F("rest_neg")].as<bool>();

    reboot0=json[F("reboot0")].as<bool>();

    switch_on=json[F("switch_on")].as<int>();
    switch_off=json[F("switch_off")].as<int>();
    switch_url_on=json[F("switch_url_on")].as<String>();
    switch_url_off=json[F("switch_url_off")].as<String>();
    switch_intervall=json[F("switch_intervall")].as<unsigned int>();
}

void ConfigClass::ApplySettingsGeneral()
{
    AmisReader.setKey(Config.amis_key.c_str());
    // TODO: Apply more settings but we must first check setup() as there are prior some MODULE.init() calls
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
