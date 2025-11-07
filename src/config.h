#pragma once

#include <WString.h>

class ConfigClass {
public:
    String DeviceName;
    bool mqtt_retain;
    unsigned mqtt_qos;
    unsigned mqtt_keep;
    String mqtt_pub;
    String mqtt_sub;
    bool mqtt_ha_discovery;
    uint8_t rfpower;
    bool mdns;
    bool use_auth;
    bool log_sys;
    bool smart_mtr;
    bool log_amis;
    String auth_passwd;
    String auth_user;
    bool thingspeak_aktiv;
    unsigned thingspeak_iv;
    unsigned channel_id;
    String read_api_key;
    String write_api_key;
    unsigned channel_id2;
    String read_api_key2;
    // 0 = Variablennamen im MQTT und bei "/rest" Abfrage mit ".", ansonsten mit "_"
    unsigned rest_var;
    signed rest_ofs;
    bool rest_neg;
    bool reboot0;
    signed switch_on;
    signed switch_off;
    String switch_url_on;
    String switch_url_off;
    unsigned switch_intervall;
    bool pingrestart_do;
    String pingrestart_ip;
    unsigned pingrestart_interval;
    unsigned pingrestart_max;
};

extern ConfigClass Config;

/* vim:set ts=4 et: */
