#pragma once

#include <WString.h>


// Still a little bit of mess:
// Configuration files:: /config_wifi /config_mqtt /config_general
// Some values just being read and not stored here


class ConfigClass {

public:
    void loadConfigGeneral();
    void applySettingsConfigGeneral();


    String DeviceName;

    bool mqtt_retain;
    unsigned mqtt_qos;
    unsigned mqtt_keep;
    String mqtt_pub;
    String mqtt_sub;
    bool mqtt_ha_discovery;

    bool use_auth;
    bool log_sys;

    bool smart_mtr;

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
};

extern ConfigClass Config;

/* vim:set ts=4 et: */
