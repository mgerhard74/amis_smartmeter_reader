#pragma once

#include <WString.h>


// Still a little bit of mess:
// Configuration files:: /config_wifi /config_mqtt /config_general
// Some values just being read and not stored here


class ConfigClass {

public:
    void init();
    void loadConfigGeneral();
    void applySettingsConfigGeneral();

    String DeviceName;

    bool log_sys;

    bool smart_mtr;

    bool use_auth;
    String auth_passwd;
    String auth_user;

    bool thingspeak_aktiv;
    unsigned thingspeak_iv;
    unsigned channel_id;
    String read_api_key;
    String write_api_key;
    unsigned channel_id2;
    String read_api_key2;

    unsigned rest_var; // 0 = Variablennamen im MQTT und bei "/rest" Abfrage mit ".", ansonsten mit "_"
    signed rest_ofs;
    bool rest_neg;

    bool reboot0;

    signed switch_on;
    signed switch_off;
    String switch_url_on;
    String switch_url_off;
    unsigned switch_intervall;

    String amis_key;

    bool developerModeEnabled;
    bool webserverTryGzipFirst;
};

extern ConfigClass Config;

/* vim:set ts=4 et: */
