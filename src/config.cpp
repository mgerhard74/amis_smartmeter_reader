#include "config.h"

#include "AmisReader.h"
#include "Application.h"
#include "DefaultConfigurations.h"
#include "Json.h"
#include "Log.h"
#define LOGMODULE   LOGMODULE_SYSTEM
#include "ModbusSmartmeterEmulation.h"
#include "Network.h"
#include "RebootAtMidnight.h"
#include "RemoteOnOff.h"
#include "ShellySmartmeterEmulation.h"
#include "ThingSpeak.h"
#include "Webserver.h"

#include <ESP8266mDNS.h>
#include <LittleFS.h>

#if (THINGSPEAK_KEY_MAXLEN != CONFIG_THINGSPEAK_KEY_MAXLEN)
#error "THINGSPEAK_KEY_MAXLEN und CONFIG_THINGSPEAK_KEY_MAXLEN müssen gleich lang sein"
#endif

#define CONFIG_AMIS_KEY_JSON_NAME "\"amis_key\""
#define CONFIG_AMIS_KEY_JSON_NAME_LEN (sizeof(CONFIG_AMIS_KEY_JSON_NAME)-1)


extern const char* Config_restValueKeys[2][9];
const char * Config_restValueKeys[2][9] =
{
    { "1.8.0", "2.8.0", "3.8.1", "4.8.1",
      "1.7.0", "2.7.0", "3.7.0", "4.7.0",
      "1.128.0" },
    { "1_8_0", "2_8_0", "3_8_1", "4_8_1",
      "1_7_0", "2_7_0", "3_7_0", "4_7_0",
      "1_128_0" }
};


// If we're in AP mode, we just read "amis_key" and DO NOT use the json parser
// This should avoid bricking the device due invalid configuration files
// We brutally search for something like:   "KEY"\s*:\s*"VALUE
void ConfigClass::loadConfigGeneralMinimal()
{
    char buffer[128];

    File configFile;
    configFile = LittleFS.open("/config_general", "r");
    if (!configFile) {
        return;
    }
    if (configFile.size() > 1024*7) {
        // Do not parse "big" (>7kiB) files
        configFile.close();
        return;
    }
    size_t rlen;
    while (configFile.available()) {
        rlen = configFile.readBytes(buffer, sizeof(buffer));
        if (rlen == 0) {
            break;
        }

        char *found;
        {
            found = (char*) memmem(buffer, rlen, CONFIG_AMIS_KEY_JSON_NAME, CONFIG_AMIS_KEY_JSON_NAME_LEN);
            if (found == nullptr) {
                continue;
            }
            if (found != buffer) {
                // move file so the next read start exacly with our searched key
                configFile.seek(-rlen + (found - buffer), SeekCur);
                continue;
            }

            found += CONFIG_AMIS_KEY_JSON_NAME_LEN;
            buffer[sizeof(buffer)-1] = 0;   // just to be sure the string ends

            while (isspace(*found)) {       // skip over spaces
                found++;
            }
            if (*found != ':') {            // check  if we found ':'
                break;
            }
            found++;

            while (isspace(*found)) {       // skip over spaces
                found++;
            }
            if (*found != '"') {            // check  if we found '"'
                break;
            }
            found++;

            size_t i;                       // grab the value into amis_key
            for (i = 0; i < sizeof(amis_key)-1 && isxdigit(*found); i++) {
                amis_key[i] = *found++;
            }
            amis_key[i] = 0;
            //Serial.printf("Amiskey='%s'\n", amis_key);
            break;
        }
    }
    configFile.close();
}


void ConfigClass::init()
{
    strcpy(DeviceName, "Amis-1");  // NOLINT

    use_auth = false;
    auth_passwd[0] = 0;
    auth_user[0] = 0;

    write_api_key[0] = 0;

    amis_key[0] = 0;
}

void ConfigClass::loadConfigGeneral()
{
    if (Application.inAPMode()) {
        // just load some values not using any json parser or syntax check in AP Mode
        // (so we should not be able bricking the device using invalid config files)
        loadConfigGeneralMinimal();
        return;
    }

    File configFile;
    configFile = LittleFS.open("/config_general", "r");
    if (!configFile) {
        LOGF_EP("Could not open %s", "/config_general");
#ifndef DEFAULT_CONFIG_GENERAL_JSON
        return;
#endif
    }

    DynamicJsonDocument json(CONFIG_JSON_CONFIG_GENERAL_DOCUMENT_SIZE);
    if (!json.capacity()) {
        LOGF_EP("Json /config_general: Out of memory");
        return;
    }
    DeserializationError error = DeserializationError::EmptyInput;
    if (configFile) {
        error = deserializeJson(json, configFile);
        configFile.close();
    } else {
#ifdef DEFAULT_CONFIG_GENERAL_JSON
        error = deserializeJson(json, DEFAULT_CONFIG_GENERAL_JSON);
#endif
    }
    if (error) {
        LOGF_EP("Failed parsing %s. Error:'%s'", "/config_general", error.c_str());
        return;
    }

    strlcpy(DeviceName, json[F("devicename")] | "", sizeof(DeviceName));

    use_auth = json[F("use_auth")].as<bool>();
    strlcpy(auth_passwd, json[F("auth_passwd")] | "", sizeof(auth_passwd));
    strlcpy(auth_user, json[F("auth_user")] | "", sizeof(auth_user));

    log_sys = json[F("log_sys")].as<bool>();

    smart_mtr = json[F("smart_mtr")].as<bool>();
    shelly_smart_mtr_udp = json[F("shelly_smart_mtr_udp")].as<bool>();
    shelly_smart_mtr_udp_device_index = json[F("shelly_smart_mtr_udp_device_index")].as<unsigned>();
    shelly_smart_mtr_udp_offset = json[F("shelly_smart_mtr_udp_offset")].as<int>();
    shelly_smart_mtr_udp_hardware_id_appendix = json[F("shelly_smart_mtr_udp_hardware_id_appendix")].as<String>();

    strlcpy(amis_key, json[F("amis_key")] | "", sizeof(amis_key));

    thingspeak_aktiv = json[F("thingspeak_aktiv")].as<bool>();
    channel_id = json[F("channel_id")].as<unsigned int>();
    strlcpy(write_api_key, json[F("write_api_key")] | "", sizeof(write_api_key));
    thingspeak_iv = json[F("thingspeak_iv")].as<unsigned int>();
    if (thingspeak_iv < 30)  {
        thingspeak_iv = 30;
    }

    /*
    // read_api_key, channel_id2 und read_api_key2 only needed by web client. No need to read them here !

    strlcpy(read_api_key, json[F("read_api_key")] | "", sizeof(read_api_key));
    channel_id2 = json[F("channel_id2")].as<unsigned int>();
    strlcpy(read_api_key2, json[F("read_api_key2")] | "", sizeof(read_api_key2));
    */

    rest_var = json[F("rest_var")].as<unsigned int>();
    if (rest_var > 1) {
        rest_var = 1;
    }
    rest_ofs = json[F("rest_ofs")].as<int>();
    rest_neg = json[F("rest_neg")].as<bool>();

    reboot0 = json[F("reboot0")].as<bool>();

    switch_on = json[F("switch_on")].as<int>();
    switch_off = json[F("switch_off")].as<int>();
    switch_url_on = json[F("switch_url_on")].as<String>();
    switch_url_on.trim();
    switch_url_off = json[F("switch_url_off")].as<String>();
    switch_url_off.trim();
    switch_intervall = json[F("switch_intervall")].as<unsigned int>();

    developerModeEnabled = json[F("developerModeEnabled")].as<bool>();
}

void ConfigClass::applySettingsConfigGeneral()
{
    if (Config.log_sys) {
        Log.setLoglevel(CONFIG_LOG_DEFAULT_LEVEL, LOGMODULE_ALL);
    } else {
        Log.setLoglevel(LOGLEVEL_NONE, LOGMODULE_ALL);
    }

    AmisReader.setKey(Config.amis_key);

    RemoteOnOff.config(Config.switch_url_on, Config.switch_url_off, Config.switch_on, Config.switch_off, Config.switch_intervall);

    ThingSpeak.setInterval(Config.thingspeak_iv);
    ThingSpeak.setApiKeyWrite(Config.write_api_key);
    ThingSpeak.setEnabled(Config.thingspeak_aktiv);

    Webserver.reloadCredentials();

    // Config.Devicename könnte geändert worden sein! ==> ev MDNS neu starten!
    Network.restartMDNSIfNeeded();

    // Shelly
    ShellySmartmeterEmulation.disable();
    if (Config.shelly_smart_mtr_udp &&
        ShellySmartmeterEmulation.init(Config.shelly_smart_mtr_udp_device_index, Config.shelly_smart_mtr_udp_hardware_id_appendix, Config.shelly_smart_mtr_udp_offset))
    {
        ShellySmartmeterEmulation.enable();
    }

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
