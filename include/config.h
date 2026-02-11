#pragma once

#include <WString.h>


// Still a little bit of mess:
// Configuration files:: /config_wifi /config_mqtt /config_general
// Some values just being read and not stored here



// DeviceName:
    // Verwendung in:
    //    MDNS.begin() ... da kann der Hostname nur 32 Zeichen lang werden!
    //    Webserver_Login - requestAuthentication() - nichts besonderes gefunden
    //    MQTT - habe jetzt mal nichts besonderes gefunden
    //    Network ... und da landet man dann irgendwann bei LwipIntf::hostname() (max 32 Zeichen)
    //      see: .platformio/packages/framework-arduinoespressif8266/cores/esp8266/LwipIntf.cpp
    //    Weboberfläche (index.html) ... auch hier auf 32 Zeichen begrenzt
#define CONFIG_DEVICENAME_MAXLEN        32

// 16 Zeichen lt. https://de.mathworks.com/help/thingspeak/channel-control.html
#define CONFIG_THINGSPEAK_KEY_MAXLEN    16

// TOD(StefanOberhumer): search definition
#define CONFIG_AMIS_KEY_MAXLEN          32


#define CONFIG_AUTH_USERNAME_MAXLEN     32
#define CONFIG_AUTH_PASSWORD_MAXLEN     32

#define CONFIG_JSON_CONFIG_GENERAL_DOCUMENT_SIZE    JSON_OBJECT_SIZE(33) + 1024


extern const char* Config_restValueKeys[2][9];

class ConfigClass {

public:
    void init();
    void loadConfigGeneral();
    void applySettingsConfigGeneral();
    static inline const char* getRestValueKeys(size_t index, unsigned rest_var/*=Config.rest_var*/)
        { return Config_restValueKeys[rest_var][index];}

    char DeviceName[CONFIG_DEVICENAME_MAXLEN + 1]; // maximale Länge: 32 + \0

    bool log_sys;

    bool smart_mtr;

    bool shelly_smart_mtr_udp;
    unsigned shelly_smart_mtr_udp_device_index;
    int shelly_smart_mtr_udp_offset;
    String shelly_smart_mtr_udp_hardware_id_appendix;

    bool use_auth;
    char auth_passwd[CONFIG_AUTH_PASSWORD_MAXLEN + 1];
    char auth_user[CONFIG_AUTH_USERNAME_MAXLEN +1];

    bool thingspeak_aktiv;
    unsigned thingspeak_iv;
    unsigned channel_id;
    // char read_api_key[CONFIG_THINGSPEAK_KEY_MAXLEN + 1];
    char write_api_key[CONFIG_THINGSPEAK_KEY_MAXLEN + 1];
    // unsigned channel_id2;
    // char read_api_key2[CONFIG_THINGSPEAK_KEY_MAXLEN + 1];

    unsigned rest_var; // 0 = Variablennamen im MQTT und bei "/rest" Abfrage mit ".", ansonsten mit "_"
    signed rest_ofs;
    bool rest_neg;

    bool reboot0;

    signed switch_on;
    signed switch_off;
    String switch_url_on;
    String switch_url_off;
    unsigned switch_intervall;

    char amis_key[CONFIG_AMIS_KEY_MAXLEN + 1]; // max Amiskey length = 32 + "\0"

    bool developerModeEnabled;

private:
    void loadConfigGeneralMinimal();
};

extern ConfigClass Config;

/* vim:set ts=4 et: */
